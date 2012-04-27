/*
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

#ifndef __TP_PROXY_INTERNAL_H__
#define __TP_PROXY_INTERNAL_H__

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/proxy-subclass.h>

typedef struct {
    const gchar *version;
    gsize size;

    DBusGProxy *(*borrow_interface_by_id) (TpProxy *,
        GQuark,
        GError **);

    TpProxyPendingCall *(*pending_call_new) (TpProxy *,
        GQuark,
        const gchar *,
        DBusGProxy *,
        TpProxyInvokeFunc,
        GCallback,
        gpointer,
        GDestroyNotify,
        GObject *,
        gboolean);
    void (*pending_call_take_pending_call) (TpProxyPendingCall *,
        DBusGProxyCall *);
    void (*pending_call_take_results) (TpProxyPendingCall *,
        GError *,
        GValueArray *);
    GDestroyNotify pending_call_completed;

    TpProxySignalConnection *(*signal_connection_new) (TpProxy *,
        GQuark,
        const gchar *,
        const GType *,
        GCallback,
        TpProxyInvokeFunc,
        GCallback,
        gpointer,
        GDestroyNotify,
        GObject *,
        GError **);
    void (*signal_connection_take_results) (TpProxySignalConnection *,
        GValueArray *);

    GType type;
} TpProxyImplementation;

DBusGProxy *_tp_proxy_borrow_interface_by_id (TpProxy *self,
    GQuark iface,
    GError **error);

TpProxyPendingCall *_tp_proxy_pending_call_new (TpProxy *self,
    GQuark iface,
    const gchar *member,
    DBusGProxy *iface_proxy,
    TpProxyInvokeFunc invoke_callback,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object,
    gboolean cancel_must_raise);

void _tp_proxy_pending_call_take_pending_call (TpProxyPendingCall *pc,
    DBusGProxyCall *pending_call);

void _tp_proxy_pending_call_take_results (TpProxyPendingCall *pc,
    GError *error,
    GValueArray *args);

void _tp_proxy_pending_call_completed (gpointer p);

TpProxySignalConnection *_tp_proxy_signal_connection_new (TpProxy *self,
    GQuark iface,
    const gchar *member,
    const GType *expected_types,
    GCallback collect_args,
    TpProxyInvokeFunc invoke_callback,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object,
    GError **error);

void _tp_proxy_signal_connection_take_results (TpProxySignalConnection *sc,
    GValueArray *args);

/*
 * Implemented in the -core library, and called by the -main library.
 *
 * This is only extern so that the -main part can call into the
 * -core part across a shared-library boundary. If you are not
 * TpProxy early initialization, don't.
 */
void tp_private_proxy_set_implementation (TpProxyImplementation *impl);

GError *_tp_proxy_take_and_remap_error (TpProxy *self, GError *error)
  G_GNUC_WARN_UNUSED_RESULT;

typedef void (*TpProxyProc) (TpProxy *);

gboolean _tp_proxy_is_preparing (gpointer self,
    GQuark feature);
void _tp_proxy_set_feature_prepared (TpProxy *self,
    GQuark feature,
    gboolean succeeded);
void _tp_proxy_set_features_failed (TpProxy *self,
    const GError *error);

void _tp_proxy_will_announce_connected_async (TpProxy *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean _tp_proxy_will_announce_connected_finish (TpProxy *self,
    GAsyncResult *result,
    GError **error);

void _tp_proxy_ensure_factory (gpointer self,
    TpClientFactory *factory);

#endif
