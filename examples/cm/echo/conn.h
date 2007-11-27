/*
 * conn.h - header for an example connection
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_CONN_H__
#define __EXAMPLE_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>

G_BEGIN_DECLS

typedef struct _ExampleConnection ExampleConnection;
typedef struct _ExampleConnectionClass ExampleConnectionClass;
typedef struct _ExampleConnectionPrivate ExampleConnectionPrivate;

struct _ExampleConnectionClass {
    TpBaseConnectionClass parent_class;
};

struct _ExampleConnection {
    TpBaseConnection parent;

    ExampleConnectionPrivate *priv;
};

GType example_connection_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_CONNECTION \
  (example_connection_get_type ())
#define EXAMPLE_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_CONNECTION, \
                              ExampleConnection))
#define EXAMPLE_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_CONNECTION, \
                           ExampleConnectionClass))
#define EXAMPLE_IS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_CONNECTION))
#define EXAMPLE_IS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_CONNECTION))
#define EXAMPLE_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_CONNECTION, \
                              ExampleConnectionClass))

G_END_DECLS

#endif /* #ifndef __EXAMPLE_CONN_H__ */
