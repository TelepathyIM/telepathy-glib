/*
 * stream.c - Source for TpStreamEngineVideoStream
 * Copyright (C) 2006-2008 Collabora Ltd.
 * Copyright (C) 2006-2008 Nokia Corporation
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

#include <string.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <gst/interfaces/xoverlay.h>

#include "videostream.h"
#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineVideoStream, tp_stream_engine_video_stream,
    TP_STREAM_ENGINE_TYPE_STREAM);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (video) %s: " format, \
      ((TpStreamEngineStream *) stream)->stream_id,     \
      G_STRFUNC,                            \
      ##__VA_ARGS__)

struct _TpStreamEngineVideoStreamPrivate
{
  GstElement *queue;

  guint output_window_id;
};


enum
{
  LINKED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};


static GstElement *
tp_stream_engine_video_stream_make_src (TpStreamEngineStream *stream);
static GstElement *
tp_stream_engine_video_stream_make_sink (TpStreamEngineStream *stream);

static void
tee_src_pad_blocked (GstPad *pad, gboolean blocked, gpointer user_data);
static void
_remove_video_sink (TpStreamEngineVideoStream *videostream, GstElement *sink);


static void
tp_stream_engine_video_stream_init (TpStreamEngineVideoStream *self)
{
  TpStreamEngineVideoStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStreamPrivate);

  self->priv = priv;
}

static GObject *
tp_stream_engine_video_stream_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineStream *stream = NULL;
  GstElement *src = NULL;
  GstElement *sink = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->constructor (type, n_props, props);

  stream = (TpStreamEngineStream *) obj;

  src = tp_stream_engine_video_stream_make_src (stream);
  sink = tp_stream_engine_video_stream_make_sink (stream);

  if (src)
    {
      DEBUG (stream, "setting source on Farsight stream");
    }
  else
    {
      DEBUG (stream, "not setting source on Farsight stream");
    }

  if (sink)
    {
      DEBUG (stream, "setting sink on Farsight stream");
    }
  else
    {
      DEBUG (stream, "not setting sink on Farsight stream");
    }


  return obj;
}

static void
tp_stream_engine_video_stream_dispose (GObject *object)
{
  TpStreamEngineVideoStream *videostream =
      TP_STREAM_ENGINE_VIDEO_STREAM (object);


  if (videostream->priv->output_window_id)
    {
      gboolean ret;
      TpStreamEngine *engine = tp_stream_engine_get ();
      ret = tp_stream_engine_remove_output_window (engine,
          videostream->priv->output_window_id);
      g_assert (ret);
      videostream->priv->output_window_id = 0;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->dispose (object);

  if (videostream->priv->queue)
    {
      TpStreamEngine *engine = tp_stream_engine_get ();
      GstElement *pipeline = tp_stream_engine_get_pipeline (engine);
      GstElement *tee = gst_bin_get_by_name (GST_BIN (pipeline), "tee");
      GstPad *pad = NULL;

      pad = gst_element_get_static_pad (tee, "sink");

      g_object_ref (object);

      if (!gst_pad_set_blocked_async (pad, TRUE, tee_src_pad_blocked, object))
        {
          g_warning ("tee source pad already blocked, lets try to dispose"
              " of it already");
          tee_src_pad_blocked (pad, TRUE, object);
        }

      /* Lets keep a ref around until we've blocked the pad
       * and removed the queue, so we dont unref the pad here. */
    }

}

static void
tp_stream_engine_video_stream_class_init (TpStreamEngineVideoStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineVideoStreamPrivate));
  object_class->dispose = tp_stream_engine_video_stream_dispose;

  object_class->constructor = tp_stream_engine_video_stream_constructor;

  signals[LINKED] =
    g_signal_new ("linked",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
queue_linked (GstPad *pad G_GNUC_UNUSED,
    GstPad *peer G_GNUC_UNUSED,
    gpointer user_data)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (user_data);

  g_signal_emit (stream, signals[LINKED], 0);
}

