/*
 * tp-stream-engine.c - Source for TpStreamEngine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <string.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-constants.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-codec.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "misc-signals-marshal.h"

#ifdef USE_INFOPRINT
#include "statusbar-gen.h"
#endif

#include "tp-stream-engine-glue.h"

#include "common/telepathy-errors.h"
#include "common/telepathy-errors-enumtypes.h"

#include "channel.h"
#include "session.h"
#include "stream.h"
#include "types.h"
#include "xerrorhandler.h"

#define BUS_NAME        "org.freedesktop.Telepathy.StreamEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/StreamEngine"

#define STATUS_BAR_SERVICE_NAME "com.nokia.statusbar"
#define STATUS_BAR_INTERFACE_NAME "com.nokia.statusbar"
#define STATUS_BAR_OBJECT_PATH "/com/nokia/statusbar"

static void
register_dbus_signal_marshallers()
{
  /*register a marshaller for the NewMediaStreamHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED_UINT_UINT_UINT, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
     G_TYPE_INVALID);

  /*register a marshaller for the NewMediaSessionHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED_STRING, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the AddRemoteCandidate signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__STRING_BOXED, G_TYPE_NONE,
     G_TYPE_STRING, TP_TYPE_TRANSPORT_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetActiveCandidatePair signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__STRING_STRING, G_TYPE_NONE,
     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCandidateList signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_TYPE_CANDIDATE_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCodecs signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_TYPE_CODEC_LIST, G_TYPE_INVALID);
}

G_DEFINE_TYPE(TpStreamEngine, tp_stream_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
  HANDLING_CHANNEL,
  NO_MORE_CHANNELS,
  RECEIVING,
  SHUTDOWN_REQUESTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpStreamEnginePrivate TpStreamEnginePrivate;
struct _TpStreamEnginePrivate
{
  gboolean dispose_has_run;

  GstElement *pipeline;
  gboolean pipeline_playing;

  GPtrArray *channels;

  GSList *output_windows;
  GSList *preview_windows;

  guint bus_async_source_id;

  TpStreamEngineXErrorHandler *handler;
  guint bad_drawable_handler_id;
  guint bad_gc_handler_id;
  guint bad_value_handler_id;
  guint bad_window_handler_id;

#ifdef MAEMO_OSSO_SUPPORT
  DBusGProxy *infoprint_proxy;
#endif
};

#define TP_STREAM_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate))

typedef struct _WindowPair WindowPair;
struct _WindowPair
{
  TpStreamEngineStream *stream;
  GstElement *sink;
  guint window_id;
  volatile gboolean removing;
  gboolean created;
};

static void
_window_pairs_free (GSList **list)
{
  GSList *tmp;

  for (tmp = *list;
       tmp != NULL;
       tmp = tmp->next)
    g_slice_free (WindowPair, tmp->data);

  g_slist_free (*list);
  *list = NULL;
}

static void
_window_pairs_add (GSList **list, TpStreamEngineStream *stream, GstElement *sink, guint window_id)
{
  g_debug ("Adding to windowpair list sink %p window_id %d", sink, window_id);
  WindowPair *wp;

  wp = g_slice_new (WindowPair);
  wp->stream = stream;
  wp->sink = sink;
  wp->window_id = window_id;
  wp->removing = FALSE;
  wp->created = FALSE;

  *list = g_slist_prepend (*list, wp);
}

static void
_window_pairs_remove (GSList **list, WindowPair *pair)
{
  g_assert (g_slist_find (*list, pair));

  *list = g_slist_remove (*list, pair);

  g_slice_free (WindowPair, pair);
}

static WindowPair *
_window_pairs_find_by_removing (GSList *list, gboolean removing)
{
  GSList *tmp;

  for (tmp = list;
       tmp != NULL;
       tmp = tmp->next)
    {
      WindowPair *wp = tmp->data;
      if (wp->removing == removing)
        return wp;
    }

  return NULL;
}

static gboolean
_gst_bin_find_element (GstBin *bin, GstElement *element)
{
  GstIterator *iter;
  gpointer item;
  gboolean found = FALSE;

  iter = gst_bin_iterate_elements (bin);

  while (!found && gst_iterator_next (iter, &item) == GST_ITERATOR_OK)
    {
      GstElement *child = (GstElement *) item;

      if (child == element)
        found = TRUE;

      gst_object_unref (child);
    }

  gst_iterator_free (iter);
  return found;
}

static WindowPair *
_window_pairs_find_by_sink (GSList *list, GstElement *sink)
{
  GSList *tmp;

  for (tmp = list;
       tmp != NULL;
       tmp = tmp->next)
    {
      WindowPair *wp = tmp->data;
      if (wp->sink == sink)
        return wp;
      else if (GST_IS_BIN (wp->sink))
        if (_gst_bin_find_element (GST_BIN (wp->sink), GST_ELEMENT (sink)))
          return wp;
    }

  return NULL;
}

static WindowPair *
_window_pairs_find_by_window_id (GSList *list, guint window_id)
{
  GSList *tmp;

  for (tmp = list;
       tmp != NULL;
       tmp = tmp->next)
    {
      WindowPair *wp = tmp->data;
      if (wp->window_id == window_id)
        return wp;
    }

  return NULL;
}

GstElement *
tp_stream_engine_make_video_sink (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  const gchar *videosink_name;
  GstElement *sink = NULL;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *tmp;
#endif

  g_assert (priv->pipeline != NULL);

  if ((videosink_name = getenv ("FS_VIDEO_SINK")) || (videosink_name = getenv("FS_VIDEOSINK")))
    {
      g_debug ("making video sink with pipeline \"%s\"", videosink_name);
      sink = gst_parse_bin_from_description (videosink_name, TRUE, NULL);
    }
  else
    {
#ifndef MAEMO_OSSO_SUPPORT
      sink = gst_element_factory_make ("gconfvideosink", NULL);

      if (sink == NULL)
        sink = gst_element_factory_make ("autovideosink", NULL);
#endif

      if (sink == NULL)
        sink = gst_element_factory_make ("xvimagesink", NULL);

#ifndef MAEMO_OSSO_SUPPORT
      if (sink == NULL)
        sink = gst_element_factory_make ("ximagesink", NULL);
#endif
    }

  if (sink != NULL)
    {
      g_debug ("made video sink element %s", GST_ELEMENT_NAME (sink));

      if (g_object_has_property (G_OBJECT (sink), "sync"))
        {
          g_debug ("setting sync to FALSE");
          g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);
        }

      gst_bin_add (GST_BIN (priv->pipeline), sink);
    }
  else
    {
      g_debug ("failed to make a video sink");
      return NULL;
    }

#ifndef MAEMO_OSSO_SUPPORT
  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL);
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (tmp, sink);
      sink = tmp;
    }

  tmp = gst_element_factory_make ("videoscale", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videoscale");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (tmp, sink);
      sink = tmp;
    }
#endif

  return sink;
}

static gboolean
_add_preview_window (TpStreamEngine *obj, guint window_id, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  WindowPair *wp;
  GstElement *tee, *sink;
  GstStateChangeReturn state_change_ret;

  g_assert (priv->pipeline != NULL);

  g_debug ("%s: called for window id %d", G_STRFUNC, window_id);
  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp == NULL)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
          "Given window id %d not in windowID list", window_id);
      g_debug ("%s: Couldn't find xwindow id in window pair list", G_STRFUNC);
      return FALSE;
    }

  g_debug ("adding preview in window %u", window_id);

  tee = gst_bin_get_by_name (GST_BIN (priv->pipeline), "tee");
  sink = tp_stream_engine_make_video_sink (obj);

  if (sink == NULL)
    goto sink_failure;

  wp->created = TRUE;
  wp->sink = sink;

  g_debug ("trying to set sink to PLAYING");
  state_change_ret = gst_element_set_state (sink, GST_STATE_PLAYING);

  if (state_change_ret != GST_STATE_CHANGE_SUCCESS &&
      state_change_ret != GST_STATE_CHANGE_NO_PREROLL &&
      state_change_ret != GST_STATE_CHANGE_ASYNC)
    {
      g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
          "Failed to set element %s to PLAYING", GST_ELEMENT_NAME (sink));
      goto link_failure;
    }

  g_debug ("trying to link tee and sink");
  if (!gst_element_link (tee, sink))
    {
      g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
          "Failed to link element %s to %s", GST_ELEMENT_NAME (tee),
          GST_ELEMENT_NAME (sink));
      goto link_failure;
    }

  gst_object_unref (tee);
  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);
  return TRUE;

link_failure:
  gst_element_set_state (sink, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (priv->pipeline), sink);

sink_failure:
  gst_object_unref (tee);

  if (error != NULL)
    g_warning ((*error)->message);

  _window_pairs_remove (&(priv->preview_windows), wp);

  return FALSE;
}

static void
_add_pending_preview_windows (TpStreamEngine *engine)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);

  g_debug ("%s: called", G_STRFUNC);

  GSList *tmp;

  for (tmp = priv->preview_windows;
       tmp != NULL;
       tmp = tmp->next)
    {
      WindowPair *wp = tmp->data;
      if (wp->created == FALSE)
        {
          GError *error = NULL;
          _add_preview_window (engine, wp->window_id, &error);

          if (error != NULL)
            g_error_free (error);
        }
    }
}

#ifdef USE_INFOPRINT
static void
tp_stream_engine_infoprint (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  TpStreamEnginePrivate *priv = (TpStreamEnginePrivate *)user_data;
  com_nokia_statusbar_system_note_infoprint (
          DBUS_G_PROXY (priv->infoprint_proxy),
          message, NULL);
  g_log_default_handler (log_domain, log_level, message, user_data);
}
#endif

static gboolean
bad_misc_cb (TpStreamEngineXErrorHandler *handler,
             guint window_id,
             gpointer data);

static gboolean
bad_other_cb (TpStreamEngineXErrorHandler *handler,
              guint window_id,
              gpointer data);

static gboolean
bad_value_cb (TpStreamEngineXErrorHandler *handler,
              guint window_id,
              gpointer data);

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data);

static void
tp_stream_engine_init (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  priv->channels = g_ptr_array_new ();

  priv->handler = tp_stream_engine_x_error_handler_get ();

  priv->bad_drawable_handler_id =
    g_signal_connect (priv->handler, "bad-drawable", (GCallback) bad_misc_cb,
      obj);
  priv->bad_gc_handler_id =
    g_signal_connect (priv->handler, "bad-gc", (GCallback) bad_other_cb,
      obj);
  priv->bad_value_handler_id =
    g_signal_connect (priv->handler, "bad-value", (GCallback) bad_value_cb,
      obj);
  priv->bad_window_handler_id =
    g_signal_connect (priv->handler, "bad-window", (GCallback) bad_window_cb,
      obj);

#ifdef USE_INFOPRINT
  priv->infoprint_proxy =
    dbus_g_proxy_new_for_name (tp_get_bus(),
        STATUS_BAR_SERVICE_NAME,
        STATUS_BAR_OBJECT_PATH,
        STATUS_BAR_INTERFACE_NAME);

  g_debug ("Using infoprint %p", priv->infoprint_proxy);
  /* handler for stream-engine messages */
  g_log_set_handler (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL |
      G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, tp_stream_engine_infoprint, priv);

  /* handler for farsight messages */
  /*
  g_log_set_handler ("Farsight", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_CRITICAL |
      G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, tp_stream_engine_infoprint, NULL);
      */

