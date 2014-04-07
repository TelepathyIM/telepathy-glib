/*
 * broken-client-types-conn.c - a connection with a broken client
 *   types implementation which inexplicably returns presence information!
 *
 * Copyright © 2011 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "broken-client-types-conn.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

G_DEFINE_TYPE_WITH_CODE (TpTestsBrokenClientTypesConnection,
    tp_tests_broken_client_types_connection,
    TP_TESTS_TYPE_CONTACTS_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CLIENT_TYPES1,
        NULL);
    );

static void
tp_tests_broken_client_types_connection_init (
    TpTestsBrokenClientTypesConnection *self)
{
}

static void
fill_contact_attributes (TpBaseConnection *base,
    const gchar *dbus_interface,
    TpHandle contact,
    GVariantDict *attributes)
{
  if (!tp_strdiff (dbus_interface,
        TP_IFACE_CONNECTION_INTERFACE_CLIENT_TYPES1))
    {
      /* Muahaha. Actually we add Presence information. */
      g_variant_dict_insert (attributes,
          TP_TOKEN_CONNECTION_INTERFACE_PRESENCE1_PRESENCE,
          "(uss)", TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, "available",
          "hi mum!");
    }
}

static void
tp_tests_broken_client_types_connection_class_init (
    TpTestsBrokenClientTypesConnectionClass *klass)
{
  TpBaseConnectionClass *base_class = TP_BASE_CONNECTION_CLASS (klass);

  base_class->fill_contact_attributes = fill_contact_attributes;
}
