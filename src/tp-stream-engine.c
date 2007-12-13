/*
 * tp-stream-engine.c - Source for TpStreamEngine
 * Copyright (C) 2005-2007 Collabora Ltd.
 * Copyright (C) 2005-2007 Nokia Corporation
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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/gtypes.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-codec.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"

#include "api/api.h"
#include "channel.h"
#include "session.h"
#include "stream.h"
#include "util.h"
#include "xerrorhandler.h"

#define BUS_NAME        "org.freedesktop.Telepathy.StreamEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/StreamEngine"

static void
register_dbus_signal_marshallers()
{
  /*register a marshaller for the NewMediaStreamHandler signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__BOXED_UINT_UINT_UINT, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
     G_TYPE_INVALID);

  /*register a marshaller for the NewMediaSessionHandler signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__BOXED_STRING, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the AddRemoteCandidate signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__STRING_BOXED, G_TYPE_NONE,
     G_TYPE_STRING, TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetActiveCandidatePair signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__STRING_STRING, G_TYPE_NONE,
     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCandidateList signal*/
  dbus_g_object_register_marshaller
    (g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_CANDIDATE_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCodecs signal*/
  dbus_g_object_register_marshaller
    (g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_CODEC_LIST, G_TYPE_INVALID);
}

static void ch_iface_init (gpointer, gpointer);
static void se_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TpStreamEngine, tp_stream_engine, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (STREAM_ENGINE_TYPE_SVC_CHANNEL_HANDLER,
      ch_iface_init);
    G_IMPLEMENT_INTERFACE (STREAM_ENGINE_TYPE_SVC_STREAM_ENGINE,
      se_iface_init))

/* signal enum */
enum
{
  HANDLING_CHANNEL,
  NO_MORE_CHANNELS,
  SHUTDOWN_REQUESTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
struct _TpStreamEnginePrivate
{
  gboolean dispose_has_run;

  TpDBusDaemon *dbus_daemon;

  GstElement *pipeline;
  gboolean pipeline_playing;

  GPtrArray *channels;
  GHashTable *channels_by_path;

  GSList *output_windows;
  GSList *preview_windows;

  guint bus_async_source_id;

  TpStreamEngineXErrorHandler *handler;
  guint bad_drawable_handler_id;
  guint bad_gc_handler_id;
  guint bad_value_handler_id;
  guint bad_window_handler_id;
};

typedef struct _WindowPair WindowPair;
struct _WindowPair
{
  TpStreamEngineStream *stream;
  GstElement *sink;
  guint window_id;
  volatile gboolean removing;
  gboolean created;
  void (*post_remove) (WindowPair *wp);
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
  wp->post_remove = NULL;

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
tp_stream_engine_make_video_sink (TpStreamEngine *obj, gboolean is_preview)
{
  const gchar *videosink_name;
  GstElement *sink = NULL;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *bin, *tmp;
  GstPad *pad;
#endif

  g_assert (obj->priv->pipeline != NULL);

  if ((videosink_name = getenv ("PREVIEW_VIDEO_SINK")) ||
      (videosink_name = getenv ("FS_VIDEO_SINK")) ||
      (videosink_name = getenv ("FS_VIDEOSINK")))
    {
      g_debug ("making video sink for local preview with pipeline \"%s\"", videosink_name);
      sink = gst_parse_bin_from_description (videosink_name, TRUE, NULL);
    }
  else
    {
#ifndef MAEMO_OSSO_SUPPORT
      if (is_preview) {
        /* hack to leave an xvimage free for the bigger output.
         * Most machines only have one xvport, so this helps in the majority of
         * cases. More intelligent widgets 
         * */
        sink = gst_element_factory_make ("ximagesink", NULL);
      }

      if (sink == NULL)
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
      if (g_object_has_property (G_OBJECT (sink), "qos"))
        {
          g_debug ("setting qos to FALSE");
          g_object_set (G_OBJECT (sink), "qos", FALSE, NULL);
        }
#ifndef MAEMO_OSSO_SUPPORT
      if (g_object_has_property (G_OBJECT (sink), "force-aspect-ratio"))
        {
          g_debug ("setting force-aspect-ratio to TRUE");
          g_object_set (G_OBJECT (sink), "force-aspect-ratio", TRUE, NULL);
        }
#endif
      /* Setting this will make sure we can have several preview windows using
       * the tee without any queue elements */
      /* Without this, elements linked to the tee just block on prerolling and
       * wait for each other to finish */
      if (g_object_has_property (G_OBJECT (sink), "preroll-queue-len"))
        {
          g_debug ("setting preroll-queue-len to 1");
          g_object_set (G_OBJECT (sink), "preroll-queue-len", TRUE, NULL);
        }
    }
  else
    {
      g_debug ("failed to make a video sink");
      return NULL;
    }

#ifndef MAEMO_OSSO_SUPPORT
  bin = gst_bin_new (NULL);
  gst_bin_add (GST_BIN (bin), sink);

  tmp = gst_element_factory_make ("videoscale", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videoscale");
      gst_bin_add (GST_BIN (bin), tmp);
      gst_element_link (tmp, sink);
      sink = tmp;
    }

  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (bin), tmp);
      gst_element_link (tmp, sink);
      sink = tmp;
    }

  pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SINK);
  gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  sink = bin;
#endif

  gst_bin_add (GST_BIN (obj->priv->pipeline), sink);

  return sink;
}

static gboolean
_add_preview_window (TpStreamEngine *obj, guint window_id, GError **error)
{
  WindowPair *wp;
  GstElement *tee, *sink;
  GstStateChangeReturn state_change_ret;

  g_assert (obj->priv->pipeline != NULL);

  g_debug ("%s: called for window id %d", G_STRFUNC, window_id);
  wp = _window_pairs_find_by_window_id (obj->priv->preview_windows, window_id);

  if (wp == NULL)
    {
      *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Given window id %d not in windowID list", window_id);
      g_debug ("%s: Couldn't find xwindow id in window pair list", G_STRFUNC);
      return FALSE;
    }

  g_debug ("adding preview in window %u", window_id);

  tee = gst_bin_get_by_name (GST_BIN (obj->priv->pipeline), "tee");
  sink = tp_stream_engine_make_video_sink (obj, TRUE);

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

  g_debug ("linked tee and sink");

  gst_object_unref (tee);
  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);
  return TRUE;

link_failure:
  gst_element_set_state (sink, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (obj->priv->pipeline), sink);

sink_failure:
  gst_object_unref (tee);

  if (error != NULL)
    g_warning ((*error)->message);

  _window_pairs_remove (&(obj->priv->preview_windows), wp);

  return FALSE;
}

static void
_add_pending_preview_windows (TpStreamEngine *engine)
{
  g_debug ("%s: called", G_STRFUNC);

  GSList *tmp;

  for (tmp = engine->priv->preview_windows;
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
  TpStreamEnginePrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (obj,
      TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate);

  obj->priv = priv;

  priv->channels = g_ptr_array_new ();

  priv->channels_by_path = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

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
}

static void tp_stream_engine_dispose (GObject *object);
static void tp_stream_engine_finalize (GObject *object);

static void
tp_stream_engine_class_init (TpStreamEngineClass *tp_stream_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_stream_engine_class);

  g_type_class_add_private (tp_stream_engine_class,
      sizeof (TpStreamEnginePrivate));

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
}

