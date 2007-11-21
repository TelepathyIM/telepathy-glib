/*
 * channel.c - proxy for a Telepathy channel
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#include "telepathy-glib/channel.h"

#include <telepathy-glib/handle.h>

#include "_gen/signals-marshal.h"

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "debug-internal.h"
#include "proxy-internal.h"

/**
 * SECTION:channel
 * @title: TpChannel
 * @short_description: proxy object for a Telepathy channel
 * @see_also: #TpConnection
 *
 * #TpChannel objects provide convenient access to Telepathy channels.
 */

/**
 * TpChannelClass:
 *
 * The class of a #TpChannel.
 */
struct _TpChannelClass {
    TpProxyClass parent_class;
    /*<private>*/
};

/**
 * TpChannel:
 *
 * A proxy object for a Telepathy channel.
 */
struct _TpChannel {
    TpProxy parent;
    /*<private>*/
    GQuark channel_type;
    TpHandleType handle_type;
    TpHandle handle;
};

enum
{
  PROP_CHANNEL_TYPE = 1,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  N_PROPS
};

G_DEFINE_TYPE (TpChannel,
    tp_channel,
    TP_TYPE_PROXY);

enum {
    SIGNAL_CHANNEL_READY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

static void
tp_channel_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value,
          g_quark_to_string (self->channel_type));
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->handle_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, self->handle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_channel_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CHANNEL_TYPE:
      /* can only be set in constructor */
      g_assert (self->channel_type == 0);
      self->channel_type = g_quark_from_string (g_value_get_string (value));
      break;
    case PROP_HANDLE_TYPE:
      self->handle_type = g_value_get_uint (value);
      break;
    case PROP_HANDLE:
      self->handle = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_channel_got_interfaces_cb (TpProxy *proxy,
                              const gchar **interfaces,
                              const GError *error,
                              gpointer unused)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (error == NULL)
    {
      DEBUG ("%p: Introspected interfaces", self);
      if (interfaces != NULL)
        {
          const gchar **iter;

          for (iter = interfaces; *iter != NULL; iter++)
            {
              DEBUG ("\t- %s", *iter);
              tp_proxy_add_interface_by_id ((TpProxy *) self,
                  g_quark_from_string (*iter));
            }
        }

      DEBUG ("%p: emitting channel-ready", self);
      g_signal_emit (self, signals[SIGNAL_CHANNEL_READY], 0,
          g_quark_to_string (self->channel_type), self->handle_type,
          self->handle, interfaces);
    }
  else
    {
      DEBUG ("%p: GetInterfaces() failed", self);
      tp_proxy_invalidated ((TpProxy *) self, error);
    }
}

static void
tp_channel_got_channel_type_cb (TpProxy *proxy,
                                const gchar *channel_type,
                                const GError *error,
                                gpointer unused)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (error == NULL)
    {
      DEBUG ("%p: Introspected channel type %s", self, channel_type);
      self->channel_type = g_quark_from_string (channel_type);
    }
  else
    {
      DEBUG ("%p: GetChannelType() failed, will self-destruct", self);
      tp_proxy_invalidated ((TpProxy *) self, error);
      return;
    }

  g_assert (self->channel_type != 0);
  tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

  tp_cli_channel_call_get_interfaces (self, -1,
      tp_channel_got_interfaces_cb, NULL, NULL);
}

static void
tp_channel_got_handle_cb (TpProxy *proxy,
                          guint handle_type,
                          guint handle,
                          const GError *error,
                          gpointer unused)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (error == NULL)
    {
      DEBUG ("%p: Introspected handle #%d of type %d", self, handle,
          handle_type);
      self->handle_type = handle_type;
      self->handle = handle;
    }
  else
    {
      DEBUG ("%p: GetHandle() failed, will self-destruct", self);
      tp_proxy_invalidated ((TpProxy *) self, error);
      return;
    }

  if (self->channel_type == 0)
    {
      tp_cli_channel_call_get_channel_type (self, -1,
          tp_channel_got_channel_type_cb, NULL, NULL);
    }
  else
    {
      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

      tp_cli_channel_call_get_interfaces (self, -1,
          tp_channel_got_interfaces_cb, NULL, NULL);
    }
}

static void
tp_channel_closed_cb (DBusGProxy *proxy,
                      TpProxySignalConnection *data)
{
  TpChannel *self = TP_CHANNEL (data->proxy);
  GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Channel was closed" };

  tp_proxy_invalidated ((TpProxy *) self, &e);
}

