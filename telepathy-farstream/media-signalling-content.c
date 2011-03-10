/*
 * media-signalling-content.c - Source for TfMediaSignallingContent
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2011 Nokia Corporation
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

/**
 * SECTION:media-signalling-content

 * @short_description: Handle the MediaSignalling interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.MediaSignalling on a
 * channel using Farsight2.
 */


#include "media-signalling-content.h"

#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fs-utils.h>

#include <stdarg.h>
#include <string.h>

#include <telepathy-glib/proxy-subclass.h>

#include "tf-signals-marshal.h"
#include "utils.h"


struct _TfMediaSignallingContent {
  TfContent parent;

  TfMediaSignallingChannel *channel;
  TfStream *stream;
  guint handle;
};

struct _TfMediaSignallingContentClass{
  TfContentClass parent_class;
};

G_DEFINE_TYPE (TfMediaSignallingContent, tf_media_signalling_content,
    TF_TYPE_CONTENT)


enum
{
  PROP_TF_CHANNEL = 1,
  PROP_FS_CONFERENCE,
  PROP_FS_SESSION,
  PROP_SINK_PAD,
  PROP_MEDIA_TYPE,
};

enum
{
  SIGNAL_COUNT
};


// static guint signals[SIGNAL_COUNT] = {0};

static void
tf_media_signalling_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec);

static void tf_media_signalling_content_error (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message);

static void src_pad_added (TfStream *stream, GstPad *pad, FsCodec *codec,
    TfMediaSignallingContent *self);

static gboolean request_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self);
static void free_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self);


static void
tf_media_signalling_content_class_init (TfMediaSignallingContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TfContentClass *content_class = TF_CONTENT_CLASS (klass);

  object_class->get_property = tf_media_signalling_content_get_property;

  content_class->content_error = tf_media_signalling_content_error;

  g_object_class_override_property (object_class, PROP_TF_CHANNEL,
      "tf-channel");
  g_object_class_override_property (object_class, PROP_FS_CONFERENCE,
      "fs-conference");
  g_object_class_override_property (object_class, PROP_FS_SESSION,
      "fs-session");
  g_object_class_override_property (object_class, PROP_SINK_PAD,
      "sink-pad");
  g_object_class_override_property (object_class, PROP_MEDIA_TYPE,
      "media-type");
}



static void
tf_media_signalling_content_init (TfMediaSignallingContent *self)
{
}


static void
tf_media_signalling_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  TfMediaSignallingContent *self = TF_MEDIA_SIGNALLING_CONTENT (object);

  switch (property_id)
    {
    case PROP_TF_CHANNEL:
      g_value_set_object (value, self->channel);
      break;
    case PROP_FS_CONFERENCE:
      g_object_get_property (G_OBJECT (self->stream),
          "farsight-conference", value);
      break;
    case PROP_FS_SESSION:
      g_object_get_property (G_OBJECT (self->stream),
          "farsight-session", value);
      break;
    case PROP_SINK_PAD:
      g_object_get_property (G_OBJECT (self->stream),
          "sink-pad", value);
      break;
    case PROP_MEDIA_TYPE:
      g_object_get_property (G_OBJECT (self->stream),
          "media-type", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


TfMediaSignallingContent *
tf_media_signalling_content_new (
    TfMediaSignallingChannel *media_signalling_channel,
    TfStream *stream,
    guint handle)
{
  TfMediaSignallingContent *self =
      g_object_new (TF_TYPE_MEDIA_SIGNALLING_CONTENT, NULL);

  self->channel = media_signalling_channel;
  self->stream = stream;
  self->handle = handle;

  tp_g_signal_connect_object (stream, "src-pad-added",
      G_CALLBACK (src_pad_added), G_OBJECT (self), 0);
  tp_g_signal_connect_object (stream, "request-resource",
      G_CALLBACK (request_resource), G_OBJECT (self), 0);
  tp_g_signal_connect_object (stream, "free-resource",
      G_CALLBACK (free_resource), G_OBJECT (self), 0);

  return self;
}

static void
src_pad_added (TfStream *stream, GstPad *pad, FsCodec *codec,
    TfMediaSignallingContent *self)
{
  FsStream *fs_stream;

  g_object_get (stream, "farsight-stream", &fs_stream, NULL);
  _tf_content_emit_src_pad_added (TF_CONTENT (self), self->handle, fs_stream,
      pad, codec);
  g_object_unref (fs_stream);
}

static gboolean
request_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self)
{
  if (direction & TP_MEDIA_STREAM_DIRECTION_SEND)
    return _tf_content_start_sending (TF_CONTENT (self));
  else
    return TRUE;
}


static void
free_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self)
{
  if (direction & TP_MEDIA_STREAM_DIRECTION_SEND)
    _tf_content_stop_sending (TF_CONTENT (self));
}


static void
tf_media_signalling_content_error (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message)
{
  TfMediaSignallingContent *self = TF_MEDIA_SIGNALLING_CONTENT (content);
  TpMediaStreamError stream_error;

  switch (reason)
    {
    case TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR:
      stream_error = TP_MEDIA_STREAM_ERROR_MEDIA_ERROR;
      break;
    default:
      stream_error = TP_MEDIA_STREAM_ERROR_UNKNOWN;
    }

  tf_stream_error (self->stream,  stream_error, message);
}
