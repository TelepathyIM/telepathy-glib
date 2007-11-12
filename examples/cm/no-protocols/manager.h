/*
 * manager.h - header for an example connection manager
 * Copyright (C) 2007 Collabora Ltd.
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