static void
tp_stream_engine_dispose (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = self->priv;

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

  if (priv->channels_by_path)
    {
      g_hash_table_destroy (priv->channels_by_path);
      priv->channels_by_path = NULL;
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

static void
tp_stream_engine_finalize (GObject *object)
{
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
  guint i;

  for (i = 0; i < self->priv->channels->len; i++)
    tp_stream_engine_channel_error (
      g_ptr_array_index (self->priv->channels, i), error, message);
}

static void
check_if_busy (TpStreamEngine *self)
{
  guint num_previews = g_slist_length (self->priv->preview_windows);

  if (self->priv->channels->len == 0 && num_previews == 0)
    {
      g_debug ("no channels or previews remaining; emitting no-more-channels");
      g_signal_emit (self, signals[NO_MORE_CHANNELS], 0);
    }
  else
    {
      g_debug ("channels remaining: %d", self->priv->channels->len);
      g_debug ("preview windows remaining: %d", num_previews);
    }
}


static void
channel_closed (TpStreamEngineChannel *chan, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  gchar *object_path;

  g_object_get (chan, "object-path", &object_path, NULL);

  g_debug ("channel %s (%p) closed", object_path, chan);

  g_ptr_array_remove_fast (self->priv->channels, chan);
  g_object_unref (chan);

  g_hash_table_remove (self->priv->channels_by_path, object_path);
  g_free (object_path);

  check_if_busy (self);
}

static void
channel_stream_state_changed (TpStreamEngineChannel *chan,
                              guint stream_id,
                              TpMediaStreamState state,
                              TpMediaStreamDirection direction,
                              gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  gchar *channel_path;

  g_object_get (chan, "object-path", &channel_path, NULL);

  stream_engine_svc_stream_engine_emit_stream_state_changed (self,
      channel_path, stream_id, state, direction);

  g_free (channel_path);
}

static void
channel_stream_receiving (TpStreamEngineChannel *chan,
                          guint stream_id,
                          gboolean receiving,
                          gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  gchar *channel_path;

  g_object_get (chan, "object-path", &channel_path, NULL);

  stream_engine_svc_stream_engine_emit_receiving (self,
      channel_path, stream_id, receiving);

  g_free (channel_path);
}

static void
_window_pairs_remove_cb (WindowPair *wp)
{
  TpStreamEngine *self = tp_stream_engine_get ();

  _window_pairs_remove (&(self->priv->preview_windows), wp);
}

static void
_window_pairs_readd_cb (WindowPair *wp)
{
  TpStreamEngine *engine = tp_stream_engine_get ();

  wp->sink = NULL;
  wp->created = FALSE;
  wp->stream = NULL;
  wp->removing = FALSE;
  wp->post_remove = NULL;

  if (engine->priv->pipeline_playing)
    {
      GError *error = NULL;
      _add_preview_window (engine, wp->window_id, &error);
      if (error) {
        g_debug ("Error creating preview window: %s", error->message);
        g_error_free (error);
      }
    }

}

static void
unblock_cb (GstPad *pad, gboolean blocked,
    gpointer user_data)
{
  g_debug ("Pad unblocked successfully after removing preview sink");
  gst_object_unref (pad);
}

static gboolean
_remove_defunct_preview_sink_idle_callback (gpointer user_data)
{
  TpStreamEngine *self = tp_stream_engine_get ();
  gboolean retval;
  GstStateChangeReturn ret;

  WindowPair *wp = user_data;
  g_assert (wp);

  g_debug ("Removing defunct preview sink for window %u", wp->window_id);

  GstPad *sink_pad = gst_element_get_pad (wp->sink, "sink");
  g_assert (sink_pad);

  GstPad *tee_src_pad = gst_pad_get_peer (sink_pad);
  g_assert (tee_src_pad);

  GstElement *sink_element = gst_pad_get_parent_element (sink_pad);
  g_assert (sink_element);

  GstElement *sink_parent = GST_ELEMENT (gst_element_get_parent (sink_element));
  g_assert (sink_parent);

  GstElement *tee = gst_bin_get_by_name (GST_BIN (self->priv->pipeline), "tee");
  g_assert (tee);

  GstPad *tee_sink_pad = gst_element_get_pad (tee, "sink");
  g_assert (tee_sink_pad);

  GstPad *tee_peer_src_pad = gst_pad_get_peer (tee_sink_pad);
  g_assert (tee_peer_src_pad);


  retval = gst_bin_remove (GST_BIN (sink_parent), sink_element);
  g_assert (retval == TRUE);

  ret = gst_element_set_state (sink_element, GST_STATE_NULL);
  g_assert (ret != GST_STATE_CHANGE_FAILURE);

  if (ret == GST_STATE_CHANGE_ASYNC) {
    ret = gst_element_get_state (sink_element, NULL, NULL, 5*GST_SECOND);
    g_assert (ret != GST_STATE_CHANGE_FAILURE);
  }

  gst_object_unref (tee_peer_src_pad);
  gst_object_unref (tee_sink_pad);
  gst_object_unref (tee);
  gst_object_unref (sink_parent);
  gst_object_unref (sink_element);
  gst_element_release_request_pad (tee, tee_src_pad);
  gst_object_unref (sink_pad);

  if (wp->post_remove)
    wp->post_remove (wp);

  check_if_busy (self);

  gst_pad_set_blocked_async (tee_peer_src_pad, FALSE, unblock_cb, NULL);

  return FALSE;
}

static void
_remove_defunct_preview_sink_callback (GstPad *tee_peer_src_pad, gboolean blocked,
    gpointer user_data)
{
  g_debug("Pad blocked, scheduling preview sink removal");
  g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_preview_sink_idle_callback, user_data, NULL);
  g_main_context_wakeup (NULL);
}

static void
_remove_defunct_preview_sink (WindowPair *wp)
{
  GstElement *tee = NULL;
  GstPad *tee_sink_pad = NULL;
  GstPad *tee_peer_src_pad = NULL;
  TpStreamEngine *self = tp_stream_engine_get ();

  if (wp->sink == NULL)
    return;

  tee = gst_bin_get_by_name (GST_BIN (self->priv->pipeline), "tee");
  g_assert (tee);

  tee_sink_pad = gst_element_get_pad (tee, "sink");
  g_assert (tee_sink_pad);

  gst_object_unref (tee);

  tee_peer_src_pad = gst_pad_get_peer (tee_sink_pad);
  g_assert (tee_peer_src_pad);

  gst_object_unref (tee_sink_pad);

  gst_pad_set_blocked_async (tee_peer_src_pad, TRUE,
      _remove_defunct_preview_sink_callback, wp);
}

static void
_remove_defunct_output_sink (WindowPair *wp)
{
  TpStreamEngine *engine = tp_stream_engine_get ();
  GError *error;

  g_debug ("%s: removing sink for output window ID %u", G_STRFUNC,
      wp->window_id);

  if (!tp_stream_engine_stream_set_output_window (wp->stream, 0, &error))
  {
    g_debug ("%s: got error: %s", G_STRFUNC, error->message);
    g_error_free (error);
  }

  check_if_busy (engine);
}

static void
close_one_video_stream (TpStreamEngineChannel *chan,
                        guint stream_id,
                        TpStreamEngineStream *stream,
                        gpointer user_data)
{
  const gchar *message = (const gchar *) user_data;
  TpMediaStreamType media_type;

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO)
    tp_stream_engine_stream_error (stream, TP_MEDIA_STREAM_ERROR_UNKNOWN,
        message);
}

