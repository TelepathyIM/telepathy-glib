/*
 * simple-channel-dispatch-operation.h - a simple channel dispatch operation
 * service.
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __SIMPLE_CHANNEL_DISPATCH_OPERATION_H__
#define __SIMPLE_CHANNEL_DISPATCH_OPERATION_H__

#include <glib-object.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

typedef struct _SimpleChannelDispatchOperation SimpleChannelDispatchOperation;
typedef struct _SimpleChannelDispatchOperationClass SimpleChannelDispatchOperationClass;
typedef struct _SimpleChannelDispatchOperationPrivate SimpleChannelDispatchOperationPrivate;

struct _SimpleChannelDispatchOperationClass {
    GObjectClass parent_class;
    TpDBusPropertiesMixinClass dbus_props_class;
};

struct _SimpleChannelDispatchOperation {
    GObject parent;

    SimpleChannelDispatchOperationPrivate *priv;
};

GType simple_channel_dispatch_operation_get_type (void);

/* TYPE MACROS */
#define SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION \
  (simple_channel_dispatch_operation_get_type ())
#define SIMPLE_CHANNEL_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION, \
                              SimpleChannelDispatchOperation))
#define SIMPLE_CHANNEL_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION, \
                           SimpleChannelDispatchOperationClass))
#define SIMPLE_IS_CHANNEL_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION))
#define SIMPLE_IS_CHANNEL_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION))
#define SIMPLE_CHANNEL_DISPATCH_OPERATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION, \
                              SimpleChannelDispatchOperationClass))

void simple_channel_dispatch_operation_set_conn_path (
    SimpleChannelDispatchOperation *self,
    const gchar *conn_path);

void simple_channel_dispatch_operation_add_channel (
    SimpleChannelDispatchOperation *self,
    TpChannel *chan);

void simple_channel_dispatch_operation_lost_channel (
    SimpleChannelDispatchOperation *self,
    TpChannel *chan);

void simple_channel_dispatch_operation_set_account_path (
    SimpleChannelDispatchOperation *self,
    const gchar *account_path);

G_END_DECLS

#endif /* #ifndef __SIMPLE_CHANNEL_DISPATCH_OPERATION_H__ */
