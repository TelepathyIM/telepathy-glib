/*
 * conn.h - header for an example connection
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_CSH_CONN_H__
#define __EXAMPLE_CSH_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef struct _ExampleCSHConnection ExampleCSHConnection;
typedef struct _ExampleCSHConnectionClass ExampleCSHConnectionClass;
typedef struct _ExampleCSHConnectionPrivate ExampleCSHConnectionPrivate;

struct _ExampleCSHConnectionClass {
    TpBaseConnectionClass parent_class;
    TpContactsMixinClass contacts_mixin;
};

struct _ExampleCSHConnection {
    TpBaseConnection parent;
    TpContactsMixin contacts_mixin;

    ExampleCSHConnectionPrivate *priv;
};

GType example_csh_connection_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_CSH_CONNECTION \
  (example_csh_connection_get_type ())
#define EXAMPLE_CSH_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_CSH_CONNECTION, \
                              ExampleCSHConnection))
#define EXAMPLE_CSH_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_CSH_CONNECTION, \
                           ExampleCSHConnectionClass))
#define EXAMPLE_IS_CSH_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_CSH_CONNECTION))
#define EXAMPLE_IS_CSH_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_CSH_CONNECTION))
#define EXAMPLE_CSH_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_CSH_CONNECTION, \
                              ExampleCSHConnectionClass))

const gchar * const * example_csh_connection_get_possible_interfaces (void);

G_END_DECLS

#endif
