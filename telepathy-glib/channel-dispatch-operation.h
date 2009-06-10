/*
 * channel-dispatch-operation.h - proxy for channels awaiting approval
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#ifndef TP_CHANNEL_DISPATCH_OPERATION_H
#define TP_CHANNEL_DISPATCH_OPERATION_H

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/dbus.h>

G_BEGIN_DECLS

typedef struct _TpChannelDispatchOperation
    TpChannelDispatchOperation;
typedef struct _TpChannelDispatchOperationClass
    TpChannelDispatchOperationClass;
typedef struct _TpChannelDispatchOperationPrivate
    TpChannelDispatchOperationPrivate;
typedef struct _TpChannelDispatchOperationClassPrivate
    TpChannelDispatchOperationClassPrivate;

struct _TpChannelDispatchOperation {
    /*<private>*/
    TpProxy parent;
    TpChannelDispatchOperationPrivate *priv;
};

struct _TpChannelDispatchOperationClass {
    /*<private>*/
    TpProxyClass parent_class;
    GCallback _padding[7];
    TpChannelDispatchOperationClassPrivate *priv;
};

GType tp_channel_dispatch_operation_get_type (void);

#define TP_TYPE_CHANNEL_DISPATCH_OPERATION \
  (tp_channel_dispatch_operation_get_type ())
#define TP_CHANNEL_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CHANNEL_DISPATCH_OPERATION, \
                               TpChannelDispatchOperation))
#define TP_CHANNEL_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CHANNEL_DISPATCH_OPERATION, \
                            TpChannelDispatchOperationClass))
#define TP_IS_CHANNEL_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CHANNEL_DISPATCH_OPERATION))
#define TP_IS_CHANNEL_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CHANNEL_DISPATCH_OPERATION))
#define TP_CHANNEL_DISPATCH_OPERATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CHANNEL_DISPATCH_OPERATION, \
                              TpChannelDispatchOperationClass))

TpChannelDispatchOperation *tp_channel_dispatch_operation_new (
    TpDBusDaemon *bus_daemon, const gchar *object_path,
    GHashTable *immutable_properties, GError **error);

void tp_channel_dispatch_operation_init_known_interfaces (void);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-channel-dispatch-operation.h>

#endif
