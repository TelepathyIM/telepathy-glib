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
#include "audiostream.h"
#include "videostream.h"
#include "util.h"
#include "xerrorhandler.h"

#define BUS_NAME        "org.freedesktop.Telepathy.StreamEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/StreamEngine"

static void tp_stream_engine_start_source (TpStreamEngine *obj);

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
  GstElement *videosrc;
  GstElement *videosrc_next;

  GPtrArray *channels;
  GHashTable *channels_by_path;

  GSList *output_windows;
  GSList *preview_windows;

  TpStreamEngineStream *audio_resource_owner;

  guint bus_async_source_id;
  guint bus_sync_source_id;

  TpStreamEngineXErrorHandler *handler;
  guint bad_drawable_handler_id;
  guint bad_gc_handler_id;
  guint bad_value_handler_id;
  guint bad_window_handler_id;

  gboolean linked;
  gboolean restart_source;
  gboolean force_fakesrc;
};

typedef struct _WindowPair WindowPair;
struct _WindowPair
{
  TpStreamEngineVideoStream *stream;
  GstElement *sink;
  guint window_id;
  volatile gboolean removing;
  gboolean created;
  guint idle_source_id;
  DBusGMethodInvocation *context;
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
_window_pairs_add (GSList **list, TpStreamEngineVideoStream *stream,
    GstElement *sink, guint window_id)
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
  wp->idle_source_id = 0;
  wp->context = NULL;

  *list = g_slist_prepend (*list, wp);
}

static void
_window_pairs_remove (GSList **list, WindowPair *pair)
{
  g_assert (g_slist_find (*list, pair));

  if (pair->idle_source_id)
    g_source_remove (pair->idle_source_id);
  pair->idle_source_id = 0;

  g_assert (pair->context == NULL);

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
  gboolean done = FALSE;

  iter = gst_bin_iterate_elements (bin);

  if (!iter)
    {
      gchar *bin_name = gst_element_get_name (GST_ELEMENT (bin));
      g_warning ("Could not get iterator for bin %s", bin_name);
      g_free (bin_name);
      return FALSE;
    }

  while (!done)
    {
      GstIteratorResult ret;

      ret = gst_iterator_next (iter, &item);
      switch (ret)
        {
          case GST_ITERATOR_OK:
            {
              GstElement *child = (GstElement *) item;
              if (child == element)
                {
                  found = TRUE;
                  done = TRUE;
                }
              gst_object_unref (child);
            }
            break;
          case GST_ITERATOR_RESYNC:
            found = FALSE;
            gst_iterator_resync (iter);
            break;
          case GST_ITERATOR_ERROR:
            {
              gchar *bin_name = gst_element_get_name (GST_ELEMENT (bin));
              g_warning ("Error iterating %s", bin_name);
              g_free (bin_name);
            }
            done = TRUE;
            break;
          case GST_ITERATOR_DONE:
            done = TRUE;
            break;
        }
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

static void
set_video_sink_props (GstBin *bin G_GNUC_UNUSED,
                      GstElement *sink,
                      void *user_data G_GNUC_UNUSED)
{
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
  if (g_object_has_property (G_OBJECT (sink), "async"))
    {
      g_debug ("setting async to FALSE");
      g_object_set (G_OBJECT (sink), "async", FALSE, NULL);
    }
  else if (g_object_has_property (G_OBJECT (sink), "preroll-queue-len"))
    {
      g_debug ("setting preroll-queue-len to 1");
      g_object_set (G_OBJECT (sink), "preroll-queue-len", 1, NULL);
    }

  if (GST_IS_BIN (sink))
    {
      gboolean done = FALSE;
      GstIterator *it = NULL;
      gpointer elem;

      g_signal_connect ((GObject *) sink, "element-added",
        G_CALLBACK (set_video_sink_props), NULL);

      it = gst_bin_iterate_recurse (GST_BIN (sink));
      while (!done)
        {
          switch (gst_iterator_next (it, &elem))
            {
              case GST_ITERATOR_OK:
                set_video_sink_props (NULL, GST_ELEMENT(elem), NULL);
                g_object_unref (elem);
                break;
              case GST_ITERATOR_RESYNC:
                gst_iterator_resync (it);
                break;
              case GST_ITERATOR_ERROR:
                g_error ("Can not iterate videosink bin");
                done = TRUE;
                break;
             case GST_ITERATOR_DONE:
               done = TRUE;
               break;
            }
        }
    }
}

GstElement *
tp_stream_engine_make_video_sink (TpStreamEngine *self, gboolean is_preview)
{
  const gchar *videosink_name;
  GstElement *sink = NULL;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *bin, *tmp;
  GstPad *pad;
#endif

  g_assert (self->priv->pipeline != NULL);

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

  if (sink == NULL)
    {
      g_debug ("failed to make a video sink");
      return NULL;
    }

  g_debug ("made video sink element %s", GST_ELEMENT_NAME (sink));

  if (GST_IS_BIN (sink))
    {
      g_signal_connect ((GObject *) sink, "element-added",
          G_CALLBACK (set_video_sink_props), NULL);
    }
  else
    {
      set_video_sink_props (NULL, sink, NULL);
    }

#ifndef MAEMO_OSSO_SUPPORT
  bin = gst_bin_new (NULL);

  if (!gst_bin_add (GST_BIN (bin), sink))
    {
      g_warning ("Could not add source bin to the pipeline");
      gst_object_unref (bin);
      gst_object_unref (sink);
      return NULL;
    }

  tmp = gst_element_factory_make ("videoscale", NULL);
  if (tmp != NULL)
    {
      g_object_set (G_OBJECT (tmp), "qos", FALSE, NULL);
      g_debug ("linking videoscale");
      if (!gst_bin_add (GST_BIN (bin), tmp))
        {
          g_warning ("Could not add videoscale to the source bin");
          gst_object_unref (tmp);
          gst_object_unref (bin);
          return NULL;
        }
      if (!gst_element_link (tmp, sink))
        {
          g_warning ("Could not link sink and videoscale elements");
          gst_object_unref (bin);
          return NULL;
        }
      sink = tmp;
    }

  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL)
    {
      g_object_set (G_OBJECT (tmp), "qos", FALSE, NULL);
      g_debug ("linking ffmpegcolorspace");
     if (!gst_bin_add (GST_BIN (bin), tmp))
       {
         g_warning ("Could not add ffmpegcolorspace to the source bin");
         gst_object_unref (tmp);
         gst_object_unref (bin);
         return NULL;
       }
      if (!gst_element_link (tmp, sink))
        {
          g_warning ("Could not link sink and ffmpegcolorspace elements");
          gst_object_unref (bin);
          return NULL;
        }
      sink = tmp;
    }

  pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SINK);

  if (!pad)
    {
      g_warning ("Could not find unconnected sink pad in the source bin");
      gst_object_unref (bin);
      return NULL;
    }

  if (!gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad)))
    {
      g_warning ("Could not add sink ghostpad to the source bin");
      gst_object_unref (bin);
      return NULL;
    }
  gst_object_unref (GST_OBJECT (pad));

  sink = bin;
