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

static gboolean
tp_channel_destroy_idle_func (gpointer user_data)
{
  DEBUG ("%p", user_data);
  g_object_run_dispose (user_data);
  return FALSE;
}

static void
tp_channel_closed_cb (gpointer object, gpointer user_data)
{
  DEBUG ("%p", object);
  g_idle_add (tp_channel_destroy_idle_func, object);
}

static void
tp_channel_got_interfaces_cb (DBusGProxy *proxy,
                              DBusGProxyCall *call,
                              gpointer user_data)
{
  TpChannel *self = TP_CHANNEL (proxy);
  GError *error = NULL;
  gchar **interfaces;

  if (dbus_g_proxy_end_call (proxy, call, &error,
        G_TYPE_STRV, &interfaces,
        G_TYPE_INVALID))
    {
      DEBUG ("%p: Introspected interfaces", self);
      if (interfaces != NULL)
        {
          gchar **iter;

          for (iter = interfaces; *iter != NULL; iter++)
            {
              tp_proxy_add_interface_by_id ((TpProxy *) self,
                  g_quark_from_string (*iter));
            }
        }

      DEBUG ("%p: emitting channel-ready", self);
      g_signal_emit (self, signals[SIGNAL_CHANNEL_READY], 0,
          g_quark_to_string (self->channel_type), self->handle_type,
          self->handle, interfaces);

      g_strfreev (interfaces);
    }
  else
    {
      DEBUG ("%p: GetInterfaces() failed", self);
      g_error_free (error);
      tp_channel_destroy_idle_func (self);
    }
}

static void
tp_channel_got_channel_type_cb (DBusGProxy *proxy,
                               DBusGProxyCall *call,
                               gpointer user_data)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (call != NULL)
    {
      GError *error = NULL;
      gchar *channel_type;

      if (dbus_g_proxy_end_call (proxy, call, &error,
            G_TYPE_STRING, &channel_type,
            G_TYPE_INVALID))
        {
          DEBUG ("%p: Introspected channel type %s", self, channel_type);
          self->channel_type = g_quark_from_string (channel_type);
          g_free (channel_type);
        }
      else
        {
          DEBUG ("%p: GetChannelType() failed, will self-destruct", self);
          g_error_free (error);
          g_idle_add (tp_channel_destroy_idle_func, self);
          return;
        }
    }

  g_assert (self->channel_type != 0);
  tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

  dbus_g_proxy_begin_call (proxy, "GetInterfaces",
      tp_channel_got_interfaces_cb, NULL, NULL, G_TYPE_INVALID);
}

static void
tp_channel_got_handle_cb (DBusGProxy *proxy,
                          DBusGProxyCall *call,
                          gpointer user_data)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (call != NULL)
    {
      guint handle_type, handle;
      GError *error = NULL;

      if (dbus_g_proxy_end_call (proxy, call, &error,
            G_TYPE_UINT, &handle_type,
            G_TYPE_UINT, &handle,
            G_TYPE_INVALID))
        {
          DEBUG ("%p: Introspected handle #%d of type %d", self, handle,
              handle_type);
          self->handle_type = handle_type;
          self->handle = handle;
        }
      else
        {
          DEBUG ("%p: GetHandle() failed, will self-destruct", self);
          g_error_free (error);
          g_idle_add (tp_channel_destroy_idle_func, self);
          return;
        }
    }

  if (self->channel_type == 0)
    {
      dbus_g_proxy_begin_call (proxy, "GetChannelType",
          tp_channel_got_channel_type_cb, NULL, NULL, G_TYPE_INVALID);
    }
  else
    {
      tp_channel_got_channel_type_cb (proxy, NULL, NULL);
    }
}

static GObject *
tp_channel_constructor (GType type,
                        guint n_params,
                        GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_channel_parent_class;
  TpChannel *self = TP_CHANNEL (object_class->constructor (type,
        n_params, params));
  DBusGProxy *proxy = (DBusGProxy *) self;

  /* connect to my own Closed signal and self-destruct when it arrives */
  dbus_g_proxy_add_signal (proxy, "Closed", G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "Closed",
      G_CALLBACK (tp_channel_closed_cb), NULL, NULL);

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self, (self->channel_type != 0) ? g_quark_to_string (self->channel_type)
                                      : "(null)",
      self->handle, self->handle_type);

  if (self->handle_type == TP_UNKNOWN_HANDLE_TYPE
      || (self->handle == 0 && self->handle_type != TP_HANDLE_TYPE_NONE))
    {
      dbus_g_proxy_begin_call (proxy, "GetHandle",
          tp_channel_got_handle_cb, NULL, NULL, G_TYPE_INVALID);
    }
  else
    {
      tp_channel_got_handle_cb (proxy, NULL, NULL);
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

  proxy_class->fixed_interface = TP_IFACE_QUARK_CHANNEL;
  proxy_class->must_have_unique_name = TRUE;

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
