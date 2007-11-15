/*
 * manager.c - trivial connection manager which supports no protocols
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "manager.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>

G_DEFINE_TYPE (ExampleConnectionManager,
    example_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

/* type definition stuff */

static void
example_connection_manager_init (ExampleConnectionManager *self)
{
}

/* private data */

/* We don't actually support any protocols */
const TpCMProtocolSpec stub_protocols[] = {
  { NULL, NULL }
};

static TpBaseConnection *
new_connection (TpBaseConnectionManager *self,
                const gchar *proto,
                TpIntSet *params_present,
                void *parsed_params,
                GError **error)
{
  g_assert_not_reached ();

  return NULL;
}

static void
example_connection_manager_class_init (ExampleConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  base_class->new_connection = new_connection;
  base_class->cm_dbus_name = "example-no-protocols";
  base_class->protocol_params = stub_protocols;
}