static void
close_all_video_streams (TpStreamEngine *self, const gchar *message)
{
  guint i;

  g_debug ("Closing all video streams");

  for (i = 0; i < self->priv->channels->len; i++)
    {
      TpStreamEngineChannel *channel = g_ptr_array_index (self->priv->channels,
          i);

      tp_stream_engine_channel_foreach_stream (channel,
          close_one_video_stream, (gpointer) message);
    }
}


static gboolean
bus_async_handler (GstBus *bus,
                   GstMessage *message,
                   gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = engine->priv;
  GError *error = NULL;
  gchar *error_string, *tmp;
  GSList *i;

  GstElement *source = GST_ELEMENT (GST_MESSAGE_SRC (message));
  gchar *name = gst_element_get_name (source);

  switch (GST_MESSAGE_TYPE (message))
    {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (message, &error, &error_string);
        tmp = g_strdup_printf ("%s: %s", error->message, error_string);

        g_debug ("%s: got error from %s: %s: %s (%d %d), destroying video "
            "pipeline", G_STRFUNC, name, error->message, error_string,
            error->domain, error->code);

        close_all_video_streams (engine, tmp);

        g_free (error_string);
        g_free (tmp);

        for (i = priv->output_windows; i; i = i->next)
          {
            WindowPair *wp = (WindowPair *) i->data;
            if (wp->removing == FALSE)
              {
                wp->removing = TRUE;
                wp->post_remove = _window_pairs_remove_cb;
                _remove_defunct_output_sink (wp);
              }
          }

        for (i = priv->preview_windows; i; i = i->next)
          {
            WindowPair *wp = (WindowPair *) i->data;
            if (wp->removing == FALSE)
              {
                wp->removing = TRUE;
                wp->post_remove = _window_pairs_remove_cb;
                _remove_defunct_preview_sink (wp);
              }
          }

        gst_element_set_state (priv->pipeline, GST_STATE_NULL);
        gst_object_unref (priv->pipeline);
        priv->pipeline = NULL;

        g_error_free (error);
        break;
      case GST_MESSAGE_WARNING:
        {
          gchar *debug_msg;
          gst_message_parse_warning (message, &error, &debug_msg);
          g_warning ("%s: got warning: %s (%s)", G_STRFUNC, error->message,
              debug_msg);
          g_free (debug_msg);
          g_error_free (error);
          break;
        }
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
  TpStreamEnginePrivate *priv = engine->priv;
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
#ifndef MAEMO_OSSO_SUPPORT
  if (g_object_has_property (G_OBJECT (GST_MESSAGE_SRC (message)),
          "force-aspect-ratio"))
    {
      g_debug ("setting force-aspect-ratio to TRUE");
      g_object_set (G_OBJECT (GST_MESSAGE_SRC (message)), 
          "force-aspect-ratio", TRUE, NULL);
    }
#endif

  gst_message_unref (message);

  return GST_BUS_DROP;
}

static void
_create_pipeline (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = obj->priv;
  GstElement *videosrc = NULL;
  GstElement *tee;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *tmp;
#endif
  GstBus *bus;
  GstCaps *filter = NULL;
  GstElement *fakesink;
  GstElement *queue;
  const gchar *elem;
  const gchar *caps_str;

  priv->pipeline = gst_pipeline_new (NULL);
  tee = gst_element_factory_make ("tee", "tee");
  fakesink = gst_element_factory_make ("fakesink", NULL);
  queue = gst_element_factory_make ("queue", NULL);

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
           "framerate", GST_TYPE_FRACTION, 15, 1,
#ifdef MAEMO_OSSO_SUPPORT
           "width", G_TYPE_INT, 176,
           "height", G_TYPE_INT, 144,
#else
           "width", G_TYPE_INT, 352,
           "height", G_TYPE_INT, 288,
           "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
#endif
           NULL);
    }
  else
    {
      g_debug ("applying custom caps '%s' on the video source\n", caps_str);
    }

  gst_bin_add_many (GST_BIN (priv->pipeline), videosrc, tee, fakesink, queue,
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
  if (tmp != NULL)
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }

  tmp = gst_element_factory_make ("videoscale", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videoscale");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }
#endif

  g_object_set(G_OBJECT(queue), "leaky", 2,
      "max-size-time", 50*GST_MSECOND, NULL);

  gst_element_link (videosrc, tee);
  gst_element_link_filtered (tee, queue, filter);
  gst_element_link (queue, fakesink);
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
  if (NULL == obj->priv->pipeline)
    {
      _create_pipeline (obj);
    }

  return obj->priv->pipeline;
}


