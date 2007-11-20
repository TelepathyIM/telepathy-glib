/*
 * connection-manager.c - proxy for a Telepathy connection manager
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

#include "telepathy-glib/connection-manager.h"

#include "telepathy-glib/defs.h"
#include "telepathy-glib/proxy-internal.h"

/**
 * SECTION:connection-manager
 * @title: TpConnectionManager
 * @short_description: proxy object for a Telepathy connection manager
 * @see_also: #TpConnection
 *
 * #TpConnectionManager objects represent Telepathy connection managers. They
 * can be used to open connections.
 */

/**
 * TpConnectionManagerClass:
 *
 * The class of a #TpConnectionManager.
 */
struct _TpConnectionManagerClass {
    TpProxyClass parent_class;
    /*<private>*/
};

/**
 * TpConnectionManager:
 *
 * A proxy object for a Telepathy connection manager.
 */
struct _TpConnectionManager {
    TpProxy parent;
    /*<private>*/
};

G_DEFINE_TYPE (TpConnectionManager,
    tp_connection_manager,
    TP_TYPE_PROXY);

static void
tp_connection_manager_init (TpConnectionManager *self)
{
}

static void
tp_connection_manager_class_init (TpConnectionManagerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  proxy_class->interface = TP_IFACE_QUARK_CONNECTION_MANAGER;
  proxy_class->on_interface_added = g_slist_prepend
      (proxy_class->on_interface_added, tp_cli_connection_manager_add_signals);
}

/**
 * tp_connection_manager_new:
 * @connection: A connection to the D-Bus session bus
 * @name: The connection manager name
 *
 * Convenience function to create a new connection manager proxy.
 *
 * Returns: a new reference to a connection manager proxy
 */
TpConnectionManager *
tp_connection_manager_new (DBusGConnection *connection,
                           const gchar *name)
{
  TpConnectionManager *cm;
  gchar *object_path = g_strdup_printf ("%s%s", TP_CM_OBJECT_PATH_BASE,
      name);
  gchar *bus_name = g_strdup_printf ("%s%s", TP_CM_BUS_NAME_BASE,
      name);

  cm = TP_CONNECTION_MANAGER (g_object_new (TP_TYPE_CONNECTION_MANAGER,
        "dbus-connection", connection,
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

  g_free (object_path);
  g_free (bus_name);
  return cm;
}