#endif
}

static void tp_stream_engine_dispose (GObject *object);
static void tp_stream_engine_finalize (GObject *object);

static void
tp_stream_engine_class_init (TpStreamEngineClass *tp_stream_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_stream_engine_class);

  g_type_class_add_private (tp_stream_engine_class, sizeof (TpStreamEnginePrivate));

  object_class->dispose = tp_stream_engine_dispose;
  object_class->finalize = tp_stream_engine_finalize;

  /**
   * TpStreamEngine::handling-channel:
   *
   * Emitted whenever this object starts handling a channel
   */
  signals[HANDLING_CHANNEL] =
  g_signal_new ("handling-channel",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * TpStreamEngine::no-more-channels:
   *
   * Emitted whenever this object is handling no channels
   */
  signals[NO_MORE_CHANNELS] =
  g_signal_new ("no-more-channels",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

   /**
   * TpStreamEngine::receiving:
   *
   * Emitted whenever a stream is received
   */
  signals[RECEIVING] =
    g_signal_new ("receiving",
        G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        tp_stream_engine_marshal_VOID__STRING_INT_BOOLEAN,
        G_TYPE_NONE, 3, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_BOOLEAN);

  /**
   * TpStreamEngine::shutdown:
   *
   * Emitted whenever stream engine needs to be shutdown
   */
  signals[SHUTDOWN_REQUESTED] =
    g_signal_new ("shutdown-requested",
        G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_stream_engine_class), &dbus_glib_tp_stream_engine_object_info);
}

