/*
 * call-stream-endpoint.h - Header for TpCallStreamEndpoint
 * Copyright (C) 2009-2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __TP_CALL_STREAM_ENDPOINT_H__
#define __TP_CALL_STREAM_ENDPOINT_H__

#include <glib-object.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>

#include <telepathy-glib/enums.h>

G_BEGIN_DECLS

typedef struct _TpCallStreamEndpoint TpCallStreamEndpoint;
typedef struct _TpCallStreamEndpointPrivate TpCallStreamEndpointPrivate;
typedef struct _TpCallStreamEndpointClass TpCallStreamEndpointClass;

struct _TpCallStreamEndpointClass {
  /*<private>*/
  GObjectClass parent_class;

  TpDBusPropertiesMixinClass dbus_props_class;
};

struct _TpCallStreamEndpoint {
  /*<private>*/
  GObject parent;

  TpCallStreamEndpointPrivate *priv;
};

GType tp_call_stream_endpoint_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_CALL_STREAM_ENDPOINT \
  (tp_call_stream_endpoint_get_type ())
#define TP_CALL_STREAM_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  TP_TYPE_CALL_STREAM_ENDPOINT, TpCallStreamEndpoint))
#define TP_CALL_STREAM_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  TP_TYPE_CALL_STREAM_ENDPOINT, TpCallStreamEndpointClass))
#define TP_IS_CALL_STREAM_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CALL_STREAM_ENDPOINT))
#define TP_IS_CALL_STREAM_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CALL_STREAM_ENDPOINT))
#define TP_CALL_STREAM_ENDPOINT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    TP_TYPE_CALL_STREAM_ENDPOINT, TpCallStreamEndpointClass))

TpCallStreamEndpoint *tp_call_stream_endpoint_new (TpDBusDaemon *dbus_daemon,
    const gchar *object_path,
    TpStreamTransportType transport,
    gboolean is_ice_lite);

const gchar *tp_call_stream_endpoint_get_object_path (
    TpCallStreamEndpoint *self);

TpStreamEndpointState tp_call_stream_endpoint_get_state (
    TpCallStreamEndpoint *self,
    TpStreamComponent component);

void tp_call_stream_endpoint_add_new_candidates (TpCallStreamEndpoint *self,
    const GPtrArray *candidates);
void tp_call_stream_endpoint_add_new_candidate (TpCallStreamEndpoint *self,
    TpStreamComponent component,
    const gchar *address,
    guint port,
    const GHashTable *info_hash);

G_END_DECLS

#endif /* #ifndef __TP_CALL_STREAM_ENDPOINT_H__*/
