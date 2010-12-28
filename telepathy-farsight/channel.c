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


static void channel_async_initable_init (GAsyncInitableIface *asynciface);

G_DEFINE_TYPE_WITH_CODE (TfChannel, tf_channel, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, channel_async_initable_init));

struct _TfChannelPrivate
{
  TpChannel *channel_proxy;

  TfMediaSignallingChannel *media_signalling_channel;
  TfCallChannel *call_channel;

  gulong channel_invalidated_handler;

  gboolean closed;
};

enum
{
  SIGNAL_CLOSED,
  SIGNAL_GET_CODEC_CONFIG,
  SIGNAL_FS_CONFERENCE_ADD,
  SIGNAL_FS_CONFERENCE_REMOVE,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

enum
{
  PROP_CHANNEL = 1,
  PROP_OBJECT_PATH,
  PROP_FS_CONFERENCES
};

static GList *media_signalling_channel_get_config (
    TfMediaSignallingChannel *msc,
    guint media_type, TfChannel *self);

static void shutdown_channel (TfChannel *self);

static void channel_fs_conference_add (GObject *chan,
    FsConference *conf, TfChannel *self);
static void channel_fs_conference_remove (GObject *chan,
    FsConference *conf, TfChannel *self);

static void tf_channel_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
static gboolean tf_channel_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error);

static void
tf_channel_init (TfChannel *self)
{
  TfChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TF_TYPE_CHANNEL, TfChannelPrivate);

  self->priv = priv;
}

static void
channel_async_initable_init (GAsyncInitableIface *asynciface)
{
  asynciface->init_async = tf_channel_init_async;
  asynciface->init_finish = tf_channel_init_finish;
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
    case PROP_FS_CONFERENCES:
      if (self->priv->call_channel)
        g_object_get_property (G_OBJECT (self->priv->call_channel),
            "fs-conferences", value);
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
call_channel_ready (GObject *obj, GAsyncResult *call_res, gpointer user_data)
{
  GError *error = NULL;
  GSimpleAsyncResult *res = user_data;
  TfChannel *self = TF_CHANNEL (
      g_async_result_get_source_object (G_ASYNC_RESULT (res)));

  self->priv->call_channel = TF_CALL_CHANNEL (g_async_initable_new_finish (
          G_ASYNC_INITABLE (obj), call_res, &error));

  if (error)
    {
      g_simple_async_result_set_op_res_gboolean (res, FALSE);
      g_simple_async_result_set_from_error (res, error);
      g_clear_error (&error);
    }
  else
    {
      g_simple_async_result_set_op_res_gboolean (res, TRUE);


      tp_g_signal_connect_object (self->priv->call_channel,
          "fs-conference-add", G_CALLBACK (channel_fs_conference_add),
          self, 0);
      tp_g_signal_connect_object (self->priv->call_channel,
          "fs-conference-remove", G_CALLBACK (channel_fs_conference_remove),
          self, 0);
    }


  g_simple_async_result_complete (res);

  self->priv->channel_invalidated_handler = g_signal_connect (
      self->priv->channel_proxy,
      "invalidated", G_CALLBACK (channel_invalidated), self);

  g_object_unref (res);
}

static void
channel_prepared (GObject *obj,
    GAsyncResult *proxy_res,
    gpointer user_data)
{
  TpChannel *channel_proxy = TP_CHANNEL (obj);
  TpProxy *as_proxy = (TpProxy *) channel_proxy;
  GSimpleAsyncResult *res = user_data;
  GError *error = NULL;
  TfChannel *self = TF_CHANNEL (
      g_async_result_get_source_object (G_ASYNC_RESULT (res)));

  if (!tp_proxy_prepare_finish (channel_proxy, proxy_res, &error))
    {
      g_simple_async_result_propagate_error (res, &error);
      g_simple_async_result_set_op_res_gboolean (res, FALSE);
      g_simple_async_result_complete (res);
      shutdown_channel (self);
      g_object_unref (res);
      return;
    }

  if (self->priv->closed)
    {
      g_simple_async_result_set_error (res, TP_ERROR, TP_ERROR_CANCELLED,
          "Channel already closed");
      g_simple_async_result_set_op_res_gboolean (res, FALSE);
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  if (tp_proxy_has_interface_by_id (as_proxy,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MEDIA_SIGNALLING))
    {
      self->priv->media_signalling_channel =
          tf_media_signalling_channel_new (channel_proxy);

      tp_g_signal_connect_object (self->priv->media_signalling_channel,
          "get-codec-config", G_CALLBACK (media_signalling_channel_get_config),
          self, 0);

      tp_g_signal_connect_object (self->priv->media_signalling_channel,
          "session-created", G_CALLBACK (channel_fs_conference_add),
          self, 0);
      g_simple_async_result_set_op_res_gboolean (res, TRUE);
    }
  else if (tp_proxy_has_interface_by_id (as_proxy,
          TF_FUTURE_IFACE_QUARK_CHANNEL_TYPE_CALL))
    {
      tf_call_channel_new_async (channel_proxy, call_channel_ready, res);

      self->priv->channel_invalidated_handler = g_signal_connect (
          self->priv->channel_proxy,
          "invalidated", G_CALLBACK (channel_invalidated), self);
      return;
    }
  else
    {
      g_simple_async_result_set_error (res, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Channel does not implement "
          TP_IFACE_CHANNEL_INTERFACE_MEDIA_SIGNALLING " or "
          TF_FUTURE_IFACE_CHANNEL_TYPE_CALL);
      g_simple_async_result_set_op_res_gboolean (res, FALSE);

      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  g_simple_async_result_complete (res);

  self->priv->channel_invalidated_handler = g_signal_connect (
      self->priv->channel_proxy,
      "invalidated", G_CALLBACK (channel_invalidated), self);

  g_object_unref (res);
}


static void
tf_channel_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TfChannel *self = TF_CHANNEL (initable);
  GSimpleAsyncResult *res;

  if (cancellable != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
          G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
          "TfChannel initialisation does not support cancellation");
      return;
    }

  res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tf_channel_init_async);
  tp_proxy_prepare_async (self->priv->channel_proxy, NULL,
      channel_prepared, res);
}

