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
 * channel using Farstream.
 */

#include "config.h"

#include "media-signalling-content.h"

#include <farstream/fs-conference.h>
#include <farstream/fs-utils.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>

#include <stdarg.h>
#include <string.h>

#include "utils.h"


struct _TfMediaSignallingContent {
  TfContent parent;

  TfMediaSignallingChannel *channel;
  TfStream *stream;
  guint handle;

  gboolean receiving;
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
  PROP_STREAM_ID
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
    const gchar *message);

static GstIterator * tf_media_signalling_content_iterate_src_pads (
    TfContent *content,
    guint *handles,
    guint handle_count);

static void src_pad_added (TfStream *stream, GstPad *pad, FsCodec *codec,
    TfMediaSignallingContent *self);

static gboolean request_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self);
static void free_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self);
static void restart_source (TfStream *stream, TfMediaSignallingContent *self);


static void
tf_media_signalling_content_class_init (TfMediaSignallingContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TfContentClass *content_class = TF_CONTENT_CLASS (klass);

  object_class->get_property = tf_media_signalling_content_get_property;

  content_class->content_error = tf_media_signalling_content_error;
  content_class->iterate_src_pads =
      tf_media_signalling_content_iterate_src_pads;

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

  g_object_class_install_property (object_class, PROP_STREAM_ID,
      g_param_spec_uint ("stream-id",
          "stream ID",
          "A number identifying this stream within "
          "its channel.",
          0, G_MAXUINT, 0,
          G_PARAM_READABLE |
          G_PARAM_STATIC_STRINGS));
}



static void
tf_media_signalling_content_init (TfMediaSignallingContent *self)
{
  self->receiving = TRUE;
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
          "farstream-conference", value);
      break;
    case PROP_FS_SESSION:
      g_object_get_property (G_OBJECT (self->stream),
          "farstream-session", value);
      break;
    case PROP_SINK_PAD:
      g_object_get_property (G_OBJECT (self->stream),
          "sink-pad", value);
      break;
    case PROP_MEDIA_TYPE:
      g_object_get_property (G_OBJECT (self->stream),
          "media-type", value);
      break;
    case PROP_STREAM_ID:
      g_object_get_property (G_OBJECT (self->stream),
          "stream-id", value);
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
  GstElement *conf;
  FsSession *session;
  GList *codec_prefs;

  self->channel = media_signalling_channel;
  self->stream = stream;
  self->handle = handle;

  tp_g_signal_connect_object (stream, "src-pad-added",
      G_CALLBACK (src_pad_added), G_OBJECT (self), 0);
  tp_g_signal_connect_object (stream, "request-resource",
      G_CALLBACK (request_resource), G_OBJECT (self), 0);
  tp_g_signal_connect_object (stream, "free-resource",
      G_CALLBACK (free_resource), G_OBJECT (self), 0);
  tp_g_signal_connect_object (stream, "restart-source",
      G_CALLBACK (restart_source), G_OBJECT (self), 0);

  g_object_get (stream,
      "farstream-conference", &conf,
      "farstream-session", &session,
      NULL);

  codec_prefs = fs_utils_get_default_codec_preferences (conf);
  if (!fs_session_set_codec_preferences (session, codec_prefs, NULL))
    tf_stream_error (stream, TP_MEDIA_STREAM_ERROR_MEDIA_ERROR,
        "Default codec preferences disabled all codecs");

  fs_codec_list_destroy (codec_prefs);
  g_object_unref (session);
  gst_object_unref (conf);

  return self;
}

static void
src_pad_added (TfStream *stream, GstPad *pad, FsCodec *codec,
    TfMediaSignallingContent *self)
{
  FsStream *fs_stream;

  g_object_get (stream, "farstream-stream", &fs_stream, NULL);
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
  else if (direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE)
    {
      if (!self->receiving)
        {
          guint handles[2] = { self->handle, 0};

          self->receiving = _tf_content_start_receiving (TF_CONTENT (self),
              handles, 1);
        }

      return self->receiving;
    }
  else
    g_assert_not_reached ();
}


static void
free_resource (TfStream *stream, guint direction,
    TfMediaSignallingContent *self)
{
  guint handles[2] = { self->handle, 0};

  if (direction & TP_MEDIA_STREAM_DIRECTION_SEND)
    _tf_content_stop_sending (TF_CONTENT (self));

  if (self->receiving && direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE)
    {
      _tf_content_stop_receiving (TF_CONTENT (self), handles, 1);
      self->receiving = FALSE;
    }
}

static void
restart_source (TfStream *stream, TfMediaSignallingContent *self)
{
  g_signal_emit_by_name (self, "restart-source");
}

static void
tf_media_signalling_content_error (TfContent *content,
    const gchar *message)
{
  TfMediaSignallingContent *self = TF_MEDIA_SIGNALLING_CONTENT (content);

  tf_stream_error (self->stream, TP_MEDIA_STREAM_ERROR_MEDIA_ERROR, message);
}

static GstIterator *
tf_media_signalling_content_iterate_src_pads (TfContent *content,
    guint *handles,
    guint handle_count)
{
  TfMediaSignallingContent *self = TF_MEDIA_SIGNALLING_CONTENT (content);
  GstIterator *iter = NULL;
  FsStream *fs_stream;

  g_return_val_if_fail (handle_count <= 1, NULL);

  g_object_get (self->stream, "farstream-stream", &fs_stream, NULL);
  iter = fs_stream_iterate_src_pads (fs_stream);
  g_object_unref (fs_stream);

  return iter;
}
