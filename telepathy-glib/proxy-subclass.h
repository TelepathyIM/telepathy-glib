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

typedef void (*TpProxyInvokeFunc) (TpProxy *self,
    GError *error, GValueArray *args, GCallback callback, gpointer user_data,
    GObject *weak_object);

TpProxyPendingCall *tp_proxy_pending_call_v0_new (TpProxy *self,
    GQuark interface, const gchar *member,
    TpProxyInvokeFunc invoke_callback,
    GCallback callback, gpointer user_data, GDestroyNotify destroy,
    GObject *weak_object);

void tp_proxy_pending_call_v0_take_pending_call (TpProxyPendingCall *self,
    DBusGProxyCall *pending_call);

void tp_proxy_pending_call_v0_take_results (TpProxyPendingCall *self,
    GError *error, GValueArray *args);

void tp_proxy_pending_call_v0_completed (gpointer p);

TpProxySignalConnection *tp_proxy_signal_connection_v0_new (TpProxy *self,
    GQuark interface, const gchar *member,
    const GType *expected_types,
    GCallback collect_args, TpProxyInvokeFunc invoke_callback,
    GCallback callback, gpointer user_data, GDestroyNotify destroy,
    GObject *weak_object);

void tp_proxy_signal_connection_v0_take_results
    (TpProxySignalConnection *self, GValueArray *args);

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
