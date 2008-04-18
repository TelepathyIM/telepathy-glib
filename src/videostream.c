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

#include <telepathy-glib/errors.h>

#include <gst/interfaces/xoverlay.h>

#include "videostream.h"
#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineVideoStream, tp_stream_engine_video_stream,
    TP_STREAM_ENGINE_TYPE_VIDEO_SINK);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (video) %s: " format, \
      ((TpStreamEngineStream *) stream)->stream_id,     \
      G_STRFUNC,                            \
      ##__VA_ARGS__)

struct _TpStreamEngineVideoStreamPrivate
{
  TpStreamEngineStream *stream;

  gulong src_pad_added_handler_id;

  GError *construction_error;

  GstPad *pad;

  GstElement *bin;

  GstElement *sink;

  GstElement *queue;

  GMutex *mutex;

  /* Everything below this line is protected by the mutex */
  guint error_idle_id;
};

/* properties */
enum
{
  PROP_0,
  PROP_STREAM,
  PROP_BIN,
  PROP_PAD,
};


static GstElement *
tp_stream_engine_video_stream_make_sink (TpStreamEngineVideoStream *stream);

static void src_pad_added_cb (TpStreamEngineStream *stream, GstPad *pad,
    FsCodec *codec, gpointer user_data);



static void tp_stream_engine_video_stream_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static void tp_stream_engine_video_stream_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);


static void
tp_stream_engine_video_stream_init (TpStreamEngineVideoStream *self)
{
  TpStreamEngineVideoStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStreamPrivate);

  self->priv = priv;

  self->priv->mutex = g_mutex_new ();
}


static GstElement *
tp_stream_engine_video_stream_make_sink (TpStreamEngineVideoStream *self)
{
  GstElement *bin = gst_bin_new (NULL);
  GstElement *sink = NULL;
  GstElement *funnel = NULL;

  g_object_get (self, "sink", &sink, NULL);

  if (!sink)
    {
      g_warning ("Could not make sink");
      goto error;
    }

  if (!gst_bin_add (GST_BIN (bin), sink))
    {
      gst_object_unref (sink);
      g_warning ("Could not add sink to bin");
      goto error;
    }

  funnel = gst_element_factory_make ("fsfunnel", "funnel");
  if (!funnel)
    {
      g_warning ("Could not make funnel");
      goto error;
    }

  if (!gst_bin_add (GST_BIN (bin), funnel))
    {
      gst_object_unref (funnel);
      g_warning ("Could not add funnel to bin");
      goto error;
    }

  if (!gst_element_link (funnel, sink))
    {
      g_warning ("Could not link funnel and sink");
      goto error;
    }

  return bin;
error:
  gst_object_unref (bin);
  return NULL;
}

static GObject *
tp_stream_engine_video_stream_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineVideoStream *self = NULL;
  GstElement *sink = NULL;
  GstPad *srcpad;
  GstPad *sinkpad;

  obj = G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->constructor (type, n_props, props);

  self = (TpStreamEngineVideoStream *) obj;

  sink = tp_stream_engine_video_stream_make_sink (self);

  if (!sink)
    return obj;

  if (!gst_bin_add (GST_BIN (self->priv->bin), sink))
    {
      g_warning ("Could not add sink to bin");
      return obj;
    }

  self->priv->sink = sink;

  if (gst_element_set_state (sink, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
    {
      g_warning ("Could not start sink");
      return obj;
    }

  self->priv->queue = gst_element_factory_make ("queue", NULL);

  if (!self->priv->queue)
    {
      g_warning ("Could not make queue element");
      return obj;
    }

  if (!gst_bin_add (GST_BIN (self->priv->bin), self->priv->queue))
    {
      g_warning ("Could not add quue to bin");
      return obj;
    }

  if (gst_element_set_state (self->priv->queue, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
    {
      g_warning ("Could not start queue");
      return obj;
    }

  sinkpad = gst_element_get_static_pad (self->priv->queue, "sink");

  if (GST_PAD_LINK_FAILED (gst_pad_link (self->priv->pad, sinkpad)))
    {
      g_warning ("Could not link sink to queue");
      gst_object_unref (sinkpad);
      return obj;
    }

  gst_object_unref (sinkpad);

  g_object_get (self->priv->stream, "sink-pad", &sinkpad, NULL);

  if (!sinkpad)
    {
      g_warning ("Could not get stream's sinkpad");
      return obj;
    }

  srcpad = gst_element_get_static_pad (self->priv->queue, "src");

  if (!sinkpad)
    {
      g_warning ("Could not get queue's srcpad");
      gst_object_unref (sinkpad);
      return obj;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (srcpad, sinkpad)))
    {
      gst_object_unref (srcpad);
      gst_object_unref (sinkpad);
      g_warning ("Could not link sink to queue");
      return obj;
    }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);


  self->priv->src_pad_added_handler_id = g_signal_connect (self->priv->stream,
      "src-pad-added", G_CALLBACK (src_pad_added_cb), self);

  return obj;
}

static void
tp_stream_engine_video_stream_dispose (GObject *object)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (object);


  if (self->priv->src_pad_added_handler_id)
    {
      g_signal_handler_disconnect (self->priv->stream,
          self->priv->src_pad_added_handler_id);
      self->priv->src_pad_added_handler_id = 0;
    }

  g_mutex_lock (self->priv->mutex);
  if (self->priv->error_idle_id)
    {
      g_source_remove (self->priv->error_idle_id);
      self->priv->error_idle_id = 0;
    }
  g_mutex_unlock (self->priv->mutex);

  if (self->priv->stream)
    {
      g_object_unref (self->priv->stream);
      self->priv->stream = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_video_stream_parent_class)->dispose (object);
}

