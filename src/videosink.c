/*
 * videosink.c - Source for TpStreamEngineVideoSink
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
#include <stdlib.h>

#include <gst/interfaces/xoverlay.h>
#include <gst/farsight/fs-element-added-notifier.h>

#include <tp-stream-engine.h>

#include <gtk/gtk.h>

#include "videosink.h"
#include "util.h"

G_DEFINE_ABSTRACT_TYPE (TpStreamEngineVideoSink, tp_stream_engine_video_sink,
    G_TYPE_OBJECT);

struct _TpStreamEngineVideoSinkPrivate
{
  GstElement *sink;
  GtkWidget *plug;

  guint window_id;

  gboolean is_preview;

  gulong delete_event_handler_id;
  gulong embedded_handler_id;
};

/* properties */
enum
{
  PROP_0,
  PROP_SINK,
  PROP_WINDOW_ID,
  PROP_IS_PREVIEW
};


/* signals */

enum
{
  PLUG_DELETED,
  SIGNAL_COUNT
};


static guint signals[SIGNAL_COUNT] = {0};

static void
tp_stream_engine_video_sink_init (TpStreamEngineVideoSink *self)
{
  TpStreamEngineVideoSinkPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_VIDEO_SINK, TpStreamEngineVideoSinkPrivate);

  self->priv = priv;
}



static GstElement *
make_video_sink (gboolean is_preview)
{
  const gchar *videosink_name;
  GstElement *sink = NULL;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *bin, *tmp;
  GstPad *pad;
#endif

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

  return sink;
}


static gboolean
delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  g_signal_emit (user_data, signals[PLUG_DELETED], 0);

  gtk_widget_hide (widget);
  return TRUE;
}

static GObject *
tp_stream_engine_video_sink_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineVideoSink *self = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_video_sink_parent_class)->constructor (type, n_props, props);

  self = (TpStreamEngineVideoSink *) obj;

  self->priv->sink = make_video_sink (self->priv->is_preview);

  if (self->priv->sink)
    gst_object_ref (self->priv->sink);

  self->priv->plug = gtk_plug_new (0);
  self->priv->delete_event_handler_id = g_signal_connect (self->priv->plug,
      "delete-event", G_CALLBACK (delete_event), self);
  self->priv->embedded_handler_id = g_signal_connect (self->priv->plug,
      "embedded", G_CALLBACK (gtk_widget_show), NULL);

  self->priv->window_id = gtk_plug_get_id (GTK_PLUG (self->priv->plug));

  return obj;
}

static void
tp_stream_engine_video_sink_dispose (GObject *object)
{
  TpStreamEngineVideoSink *self = TP_STREAM_ENGINE_VIDEO_SINK (object);

  if (self->priv->sink)
    {
      gst_object_unref (self->priv->sink);
      self->priv->sink = NULL;
    }

  if (self->priv->delete_event_handler_id)
    {
      g_signal_handler_disconnect (self->priv->plug,
          self->priv->delete_event_handler_id);
      self->priv->delete_event_handler_id = 0;
    }

  if (self->priv->embedded_handler_id)
    {
      g_signal_handler_disconnect (self->priv->plug,
          self->priv->embedded_handler_id);
      self->priv->embedded_handler_id = 0;
    }

  if (self->priv->plug)
    {
      gtk_widget_destroy (self->priv->plug);
      self->priv->plug = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_video_sink_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_video_sink_parent_class)->dispose (
        object);
}



static void
tp_stream_engine_video_sink_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoSink *self = TP_STREAM_ENGINE_VIDEO_SINK (object);

    switch (property_id)
    {
    case PROP_IS_PREVIEW:
      self->priv->is_preview = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_video_sink_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineVideoSink *self = TP_STREAM_ENGINE_VIDEO_SINK (object);

    switch (property_id)
    {
    case PROP_SINK:
      g_value_set_object (value, self->priv->sink);
      break;
    case PROP_WINDOW_ID:
      g_value_set_uint (value, self->priv->window_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


static void
tp_stream_engine_video_sink_class_init (TpStreamEngineVideoSinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineVideoSinkPrivate));
  object_class->dispose = tp_stream_engine_video_sink_dispose;
  object_class->constructor = tp_stream_engine_video_sink_constructor;
  object_class->set_property = tp_stream_engine_video_sink_set_property;
  object_class->get_property = tp_stream_engine_video_sink_get_property;

  param_spec = g_param_spec_object ("sink",
      "The video sink element",
      "The GstElement that is used as videosink",
      GST_TYPE_ELEMENT,
      G_PARAM_READABLE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SINK, param_spec);

  param_spec = g_param_spec_uint ("window-id",
      "Window id",
      "The window id to Xembed",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_WINDOW_ID,
      param_spec);

  param_spec = g_param_spec_boolean ("is-preview",
      "Window id",
      "The window id to Xembed",
      FALSE,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_IS_PREVIEW,
      param_spec);

  signals[PLUG_DELETED] =
      g_signal_new ("plug-deleted",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
}


gboolean
tp_stream_engine_video_sink_bus_sync_message (
    TpStreamEngineVideoSink *self,
    GstMessage *message)
{
  const GstStructure *s;
  gboolean found = FALSE;
#ifndef MAEMO_OSSO_SUPPORT
  gboolean done = FALSE;
  GstIterator *it = NULL;
  gpointer item;
#endif

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return FALSE;

  s = gst_message_get_structure (message);
  if (!gst_structure_has_name (s, "prepare-xwindow-id"))
    return FALSE;

#ifdef MAEMO_OSSO_SUPPORT

  if (GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (self->priv->sink))
    {
      g_debug ("Setting window id on sink");
      gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (self->priv->sink),
          self->priv->window_id);
      found = TRUE;
    }
#else

  it = gst_bin_iterate_all_by_interface (GST_BIN (self->priv->sink),
      GST_TYPE_X_OVERLAY);

  while (!done) {
    switch (gst_iterator_next (it, &item)) {
    case GST_ITERATOR_OK:
      if (GST_MESSAGE_SRC (message) == item)
        {
          GstXOverlay *xov = item;
          g_debug ("Setting window id on sink");
          gst_x_overlay_set_xwindow_id (xov, self->priv->window_id);
          done = TRUE;
          found = TRUE;
        }
      gst_object_unref (item);
      break;
    case GST_ITERATOR_RESYNC:
      gst_iterator_resync (it);
      break;
    case GST_ITERATOR_ERROR:
      g_warning ("Error finding interface");
      done = TRUE;
      break;
    case GST_ITERATOR_DONE:
      done = TRUE;
      break;
    }
  }
  gst_iterator_free (it);

#endif

  return found;
}
