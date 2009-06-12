/*
 * channel-dispatcher.c - proxy for the Telepathy channel dispatcher
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#include "telepathy-glib/channel-dispatcher.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_DISPATCHER
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-channel-dispatcher-body.h"

/**
 * SECTION:channel-dispatcher
 * @title: TpChannelDispatcher
 * @short_description: proxy object for the Telepathy channel dispatcher
 * @see_also: #TpChannelDispatchOperation, #TpChannelRequest
 *
 * The #TpChannelDispatcher object can be used to communicate with any
 * implementation of the Telepathy ChannelDispatcher service to request
 * new channels.
 */

/**
 * TpChannelDispatcher:
 *
 * The Channel Dispatcher's main D-Bus API is used to request channels,
 * which is done by calling tp_cli_channel_dispatcher_call_create_channel() or
 * tp_cli_channel_dispatcher_call_ensure_channel() as appropriate.
 *
 * The Telepathy Channel Dispatcher is also responsible for responding to new
 * channels and launching client processes to handle them. However, clients
 * that can work with incoming channels do not have to call methods
 * on the channel dispatcher: instead, they must register with the channel
 * dispatcher passively, by taking a bus name starting with
 * %TP_CLIENT_BUS_NAME_BASE and implementing the #TpSvcClient interface.
 * See the Telepathy D-Bus Interface Specification for details.
 *
 * This proxy is usable but incomplete: convenience methods will be added in
 * a later version of telepathy-glib, along with a mechanism similar to
 * tp_connection_call_when_ready().
 *
 * Since: 0.7.32
 */

/**
 * TpChannelDispatcherClass:
 *
 * The class of a #TpChannelDispatcher.
 */

struct _TpChannelDispatcherPrivate {
    gpointer dummy;
};

G_DEFINE_TYPE (TpChannelDispatcher, tp_channel_dispatcher, TP_TYPE_PROXY);

static void
tp_channel_dispatcher_init (TpChannelDispatcher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CHANNEL_DISPATCHER,
      TpChannelDispatcherPrivate);
}

static void
tp_channel_dispatcher_constructed (GObject *object)
{
  TpChannelDispatcher *self = TP_CHANNEL_DISPATCHER (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_channel_dispatcher_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);
}

static void
tp_channel_dispatcher_class_init (TpChannelDispatcherClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpChannelDispatcherPrivate));

  object_class->constructed = tp_channel_dispatcher_constructed;

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL_DISPATCHER;
  tp_channel_dispatcher_init_known_interfaces ();
}

/**
 * tp_channel_dispatcher_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpChannelDispatcher have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CHANNEL_DISPATCHER.
 *
 * Since: 0.7.32
 */
void
tp_channel_dispatcher_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_CHANNEL_DISPATCHER;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_channel_dispatcher_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

/**
 * tp_channel_dispatcher_new:
 * @bus_daemon: Proxy for the D-Bus daemon
 *
 * Convenience function to create a new channel dispatcher proxy.
 *
 * Returns: a new reference to a channel dispatcher proxy
 */
TpChannelDispatcher *
tp_channel_dispatcher_new (TpDBusDaemon *bus_daemon)
{
  TpChannelDispatcher *self;

  g_return_val_if_fail (bus_daemon != NULL, NULL);

  self = TP_CHANNEL_DISPATCHER (g_object_new (TP_TYPE_CHANNEL_DISPATCHER,
        "dbus-daemon", bus_daemon,
        "dbus-connection", ((TpProxy *) bus_daemon)->dbus_connection,
        "bus-name", TP_CHANNEL_DISPATCHER_BUS_NAME,
        "object-path", TP_CHANNEL_DISPATCHER_OBJECT_PATH,
        NULL));

  return self;
}