static gboolean
tf_channel_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *simple_res;

  g_return_val_if_fail (g_simple_async_result_is_valid (res,
          G_OBJECT (initable), tf_channel_init_async), FALSE);
  simple_res = G_SIMPLE_ASYNC_RESULT (res);

  g_simple_async_result_propagate_error (simple_res, error);

  return g_simple_async_result_get_op_res_gboolean (simple_res);
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


  g_object_class_install_property (object_class, PROP_FS_CONFERENCES,
      g_param_spec_object ("fs-conferences",
          "Farsight2 FsConferences objects",
          "GPtrArray of Farsight2 FsConferences for this channel",
          G_TYPE_PTR_ARRAY,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * TfChannel::closed:
   *
   * This function is called after a channel is closed, either because
   * it has been closed by the connection manager or because we had a locally
   * generated error.
   */

  signals[SIGNAL_CLOSED] =
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

  signals[SIGNAL_GET_CODEC_CONFIG] =
      g_signal_new ("get-codec-config",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL,
          _tf_marshal_BOXED__UINT,
          FS_TYPE_CODEC_LIST, 1, G_TYPE_UINT);


  /**
   * TfChannel::fs-conference-add
   * @tfchannel: the #TfChannel
   * @conf: a #FsConference
   *
   * When this signal is emitted, the conference should be added to the
   * application's pipeline.
   */

  signals[SIGNAL_FS_CONFERENCE_ADD] = g_signal_new ("fs-conference-add",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tf_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, FS_TYPE_CONFERENCE);

  /**
   * TfChannel::fs-conference-remove
   * @tfchannel: the #TfChannel
   * @conf: a #FsConference
   *
   * When this signal is emitted, the conference should be remove from the
   * application's pipeline.
   */

  signals[SIGNAL_FS_CONFERENCE_REMOVE] = g_signal_new ("fs-conference-remove",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
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
      if (self->priv->channel_invalidated_handler)
        {
          g_signal_handler_disconnect (
            self->priv->channel_proxy, self->priv->channel_invalidated_handler);
          self->priv->channel_invalidated_handler = 0;
        }
    }

  g_signal_emit (self, signals[SIGNAL_CLOSED], 0);

  self->priv->closed = TRUE;
}

static void
channel_invalidated (TpChannel *channel_proxy,
    guint domain,
    gint code,
    gchar *message,
    TfChannel *self)
{
  shutdown_channel (self);
}

/**
 * tf_channel_new_async:
 * @channel_proxy: a #TpChannel proxy
 *
 * Creates a new #TfChannel from an existing channel proxy, the new
 * TfChannel object will be return in the async callback.
 *
 * The user must call g_async_initable_new_finish() in the callback
 * to get the finished object.
 */

void
tf_channel_new_async (TpChannel *channel_proxy,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  return g_async_initable_new_async (TF_TYPE_CHANNEL,
      0, NULL, callback, user_data,
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

  g_signal_emit (self, signals[SIGNAL_GET_CODEC_CONFIG], 0,
      media_type,
      &local_codec_config);

  return local_codec_config;
}

static void
channel_fs_conference_add (GObject *chan, FsConference *conf,
    TfChannel *self)
{
  g_object_notify (G_OBJECT (self), "fs-conferences");
  g_signal_emit (self, signals[SIGNAL_FS_CONFERENCE_ADD], 0,
      conf);
}

static void
channel_fs_conference_remove (GObject *chan, FsConference *conf,
    TfChannel *self)
{
  g_object_notify (G_OBJECT (self), "fs-conferences");
  g_signal_emit (self, signals[SIGNAL_FS_CONFERENCE_REMOVE], 0,
      conf);
}