#endif

  if (!gst_bin_add (GST_BIN (self->priv->pipeline), sink))
    {
      g_warning ("Could not add the sink to the pipeline");
      gst_object_unref (sink);
      return NULL;
    }

  gst_object_ref (sink);

  return sink;
}

static gboolean
_add_preview_window (TpStreamEngine *self, guint window_id, GError **error)
{
  WindowPair *wp;
  GstElement *tee, *sink;
  GstStateChangeReturn state_change_ret;

  g_assert (self->priv->pipeline != NULL);

  g_debug ("%s: called for window id %d", G_STRFUNC, window_id);
  wp = _window_pairs_find_by_window_id (self->priv->preview_windows,
      window_id);

  if (wp == NULL)
    {
      *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Given window id %d not in windowID list", window_id);
      g_debug ("%s: Couldn't find xwindow id in window pair list", G_STRFUNC);
      return FALSE;
    }

  g_debug ("adding preview in window %u", window_id);

  tee = gst_bin_get_by_name (GST_BIN (self->priv->pipeline), "tee");

  if (!tee)
    {
      g_set_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
          "Failed to get tee in the bin %s",
          GST_ELEMENT_NAME (self->priv->pipeline));
      goto tee_failure;
    }

  sink = tp_stream_engine_make_video_sink (self, TRUE);

  if (sink == NULL)
    goto sink_failure;

  /* FIXME:
   * We dont keep a ref, this means we can potentially point to a
   * dead object
   */
  gst_object_unref (sink);

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

  tp_stream_engine_start_source (self);

  gst_object_unref (tee);
  g_signal_emit (self, signals[HANDLING_CHANNEL], 0);
  return TRUE;

link_failure:
  gst_element_set_state (sink, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self->priv->pipeline), sink);

