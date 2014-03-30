/*<private_header>*/
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

    gboolean (*check_interface_by_id) (TpProxy *,
        GQuark,
        GError **);

    TpProxyPendingCall *(*pending_call_new) (TpProxy *,
        gint,
        GQuark,
        const gchar *,
        GVariant *,
        const GVariantType *,
        TpProxyWrapperFunc,
        GCallback,
        gpointer,
        GDestroyNotify,
        GObject *);

    TpProxySignalConnection *(*signal_connection_new) (TpProxy *,
        GQuark,
        const gchar *,
        const GVariantType *,
        TpProxyWrapperFunc,
        GCallback,
        gpointer,
        GDestroyNotify,
        GObject *,
        GError **);

    GType type;
} TpProxyImplementation;

gboolean _tp_proxy_check_interface_by_id (TpProxy *self,
    GQuark iface,
    GError **error);

TpProxyPendingCall *
_tp_proxy_pending_call_v1_new (TpProxy *proxy,
    gint timeout_ms,
    GQuark iface,
    const gchar *member,
    GVariant *args,
    const GVariantType *reply_type,
    TpProxyWrapperFunc wrapper,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object);

TpProxySignalConnection *
_tp_proxy_signal_connection_v1_new (TpProxy *self,
    GQuark iface,
    const gchar *member,
    const GVariantType *expected_types,
    TpProxyWrapperFunc wrapper,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object,
    GError **error);

/*
 * Implemented in the -core library, and called by the -main library.
 *
 * This is only extern so that the -main part can call into the
 * -core part across a shared-library boundary. If you are not
 * TpProxy early initialization, don't.
 */
void tp_private_proxy_set_implementation (TpProxyImplementation *impl);

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

void _tp_proxy_add_signal_connection (TpProxy *self,
    TpProxySignalConnection *sc);
void _tp_proxy_remove_signal_connection (TpProxy *self,
    TpProxySignalConnection *sc);

#endif