void
tp_stream_engine_dispose (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  if (priv->channels)
    {
      guint i;

      for (i = 0; i < priv->channels->len; i++)
        g_object_unref (g_ptr_array_index (priv->channels, i));

      g_ptr_array_free (priv->channels, TRUE);
      priv->channels = NULL;
    }

  if (priv->pipeline)
    {
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_object_unref (priv->pipeline);
      priv->pipeline = NULL;
    }

  if (priv->preview_windows != NULL)
    {
      _window_pairs_free (&(priv->preview_windows));
    }

  if (priv->output_windows != NULL)
    {
      _window_pairs_free (&(priv->output_windows));
    }

  if (priv->bus_async_source_id)
    {
      g_source_remove (priv->bus_async_source_id);
      priv->bus_async_source_id = 0;
    }

  if (priv->bad_drawable_handler_id)
    {
      g_signal_handler_disconnect (priv->handler, priv->bad_drawable_handler_id);
      priv->bad_drawable_handler_id = 0;
    }

  if (priv->bad_gc_handler_id)
    {
      g_signal_handler_disconnect (priv->handler, priv->bad_gc_handler_id);
      priv->bad_gc_handler_id = 0;
    }

  if (priv->bad_value_handler_id)
    {
      g_signal_handler_disconnect (priv->handler, priv->bad_value_handler_id);
      priv->bad_value_handler_id = 0;
    }

  if (priv->bad_window_handler_id)
    {
      g_signal_handler_disconnect (priv->handler, priv->bad_window_handler_id);
      priv->bad_window_handler_id = 0;
    }

  g_object_unref (priv->handler);

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose (object);
}