static GstElement *
tp_stream_engine_video_stream_make_src (TpStreamEngineStream *stream)
{
  TpStreamEngineVideoStream *videostream =
      TP_STREAM_ENGINE_VIDEO_STREAM (stream);
  TpStreamEngine *engine = tp_stream_engine_get ();
  GstElement *pipeline = tp_stream_engine_get_pipeline (engine);
  GstElement *tee = gst_bin_get_by_name (GST_BIN (pipeline), "tee");
  GstElement *queue = gst_element_factory_make ("queue", NULL);
  GstPad *pad = NULL;
  GstStateChangeReturn state_ret;

  g_return_val_if_fail (tee, NULL);

  if (!queue)
    g_error("Could not create queue element");

  g_object_set(G_OBJECT(queue), "leaky", 2,
      "max-size-time", 50*GST_MSECOND, NULL);

  pad = gst_element_get_static_pad (queue, "src");

  g_return_val_if_fail (pad, NULL);

  g_signal_connect (pad, "linked", G_CALLBACK (queue_linked), stream);

  videostream->priv->queue = queue;
  gst_object_ref (queue);

  if (!gst_bin_add(GST_BIN(pipeline), queue))
    {
      g_warning ("Culd not add queue to pipeline");
      gst_object_unref (queue);
      return NULL;
    }

  state_ret = gst_element_set_state(queue, GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    {
      g_warning ("Could not set the queue to playing");
      gst_bin_remove (GST_BIN(pipeline), queue);
      return NULL;
    }

  if (!gst_element_link(tee, queue))
    {
      g_warning ("Could not link the tee to its queue");
      gst_bin_remove (GST_BIN(pipeline), queue);
      return NULL;
    }

  /*
   * We need to keep a second ref
   * one will be given to farsight and the second one is kept by s-e
   */
  gst_object_ref (queue);

  gst_object_unref (tee);

  return queue;
}


static GstElement *
tp_stream_engine_video_stream_make_sink (TpStreamEngineStream *stream)
{
  TpStreamEngineVideoStream *videostream =
      TP_STREAM_ENGINE_VIDEO_STREAM (stream);
  const gchar *elem;
  GstElement *sink = NULL;

  if ((elem = getenv ("STREAM_VIDEO_SINK")) ||
      (elem = getenv ("FS_VIDEO_SINK")) ||
      (elem = getenv ("FS_VIDEOSINK")))
    {
      TpStreamEngine *engine = tp_stream_engine_get ();
      GstStateChangeReturn state_ret;

      DEBUG (videostream, "making video sink with pipeline \"%s\"", elem);
      sink = gst_parse_bin_from_description (elem, TRUE, NULL);
      g_assert (sink != NULL);
      g_assert (GST_IS_BIN (sink));

      gst_object_ref (sink);
      if (!gst_bin_add (GST_BIN (tp_stream_engine_get_pipeline (engine)),
              sink))
        {
          g_warning ("Could not add sink bin to the pipeline");
          gst_object_unref (sink);
          gst_object_unref (sink);
          return NULL;
        }

      state_ret = gst_element_set_state (sink, GST_STATE_PLAYING);
      if (state_ret == GST_STATE_CHANGE_FAILURE)
        {
          g_warning ("Could not set sink to PLAYING");
          gst_object_unref (sink);
          gst_object_unref (sink);
          return NULL;
        }
    }
  else
    {
      /* do nothing: we set a sink when we get a window ID to send video
       * to */

      DEBUG (stream, "not making a video sink");
    }

  return sink;
}

static void
tee_src_pad_unblocked (GstPad *pad,
    gboolean blocked G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED)
{
  gst_object_unref (pad);
}

static void
tee_src_pad_blocked (GstPad *pad, gboolean blocked G_GNUC_UNUSED,
    gpointer user_data)
{
  TpStreamEngineVideoStream *videostream =
      TP_STREAM_ENGINE_VIDEO_STREAM (user_data);
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngine *engine = tp_stream_engine_get ();
  GstPad *queuesinkpad = NULL;
  GstElement *pipeline = NULL;
  GstElement *tee = NULL;
  GstPad *teesrcpad = NULL;

  GstStateChangeReturn ret;

  if (!videostream->priv->queue)
    {
      gst_object_unref (pad);
      return;
    }
  pipeline = tp_stream_engine_get_pipeline (engine);
  g_assert (pipeline);
  tee = gst_bin_get_by_name (GST_BIN (pipeline), "tee");
  g_assert (tee);
  queuesinkpad = gst_element_get_static_pad (videostream->priv->queue, "sink");
  teesrcpad = gst_pad_get_peer (queuesinkpad);
  g_assert (teesrcpad);

  gst_object_unref (queuesinkpad);

  if (!gst_bin_remove (GST_BIN (pipeline), videostream->priv->queue))
    {
      g_warning ("Could not remove the queue from the bin");
    }

  ret = gst_element_set_state (videostream->priv->queue, GST_STATE_NULL);

  if (ret == GST_STATE_CHANGE_ASYNC)
    {
      g_warning ("%s is going to NULL async, lets wait 2 seconds",
          GST_OBJECT_NAME (videostream->priv->queue));
      ret = gst_element_get_state (videostream->priv->queue, NULL, NULL,
          2*GST_SECOND);
    }

  if (ret == GST_STATE_CHANGE_ASYNC)
    g_warning ("%s still hasn't going NULL, we have to leak it",
        GST_OBJECT_NAME (videostream->priv->queue));
  else if (ret == GST_STATE_CHANGE_FAILURE)
    g_warning ("There was an error bringing %s to the NULL state",
        GST_OBJECT_NAME (videostream->priv->queue));
  else
    gst_object_unref (videostream->priv->queue);

  videostream->priv->queue = NULL;

  gst_element_release_request_pad (tee, teesrcpad);

  gst_object_unref (tee);

  gst_object_unref (stream);

  if (!gst_pad_set_blocked_async (pad, FALSE, tee_src_pad_unblocked, NULL))
    gst_object_unref (pad);
}


gboolean
tp_stream_engine_video_stream_set_output_window (
  TpStreamEngineVideoStream *videostream,
  guint window_id,
  GError **error)
{
  TpStreamEngine *engine;
  GstElement *sink;
  GstElement *old_sink = NULL;
  GstStateChangeReturn ret;
  TpStreamEngineStream *stream = (TpStreamEngineStream *) videostream;

  if (videostream->priv->output_window_id == window_id)
    {
      DEBUG (videostream, "not doing anything, output window is already set to "
          "window ID %u", window_id);
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "not doing "
          "anything, output window is already set window ID %u", window_id);
      return FALSE;
    }

  engine = tp_stream_engine_get ();

  if (videostream->priv->output_window_id != 0)
    {
      tp_stream_engine_remove_output_window (engine,
          videostream->priv->output_window_id);
    }

  videostream->priv->output_window_id = 0;

  if (window_id == 0)
    {
      GstElement *stream_sink = NULL;
      g_object_get (G_OBJECT (stream), "sink", &stream_sink, NULL);
      g_object_set (G_OBJECT (stream), "sink", NULL, NULL);
      _remove_video_sink (videostream, stream_sink);
      g_object_unref (stream_sink);
      return TRUE;
    }

  sink = tp_stream_engine_make_video_sink (engine, FALSE);

  if (sink == NULL)
    {
      DEBUG (stream, "failed to make video sink, no output for window %d :(",
          window_id);
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "failed to make a "
          "video sink");
      return FALSE;
    }

  DEBUG (stream, "putting video output in window %d", window_id);

  g_object_get (G_OBJECT (stream), "sink", &old_sink, NULL);

  if (old_sink)
    {
      _remove_video_sink (videostream, old_sink);
      g_object_unref (old_sink);
    }

  tp_stream_engine_add_output_window (engine, videostream, sink, window_id);

  videostream->priv->output_window_id = window_id;

  ret = gst_element_set_state (sink, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      DEBUG (stream, "failed to set video sink to playing");
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "failed to set video sink to playing");
      return FALSE;
    }

  g_object_set (G_OBJECT (stream), "sink", sink, NULL);

  return TRUE;
}


