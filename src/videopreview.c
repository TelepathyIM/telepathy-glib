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

#include <gst/interfaces/xoverlay.h>
#include <gst/farsight/fs-element-added-notifier.h>

#include <telepathy-glib/errors.h>

#include <tp-stream-engine.h>

#include "videopreview.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineVideoPreview, tp_stream_engine_video_preview,
    TP_STREAM_ENGINE_TYPE_VIDEO_SINK);

struct _TpStreamEngineVideoPreviewPrivate
{
  GstPad *pad;
  GstElement *bin;

  FsElementAddedNotifier *element_added_notifier;

  GMutex *mutex;

  GstElement *sinkbin;

  GError *construction_error;
};

/* properties */
enum
{
  PROP_0,
  PROP_BIN,
  PROP_PAD
};

static void
set_preview_props (FsElementAddedNotifier *notifier,
    GstBin *parent G_GNUC_UNUSED,
    GstElement *element,
    gpointer user_data)
{
  if (g_object_has_property ((GObject *) element, "sync"))
    g_object_set ((GObject *) element, "sync", FALSE, NULL);
  if (g_object_has_property ((GObject *) element, "async"))
    g_object_set ((GObject *) element, "async", FALSE, NULL);
}


static void
tp_stream_engine_video_preview_init (TpStreamEngineVideoPreview *self)
{
  TpStreamEngineVideoPreviewPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW, TpStreamEngineVideoPreviewPrivate);

  self->priv = priv;

  self->priv->element_added_notifier = fs_element_added_notifier_new ();

  g_signal_connect (self->priv->element_added_notifier,
      "element-added", G_CALLBACK (set_preview_props), NULL);

  self->priv->mutex = g_mutex_new ();
}

static GstElement *
make_sink (TpStreamEngineVideoPreview *self)
{
  GstElement *bin = gst_bin_new (NULL);
  GstElement *queue;
  GstElement *sink;
  GstPad *pad;

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

  queue = gst_element_factory_make ("queue", NULL);
  if (!queue)
    {
      g_warning ("Could not make queue");
      goto error;
    }

  if (!gst_bin_add (GST_BIN (bin), queue))
    {
      gst_object_unref (queue);
      g_warning ("Could not add queue to bin");
      goto error;
    }

  g_object_set (queue,
      "leaky", 2,
      NULL);

  if (!gst_element_link (queue, sink))
    {
      g_warning ("Could not link queue and sink");
      goto error;
    }

  pad = gst_element_get_static_pad (queue, "sink");
  if (!pad)
    {
      g_warning ("Could not get queue sink pad");
      goto error;
    }

  if (!gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad)))
    {
      g_warning ("Could not add ghostpad to bin");
      goto error;
    }

  gst_object_unref (pad);

  return bin;

 error:
  gst_object_unref (bin);
  return NULL;
}


static GObject *
tp_stream_engine_video_preview_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineVideoPreview *self = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_video_preview_parent_class)->constructor (type, n_props, props);

  self = (TpStreamEngineVideoPreview *) obj;

  if (!self->priv->bin || !self->priv->pad)
    {
      self->priv->construction_error = g_error_new (TP_ERRORS,
          TP_ERROR_INVALID_ARGUMENT,
          "You must set the bin, pad and window-id properties");
      return obj;
    }

  self->priv->sinkbin = make_sink (self);

  if (!self->priv->sinkbin)
    {
      self->priv->construction_error = g_error_new (TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
          "Unable to make sink");
      return obj;;
    }

  fs_element_added_notifier_add (self->priv->element_added_notifier,
      GST_BIN (self->priv->sinkbin));

  if (!gst_bin_add (GST_BIN (self->priv->bin), self->priv->sinkbin))
    {
      self->priv->construction_error = g_error_new (TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
          "Unable to add sink to the bin");
      gst_object_unref (self->priv->sinkbin);
      self->priv->sinkbin = NULL;
      return obj;
    }

  if (gst_element_set_state (self->priv->sinkbin, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
    {
      self->priv->construction_error = g_error_new (TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
          "Could set sink to playing");
      return obj;
    }

  return obj;
}

static void
tp_stream_engine_video_preview_dispose (GObject *object)
{
  TpStreamEngineVideoPreview *self = TP_STREAM_ENGINE_VIDEO_PREVIEW (object);

  if (self->priv->element_added_notifier)
    {
      g_object_unref (self->priv->element_added_notifier);
      self->priv->element_added_notifier = NULL;
    }

  if (self->priv->pad)
    {
      gst_object_unref (self->priv->pad);
      self->priv->pad = NULL;
    }

  if (self->priv->sinkbin)
    {
      gst_element_set_locked_state (self->priv->sinkbin, TRUE);
      gst_element_set_state (self->priv->sinkbin, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self->priv->bin), self->priv->sinkbin);
      self->priv->sinkbin = NULL;
    }

  if (self->priv->bin)
    {
      gst_object_unref (self->priv->bin);
      self->priv->bin = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_video_preview_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_video_preview_parent_class)->dispose (
        object);
}


static void
tp_stream_engine_video_preview_finalize (GObject *object)
{
  TpStreamEngineVideoPreview *self = TP_STREAM_ENGINE_VIDEO_PREVIEW (object);

  g_mutex_free (self->priv->mutex);

  if (G_OBJECT_CLASS (tp_stream_engine_video_preview_parent_class)->finalize)
    G_OBJECT_CLASS (tp_stream_engine_video_preview_parent_class)->finalize (
        object);
}


static void
tp_stream_engine_video_preview_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoPreview *self = TP_STREAM_ENGINE_VIDEO_PREVIEW (object);

    switch (property_id)
    {
    case PROP_BIN:
      self->priv->bin = g_value_dup_object (value);
      break;
    case PROP_PAD:
      {
        GstPad *pad;

        if (self->priv->pad)
          {
            g_warning ("Trying to set already set pad");
            return;
          }

        self->priv->pad = g_value_dup_object (value);

        pad = gst_element_get_static_pad (self->priv->sinkbin, "sink");

        if (GST_PAD_LINK_FAILED (gst_pad_link (self->priv->pad, pad)))
          g_warning ("Could not link pad to preview sink");
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_video_preview_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoPreview *self = TP_STREAM_ENGINE_VIDEO_PREVIEW (object);

    switch (property_id)
    {
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
tp_stream_engine_video_preview_class_init (TpStreamEngineVideoPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineVideoPreviewPrivate));
  object_class->dispose = tp_stream_engine_video_preview_dispose;
  object_class->finalize = tp_stream_engine_video_preview_finalize;
  object_class->constructor = tp_stream_engine_video_preview_constructor;
  object_class->set_property = tp_stream_engine_video_preview_set_property;
  object_class->get_property = tp_stream_engine_video_preview_get_property;

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
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PAD, param_spec);
}



TpStreamEngineVideoPreview *
tp_stream_engine_video_preview_new (GstBin *bin,
    GError **error)
{
  TpStreamEngineVideoPreview *self = NULL;

  self = g_object_new (TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW,
      "bin", bin,
      "is-preview", TRUE,
      NULL);

  if (self->priv->construction_error)
    {
      g_propagate_error (error, self->priv->construction_error);
      g_object_unref (self);
      return NULL;
    }

  return self;
}
