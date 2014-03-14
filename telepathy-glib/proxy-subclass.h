/*
 * proxy-subclass.h - Base class for Telepathy client proxies
 *  (API for subclasses only)
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#define _TP_GLIB_H_INSIDE

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef void (*TpProxyWrapperFunc) (TpProxy *self,
    const GError *error, GVariant *args,
    GCallback callback, gpointer user_data, GObject *weak_object);

TpProxyPendingCall *tp_proxy_pending_call_v1_new (TpProxy *self,
    gint timeout_ms, GQuark iface, const gchar *member,
    GVariant *args, const GVariantType *reply_type, TpProxyWrapperFunc wrapper,
    GCallback callback, gpointer user_data, GDestroyNotify destroy,
    GObject *weak_object);

TpProxySignalConnection *tp_proxy_signal_connection_v1_new (TpProxy *self,
    GQuark iface, const gchar *member, const GVariantType *expected_types,
    TpProxyWrapperFunc wrapper,
    GCallback callback, gpointer user_data, GDestroyNotify destroy,
    GObject *weak_object, GError **error);

GDBusProxy *tp_proxy_get_interface_by_id (TpProxy *self, GQuark iface,
    GError **error);

void tp_proxy_add_interface_by_id (TpProxy *self, GQuark iface);
void tp_proxy_add_interfaces (TpProxy *self, const gchar * const *interfaces);

void tp_proxy_invalidate (TpProxy *self, const GError *error);

G_END_DECLS

#undef _TP_GLIB_H_INSIDE

#endif /* #ifndef __TP_PROXY_SUBCLASS_H__*/
