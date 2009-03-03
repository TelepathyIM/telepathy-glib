/*
 * manager.h - header for an example connection manager
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __EXAMPLE_NO_PROTOCOLS_CONNECTION_MANAGER_H__
#define __EXAMPLE_NO_PROTOCOLS_CONNECTION_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

typedef struct _ExampleNoProtocolsConnectionManager
    ExampleNoProtocolsConnectionManager;
typedef struct _ExampleNoProtocolsConnectionManagerPrivate
    ExampleNoProtocolsConnectionManagerPrivate;
typedef struct _ExampleNoProtocolsConnectionManagerClass
    ExampleNoProtocolsConnectionManagerClass;
typedef struct _ExampleNoProtocolsConnectionManagerClassPrivate
    ExampleNoProtocolsConnectionManagerClassPrivate;

struct _ExampleNoProtocolsConnectionManagerClass {
    TpBaseConnectionManagerClass parent_class;

    ExampleNoProtocolsConnectionManagerClassPrivate *priv;
};

struct _ExampleNoProtocolsConnectionManager {
    TpBaseConnectionManager parent;

    ExampleNoProtocolsConnectionManagerPrivate *priv;
};

GType example_no_protocols_connection_manager_get_type (void);

/* TYPE MACROS */
#define EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER \
  (example_no_protocols_connection_manager_get_type ())
#define EXAMPLE_NO_PROTOCOLS_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER, \
                              ExampleNoProtocolsConnectionManager))
#define EXAMPLE_NO_PROTOCOLS_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
                           EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER, \
                           ExampleNoProtocolsConnectionManagerClass))
#define EXAMPLE_IS_NO_PROTOCOLS_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                              EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER))
#define EXAMPLE_IS_NO_PROTOCOLS_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
                           EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER))
#define EXAMPLE_NO_PROTOCOLS_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                              EXAMPLE_TYPE_NO_PROTOCOLS_CONNECTION_MANAGER, \
                              ExampleNoProtocolsConnectionManagerClass))

G_END_DECLS

#endif