static gboolean
_remove_defunct_sinks_idle_cb (WindowPair *wp)
{
  if (wp->stream)
    _remove_defunct_output_sink (wp);
  else
    _remove_defunct_preview_sink (wp);

  return FALSE;
}

/**
 * tp_stream_engine_add_preview_window
 *
 * Implements DBus method AddPreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_add_preview_window (StreamEngineSvcStreamEngine *iface,
                                     guint window_id,
                                     DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEnginePrivate *priv = obj->priv;
  WindowPair *wp;

  g_debug ("%s: called for %u", G_STRFUNC, window_id);

  if (priv->pipeline == NULL)
    {
      _create_pipeline (obj);
    }

  wp = _window_pairs_find_by_window_id (priv->preview_windows, window_id);

  if (wp != NULL)
    {
      if (wp->removing && wp->post_remove == _window_pairs_remove_cb)
        {
          g_debug ("window ID %u is already a preview window being removed, "
              "will be re-added", window_id);
          wp->post_remove = _window_pairs_readd_cb;
          stream_engine_svc_stream_engine_return_from_add_preview_window
            (context);
        }
      else
        {
          GError *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "window ID %u is already a preview window", window_id);
          g_debug ("%s", error->message);

          dbus_g_method_return_error (context, error);
          g_error_free (error);
        }

      return;
    }

  if (!priv->pipeline_playing)
    {
      g_debug ("%s: pipeline not playing, adding later", G_STRFUNC);
      _window_pairs_add (&(priv->preview_windows), NULL, NULL, window_id);
      stream_engine_svc_stream_engine_return_from_add_preview_window
        (context);
    }
  else
    {
      GError *error = NULL;

      g_debug ("%s: pipeline playing, adding now", G_STRFUNC);
      _window_pairs_add (&(priv->preview_windows), NULL, NULL, window_id);
      if (_add_preview_window (obj, window_id, &error))
        {
          stream_engine_svc_stream_engine_return_from_add_preview_window
            (context);
        }
      else
        {
          dbus_g_method_return_error (context, error);
          g_error_free (error);
        }
    }
}

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data)
{
  TpStreamEngine *engine = data;
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (engine->priv->preview_windows,
      window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (engine->priv->output_windows,
        window_id);

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
  wp->post_remove = _window_pairs_remove_cb;

  g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_sinks_idle_cb, wp, NULL);
  g_main_context_wakeup (NULL);

  return TRUE;
}


static gboolean
bad_misc_cb (TpStreamEngineXErrorHandler *handler,
             guint window_id,
             gpointer data)
{
  TpStreamEngine *engine = data;
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (engine->priv->preview_windows,
      window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (engine->priv->output_windows,
        window_id);

  if (wp == NULL)
    {
      g_debug ("%s: BadDrawable(%u) not for a preview or output window, not "
          "handling", G_STRFUNC, window_id);
      return FALSE;
    }

  if (wp->removing)
    {
      g_debug ("%s: BadDrawable(%u) for a %s window being removed, ignoring",
          G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");
      return TRUE;
    }

  g_debug ("%s: BadDrawable(%u) for a %s window, scheduling sink removal",
      G_STRFUNC, window_id, wp->stream == NULL ? "preview" : "output");

  /* set removing to TRUE so that we know this window ID is being removed and X
   * errors can be ignored */
  wp->removing = TRUE;
  wp->post_remove = _window_pairs_remove_cb;

  g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_sinks_idle_cb, wp, NULL);
  g_main_context_wakeup (NULL);

  return TRUE;
}