sink_failure:
  gst_object_unref (tee);

tee_failure:

  if (error != NULL)
    g_warning ((*error)->message);

  _window_pairs_remove (&(self->priv->preview_windows), wp);

  return FALSE;
}

static gboolean
bad_drawable_cb (TpStreamEngineXErrorHandler *handler,
             guint window_id,
             gpointer data);

static gboolean
bad_gc_cb (TpStreamEngineXErrorHandler *handler,
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
tp_stream_engine_init (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate);

  self->priv = priv;

  priv->linked = FALSE;
  priv->restart_source = FALSE;

  priv->channels = g_ptr_array_new ();

  priv->channels_by_path = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  priv->audio_resource_owner = NULL;

  priv->handler = tp_stream_engine_x_error_handler_get ();

  priv->bad_drawable_handler_id =
    g_signal_connect (priv->handler, "bad-drawable",
        (GCallback) bad_drawable_cb, self);
  priv->bad_gc_handler_id =
    g_signal_connect (priv->handler, "bad-gc", (GCallback) bad_gc_cb,
      self);
  priv->bad_value_handler_id =
    g_signal_connect (priv->handler, "bad-value", (GCallback) bad_value_cb,
      self);
  priv->bad_window_handler_id =
    g_signal_connect (priv->handler, "bad-window", (GCallback) bad_window_cb,
      self);
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

  if (priv->audio_resource_owner)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->audio_resource_owner),
          (gpointer) &priv->audio_resource_owner);
      priv->audio_resource_owner = NULL;
    }

  if (priv->pipeline)
    {
      /*
       * Lets not check the return values
       * if it fails or is async, it will crash on the following
       * unref anyways
       */
      gst_element_set_state (priv->videosrc, GST_STATE_NULL);
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_object_unref (priv->pipeline);
      priv->pipeline = NULL;
      priv->videosrc = NULL;
      priv->videosrc_next = NULL;
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

  if (priv->bus_sync_source_id)
    {
      g_source_remove (priv->bus_sync_source_id);
      priv->bus_sync_source_id = 0;
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
stream_free_resource (TpStreamEngineStream *stream, gpointer user_data)
{
#ifdef MAEMO_OSSO_SUPPORT
  TpStreamEngine *engine = TP_STREAM_ENGINE (user_data);

  g_assert (engine->priv->audio_resource_owner == stream);

  g_object_remove_weak_pointer (G_OBJECT (engine->priv->audio_resource_owner),
      (gpointer) &engine->priv->audio_resource_owner);
  engine->priv->audio_resource_owner = NULL;
#endif
}

static gboolean
stream_request_resource (TpStreamEngineStream *stream, gpointer user_data)
{
#ifdef MAEMO_OSSO_SUPPORT
  TpStreamEngine *engine = TP_STREAM_ENGINE (user_data);

  guint mediatype;

  g_object_get (G_OBJECT (stream), "media-type", &mediatype, NULL);

  if (mediatype == TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      if (engine->priv->audio_resource_owner != NULL)
        {
          return FALSE;
        }

      g_assert (engine->priv->audio_resource_owner == NULL);

      engine->priv->audio_resource_owner = stream;

      g_object_add_weak_pointer (G_OBJECT (engine->priv->audio_resource_owner),
          (gpointer) &engine->priv->audio_resource_owner);

      return TRUE;
    }
  else
#endif
    {
      return TRUE;
    }
}


static void
session_invalidated (TpStreamEngineSession *session G_GNUC_UNUSED,
    gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GstElement *conf;

  g_object_get (session, "farsight-conference", &conf, NULL);

  gst_element_set_locked_state (conf, TRUE);
  gst_element_set_state (conf, GST_STATE_NULL);
  gst_element_get_state (conf, NULL, NULL, GST_CLOCK_TIME_NONE);

  gst_bin_remove (GST_BIN (self->priv->pipeline), conf);
}

static void
channel_session_created (TpStreamEngineChannel *chan G_GNUC_UNUSED,
    TpStreamEngineSession *session, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GstElement *conf;

  g_object_get (session, "farsight-conference", &conf, NULL);

  if (g_object_has_property ((GObject *)conf, "latency"))
    g_object_set (conf, "latency", 100, NULL);

  tp_stream_engine_get_pipeline (self);

  if (!gst_bin_add (GST_BIN (self->priv->pipeline), conf))
    g_error ("Could not add conference to pipeline");

  gst_element_set_state (conf, GST_STATE_PLAYING);

  g_signal_connect (session, "invalidated", G_CALLBACK (session_invalidated),
      user_data);
}



static void
stream_linked (TpStreamEngineStream *stream G_GNUC_UNUSED, gpointer user_data)
{
  TpStreamEngine *obj = TP_STREAM_ENGINE (user_data);

  tp_stream_engine_start_source (obj);
}

static void
channel_stream_created (TpStreamEngineChannel *chan G_GNUC_UNUSED,
    TpStreamEngineStream *stream, gpointer user_data)
{
  guint mediatype;

  g_object_get (G_OBJECT (stream), "media-type", &mediatype, NULL);

  if (mediatype == TP_MEDIA_STREAM_TYPE_VIDEO)
    {
      g_signal_connect (stream, "linked", G_CALLBACK (stream_linked), user_data);
    }
  g_signal_connect (stream, "request-resource",
      G_CALLBACK (stream_request_resource), user_data);
  g_signal_connect (stream, "free-resource",
      G_CALLBACK (stream_free_resource), user_data);
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
                          gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  gchar *channel_path;

  g_object_get (chan, "object-path", &channel_path, NULL);

  stream_engine_svc_stream_engine_emit_receiving (self,
      channel_path, stream_id, TRUE);

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
  GError *error = NULL;

  wp->sink = NULL;
  wp->created = FALSE;
  wp->stream = NULL;
  wp->removing = FALSE;
  wp->post_remove = NULL;

  _add_preview_window (engine, wp->window_id, &error);
  if (error)
    {
      g_debug ("Error creating preview window: %s", error->message);
      g_error_free (error);
    }
}

static void
unblock_cb (GstPad *pad, gboolean blocked G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED)
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

  wp->idle_source_id = 0;

  if (self->priv->pipeline == NULL)
    {
      if (wp->context)
        {
          stream_engine_svc_stream_engine_return_from_remove_preview_window (
              wp->context);
          wp->context = NULL;
        }

      check_if_busy (self);
      return FALSE;
    }

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
    if (ret == GST_STATE_CHANGE_ASYNC)
      g_warning ("Could not stop the video sink in 5 seconds!!");
  }

  gst_object_unref (tee_peer_src_pad);
  gst_object_unref (tee_sink_pad);
  gst_object_unref (tee);
  gst_object_unref (sink_parent);
  gst_object_unref (sink_element);
  gst_element_release_request_pad (tee, tee_src_pad);
  gst_object_unref (sink_pad);

  if (wp->context)
    {
      stream_engine_svc_stream_engine_return_from_remove_preview_window (
          wp->context);
      wp->context = NULL;
    }

  if (wp->post_remove)
    wp->post_remove (wp);

  check_if_busy (self);

  if (!gst_pad_set_blocked_async (tee_peer_src_pad, FALSE, unblock_cb, NULL)) {
    gst_object_unref (tee_peer_src_pad);
  }

  return FALSE;
}

