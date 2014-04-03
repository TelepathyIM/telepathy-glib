/*
 * Copyright © 2014 Collabora Ltd.
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

#if !defined (_TP_GLIB_DBUS_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib-dbus.h> can be included directly."
#endif

#ifndef __TP_CLI_PROXY_H__
#define __TP_CLI_PROXY_H__

#include <gio/gio.h>

#include <telepathy-glib/telepathy-glib.h>

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

G_END_DECLS

#endif