void
tp_stream_engine_finalize (GObject *object)
{
#ifdef MAEMO_OSSO_SUPPORT
  TpStreamEngine *self = TP_STREAM_ENGINE (object);

  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  if (priv->infoprint_proxy)
    {
      g_debug ("priv->infoprint_proxy->ref_count before unref == %d", G_OBJECT (priv->infoprint_proxy)->ref_count);
      g_object_unref (priv->infoprint_proxy);
      priv->infoprint_proxy = NULL;
    }
#endif

  G_OBJECT_CLASS (tp_stream_engine_parent_class)->finalize (object);
}

/**
 * tp_stream_engine_error
 *
 * Used to inform the stream engine than an exceptional situation has ocurred.
 *
 * @error:   The error ID, as per the
 *           org.freedesktop.Telepathy.Media.StreamHandler::Error signal.
 * @message: The human-readable error message.
 */
void
tp_stream_engine_error (TpStreamEngine *self, int error, const char *message)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);
  guint i;

  for (i = 0; i < priv->channels->len; i++)
    tp_stream_engine_channel_error (
      g_ptr_array_index (priv->channels, i), error, message);
}

static void
check_if_busy (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);
  guint num_previews = g_slist_length (priv->preview_windows);

  if (priv->channels->len == 0 && num_previews == 0)
    {
      g_debug ("no channels or previews remaining; emitting no-more-channels");
      g_signal_emit (self, signals[NO_MORE_CHANNELS], 0);
    }
  else
    {
      g_debug ("channels remaining: %d", priv->channels->len);
      g_debug ("preview windows remaining: %d", num_previews);
    }
}


static void
channel_closed (TpStreamEngineChannel *chan, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  g_debug ("channel closed: %p", chan);
  g_ptr_array_remove_fast (priv->channels, chan);
  g_object_unref (chan);
  check_if_busy (self);
}


static void
_remove_defunct_preview_sinks (TpStreamEngine *engine, gboolean clear_wp_list)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp = NULL;

  while ((wp = _window_pairs_find_by_removing (priv->preview_windows, TRUE)) !=
      NULL)
    {
      GstElement *tee;

      g_debug ("%s: removing sink for preview window ID %u", G_STRFUNC,
          wp->window_id);

      if (wp->created)
        {
          g_assert (wp->sink != NULL);
          tee = gst_bin_get_by_name (GST_BIN (priv->pipeline), "tee");
          g_assert (tee != NULL);
          gst_element_unlink (tee, wp->sink);
          gst_object_unref (GST_OBJECT (tee));
          g_debug ("unlinked sink from tee, now setting state to NULL");

          gst_element_set_state (wp->sink, GST_STATE_NULL);
          g_debug ("Done setting state to NULL, now removing from bin");
          gst_bin_remove (GST_BIN (priv->pipeline), wp->sink);
          g_debug ("Done removing from bin, calling _window_pairs_remove"
              "refcount %d", GST_OBJECT_REFCOUNT_VALUE (wp->sink));
        }
      else
        {
          g_debug ("No sink created yet, removing window_pair");
          g_assert (wp->sink == NULL);
        }

      if (clear_wp_list)
        {
          _window_pairs_remove (&(priv->preview_windows), wp);
        }
      else
        {
          wp->sink = NULL;
          wp->created = FALSE;
          wp->stream = NULL;
          wp->removing = FALSE;
        }
      g_debug ("Done _window_pairs_remove");
    }

    check_if_busy (engine);
}

static void
_remove_defunct_output_sinks (TpStreamEngine *engine)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp = NULL;

  while ((wp = _window_pairs_find_by_removing (priv->output_windows, TRUE)) !=
      NULL)
    {
      GError *error;

      g_debug ("%s: removing sink for output window ID %u", G_STRFUNC,
          wp->window_id);

      if (!tp_stream_engine_stream_set_output_window (wp->stream, 0, &error))
        {
          g_debug ("%s: got error: %s", G_STRFUNC, error->message);
          g_error_free (error);
        }
    }

  check_if_busy (engine);
}


static void
close_all_video_streams (TpStreamEngine *self, const gchar *message)
{
  g_debug ("Closing all video streams");
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);
  guint i, j, k;

  for (i = 0; i < priv->channels->len; i++)
    {
      TpStreamEngineChannel *channel = g_ptr_array_index (
            priv->channels, i);

      for (j = 0; j < channel->sessions->len; j++)
        {
          TpStreamEngineSession *session = g_ptr_array_index (
              channel->sessions, j);

          for (k = 0; k < session->streams->len; k++)
            {
              TpStreamEngineStream *stream = g_ptr_array_index (
                session->streams, k);

              if (stream->media_type == TP_MEDIA_STREAM_TYPE_VIDEO)
                tp_stream_engine_stream_error (stream,
                    TP_MEDIA_STREAM_ERROR_UNKNOWN, message);
            }
        }
    }
}