static void
_remove_defunct_preview_sink_callback (GstPad *tee_peer_src_pad G_GNUC_UNUSED,
    gboolean blocked G_GNUC_UNUSED,
    gpointer user_data)
{
  WindowPair *wp = user_data;

  g_debug("Pad blocked, scheduling preview sink removal");
  wp->idle_source_id = g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_preview_sink_idle_callback, wp, NULL);
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

  if (!gst_pad_set_blocked_async (tee_peer_src_pad, TRUE,
          _remove_defunct_preview_sink_callback, wp))
    {
      g_warning ("Pad already blocked, "
          "we will try calling the remove function directly");
      _remove_defunct_preview_sink_idle_callback (wp);
    }
}

static void
_remove_defunct_output_sink (WindowPair *wp)
{
  TpStreamEngine *engine = tp_stream_engine_get ();
  GError *error = NULL;

  g_debug ("%s: removing sink for output window ID %u", G_STRFUNC,
      wp->window_id);

  if (!tp_stream_engine_video_stream_set_output_window (wp->stream, 0, &error))
  {
    g_debug ("%s: got error: %s", G_STRFUNC, error->message);
    g_error_free (error);
  }

  check_if_busy (engine);
}

static void
close_one_video_stream (TpStreamEngineChannel *chan G_GNUC_UNUSED,
                        guint stream_id G_GNUC_UNUSED,
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

static void
bus_sync_message (GstBus *bus G_GNUC_UNUSED,
    GstMessage *message, gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  GError *error = NULL;
  gchar *error_string = NULL;

  switch (GST_MESSAGE_TYPE (message))
    {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &error_string);
      if (error->domain == GST_STREAM_ERROR &&
          error->code == GST_STREAM_ERROR_FAILED &&
          strstr (error_string, "not-linked"))
        {

          engine->priv->linked = FALSE;
          engine->priv->restart_source = TRUE;

          g_debug ("Got non-linked error (got no preview windows or video "
              "streams any more), unlinking the source to stop the EOS");
          gst_element_unlink (engine->priv->videosrc,
              engine->priv->videosrc_next);
        }
      g_free (error_string);
      g_error_free (error);
      break;
    default:
      break;
    }

}