static gboolean
bad_other_cb (TpStreamEngineXErrorHandler *handler,
              guint gc_id,
              gpointer data)
{
  TpStreamEngine *engine = data;
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_removing (engine->priv->preview_windows, TRUE);

  if (wp == NULL)
    wp = _window_pairs_find_by_removing (engine->priv->output_windows, TRUE);

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
  WindowPair *wp;

  /* BEWARE: THIS CALLBACK IS NOT CALLED FROM THE MAINLOOP THREAD */

  wp = _window_pairs_find_by_window_id (engine->priv->preview_windows,
      window_id);

  if (wp == NULL)
    wp = _window_pairs_find_by_window_id (engine->priv->output_windows,
        window_id);

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
 */
static void
tp_stream_engine_remove_preview_window (StreamEngineSvcStreamEngine *iface,
                                        guint window_id,
                                        DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  WindowPair *wp;

  g_debug ("%s: called for %u", G_STRFUNC, window_id);

  wp = _window_pairs_find_by_window_id (obj->priv->preview_windows, window_id);

  if (wp == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "Window ID not found" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  if (wp->removing)
    {
      if (wp->post_remove == _window_pairs_readd_cb)
        wp->post_remove = _window_pairs_remove_cb;

      /* already being removed, nothing to do */
      goto success;
    }

  if (wp->created == FALSE)
    {
      g_debug ("Window not created yet, can remove right away");
      _window_pairs_remove (&(obj->priv->preview_windows), wp);
      goto success;
    }

  wp->removing = TRUE;
  wp->post_remove = _window_pairs_remove_cb;

  _remove_defunct_preview_sink (wp);

success:
  stream_engine_svc_stream_engine_return_from_remove_preview_window (context);
}


gboolean
tp_stream_engine_add_output_window (TpStreamEngine *obj,
                                    TpStreamEngineStream *stream,
                                    GstElement *sink,
                                    guint window_id)
{
  _window_pairs_add (&(obj->priv->output_windows), stream, sink, window_id);

  return TRUE;
}


gboolean
tp_stream_engine_remove_output_window (TpStreamEngine *obj,
                                       guint window_id)
{
  WindowPair *wp;

  wp = _window_pairs_find_by_window_id (obj->priv->output_windows, window_id);

  if (wp == NULL)
    return FALSE;

  _window_pairs_remove (&(obj->priv->output_windows), wp);

  return TRUE;
}


/**
 * tp_stream_engine_handle_channel
 *
 * Implements DBus method HandleChannel
 * on interface org.freedesktop.Telepathy.ChannelHandler
 */
static void
tp_stream_engine_handle_channel (StreamEngineSvcChannelHandler *iface,
                                 const gchar *bus_name,
                                 const gchar *connection,
                                 const gchar *channel_type,
                                 const gchar *channel,
                                 guint handle_type,
                                 guint handle,
                                 DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEngineChannel *chan = NULL;
  GError *error = NULL;

  g_debug("HandleChannel called");

  if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA) != 0)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Stream Engine was passed a channel that was not a "
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA };

      g_message ("%s", e.message);
      dbus_g_method_return_error (context, &e);
     }

  chan = tp_stream_engine_channel_new (obj->priv->dbus_daemon, bus_name,
      channel, handle_type, handle, &error);

  if (chan == NULL)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_set ((GObject *) chan,
      "video-pipeline", tp_stream_engine_get_pipeline (obj),
      NULL);

  g_ptr_array_add (obj->priv->channels, chan);
  g_hash_table_insert (obj->priv->channels_by_path, g_strdup (channel), chan);

  g_signal_connect (chan, "closed", G_CALLBACK (channel_closed), obj);
  g_signal_connect (chan, "stream-state-changed",
      G_CALLBACK (channel_stream_state_changed), obj);
  g_signal_connect (chan, "stream-receiving",
      G_CALLBACK (channel_stream_receiving), obj);

  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);

  stream_engine_svc_channel_handler_return_from_handle_channel (context);
}

