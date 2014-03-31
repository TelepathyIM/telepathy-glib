/*
 * client.c - proxy for a Telepathy client
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

#include "config.h"

#include "telepathy-glib/client.h"

#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_DISPATCHER
#include "telepathy-glib/debug-internal.h"

/**
 * SECTION:client
 * @title: TpClient
 * @short_description: proxy object for a client of the ChannelDispatcher
 *
 * Each client to which the ChannelDispatcher can send channels must implement
 * the Client interface. This object represents such a client, and is mainly
 * useful in the implementation of the ChannelDispatcher itself.
 *
 * Since: 0.7.32
 */

/**
 * TpClient:
 *
 * Each client to which the ChannelDispatcher can send channels must implement
 * the Client interface. This object represents such a client, and is mainly
 * useful in the implementation of the ChannelDispatcher itself.
 *
 * This proxy is usable but very incomplete: accessors for D-Bus properties
 * will be added in a later version of telepathy-glib.
 *
 * Many operations performed on a Client are done via D-Bus properties.
 * Until convenience methods for this are implemented, use of the generic
 * tp_cli_dbus_properties_call_get_all() and tp_cli_dbus_properties_call_set()
 * methods is recommended.
 *
 * Since: 0.7.32
 */

/**
 * TpClientClass:
 *
 * The class of a #TpClient.
 */

struct _TpClientPrivate {
    gpointer dummy;
};

G_DEFINE_TYPE (TpClient, tp_client, TP_TYPE_PROXY)

static void
tp_client_init (TpClient *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CLIENT,
      TpClientPrivate);
}

static void
tp_client_class_init (TpClientClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  g_type_class_add_private (klass, sizeof (TpClientPrivate));

  proxy_class->interface = TP_IFACE_QUARK_CLIENT;
}
