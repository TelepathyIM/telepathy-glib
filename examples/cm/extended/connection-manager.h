/*
 * manager.h - header for an example connection manager
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_EXTENDED_CONNECTION_MANAGER_H__
#define __EXAMPLE_EXTENDED_CONNECTION_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

typedef struct _ExampleExtendedConnectionManager
    ExampleExtendedConnectionManager;
typedef struct _ExampleExtendedConnectionManagerPrivate
    ExampleExtendedConnectionManagerPrivate;
typedef struct _ExampleExtendedConnectionManagerClass
    ExampleExtendedConnectionManagerClass;
typedef struct _ExampleExtendedConnectionManagerClassPrivate
    ExampleExtendedConnectionManagerClassPrivate;

struct _ExampleExtendedConnectionManagerClass {
    TpBaseConnectionManagerClass parent_class;

    ExampleExtendedConnectionManagerClassPrivate *priv;
};

struct _ExampleExtendedConnectionManager {
    TpBaseConnectionManager parent;

    ExampleExtendedConnectionManagerPrivate *priv;
};

GType example_extended_connection_manager_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER \
  (example_extended_connection_manager_get_type ())
#define EXAMPLE_EXTENDED_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER, \
                              ExampleExtendedConnectionManager))
#define EXAMPLE_EXTENDED_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
                           EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER, \
                           ExampleExtendedConnectionManagerClass))
#define EXAMPLE_IS_EXTENDED_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                              EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER))
#define EXAMPLE_IS_EXTENDED_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
                           EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER))
#define EXAMPLE_EXTENDED_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                              EXAMPLE_TYPE_EXTENDED_CONNECTION_MANAGER, \
                              ExampleExtendedConnectionManagerClass))

G_END_DECLS

#endif
