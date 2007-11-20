/*
 * proxy-internal.h - Protected definitions for Telepathy client proxies
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

#ifndef __TP_PROXY_INTERNAL_H__
#define __TP_PROXY_INTERNAL_H__

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

struct _TpProxyClass {
    GObjectClass parent_class;

    /*<protected>*/
    GQuark interface;
    GSList *on_interface_added;
    gboolean must_have_unique_name:1;
};

struct _TpProxy {
    GObject parent;

    /*<private>*/
    TpProxy *dbus_daemon;
    DBusGConnection *dbus_connection;
    gchar *bus_name;
    gchar *object_path;
    /* GQuark for interface => ref'd DBusGProxy * */
    GData *interfaces;

    gboolean valid:1;
    gboolean dispose_has_run:1;
};

TpProxyPendingCall *tp_proxy_pending_call_new (TpProxy *self,
    GCallback callback, gpointer user_data, GDestroyNotify destroy);

void tp_proxy_pending_call_free (gpointer self);

TpProxySignalConnection *tp_proxy_signal_connection_new (TpProxy *self,
    GQuark interface, const gchar *member, GCallback callback,
    gpointer user_data, GDestroyNotify destroy);

void tp_proxy_signal_connection_free_closure (gpointer self, GClosure *unused);

DBusGProxy *tp_proxy_borrow_interface_by_id (TpProxy *self, GQuark interface,
    GError **error);

DBusGProxy *tp_proxy_add_interface_by_id (TpProxy *self, GQuark interface);

void tp_proxy_invalidated (TpProxy *self, const GError *error);

G_END_DECLS

#endif
