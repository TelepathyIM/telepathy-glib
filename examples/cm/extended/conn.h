/*
 * conn.h - header for an example connection
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_EXTENDED_CONN_H__
#define __EXAMPLE_EXTENDED_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>

G_BEGIN_DECLS

typedef struct _ExampleExtendedConnection ExampleExtendedConnection;
typedef struct _ExampleExtendedConnectionClass ExampleExtendedConnectionClass;
typedef struct _ExampleExtendedConnectionPrivate ExampleExtendedConnectionPrivate;

struct _ExampleExtendedConnectionClass {
    TpBaseConnectionClass parent_class;
};

struct _ExampleExtendedConnection {
    TpBaseConnection parent;

    ExampleExtendedConnectionPrivate *priv;
};

GType example_extended_connection_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_EXTENDED_CONNECTION \
  (example_extended_connection_get_type ())
#define EXAMPLE_EXTENDED_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_EXTENDED_CONNECTION, \
                              ExampleExtendedConnection))
#define EXAMPLE_EXTENDED_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_EXTENDED_CONNECTION, \
                           ExampleExtendedConnectionClass))
#define EXAMPLE_IS_EXTENDED_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_EXTENDED_CONNECTION))
#define EXAMPLE_IS_EXTENDED_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_EXTENDED_CONNECTION))
#define EXAMPLE_EXTENDED_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_EXTENDED_CONNECTION, \
                              ExampleExtendedConnectionClass))

G_END_DECLS

#endif
