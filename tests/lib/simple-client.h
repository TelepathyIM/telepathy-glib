/*
 * simple-client.h - header for a simple client
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __SIMPLE_CLIENT_H__
#define __SIMPLE_CLIENT_H__

#include <glib-object.h>
#include <telepathy-glib/base-client.h>

G_BEGIN_DECLS

typedef struct _SimpleClient SimpleClient;
typedef struct _SimpleClientClass SimpleClientClass;

struct _SimpleClientClass {
    TpBaseClientClass parent_class;
};

struct _SimpleClient {
    TpBaseClient parent;

    TpObserveChannelsContext *observe_ctx;
    TpAddDispatchOperationContext *add_dispatch_ctx;
};

GType simple_client_get_type (void);

/* TYPE MACROS */
#define SIMPLE_TYPE_CLIENT \
  (simple_client_get_type ())
#define SIMPLE_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SIMPLE_TYPE_CLIENT, \
                              SimpleClient))
#define SIMPLE_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), SIMPLE_TYPE_CLIENT, \
                           SimpleClientClass))
#define SIMPLE_IS_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SIMPLE_TYPE_CLIENT))
#define SIMPLE_IS_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SIMPLE_TYPE_CLIENT))
#define SIMPLE_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SIMPLE_TYPE_CLIENT, \
                              SimpleClientClass))

SimpleClient * simple_client_new (TpDBusDaemon *dbus_daemon,
    const gchar *name,
    gboolean uniquify_name);

G_END_DECLS

#endif /* #ifndef __SIMPLE_CONN_H__ */
