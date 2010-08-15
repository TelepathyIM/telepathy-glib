/*
 * channel.c - Source for TfChannel
 * Copyright (C) 2006-2007 Collabora Ltd.
 * Copyright (C) 2006-2007 Nokia Corporation
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
 * SECTION:channel
 * @short_description: Handle the MediaSignalling interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.MediaSignalling on a
 * channel using Farsight2.
 */

#include <stdlib.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include <gst/farsight/fs-conference-iface.h>

#include "extensions/extensions.h"

#include "channel.h"
#include "channel-priv.h"
#include "tf-signals-marshal.h"
#include "media-signalling-channel.h"
#include "call-channel.h"

G_DEFINE_TYPE (TfChannel, tf_channel, G_TYPE_OBJECT);

struct _TfChannelPrivate
{
  TpChannel *channel_proxy;

  TfMediaSignallingChannel *media_signalling_channel;
  FsConference *fsconference;

  TfCallChannel *call_channel;

  gulong channel_invalidated_handler;
  guint  channel_ready_idle;

  gboolean channel_handled;
};

enum
{
  CLOSED,
  HANDLER_RESULT,
  GET_CODEC_CONFIG,
  FS_CONFERENCE_READY,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

enum
{
  PROP_CHANNEL = 1,
  PROP_OBJECT_PATH,
  PROP_FS_CONFERENCE
};

static GList *media_signalling_channel_get_config (
    TfMediaSignallingChannel *msc,
    guint media_type, TfChannel *self);
static void media_signalling_session_created (TfMediaSignallingChannel *msc,
    FsConference *fsconference, TfChannel *self);

static void shutdown_channel (TfChannel *self);


static void
tf_channel_init (TfChannel *self)
{
  TfChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TF_TYPE_CHANNEL, TfChannelPrivate);

  self->priv = priv;
}

static void
tf_channel_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TfChannel *self = TF_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_value_set_object (value, self->priv->channel_proxy);
      break;
    case PROP_OBJECT_PATH:
        {
          TpProxy *as_proxy = (TpProxy *) self->priv->channel_proxy;

          g_value_set_string (value, as_proxy->object_path);
        }
      break;
    case PROP_FS_CONFERENCE:
      if (self->priv->fsconference)
        g_value_set_object (value, self->priv->fsconference);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tf_channel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  TfChannel *self = TF_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      self->priv->channel_proxy = TP_CHANNEL (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void channel_invalidated (TpChannel *channel_proxy,
    guint domain, gint code, gchar *message, TfChannel *self);


static void
channel_ready (TpChannel *channel_proxy,
               const GError *error,
               gpointer user_data)
{
  TfChannel *self = TF_CHANNEL (user_data);
  TpProxy *as_proxy = (TpProxy *) channel_proxy;


  if (error)
    {
      if (!self->priv->channel_handled)
        {
          self->priv->channel_handled = TRUE;
          g_signal_emit (self, signals[HANDLER_RESULT], 0, error);
        }

      shutdown_channel (self);
      return;
    }


  if (self->priv->channel_handled)
    return;


  if (tp_proxy_has_interface_by_id (as_proxy,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MEDIA_SIGNALLING))
    {

      self->priv->media_signalling_channel =
          tf_media_signalling_channel_new (channel_proxy);

      tp_g_signal_connect_object (self->priv->media_signalling_channel,
          "get-codec-config", G_CALLBACK (media_signalling_channel_get_config),
          self, 0);

      tp_g_signal_connect_object (self->priv->media_signalling_channel,
          "session-created", G_CALLBACK (media_signalling_session_created),
          self, 0);

    }
  else if (tp_proxy_has_interface_by_id (as_proxy,
          TF_FUTURE_IFACE_QUARK_CHANNEL_TYPE_CALL))
    {
      self->priv->call_channel = tf_call_channel_new (channel_proxy);

      tp_g_signal_connect_object (self->priv->media_signalling_channel,
          "get-codec-config", G_CALLBACK (media_signalling_channel_get_config),
          self, 0);
    }
  else
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
        "Channel does not implement "
        TP_IFACE_CHANNEL_INTERFACE_MEDIA_SIGNALLING " or "
                   TF_FUTURE_IFACE_CHANNEL_TYPE_CALL};

      g_message ("%s", e.message);
      self->priv->channel_handled = TRUE;
      g_signal_emit (self, signals[HANDLER_RESULT], 0, &e);
      return;
    }

  self->priv->channel_handled = TRUE;
  g_signal_emit (self, signals[HANDLER_RESULT], 0, NULL);


  self->priv->channel_invalidated_handler = g_signal_connect (
      self->priv->channel_proxy,
      "invalidated", G_CALLBACK (channel_invalidated), self);

}

