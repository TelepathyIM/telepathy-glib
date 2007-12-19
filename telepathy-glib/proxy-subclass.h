/*
 * proxy-subclass.h - Base class for Telepathy client proxies
 *  (API for subclasses only)
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#ifndef __TP_PROXY_SUBCLASS_H__
#define __TP_PROXY_SUBCLASS_H__

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

TpProxyPendingCall *tp_proxy_pending_call_new (TpProxy *self,
    GQuark interface, const gchar *member, GCallback callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object,
    void (*raise_error) (TpProxyPendingCall *));

void tp_proxy_pending_call_free (gpointer self);

GCallback tp_proxy_pending_call_steal_callback (TpProxyPendingCall *self,
    TpProxy **proxy_out, gpointer *user_data_out, GObject **weak_object_out);

void tp_proxy_pending_call_take_pending_call (TpProxyPendingCall *self,
    DBusGProxyCall *pending_call);

TpProxySignalConnection *tp_proxy_signal_connection_new (TpProxy *self,
    GQuark interface, const gchar *member, GCallback callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object,
    GCallback impl_callback);

void tp_proxy_signal_connection_free_closure (gpointer self, GClosure *unused);

GCallback tp_proxy_signal_connection_get_callback
    (TpProxySignalConnection *self, TpProxy **proxy_out,
     gpointer *user_data_out, GObject **weak_object_out);

typedef void (*TpProxyInterfaceAddedCb) (TpProxy *self,
    guint quark, DBusGProxy *proxy, gpointer unused);

void tp_proxy_class_hook_on_interface_add (TpProxyClass *klass,
    TpProxyInterfaceAddedCb callback);

DBusGProxy *tp_proxy_borrow_interface_by_id (TpProxy *self, GQuark interface,
    GError **error);

DBusGProxy *tp_proxy_add_interface_by_id (TpProxy *self, GQuark interface);

void tp_proxy_invalidated (TpProxy *self, const GError *error);

G_END_DECLS

#endif /* #ifndef __TP_PROXY_SUBCLASS_H__*/