static gboolean
bus_async_handler (GstBus *bus,
                   GstMessage *message,
                   gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  GError *error = NULL;
  WindowPair *wp;
  gchar *error_string;

  GstElement *source = GST_ELEMENT (GST_MESSAGE_SRC (message));
  gchar *name = gst_element_get_name (source);

  switch (GST_MESSAGE_TYPE (message))
    {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (message, &error, &error_string);

        g_debug ("%s: got error from %s", G_STRFUNC, name);
        g_debug ("%s: got error: %s %s %d %d", G_STRFUNC, error->message,
            error_string, error->domain, error->code);
        g_free (error_string);

        /*
        TpStreamEngineStream *stream = NULL;
        guint xid;
        gboolean is_preview = TRUE;
        */
        if (g_strrstr (name, "xvimagesink") &&
            error->domain == GST_RESOURCE_ERROR &&
            (error->code == GST_RESOURCE_ERROR_WRITE ||
             error->code == GST_RESOURCE_ERROR_BUSY))
          {
            g_debug ("%s: error from xvimagesink, shutting down video streams",
                G_STRFUNC);
            wp = _window_pairs_find_by_sink (priv->preview_windows,
                source);

            if (wp == NULL)
            {
              /* is_preview = FALSE; */
              wp = _window_pairs_find_by_sink (priv->output_windows,
                  source);
            }

            if (wp != NULL)
              {
                g_debug ("%s: sink for %s window (id %u) has gone, removing",
                        G_STRFUNC, wp->stream == NULL ? "preview" : "output",
                        wp->window_id);

                wp->removing = TRUE;
                /*
                xid = wp->window_id;
                stream = wp->stream;
                */
                _remove_defunct_preview_sinks (engine, TRUE);
                _remove_defunct_output_sinks (engine);

                /* let's try recreating a new xvimagesink */
                /*
                if (is_preview)
                  {
                    tp_stream_engine_add_preview_window (engine, xid, &error);
                  }
                else
                  {
                    g_debug ("adding new output window with xid %d and stream %p", xid, stream);
                    tp_stream_engine_stream_set_output_window (stream, xid,
                        &error);
                  }
                */
                /* let's shutdown the video stream */
                close_all_video_streams (engine, error->message);
              }
          }
        else
          {
            GSList *i;

            g_debug ("%s: got an error on the video pipeline: %s", G_STRFUNC,
                error->message);
            g_debug ("%s: will teardown video pipeline and try a new one",
                G_STRFUNC);

            close_all_video_streams (engine, error->message);

            g_debug ("%s: destroying video pipeline", G_STRFUNC);

            for (i = priv->output_windows; i; i = i->next)
              ((WindowPair *) i->data)->removing = TRUE;

            for (i = priv->preview_windows; i; i = i->next)
              ((WindowPair *) i->data)->removing = TRUE;

            _remove_defunct_preview_sinks (engine, FALSE);
            _remove_defunct_output_sinks (engine);
            gst_element_set_state (priv->pipeline, GST_STATE_NULL);
            gst_object_unref (priv->pipeline);
            priv->pipeline = NULL;

            g_debug ("%s: Creating new pipeline", G_STRFUNC);
            priv->pipeline = tp_stream_engine_get_pipeline (engine);
          }
        g_error_free (error);
        break;
      case GST_MESSAGE_WARNING:
        gst_message_parse_warning (message, &error, NULL);
        g_warning ("%s: got warning: %s", G_STRFUNC, error->message);
        g_error_free (error);
        break;
      case GST_MESSAGE_STATE_CHANGED:
        {
          GstState new_state;
          GstState old_state;
          GstState pending_state;
          gst_message_parse_state_changed (message, &old_state, &new_state,
              &pending_state);
          /*
          g_debug ("State changed for %s refcount %d old:%s new:%s pending:%s",
              name,
              GST_OBJECT_REFCOUNT_VALUE (source),
              gst_element_state_get_name (old_state),
              gst_element_state_get_name (new_state),
              gst_element_state_get_name (pending_state));
          */
          if (source == priv->pipeline)
            {
              if (new_state == GST_STATE_PLAYING)
                {
                  g_debug ("Pipeline is playing, setting to TRUE");
                  priv->pipeline_playing = TRUE;
                  _add_pending_preview_windows (engine);
                }
              else
                {
                  g_debug ("Pipeline is not playing, setting to FALSE");
                  priv->pipeline_playing = FALSE;
                }
            }
          break;
        }
      default:
        break;
    }

  g_free (name);
  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus *bus, GstMessage *message, gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  GstElement *element;
  WindowPair *wp = NULL;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_structure_has_name (message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;

  element = GST_ELEMENT (GST_MESSAGE_SRC (message));

  g_debug ("got prepare-xwindow-id message from %s",
      gst_element_get_name (element));

  while (element != NULL)
    {
      wp = _window_pairs_find_by_sink (priv->output_windows, element);

      if (wp == NULL)
        wp = _window_pairs_find_by_sink (priv->preview_windows, element);

      if (wp != NULL)
        break;

      element = GST_ELEMENT_PARENT (element);
    }

  if (wp == NULL)
    return GST_BUS_PASS;

  g_debug ("Giving video sink %p window id %d", wp->sink, wp->window_id);
  gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (GST_MESSAGE_SRC (message)),
      wp->window_id);

  if (g_object_has_property (G_OBJECT (GST_MESSAGE_SRC (message)), "sync"))
    {
      g_debug ("setting sync to FALSE");
      g_object_set (G_OBJECT (GST_MESSAGE_SRC (message)), "sync", FALSE, NULL);
    }

  gst_message_unref (message);

  return GST_BUS_DROP;
}

