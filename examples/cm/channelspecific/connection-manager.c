/*
 * manager.c - an example connection manager
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "connection-manager.h"

#include <telepathy-glib/telepathy-glib.h>

#include "protocol.h"

G_DEFINE_TYPE (ExampleCSHConnectionManager,
    example_csh_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

static void
example_csh_connection_manager_init (ExampleCSHConnectionManager *self)
{
}

static void
example_csh_connection_manager_constructed (GObject *object)
{
  ExampleCSHConnectionManager *self = EXAMPLE_CSH_CONNECTION_MANAGER (object);
  TpBaseConnectionManager *base = (TpBaseConnectionManager *) self;
  void (*constructed) (GObject *) =
    ((GObjectClass *) example_csh_connection_manager_parent_class)->constructed;
  TpBaseProtocol *protocol;

  if (constructed != NULL)
    constructed (object);

  protocol = g_object_new (EXAMPLE_TYPE_CSH_PROTOCOL,
      "name", "example",
      NULL);
  tp_base_connection_manager_add_protocol (base, protocol);
  g_object_unref (protocol);
}

static void
example_csh_connection_manager_class_init (
    ExampleCSHConnectionManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  object_class->constructed = example_csh_connection_manager_constructed;
  base_class->cm_dbus_name = "example_csh";
}