void
tp_stream_engine_register (TpStreamEngine *self)
{
  DBusGConnection *bus;
  GError *error = NULL;
  guint request_name_result;

  g_assert (TP_IS_STREAM_ENGINE (self));

  bus = tp_get_bus ();
  self->priv->dbus_daemon = tp_dbus_daemon_new (bus);

  g_debug("Requesting " BUS_NAME);

  if (!tp_cli_dbus_daemon_block_on_request_name (self->priv->dbus_daemon, -1,
        BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &request_name_result, &error))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, stream engine already running?");

  g_debug("registering StreamEngine at " OBJECT_PATH);
  dbus_g_connection_register_g_object (bus, OBJECT_PATH, G_OBJECT (self));

  register_dbus_signal_marshallers();
}

static TpStreamEngineStream *
_lookup_stream (TpStreamEngine *obj,
                const gchar *path,
                guint stream_id,
                GError **error)
{
  TpStreamEngineChannel *channel;
  TpStreamEngineStream *stream;

  channel = g_hash_table_lookup (obj->priv->channels_by_path, path);
  if (channel == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
        "stream-engine is not handling the channel %s", path);

      return NULL;
    }

  stream = tp_stream_engine_channel_lookup_stream (channel, stream_id);
  if (stream == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "the channel %s has no stream with id %d", path, stream_id);

      return NULL;
    }

  return stream;
}