static GObject *
tp_channel_constructor (GType type,
                        guint n_params,
                        GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_channel_parent_class;
  TpChannel *self = TP_CHANNEL (object_class->constructor (type,
        n_params, params));

  /* connect to my own Closed signal and self-destruct when it arrives */
  tp_cli_channel_connect_to_closed (self, tp_channel_closed_cb, NULL, NULL);

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self, (self->channel_type != 0) ? g_quark_to_string (self->channel_type)
                                      : "(null)",
      self->handle, self->handle_type);

  if (self->handle_type == TP_UNKNOWN_HANDLE_TYPE
      || (self->handle == 0 && self->handle_type != TP_HANDLE_TYPE_NONE))
    {
      tp_cli_channel_call_get_handle (self, -1,
          tp_channel_got_handle_cb, NULL, NULL);
    }
  else if (self->channel_type == 0)
    {
      tp_cli_channel_call_get_channel_type (self, -1,
          tp_channel_got_channel_type_cb, NULL, NULL);
    }
  else
    {
      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

      tp_cli_channel_call_get_interfaces (self, -1,
          tp_channel_got_interfaces_cb, NULL, NULL);
    }

  return (GObject *) self;
}

static void
tp_channel_init (TpChannel *self)
{
  DEBUG ("%p", self);
}

static void
tp_channel_dispose (GObject *object)
{
  DEBUG ("%p", object);

  ((GObjectClass *) tp_channel_parent_class)->dispose (object);
}

static void
tp_channel_class_init (TpChannelClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructor = tp_channel_constructor;
  object_class->get_property = tp_channel_get_property;
  object_class->set_property = tp_channel_set_property;
  object_class->dispose = tp_channel_dispose;

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL;
  proxy_class->must_have_unique_name = TRUE;
  tp_proxy_class_hook_on_interface_add (proxy_class,
      tp_cli_channel_add_signals);

  /**
   * TpChannel:channel-type:
   *
   * The D-Bus interface representing the type of this channel.
   *
   * Read-only except during construction. If %NULL during construction
   * (default), we ask the remote D-Bus object what its channel type is;
   * reading this property will yield %NULL until we get the reply, or if
   * GetChannelType() fails.
   */
  param_spec = g_param_spec_string ("channel-type", "Telepathy channel type",
      "The D-Bus interface representing the type of this channel",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CHANNEL_TYPE,
      param_spec);

  /**
   * TpChannel:handle-type:
   *
   * The #TpHandleType of this channel's associated handle, or 0 if no
   * handle, or TP_UNKNOWN_HANDLE_TYPE if unknown.
   *
   * Read-only except during construction. If this is TP_UNKNOWN_HANDLE_TYPE
   * during construction (default), we ask the remote D-Bus object what its
   * handle type is; reading this property will yield TP_UNKNOWN_HANDLE_TYPE
   * until we get the reply.
   */
  param_spec = g_param_spec_uint ("handle-type", "Handle type",
      "The TpHandleType of this channel",
      0, G_MAXUINT32, TP_UNKNOWN_HANDLE_TYPE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_HANDLE_TYPE,
      param_spec);

  /**
   * TpChannel:handle:
   *
   * This channel's associated handle, or 0 if no handle or unknown.
   *
   * Read-only except during construction. If this is 0
   * during construction, and handle-type is not TP_HANDLE_TYPE_NONE (== 0),
   * we ask the remote D-Bus object what its handle type is; reading this
   * property will yield 0 until we get the reply, or if GetHandle()
   * fails.
   */
  param_spec = g_param_spec_uint ("handle", "Handle",
      "The TpHandle of this channel", 0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_HANDLE,
      param_spec);

  /**
   * TpChannel::channel-ready:
   * @self: the channel proxy
   * @channel_type: the type of the channel (a D-Bus interface name)
   * @handle_type: the type of the handle, or 0 if @handle is 0
   *    (a member of #TpHandleType)
   * @handle: the handle (contact, etc.) with which the channel communicates,
   *    or 0 if @handle is 0
   *
   * Emitted once, when the channel's channel type, handle type, handle and
   * extra interfaces have all been retrieved.
   *
   * By the time this signal is emitted, the channel-type, handle-type and
   * handle properties are guaranteed to be valid, and the interfaces will
   * have been added in the #TpProxy code.
   */
  signals[SIGNAL_CHANNEL_READY] = g_signal_new ("channel-ready",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__STRING_UINT_UINT_BOXED,
      G_TYPE_NONE, 4,
      G_TYPE_STRING,    /* Channel type */
      G_TYPE_UINT,      /* Handle type */
      G_TYPE_UINT,      /* Handle */
      G_TYPE_STRV);     /* Extra interfaces */
}