static gboolean
bus_async_handler (GstBus *bus G_GNUC_UNUSED,
                   GstMessage *message,
                   gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = engine->priv;
  GError *error = NULL;
  gchar *error_string, *tmp;
  GSList *i;
  guint ii;
  GstElement *source = GST_ELEMENT (GST_MESSAGE_SRC (message));
  gchar *name = gst_element_get_name (source);


  for (ii = 0; ii < priv->channels->len; ii++)
    if (tp_stream_engine_channel_bus_message (
            g_ptr_array_index (priv->channels, ii), message))
      return TRUE;

  switch (GST_MESSAGE_TYPE (message))
    {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (message, &error, &error_string);

        if (strstr (error_string, "not-linked"))
          {
            g_debug ("%s: ignoring not-linked from %s: %s: %s (%d %d)",
                G_STRFUNC, name, error->message, error_string,
                error->domain, error->code);
            g_free (error_string);
            g_error_free (error);

            if (priv->restart_source)
              {
                g_debug ("%s: setting video source to NULL state", G_STRFUNC);
                gst_element_set_state (priv->videosrc, GST_STATE_NULL);
              }

            break;
          }

        g_debug ("%s: got error from %s: %s: %s (%d %d), destroying video "
            "pipeline", G_STRFUNC, name, error->message, error_string,
            error->domain, error->code);

        if (priv->force_fakesrc)
          g_error ("Could not even start fakesrc");

        tmp = g_strdup_printf ("%s: %s", error->message, error_string);
        g_free (error_string);
        g_error_free (error);

        close_all_video_streams (engine, tmp);

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
        gst_element_set_state (priv->videosrc, GST_STATE_NULL);
        gst_object_unref (priv->pipeline);
        priv->pipeline = NULL;
        priv->videosrc = NULL;
        priv->videosrc_next = NULL;

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

        /*
      case GST_MESSAGE_STATE_CHANGED:
        {
          GstState new_state;
          GstState old_state;
          GstState pending_state;
          gst_message_parse_state_changed (message, &old_state, &new_state,
              &pending_state);

          g_debug ("State changed for %s refcount %d old:%s new:%s pending:%s",
              name,
              GST_OBJECT_REFCOUNT_VALUE (source),
              gst_element_state_get_name (old_state),
              gst_element_state_get_name (new_state),
              gst_element_state_get_name (pending_state));
          break;
        }
        */
      default:
        break;
    }

  g_free (name);
  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus *bus G_GNUC_UNUSED, GstMessage *message, gpointer data)
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

  gst_message_unref (message);

  return GST_BUS_DROP;
}