static gboolean
channel_ready_idle (gpointer data)
{
  TfChannel *self = data;

  tp_channel_call_when_ready (self->priv->channel_proxy, channel_ready, self);

  return FALSE;
}

static GObject *
tf_channel_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TfChannel *self;

  obj = G_OBJECT_CLASS (tf_channel_parent_class)->
           constructor (type, n_props, props);
  self = (TfChannel *) obj;

  self->priv->channel_ready_idle = g_idle_add (channel_ready_idle, self);

  return obj;
}

static void
tf_channel_dispose (GObject *object)
{
  TfChannel *self = TF_CHANNEL (object);

  g_debug (G_STRFUNC);

  if (self->priv->media_signalling_channel)
    {
      g_object_unref (self->priv->media_signalling_channel);
      self->priv->media_signalling_channel = NULL;
    }

  if (self->priv->channel_ready_idle)
    {
      g_source_remove (self->priv->channel_ready_idle);
      self->priv->channel_ready_idle = 0;
    }

  if (self->priv->channel_proxy)
    {
      TpChannel *tmp;

      if (self->priv->channel_invalidated_handler != 0)
        g_signal_handler_disconnect (self->priv->channel_proxy,
            self->priv->channel_invalidated_handler);

      tmp = self->priv->channel_proxy;
      self->priv->channel_proxy = NULL;
      g_object_unref (tmp);
    }

  if (G_OBJECT_CLASS (tf_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tf_channel_parent_class)->dispose (object);
}

static void
tf_channel_class_init (TfChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TfChannelPrivate));

  object_class->set_property = tf_channel_set_property;
  object_class->get_property = tf_channel_get_property;

  object_class->constructor = tf_channel_constructor;

  object_class->dispose = tf_channel_dispose;

  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_object ("channel",
          "TpChannel object",
          "Telepathy channel object which this media channel should operate on",
          TP_TYPE_CHANNEL,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      g_param_spec_string ("object-path",
          "channel object path",
          "D-Bus object path of the Telepathy channel which this channel"
          " operates on",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


  g_object_class_install_property (object_class, PROP_FS_CONFERENCE,
      g_param_spec_object ("fs-conference",
          "Farsight2 FsConference ",
          "The Farsight2 conference for this channel",
          FS_TYPE_CONFERENCE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * TfChannel::handler-result:
   * @error: a #GError containing the error or %NULL if there was no error
   *
   * This message is emitted when we are ready to handle the channel with %NULL
   * or with an #GError if we can not handle the channel.
   */

  signals[HANDLER_RESULT] = g_signal_new ("handler-result",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * TfChannel::closed:
   *
   * This function is called after a channel is closed, either because
   * it has been closed by the connection manager or because we had a locally
   * generated error.
   */

  signals[CLOSED] =
      g_signal_new ("closed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

  /**
   * TfChannel::stream-get-codec-config:
   * @tfchannel: the #TfChannel
   * @media_type: The #TpMediaStreamType of the stream
   *
   * This is emitted when a new stream is created and allows the caller to
   * specify his codec preferences.
   *
   * Returns: a #GList of #FsCodec
   */

  signals[GET_CODEC_CONFIG] =
      g_signal_new ("get-codec-config",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          _tf_marshal_BOXED__UINT,
          FS_TYPE_CODEC_LIST, 1, G_TYPE_UINT);


  /**
   * TfChannel::fs-conference-ready
   * @tfchannel: the #TfChannel
   * @fsconference: the #FsConference object
   *
   * This is emitted when the #FsConference object is created and is ready
   * to be added to the pipeline
   */

  signals[FS_CONFERENCE_READY] =
      g_signal_new ("fs-conference-ready",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          _tf_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1, FS_TYPE_CONFERENCE);
}

static void
shutdown_channel (TfChannel *self)
{
  if (self->priv->media_signalling_channel)
    {
      g_object_unref (self->priv->media_signalling_channel);
      self->priv->media_signalling_channel = NULL;
    }

  if (self->priv->channel_proxy != NULL)
    {
      /* I've ensured that this is true everywhere this function is called */
      g_assert (self->priv->channel_ready_idle == 0);

      if (self->priv->channel_invalidated_handler)
        {
          g_signal_handler_disconnect (
            self->priv->channel_proxy, self->priv->channel_invalidated_handler);
          self->priv->channel_invalidated_handler = 0;
        }
    }

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
channel_invalidated (TpChannel *channel_proxy,
    guint domain,
    gint code,
    gchar *message,
    TfChannel *self)
{
  GError e = { domain, code, message };

  if (!self->priv->channel_handled)
    {
      self->priv->channel_handled = TRUE;
      g_signal_emit (self, signals[HANDLER_RESULT], 0, &e);
    }

  if (self->priv->channel_ready_idle)
    {
      g_source_remove (self->priv->channel_ready_idle);
      self->priv->channel_ready_idle = 0;
    }

  shutdown_channel (self);
}

/**
 * tf_channel_new:
 * @channel_proxy: a #TpChannel proxy
 *
 * Creates a new #TfChannel from an existing channel proxy
 *
 * Returns: a new #TfChannel
 */

TfChannel *
tf_channel_new (TpChannel *channel_proxy)
{
  return g_object_new (TF_TYPE_CHANNEL,
      "channel", channel_proxy,
      NULL);
}

/**
 * tf_channel_error:
 * @chan: a #TfChannel
 * @error: the error number of type #TpMediaStreamError
 * @message: the error message
 *
 * Stops the channel and all stream related to it and sends an error to the
 * connection manager.
 */

void
tf_channel_error (TfChannel *chan,
    TpMediaStreamError error,
    const gchar *message)
{
  if (chan->priv->media_signalling_channel)
    tf_media_signalling_channel_error (chan->priv->media_signalling_channel,
        error, message);

  if (!chan->priv->channel_handled)
    {
      /* we haven't yet decided whether we're handling this channel. This
       * seems an unlikely situation at this point, but for the sake of
       * returning *something* from HandleChannel, let's claim we are */

      g_signal_emit (chan, signals[HANDLER_RESULT], 0, NULL);
    }

  if (chan->priv->channel_ready_idle != 0)
    {
      if (chan->priv->channel_ready_idle)
        g_source_remove (chan->priv->channel_ready_idle);
      chan->priv->channel_ready_idle = 0;
    }

  shutdown_channel (chan);
}

/**
 * tf_channel_bus_message:
 * @channel: A #TfChannel
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
tf_channel_bus_message (TfChannel *channel,
    GstMessage *message)
{
  if (channel->priv->media_signalling_channel)
    return tf_media_signalling_channel_bus_message (
        channel->priv->media_signalling_channel, message);
  else
    return FALSE;
}

static GList *
media_signalling_channel_get_config (TfMediaSignallingChannel *msc,
    guint media_type, TfChannel *self)
{
  GList *local_codec_config = NULL;

  g_signal_emit (self, signals[GET_CODEC_CONFIG], 0,
      media_type,
      &local_codec_config);

  return local_codec_config;
}

static void
media_signalling_session_created (TfMediaSignallingChannel *msc,
    FsConference *fsconference, TfChannel *self)
{
  g_assert (self->priv->fsconference == NULL);

  self->priv->fsconference = fsconference;

  g_object_notify (G_OBJECT (self), "fs-conference");
  g_signal_emit (self, signals[FS_CONFERENCE_READY], 0, fsconference);
}
