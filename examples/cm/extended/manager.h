/*
 * manager.h - header for an example connection manager
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_CONNECTION_MANAGER_H__
#define __EXAMPLE_CONNECTION_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

typedef struct _ExampleConnectionManager ExampleConnectionManager;
typedef struct _ExampleConnectionManagerClass ExampleConnectionManagerClass;

struct _ExampleConnectionManagerClass {
    TpBaseConnectionManagerClass parent_class;
};

struct _ExampleConnectionManager {
    TpBaseConnectionManager parent;

    gpointer priv;
};

GType example_connection_manager_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_CONNECTION_MANAGER \
  (example_connection_manager_get_type ())
#define EXAMPLE_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EXAMPLE_TYPE_CONNECTION_MANAGER, \
                              ExampleConnectionManager))
#define EXAMPLE_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EXAMPLE_TYPE_CONNECTION_MANAGER, \
                           ExampleConnectionManagerClass))
#define EXAMPLE_IS_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EXAMPLE_TYPE_CONNECTION_MANAGER))
#define EXAMPLE_IS_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EXAMPLE_TYPE_CONNECTION_MANAGER))
#define EXAMPLE_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXAMPLE_TYPE_CONNECTION_MANAGER, \
                              ExampleConnectionManagerClass))

G_END_DECLS

#endif /* #ifndef __EXAMPLE_CONNECTION_MANAGER_H__*/
