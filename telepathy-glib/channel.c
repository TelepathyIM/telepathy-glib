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

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/interfaces.h>

#include "_gen/signals-marshal.h"

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "debug-internal.h"

#include "_gen/tp-cli-channel-body.h"

/**
 * SECTION:channel
 * @title: TpChannel
 * @short_description: proxy object for a Telepathy channel
 * @see_also: #TpConnection
 *
 * #TpChannel objects provide convenient access to Telepathy channels.
 *
 * Since: 0.7.1
 */

/**
 * TpChannelClass:
 * @parent_class: parent class
 * @priv: pointer to opaque private data
 *
 * The class of a #TpChannel.
 */
struct _TpChannelClass {
    TpProxyClass parent_class;
    gpointer priv;
};

/**
 * TpChannel:
 * @parent: parent class instance
 * @channel_type: quark representing the channel type; should be considered
 *  read-only
 * @handle_type: the handle type (%TP_UNKNOWN_HANDLE_TYPE if not yet known);
 *  should be considered read-only
 * @handle: the handle with which this channel communicates (0 if
 *  not yet known or if @handle_type is %TP_HANDLE_TYPE_NONE); should be
 *  considered read-only
 * @priv: pointer to opaque private data
 *
 * A proxy object for a Telepathy channel.
 */
struct _TpChannel {
    TpProxy parent;

    GQuark channel_type;
    TpHandleType handle_type;
    TpHandle handle;

    gpointer priv;
};

enum
{
  PROP_CHANNEL_TYPE = 1,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (TpChannel,
    tp_channel,
    TP_TYPE_PROXY,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL));

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

              if (tp_dbus_check_valid_interface_name (*iter, NULL))
                {
                  tp_proxy_add_interface_by_id ((TpProxy *) self,
                      g_quark_from_string (*iter));
                }
              else
                {
                  DEBUG ("\t\tInterface %s not valid", *iter);
                }
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
      GError *err2 = NULL;

      if (tp_dbus_check_valid_interface_name (channel_type, &err2))
        {
          self->channel_type = g_quark_from_string (channel_type);
        }
      else
        {
          DEBUG ("\t\tChannel type not valid: %s", err2->message);
          tp_proxy_invalidated ((TpProxy *) self, err2);
          return;
        }
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
      tp_channel_got_interfaces_cb, NULL, NULL, NULL);
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
          tp_channel_got_channel_type_cb, NULL, NULL, NULL);
    }
  else
    {
      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

      tp_cli_channel_call_get_interfaces (self, -1,
          tp_channel_got_interfaces_cb, NULL, NULL, NULL);
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
  tp_cli_channel_connect_to_closed (self, tp_channel_closed_cb, NULL, NULL,
      NULL);

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self, (self->channel_type != 0) ? g_quark_to_string (self->channel_type)
                                      : "(null)",
      self->handle, self->handle_type);

  if (self->handle_type == TP_UNKNOWN_HANDLE_TYPE
      || (self->handle == 0 && self->handle_type != TP_HANDLE_TYPE_NONE))
    {
      tp_cli_channel_call_get_handle (self, -1,
          tp_channel_got_handle_cb, NULL, NULL, NULL);
    }
  else if (self->channel_type == 0)
    {
      tp_cli_channel_call_get_channel_type (self, -1,
          tp_channel_got_channel_type_cb, NULL, NULL, NULL);
    }
  else
    {
      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

      tp_cli_channel_call_get_interfaces (self, -1,
          tp_channel_got_interfaces_cb, NULL, NULL, NULL);
    }

  return (GObject *) self;
}

static void
tp_channel_init (TpChannel *self)
{
  DEBUG ("%p", self);

  self->channel_type = 0;
  self->handle_type = TP_UNKNOWN_HANDLE_TYPE;
  self->handle = 0;
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
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");

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
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");

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
  g_object_class_override_property (object_class, PROP_HANDLE,
      "handle");

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

/**
 * tp_channel_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @bus_name: the bus name of the connection process; may not be %NULL.
 *  If this is a well-known name, this function will make a blocking call
 *  to the bus daemon to resolve the unique name.
 * @object_path: the object path of the channel; may not be %NULL
 * @optional_channel_type: the channel type if already known, or %NULL if not
 * @optional_handle_type: the handle type if already known, or
 *  %TP_UNKNOWN_HANDLE_TYPE if not
 * @optional_handle: the handle if already known, or 0 if not
 *  (if @optional_handle_type is %TP_UNKNOWN_HANDLE_TYPE or
 *  %TP_HANDLE_TYPE_NONE, this must be 0)
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new channel proxy, or %NULL on invalid arguments.
 */
TpChannel *
tp_channel_new (TpDBusDaemon *dbus,
                const gchar *bus_name,
                const gchar *object_path,
                const gchar *optional_channel_type,
                TpHandleType optional_handle_type,
                TpHandle optional_handle,
                GError **error)
{
  TpChannel *ret = NULL;
  gchar *dup = NULL;

  g_return_val_if_fail (dbus != NULL, NULL);
  g_return_val_if_fail (bus_name != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  if (!tp_dbus_check_valid_bus_name (bus_name,
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, error))
    goto finally;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  if (optional_channel_type != NULL &&
      !tp_dbus_check_valid_interface_name (optional_channel_type, error))
    goto finally;

  if (optional_handle_type == TP_UNKNOWN_HANDLE_TYPE ||
      optional_handle_type == TP_HANDLE_TYPE_NONE)
    {
      if (optional_handle != 0)
        {
          /* in the properties, we do actually allow the user to give us an
           * assumed-valid handle of unknown type - but that'd be silly */
          g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "Nonzero handle of type NONE or unknown makes no sense");
          goto finally;
        }
    }
  else if (!tp_handle_type_is_valid (optional_handle_type, error))
    {
      goto finally;
    }

  /* Resolve unique name if necessary */
  if (bus_name[0] != ':')
    {
      if (!tp_cli_dbus_daemon_block_on_get_name_owner (dbus, 2000, bus_name,
          &dup, error))
        goto finally;

      bus_name = dup;

      if (!tp_dbus_check_valid_bus_name (bus_name,
          TP_DBUS_NAME_TYPE_UNIQUE, error))
        goto finally;
    }

  ret = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "dbus-daemon", dbus,
        "bus-name", bus_name,
        "object-path", object_path,
        "channel-type", optional_channel_type,
        "handle-type", optional_handle_type,
        "handle", optional_handle,
        NULL));

finally:
  g_free (dup);

  return ret;
}
