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

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "videostream.h"
#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineVideoStream, tp_stream_engine_video_stream,
    TP_STREAM_ENGINE_TYPE_STREAM);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (video) %s: " format, \
    stream->priv->stream_id, \
    G_STRFUNC, \
    ##__VA_ARGS__)

struct _TpStreamEngineVideoStreamPrivate
{
  GstElement *queue;
};


enum
{
  LINKED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};


static GstElement *
tp_stream_engine_video_stream_make_src (TpStreamEngineStream *stream);

static void
tee_src_pad_blocked (GstPad *pad, gboolean blocked, gpointer user_data);

static void
tp_stream_engine_video_stream_init (TpStreamEngineVideoStream *self)
{
  TpStreamEngineVideoStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStreamPrivate);

  self->priv = priv;
}

static void
tp_stream_engine_video_stream_dispose (GObject *object)
{
  TpStreamEngineVideoStream *videostream =
      TP_STREAM_ENGINE_VIDEO_STREAM (object);

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
  TpStreamEngineStreamClass *stream_class =
      TP_STREAM_ENGINE_STREAM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineVideoStreamPrivate));
  object_class->dispose = tp_stream_engine_video_stream_dispose;

  stream_class->make_src = tp_stream_engine_video_stream_make_src;

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
queue_linked (GstPad *pad, GstPad *peer, gpointer user_data)
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



static void
tee_src_pad_unblocked (GstPad *pad, gboolean blocked, gpointer user_data)
{
  gst_object_unref (pad);
}

static void
tee_src_pad_blocked (GstPad *pad, gboolean blocked, gpointer user_data)
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