static gboolean
video_sink_unlinked_idle_cb (gpointer user_data)
{
  GstElement *sink = GST_ELEMENT (user_data);
  GstElement *binparent = NULL;
  gboolean retval;
  GstStateChangeReturn ret;

  binparent = GST_ELEMENT (gst_element_get_parent (sink));

  if (!binparent)
    goto out;

  retval = gst_bin_remove (GST_BIN (binparent), sink);
  g_assert (retval);

  ret = gst_element_set_state (sink, GST_STATE_NULL);

  if (ret == GST_STATE_CHANGE_ASYNC) {
    ret = gst_element_get_state (sink, NULL, NULL, 5*GST_SECOND);
  }
  g_assert (ret != GST_STATE_CHANGE_FAILURE);

 out:
  gst_object_unref (sink);

  return FALSE;
}


static void
video_sink_unlinked_cb (GstPad *pad, GstPad *peer G_GNUC_UNUSED,
    gpointer user_data)
{
  g_idle_add (video_sink_unlinked_idle_cb, user_data);

  gst_object_unref (pad);
}

static void
_remove_video_sink (TpStreamEngineVideoStream *videostream, GstElement *sink)
{
  GstPad *sink_pad;

  DEBUG (videostream, "removing video sink");

  if (sink == NULL)
    return;

  sink_pad = gst_element_get_static_pad (sink, "sink");

  if (!sink_pad)
    return;

  if (g_signal_handler_find (sink_pad,
        G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL,
        video_sink_unlinked_cb, sink))
    {
      DEBUG (videostream, "found existing unlink callback,"
          " not adding a new one");
      return;
    }

  gst_object_ref (sink);

  g_signal_connect (sink_pad, "unlinked", G_CALLBACK (video_sink_unlinked_cb),
      sink);
}