static void
tp_stream_engine_video_stream_finalize (GObject *object)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (object);

  g_mutex_free (self->priv->mutex);
}

static void
tp_stream_engine_video_stream_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (object);

    switch (property_id)
    {
    case PROP_STREAM:
      self->priv->stream = g_value_dup_object (value);
      break;
    case PROP_BIN:
      self->priv->bin = g_value_dup_object (value);
      break;
    case PROP_PAD:
      self->priv->pad = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_video_stream_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (object);

    switch (property_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, self->priv->stream);
      break;
    case PROP_BIN:
      g_value_set_object (value, self->priv->bin);
      break;
   case PROP_PAD:
      g_value_set_object (value, self->priv->pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_video_stream_class_init (TpStreamEngineVideoStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineVideoStreamPrivate));
  object_class->dispose = tp_stream_engine_video_stream_dispose;
  object_class->finalize = tp_stream_engine_video_stream_finalize;
  object_class->constructor = tp_stream_engine_video_stream_constructor;
  object_class->set_property = tp_stream_engine_video_stream_set_property;
  object_class->get_property = tp_stream_engine_video_stream_get_property;

  param_spec = g_param_spec_object ("stream",
      "Tp StreamEngine Stream",
      "The Telepathy Stream Engine Stream",
      TP_STREAM_ENGINE_TYPE_STREAM,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STREAM, param_spec);

  param_spec = g_param_spec_object ("bin",
      "The Bin to add stuff to",
      "The Bin to add the elements to",
      GST_TYPE_BIN,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_BIN, param_spec);

  param_spec = g_param_spec_object ("pad",
      "The pad to get the data from",
      "the GstPad the data comes from",
      GST_TYPE_PAD,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PAD, param_spec);
}


static gboolean
src_pad_added_idle_error (gpointer user_data)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (user_data);

  tp_stream_engine_stream_error (self->priv->stream, 0,
      "Error setting up video reception");


  g_mutex_lock (self->priv->mutex);
  self->priv->error_idle_id = 0;
  g_mutex_unlock (self->priv->mutex);

  return FALSE;
}

static void
src_pad_added_cb (TpStreamEngineStream *stream, GstPad *pad, FsCodec *codec,
    gpointer user_data)
{
  TpStreamEngineVideoStream *self = TP_STREAM_ENGINE_VIDEO_STREAM (user_data);
  GstPad *sinkpad;
  GstPad *ghost;
  GstElement *funnel;

  funnel = gst_bin_get_by_name (GST_BIN (self->priv->sink), "funnel");

  if (!funnel)
    {
      g_warning ("Could not get funnel");
      goto error;
    }


  sinkpad = gst_element_get_request_pad (funnel, "sink%d");
  if (!sinkpad)
    {
      gst_object_unref (funnel);
      g_warning ("Could not get funnel sink pad");
      goto error;
    }

  gst_object_unref (funnel);

  ghost = gst_ghost_pad_new (NULL, sinkpad);

  gst_object_unref (sinkpad);

  gst_pad_set_active (ghost, TRUE);

  if (!gst_element_add_pad (self->priv->sink, ghost))
    {
      g_warning ("Could not add ghost pad to sink bin");
      gst_object_unref (ghost);
      goto error;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (pad, ghost)))
    {
      g_warning ("Could not link pad to ghost pad");
      goto error;
    }

  return;


 error:

  g_mutex_lock (self->priv->mutex);
  if (!self->priv->error_idle_id)
    self->priv->error_idle_id =
        g_idle_add (src_pad_added_idle_error, self);
  g_mutex_unlock (self->priv->mutex);
}



TpStreamEngineVideoStream *
tp_stream_engine_video_stream_new (
  TpStreamEngineStream *stream,
  GstBin *bin,
  GstPad *pad,
  GError **error)
{
  TpStreamEngineVideoStream *self = NULL;


  self = g_object_new (TP_STREAM_ENGINE_TYPE_VIDEO_STREAM,
      "stream", stream,
      "bin", bin,
      "pad", pad,
      "is-preview", FALSE,
      NULL);

  if (self->priv->construction_error)
    {
      g_propagate_error (error, self->priv->construction_error);
      g_object_unref (self);
      return NULL;
    }

  return self;
}