static void
_create_pipeline (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = self->priv;
  GstElement *videosrc = NULL;
  GstElement *tee;
  GstElement *capsfilter;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *tmp;
#endif
  GstBus *bus;
  GstCaps *filter = NULL;
  const gchar *elem;
  const gchar *caps_str;
  GstStateChangeReturn state_ret;
  gboolean ret;
  GstElement *fakesink;

  priv->pipeline = gst_pipeline_new (NULL);

 try_again:

  if ((elem = getenv ("FS_VIDEO_SRC")) || (elem = getenv ("FS_VIDEOSRC")))
    {
      if (priv->force_fakesrc)
        g_error ("Invalid video source passed in FS_VIDEOSRC");
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
      if (priv->force_fakesrc)
        {
          videosrc = gst_element_factory_make ("fakesrc", NULL);
          g_object_set (videosrc, "is-live", TRUE, NULL);
        }

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("gconfvideosrc", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4l2src", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4lsrc", NULL);
#endif

      if (videosrc != NULL)
        {
          g_debug ("using %s as video source", GST_ELEMENT_NAME (videosrc));
        }
      else
        {
          videosrc = gst_element_factory_make ("videotestsrc", NULL);
          g_object_set (videosrc, "is-live", TRUE, NULL);
          if (videosrc == NULL)
            g_error ("failed to create any video source");
        }
    }

  if ((caps_str = getenv ("FS_VIDEO_SRC_CAPS")) || (caps_str = getenv ("FS_VIDEOSRC_CAPS")))
    {
      filter = gst_caps_from_string (caps_str);
    }

  if (!filter)
    {
      filter = gst_caps_new_simple ("video/x-raw-yuv",
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

  fakesink = gst_element_factory_make ("fakesink", NULL);

  gst_bin_add_many (GST_BIN (priv->pipeline), videosrc, fakesink, NULL);

  if (!gst_element_link (videosrc, fakesink))
    g_error ("Could not link fakesink to videosrc");

  state_ret = gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  g_debug ("state_ret: %d", state_ret);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    {
      if (priv->force_fakesrc)
        {
          g_error ("Could not even start fakesrc");
        }
      else
        {
          g_debug ("Video source failed, falling back to fakesrc");
          state_ret = gst_element_set_state (priv->pipeline, GST_STATE_NULL);
          g_assert (state_ret == GST_STATE_CHANGE_SUCCESS);
          if (!gst_bin_remove (GST_BIN (priv->pipeline), fakesink))
            g_error ("Could not remove fakesink");

          priv->force_fakesrc = TRUE;
          gst_bin_remove (GST_BIN (priv->pipeline), videosrc);
          goto try_again;
        }
    }
  else
    {
      state_ret = gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_assert (state_ret == GST_STATE_CHANGE_SUCCESS);
      gst_bin_remove (GST_BIN (priv->pipeline), fakesink);
    }

  priv->videosrc = videosrc;

  gst_element_set_locked_state (videosrc, TRUE);

  tee = gst_element_factory_make ("tee", "tee");
  g_assert (tee);
  if (!gst_bin_add (GST_BIN (priv->pipeline), tee))
    g_error ("Could not add tee to pipeline");



#ifndef MAEMO_OSSO_SUPPORT
#if 0
  /* <wtay> Tester_, yes, videorate does not play nice with live pipelines */
  tmp = gst_element_factory_make ("videorate", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videorate");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      if (priv->videosrc_next == NULL)
        priv->videosrc_next = tmp;
      videosrc = tmp;
    }
#endif

  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      if (priv->videosrc_next == NULL)
        priv->videosrc_next = tmp;
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

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  g_assert (capsfilter);
  ret = gst_bin_add (GST_BIN (priv->pipeline), capsfilter);
  g_assert (ret);

  if (priv->videosrc_next == NULL)
    priv->videosrc_next = capsfilter;

  g_object_set (capsfilter, "caps", filter, NULL);
  gst_caps_unref (filter);

  ret = gst_element_link (videosrc, capsfilter);
  g_assert (ret);
  ret = gst_element_link (capsfilter, tee);
  g_assert (ret);

  state_ret = gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  g_assert (state_ret != GST_STATE_CHANGE_FAILURE);

  /* connect a callback to the stream bus so that we can set X window IDs
   * at the right time, and detect when sinks have gone away */
  bus = gst_element_get_bus (priv->pipeline);
  gst_bus_set_sync_handler (bus, bus_sync_handler, self);
  priv->bus_async_source_id =
    gst_bus_add_watch (bus, bus_async_handler, self);
  gst_bus_enable_sync_message_emission (bus);
  priv->bus_sync_source_id = g_signal_connect (bus, "sync-message",
      G_CALLBACK (bus_sync_message), self);
  gst_object_unref (bus);
}


static void
tp_stream_engine_start_source (TpStreamEngine *self)
{
  GstStateChangeReturn state_ret;

  g_debug ("Starting source");

  self->priv->linked = TRUE;

  /*
   * FIXME: We should instead get the pads, and check if they are already
   * linked, then we'd be able to check properly for errors
   */
  gst_element_link (self->priv->videosrc, self->priv->videosrc_next);

  if (self->priv->force_fakesrc)
    return;

  if (self->priv->restart_source)
    {
      self->priv->restart_source = FALSE;

      state_ret = gst_element_set_state (self->priv->videosrc, GST_STATE_NULL);
      if (state_ret == GST_STATE_CHANGE_ASYNC)
        {
          g_warning ("The video source tries to change state async to NULL");

          state_ret = gst_element_get_state (self->priv->videosrc, NULL, NULL,
              GST_SECOND);
        }

      if (state_ret == GST_STATE_CHANGE_ASYNC)
        g_warning ("Could not change the video source to NULL in a reasonable"
            " delay (1 second)");
      else if (state_ret == GST_STATE_CHANGE_FAILURE)
        g_warning ("Failure while stopping the video source");

    }

  state_ret = gst_element_set_state (self->priv->videosrc, GST_STATE_PLAYING);

  if (state_ret == GST_STATE_CHANGE_FAILURE)
    g_error ("Error starting the video source");
}

/*
 * tp_stream_engine_get_pipeline
 *
 * Return the GStreamer pipeline belonging to the stream engine. Caller does
 * not own a reference to the pipeline.
 */

GstElement *
tp_stream_engine_get_pipeline (TpStreamEngine *self)
{
  if (NULL == self->priv->pipeline)
    {
      _create_pipeline (self);
    }

  return self->priv->pipeline;
}


static gboolean
_remove_defunct_sinks_idle_cb (WindowPair *wp)
{
  wp->idle_source_id = 0;

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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEnginePrivate *priv = self->priv;
  WindowPair *wp;
  GError *error = NULL;

  g_debug ("%s: called for %u", G_STRFUNC, window_id);

  if (priv->pipeline == NULL)
    {
      _create_pipeline (self);
    }

  if (priv->force_fakesrc)
    {
      GError *error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Could not get a video source");
      g_debug ("%s", error->message);

      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
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

  g_debug ("%s: pipeline playing, adding now", G_STRFUNC);
  _window_pairs_add (&(priv->preview_windows), NULL, NULL, window_id);
  if (_add_preview_window (self, window_id, &error))
    {
      stream_engine_svc_stream_engine_return_from_add_preview_window (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler G_GNUC_UNUSED,
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
      g_debug ("%s: BadWindow(%u) not for a preview or output window, "
          "ignoring", G_STRFUNC, window_id);
      return TRUE;
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

  wp->idle_source_id = g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_sinks_idle_cb, wp, NULL);
  g_main_context_wakeup (NULL);

  return TRUE;
}


static gboolean
bad_drawable_cb (TpStreamEngineXErrorHandler *handler G_GNUC_UNUSED,
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
      g_debug ("%s: BadDrawable(%u) not for a preview or output window, "
          "ignoring", G_STRFUNC, window_id);
      return TRUE;
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

  wp->idle_source_id = g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _remove_defunct_sinks_idle_cb, wp, NULL);
  g_main_context_wakeup (NULL);

  return TRUE;
}


static gboolean
bad_gc_cb (TpStreamEngineXErrorHandler *handler G_GNUC_UNUSED,
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
bad_value_cb (TpStreamEngineXErrorHandler *handler G_GNUC_UNUSED,
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  WindowPair *wp;

  g_debug ("%s: called for %u", G_STRFUNC, window_id);

  wp = _window_pairs_find_by_window_id (self->priv->preview_windows,
      window_id);

  if (wp == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "Window ID not found" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  if (wp->context)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
                   "Window ID has already been removed" };

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
      _window_pairs_remove (&(self->priv->preview_windows), wp);
      goto success;
    }

  wp->removing = TRUE;
  wp->post_remove = _window_pairs_remove_cb;
  wp->context = context;

  _remove_defunct_preview_sink (wp);

  return;

success:
  stream_engine_svc_stream_engine_return_from_remove_preview_window (context);
}


gboolean
tp_stream_engine_add_output_window (TpStreamEngine *self,
                                    TpStreamEngineVideoStream *stream,
                                    GstElement *sink,
                                    guint window_id)
{
  _window_pairs_add (&(self->priv->output_windows), stream, sink, window_id);

  return TRUE;
}


gboolean
tp_stream_engine_remove_output_window (TpStreamEngine *self,
                                       guint window_id)
{
  WindowPair *wp;

  wp = _window_pairs_find_by_window_id (self->priv->output_windows, window_id);

  if (wp == NULL)
    return FALSE;

  _window_pairs_remove (&(self->priv->output_windows), wp);

  return TRUE;
}


static void
handler_result (TpStreamEngineChannel *chan G_GNUC_UNUSED,
                GError *error,
                DBusGMethodInvocation *context)
{
  if (error == NULL)
    stream_engine_svc_channel_handler_return_from_handle_channel (context);
  else
    dbus_g_method_return_error (context, error);
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
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
      return;
     }

  chan = tp_stream_engine_channel_new (self->priv->dbus_daemon, bus_name,
      connection, channel, handle_type, handle, &error);

  if (chan == NULL)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_set ((GObject *) chan,
      "video-pipeline", tp_stream_engine_get_pipeline (self),
      "audio-stream-gtype", TP_STREAM_ENGINE_TYPE_AUDIO_STREAM,
      "video-stream-gtype", TP_STREAM_ENGINE_TYPE_VIDEO_STREAM,
      NULL);

  g_ptr_array_add (self->priv->channels, chan);
  g_hash_table_insert (self->priv->channels_by_path, g_strdup (channel), chan);

  g_signal_connect (chan, "handler-result", G_CALLBACK (handler_result),
      context);
  g_signal_connect (chan, "closed", G_CALLBACK (channel_closed), self);
  g_signal_connect (chan, "session-created",
      G_CALLBACK (channel_session_created), self);
  g_signal_connect (chan, "stream-created",
      G_CALLBACK (channel_stream_created), self);
  g_signal_connect (chan, "stream-receiving",
      G_CALLBACK (channel_stream_receiving), self);

  g_signal_emit (self, signals[HANDLING_CHANNEL], 0);
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

  g_debug("registering StreamEngine at " OBJECT_PATH);
  dbus_g_connection_register_g_object (bus, OBJECT_PATH, G_OBJECT (self));

  register_dbus_signal_marshallers();

  g_debug("Requesting " BUS_NAME);

  if (!tp_cli_dbus_daemon_run_request_name (self->priv->dbus_daemon, -1,
        BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &request_name_result, &error,
        NULL))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, stream engine already running?");
}

static TpStreamEngineStream *
_lookup_stream (TpStreamEngine *self,
                const gchar *path,
                guint stream_id,
                GError **error)
{
  TpStreamEngineChannel *channel;
  TpStreamEngineStream *stream;

  channel = g_hash_table_lookup (self->priv->channels_by_path, path);
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (self, channel_path, stream_id, &error);

  if (stream != NULL)
    {
      if (!TP_STREAM_ENGINE_IS_AUDIO_STREAM (stream))
        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "MuteInput can only be called on audio streams");
      else
        tp_stream_engine_audio_stream_mute_input (
            TP_STREAM_ENGINE_AUDIO_STREAM (stream), mute_state, &error);
    }

  if (error)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
  else
    {
      stream_engine_svc_stream_engine_return_from_mute_input (context);
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (self, channel_path, stream_id, &error);

  if (stream != NULL)
    {
      if (!TP_STREAM_ENGINE_IS_AUDIO_STREAM (stream))
        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "MuteOutput can only be called on audio streams");
      else
        tp_stream_engine_audio_stream_mute_output (
            TP_STREAM_ENGINE_AUDIO_STREAM (stream), mute_state, &error);
    }

  if (error)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
  else
    {
      stream_engine_svc_stream_engine_return_from_mute_output (context);
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  stream = _lookup_stream (self, channel_path, stream_id, &error);

  if (stream != NULL)
    {
      if (!TP_STREAM_ENGINE_IS_AUDIO_STREAM (stream))
        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "SetOutputVolume can only be called on audio streams");
      else
        tp_stream_engine_audio_stream_set_output_volume (
            TP_STREAM_ENGINE_AUDIO_STREAM (stream), volume, &error);
    }

  if (error)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
  else
    {
      stream_engine_svc_stream_engine_return_from_set_output_volume (context);
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
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  g_debug ("%s: channel_path=%s, stream_id=%u, window_id=%u", G_STRFUNC,
      channel_path, stream_id, window_id);

  stream = _lookup_stream (self, channel_path, stream_id, &error);

  if (stream != NULL)
    {
      if (!TP_STREAM_ENGINE_IS_VIDEO_STREAM (stream))
        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "SetOutputWindow can only be called on video streams");
      else
        tp_stream_engine_video_stream_set_output_window (
            TP_STREAM_ENGINE_VIDEO_STREAM (stream), window_id, &error);
    }

  if (error)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
  else
    {
      stream_engine_svc_stream_engine_return_from_set_output_window (context);
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
se_iface_init (gpointer iface, gpointer data G_GNUC_UNUSED)
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
ch_iface_init (gpointer iface, gpointer data G_GNUC_UNUSED)
{
  StreamEngineSvcChannelHandlerClass *klass = iface;

#define IMPLEMENT(x) stream_engine_svc_channel_handler_implement_##x (\
    klass, tp_stream_engine_##x)
  IMPLEMENT (handle_channel);
#undef IMPLEMENT
}
