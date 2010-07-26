/*
 * A trivial connection manager which supports no protocols
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "connection-manager.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>

G_DEFINE_TYPE (ExampleNoProtocolsConnectionManager,
    example_no_protocols_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

/* type definition stuff */

static void
example_no_protocols_connection_manager_init (
    ExampleNoProtocolsConnectionManager *self)
{
}

/* private data */

static void
example_no_protocols_connection_manager_class_init (
    ExampleNoProtocolsConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  base_class->cm_dbus_name = "example_no_protocols";
}