static void
_create_pipeline (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  GstElement *videosrc = NULL;
  GstElement *tee;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *tmp;
#endif
  GstBus *bus;
  GstCaps *filter = NULL;
  GstElement *fakesink;
  const gchar *elem;
  const gchar *caps_str;

  priv->pipeline = gst_pipeline_new (NULL);
  tee = gst_element_factory_make ("tee", "tee");
  fakesink = gst_element_factory_make ("fakesink", NULL);

  if ((elem = getenv ("FS_VIDEO_SRC")) || (elem = getenv ("FS_VIDEOSRC")))
    {
      g_debug ("making video src with pipeline \"%s\"", elem);
      videosrc = gst_parse_bin_from_description (elem, TRUE, NULL);
      g_assert (videosrc);
      gst_element_set_name (videosrc, "videosrc");
    }
  else
    {
#ifdef MAEMO_OSSO_SUPPORT
      videosrc = gst_element_factory_make ("gconfv4l2src", NULL);
#else
      videosrc = gst_element_factory_make ("gconfvideosrc", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4l2src", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4lsrc", NULL);
#endif

      if (videosrc != NULL)
        g_debug ("using %s as video source", GST_ELEMENT_NAME (videosrc));
      else
        g_debug ("failed to create video source");
    }

  if ((caps_str = getenv ("FS_VIDEO_SRC_CAPS")) || (caps_str = getenv ("FS_VIDEOSRC_CAPS")))
    {
      filter = gst_caps_from_string (caps_str);
    }

  if (!filter)
    {
      filter = gst_caps_new_simple ("video/x-raw-yuv",
           "width", G_TYPE_INT, 176,
           "height", G_TYPE_INT, 144,
           "framerate", GST_TYPE_FRACTION, 15, 1,
#ifndef MAEMO_OSSO_SUPPORT
           "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
#endif
           NULL);
    }
  else
    {
      g_debug ("applying custom caps '%s' on the video source\n", caps_str);
    }

  gst_bin_add_many (GST_BIN (priv->pipeline), videosrc, tee, fakesink,
      NULL);

#ifndef MAEMO_OSSO_SUPPORT
  tmp = gst_element_factory_make ("videorate", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videorate");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }

  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL);
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }
#endif

  gst_element_link (videosrc, tee);
  gst_element_link_filtered (tee, fakesink, filter);
  gst_caps_unref (filter);

  g_debug ("Setting pipeline to playing");
  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  /* connect a callback to the stream bus so that we can set X window IDs
   * at the right time, and detect when sinks have gone away */
  bus = gst_element_get_bus (priv->pipeline);
  gst_bus_set_sync_handler (bus, bus_sync_handler, obj);
  priv->bus_async_source_id =
    gst_bus_add_watch (bus, bus_async_handler, obj);
  gst_object_unref (bus);
}

/*
 * tp_stream_engine_get_pipeline
 *
 * Return the GStreamer pipeline belonging to the stream engine. Caller does
 * not own a reference to the pipeline.
 */

GstElement *
tp_stream_engine_get_pipeline (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  if (NULL == priv->pipeline)
    {
      _create_pipeline (obj);
    }

  return priv->pipeline;
}


static gboolean
_remove_defunct_sinks_idle_cb (TpStreamEngine *engine)
{
  _remove_defunct_preview_sinks (engine, TRUE);
  _remove_defunct_output_sinks (engine);

  return FALSE;
}

