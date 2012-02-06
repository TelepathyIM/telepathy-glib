/*
 * manager.c - an example connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "connection-manager.h"

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>

#include "protocol.h"

G_DEFINE_TYPE (ExampleExtendedConnectionManager,
    example_extended_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

static void
example_extended_connection_manager_init (
    ExampleExtendedConnectionManager *self)
{
}

static void
example_extended_connection_manager_constructed (GObject *object)
{
  ExampleExtendedConnectionManager *self =
    EXAMPLE_EXTENDED_CONNECTION_MANAGER (object);
  TpBaseConnectionManager *base = (TpBaseConnectionManager *) self;
  void (*constructed) (GObject *) =
    ((GObjectClass *) example_extended_connection_manager_parent_class)->constructed;
  TpBaseProtocol *protocol;

  if (constructed != NULL)
    constructed (object);

  protocol = g_object_new (EXAMPLE_TYPE_EXTENDED_PROTOCOL,
      "name", "example",
      NULL);
  tp_base_connection_manager_add_protocol (base, protocol);
  g_object_unref (protocol);
}

static void
example_extended_connection_manager_class_init (
    ExampleExtendedConnectionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  object_class->constructed = example_extended_connection_manager_constructed;
  base_class->cm_dbus_name = "example_extended";
}
