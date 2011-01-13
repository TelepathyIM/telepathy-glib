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

GError *_tp_proxy_take_and_remap_error (TpProxy *self, GError *error)
  G_GNUC_WARN_UNUSED_RESULT;

typedef void (*TpProxyProc) (TpProxy *);

typedef void (* TpProxyPrepareAsync) (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef struct _TpProxyFeaturePrivate TpProxyFeaturePrivate;

struct _TpProxyFeature {
    /*<public>*/
    GQuark name;
    gboolean core;

    TpProxyProc start_preparing;
    TpProxyPrepareAsync prepare_async;

    const GQuark *interfaces_needed;
    /* Features we depend on */
    const GQuark *depends_on;

    /*<private>*/
    GCallback _reserved[4];
    TpProxyFeaturePrivate *priv;
};

gboolean _tp_proxy_is_preparing (gpointer self,
    GQuark feature);
void _tp_proxy_set_feature_prepared (TpProxy *self,
    GQuark feature,
    gboolean succeeded);
void _tp_proxy_set_features_failed (TpProxy *self,
    const GError *error);

#endif