/**
 * tp_stream_engine_add_preview_window
 *
 * Implements DBus method AddPreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_add_preview_window (TpStreamEngine *obj,
    guint window_id, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  WindowPair *wp;

  g_debug ("%s: called", G_STRFUNC);

  if (priv->pipeline == NULL)
    {
      _create_pipeline (obj);
    }

  /* try and remove any sinks which have removing = TRUE to free up Xv ports */
  _remove_defunct_preview_sinks (obj, TRUE);
  _remove_defunct_output_sinks (obj);

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp != NULL)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "window ID %u is already a preview window", window_id);
      return FALSE;
    }

  if (!priv->pipeline_playing)
    {
      g_debug ("%s: pipeline not playing, adding later", G_STRFUNC);
      _window_pairs_add (&(priv->preview_windows), NULL, NULL, window_id);
      return FALSE;
    }
  else
    {
      g_debug ("%s: pipeline playing, adding now", G_STRFUNC);
      _window_pairs_add (&(priv->preview_windows), NULL, NULL, window_id);
      return _add_preview_window (obj, window_id, error);
    }
}

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data)
{
  TpStreamEngine *engine = data;
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (priv->output_windows, window_id);

  if (wp == NULL)
    {
      g_debug ("%s: BadWindow(%u) not for a preview or output window, not "
          "handling", G_STRFUNC, window_id);
      return FALSE;
    }

  if (wp->removing)
    {
      g_debug ("%s: BadWindow(%u) for a %s window being removed, ignoring",
          G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");
      return TRUE;
    }

  g_debug ("%s: BadWindow(%u) for a %s window, scheduling sink removal",
      G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");

  /* set removing to TRUE so that we know this window ID is being removed and X
   * errors can be ignored */
  wp->removing = TRUE;

  g_idle_add ((GSourceFunc) _remove_defunct_sinks_idle_cb, engine);
  g_main_context_wakeup (NULL);

  return TRUE;
}


static gboolean
bad_misc_cb (TpStreamEngineXErrorHandler *handler,
             guint window_id,
             gpointer data)
{
  TpStreamEngine *engine = data;
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (priv->output_windows, window_id);

  if (wp == NULL)
    {
      g_debug ("%s: BadDrawable(%u) not for a preview or output window, not "
          "handling", G_STRFUNC, window_id);
      return FALSE;
    }

  if (!wp->removing)
    {
      g_debug ("%s: BadDrawable(%u) for a %s window not being removed, not "
          "handling", G_STRFUNC, window_id,
          wp->stream == NULL ? "preview" : "output");
      return FALSE;
    }

  g_debug ("%s: BadDrawable(%u) for a %s window being removed, ignoring",
      G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");

  return TRUE;
}


static gboolean
bad_other_cb (TpStreamEngineXErrorHandler *handler,
              guint gc_id,
              gpointer data)
{
  TpStreamEngine *engine = data;
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_removing (priv->preview_windows, TRUE);

  if (wp == NULL)
    wp = _window_pairs_find_by_removing (priv->output_windows, TRUE);

  if (wp == NULL)
    {
      g_debug ("%s: BadGC(%u) when no preview or output windows are being "
          "removed, not handling", G_STRFUNC, gc_id);
      return FALSE;
    }

  g_debug ("%s: BadGC(%u) when a preview or output window is being removed, "
      "ignoring", G_STRFUNC, gc_id);

  return TRUE;
}


static gboolean
bad_value_cb (TpStreamEngineXErrorHandler *handler,
              guint window_id,
              gpointer data)
{
  TpStreamEngine *engine = data;
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (priv->output_windows, window_id);

  if (wp == NULL)
    {
      g_debug ("%s: BadValue(%u) not for a preview or output window, not "
          "handling", G_STRFUNC, window_id);
      return FALSE;
    }

  g_debug ("%s: BadValue(%u) for a %s window being removed, ignoring",
      G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");

  return TRUE;
}


/**
 * tp_stream_engine_remove_preview_window
 *
 * Implements DBus method RemovePreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_remove_preview_window (TpStreamEngine *obj, guint window_id, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  WindowPair *wp;

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp == NULL)
    {
      /* FIXME: set *error here, or just remove this entire method... its kinda
       * useless */
      return FALSE;
    }

  if (wp->removing)
    {
      /* already being removed, nothing to do */
      return TRUE;
    }

  wp->removing = TRUE;

  _remove_defunct_preview_sinks (obj, TRUE);

  return TRUE;
}


gboolean
tp_stream_engine_add_output_window (TpStreamEngine *obj,
                                    TpStreamEngineStream *stream,
                                    GstElement *sink,
                                    guint window_id)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  _remove_defunct_preview_sinks (obj, TRUE);
  _remove_defunct_output_sinks (obj);

  _window_pairs_add (&(priv->output_windows), stream, sink, window_id);

  return TRUE;
}


gboolean
tp_stream_engine_remove_output_window (TpStreamEngine *obj,
                                       guint window_id)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  WindowPair *wp;

  wp = _window_pairs_find_by_window_id (priv->output_windows, window_id);

  if (wp == NULL)
    return FALSE;

  _window_pairs_remove (&(priv->output_windows), wp);

  return TRUE;
}


