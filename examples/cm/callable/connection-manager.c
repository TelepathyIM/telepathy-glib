/*
 * manager.c - an example connection manager
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
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

#include "config.h"

#include "connection-manager.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>

#include "conn.h"
#include "protocol.h"

G_DEFINE_TYPE (ExampleCallableConnectionManager,
    example_callable_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

struct _ExampleCallableConnectionManagerPrivate
{
  int dummy;
};

static void
example_callable_connection_manager_init (
    ExampleCallableConnectionManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALLABLE_CONNECTION_MANAGER,
      ExampleCallableConnectionManagerPrivate);
}

static void
example_callable_connection_manager_constructed (GObject *object)
{
  ExampleCallableConnectionManager *self =
    EXAMPLE_CALLABLE_CONNECTION_MANAGER (object);
  TpBaseConnectionManager *base = (TpBaseConnectionManager *) self;
  void (*constructed) (GObject *) =
    ((GObjectClass *) example_callable_connection_manager_parent_class)->constructed;
  TpBaseProtocol *protocol;

  if (constructed != NULL)
    constructed (object);

  protocol = g_object_new (EXAMPLE_TYPE_CALLABLE_PROTOCOL,
      "name", "example",
      NULL);
  tp_base_connection_manager_add_protocol (base, protocol);
  g_object_unref (protocol);
}

static void
example_callable_connection_manager_class_init (
    ExampleCallableConnectionManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  g_type_class_add_private (klass,
      sizeof (ExampleCallableConnectionManagerPrivate));

  object_class->constructed = example_callable_connection_manager_constructed;
  base_class->cm_dbus_name = "example_callable";
}