/**
 * tp_stream_engine_mute_input
 *
 * Implements DBus method MuteInput
 * on interface org.freedesktop.Telepathy.StreamEngine
 */

static void
tp_stream_engine_mute_input (StreamEngineSvcStreamEngine *iface,
                             const gchar *channel_path,
                             guint stream_id,
                             gboolean mute_state,
                             DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (obj, channel_path, stream_id, &error);

  if (stream != NULL &&
      tp_stream_engine_stream_mute_input (stream, mute_state, &error))
    {
      stream_engine_svc_stream_engine_return_from_mute_input (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}

/**
 * tp_stream_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_mute_output (StreamEngineSvcStreamEngine *iface,
                              const gchar *channel_path,
                              guint stream_id,
                              gboolean mute_state,
                              DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (obj, channel_path, stream_id, &error);

  if (stream != NULL &&
      tp_stream_engine_stream_mute_output (stream, mute_state, &error))
    {
      stream_engine_svc_stream_engine_return_from_mute_output (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}


/**
 * tp_stream_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_set_output_volume (StreamEngineSvcStreamEngine *iface,
                                    const gchar *channel_path,
                                    guint stream_id,
                                    guint volume,
                                    DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (obj, channel_path, stream_id, &error);

  if (stream != NULL &&
      tp_stream_engine_stream_set_output_volume (stream, volume, &error))
    {
      stream_engine_svc_stream_engine_return_from_set_output_window (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}

/**
 * tp_stream_engine_set_output_window
 *
 * Implements DBus method SetOutputWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_set_output_window (StreamEngineSvcStreamEngine *iface,
                                    const gchar *channel_path,
                                    guint stream_id,
                                    guint window_id,
                                    DBusGMethodInvocation *context)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  g_debug ("%s: channel_path=%s, stream_id=%u, window_id=%u", G_STRFUNC,
      channel_path, stream_id, window_id);

  stream = _lookup_stream (obj, channel_path, stream_id, &error);

  if (stream != NULL &&
      tp_stream_engine_stream_set_output_window (stream, window_id, &error))
    {
      stream_engine_svc_stream_engine_return_from_set_output_window (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
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

/**
 * tp_stream_engine_shutdown
 *
 * Implements DBus method Shutdown
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_shutdown (StreamEngineSvcStreamEngine *iface,
                           DBusGMethodInvocation *context)
{
  g_debug ("%s: Emitting shutdown signal", G_STRFUNC);
  g_signal_emit (iface, signals[SHUTDOWN_REQUESTED], 0);
  stream_engine_svc_stream_engine_return_from_shutdown (context);
}

static void
se_iface_init (gpointer iface, gpointer data)
{
  StreamEngineSvcStreamEngineClass *klass = iface;

#define IMPLEMENT(x) stream_engine_svc_stream_engine_implement_##x (\
    klass, tp_stream_engine_##x)
  IMPLEMENT (set_output_volume);
  IMPLEMENT (mute_input);
  IMPLEMENT (mute_output);
  IMPLEMENT (set_output_window);
  IMPLEMENT (add_preview_window);
  IMPLEMENT (remove_preview_window);
  IMPLEMENT (shutdown);
#undef IMPLEMENT
}

static void
ch_iface_init (gpointer iface, gpointer data)
{
  StreamEngineSvcChannelHandlerClass *klass = iface;

#define IMPLEMENT(x) stream_engine_svc_channel_handler_implement_##x (\
    klass, tp_stream_engine_##x)
  IMPLEMENT (handle_channel);
#undef IMPLEMENT
}