/**
 * tp_stream_engine_handle_channel
 *
 * Implements DBus method HandleChannel
 * on interface org.freedesktop.Telepathy.ChannelHandler
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_handle_channel (TpStreamEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  TpStreamEngineChannel *chan = NULL;

  g_debug("HandleChannel called");

  if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA) != 0)
    {
      const gchar *message =
        "Stream Engine was passed a channel that was not a "
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA;
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument, message);
      g_message (message);
      goto ERROR;
     }

  chan = tp_stream_engine_channel_new ();

  if (!tp_stream_engine_channel_go (chan, bus_name, connection, channel,
      handle_type, handle, error))
    goto ERROR;

  g_ptr_array_add (priv->channels, chan);

  g_signal_connect (chan, "closed", G_CALLBACK (channel_closed), obj);

  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);

  return TRUE;

ERROR:
  if (chan)
    g_object_unref (chan);

  return FALSE;
}

void
tp_stream_engine_register (TpStreamEngine *self)
{
  DBusGConnection *bus;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint request_name_result;

  g_assert (TP_IS_STREAM_ENGINE (self));

  bus = tp_get_bus ();
  bus_proxy = tp_get_bus_proxy ();

  g_debug("Requesting " BUS_NAME);

  if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
                          G_TYPE_STRING, BUS_NAME,
                          G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, stream engine already running?");

  g_debug("registering StreamEngine at " OBJECT_PATH);
  dbus_g_connection_register_g_object (bus, OBJECT_PATH, G_OBJECT (self));

  register_dbus_signal_marshallers();
}

static TpStreamEngineStream *
_lookup_stream (TpStreamEngine *obj, const gchar *path, guint stream_id,
  GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  guint i, j, k;

  for (i = 0; i < priv->channels->len; i++)
    {
      TpStreamEngineChannel *channel = TP_STREAM_ENGINE_CHANNEL (
        priv->channels->pdata[i]);

      if (0 == strcmp (path, channel->channel_path))
        {
          for (j = 0; j < channel->sessions->len; j++)
            {
              TpStreamEngineSession *session = TP_STREAM_ENGINE_SESSION (
                channel->sessions->pdata[j]);

              for (k = 0; k < session->streams->len; k++)
                {
                  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (
                    session->streams->pdata[k]);

                  if (stream_id == stream->stream_id)
                    return stream;
                }
            }

          *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
            "the channel %s has no stream with id %d", path, stream_id);
          return NULL;
        }
    }

  *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
    "stream-engine is not handling the channel %s", path);
  return NULL;
}


/**
 * tp_stream_engine_mute_input
 *
 * Implements DBus method MuteInput
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_mute_input (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, gboolean mute_state, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_mute_input (stream, mute_state, error);
}

/**
 * tp_stream_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_mute_output (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, gboolean mute_state, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_mute_output (stream, mute_state, error);
}


/**
 * tp_stream_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_volume (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, guint volume, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_set_output_volume (stream, volume, error);
}

/**
 * tp_stream_engine_set_output_window
 *
 * Implements DBus method SetOutputWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_window (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, guint window_id, GError **error)
{
  TpStreamEngineStream *stream;

  g_debug ("%s: channel_path=%s, stream_id=%u, window_id=%u", G_STRFUNC,
      channel_path, stream_id, window_id);

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    {
      g_debug ("%s: stream not found, not doing anything!", G_STRFUNC);
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument, "stream not "
          "found, not doing anything");
      return FALSE;
    }

  return tp_stream_engine_stream_set_output_window (stream, window_id, error);
}

/*
 * tp_stream_engine_get
 *
 * Return the stream engine singleton. Caller does not own a reference to the
 * stream engine.
 */

TpStreamEngine *
tp_stream_engine_get ()
{
  static TpStreamEngine *engine = NULL;

  if (NULL == engine)
    {
      engine = g_object_new (TP_TYPE_STREAM_ENGINE, NULL);
      g_object_add_weak_pointer (G_OBJECT (engine), (gpointer) &engine);
    }

  return engine;
}

/*
 * tp_stream_engine_emit_received
 *
 * Triggers stream engine to emit the TpStreamEngine::receiving signal
 *
 */
void
tp_stream_engine_emit_receiving (TpStreamEngine *obj, gchar *channel_path, guint
    stream_id, gboolean state)
{
  g_signal_emit (G_OBJECT (obj), signals[RECEIVING], 0, channel_path,
      stream_id, TRUE);
}

/**
 * tp_stream_engine_shutdown
 *
 * Implements DBus method Shutdown
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_shutdown (TpStreamEngine *obj, GError **error)
{
  g_debug ("%s: Emitting shutdown signal", G_STRFUNC);
  g_signal_emit (obj, signals[SHUTDOWN_REQUESTED], 0);
  return TRUE;
}

