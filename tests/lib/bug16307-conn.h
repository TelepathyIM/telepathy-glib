/*
 * bug16307-conn.h - header for a connection that reproduces the #15307 bug
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __BUG16307_CONN_H__
#define __BUG16307_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>

#include "simple-conn.h"

G_BEGIN_DECLS

typedef struct _Bug16307Connection Bug16307Connection;
typedef struct _Bug16307ConnectionClass Bug16307ConnectionClass;
typedef struct _Bug16307ConnectionPrivate Bug16307ConnectionPrivate;

struct _Bug16307ConnectionClass {
    SimpleConnectionClass parent_class;
};

struct _Bug16307Connection {
    SimpleConnection parent;

    Bug16307ConnectionPrivate *priv;
};

GType bug16307_connection_get_type (void);

/* TYPE MACROS */
#define BUG16307_TYPE_CONNECTION \
  (bug16307_connection_get_type ())
#define BUG16307_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), BUG16307_TYPE_CONNECTION, \
                              Bug16307Connection))
#define BUG16307_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), BUG16307_TYPE_CONNECTION, \
                           Bug16307ConnectionClass))
#define BUG16307_IS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), BUG16307_TYPE_CONNECTION))
#define BUG16307_IS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), BUG16307_TYPE_CONNECTION))
#define BUG16307_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), BUG16307_TYPE_CONNECTION, \
                              Bug16307ConnectionClass))

/* Cause "network events", for debugging/testing */

void bug16307_connection_inject_connect_succeed (Bug16307Connection *self);

void bug16307_connection_inject_get_status_return (Bug16307Connection *self);

G_END_DECLS

#endif /* #ifndef __BUG16307_CONN_H__ */
