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
    TpContactAttributeMap *attributes)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type = G_TYPE_VALUE_ARRAY;
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (!tp_strdiff (dbus_interface,
        TP_IFACE_CONNECTION_INTERFACE_CLIENT_TYPES1))
    {
      /* Muahaha. Actually we add Presence information. */
      GValueArray *presence = tp_value_array_build (3,
          G_TYPE_UINT, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE,
          G_TYPE_STRING, "available",
          G_TYPE_STRING, "hi mum!",
          G_TYPE_INVALID);

      tp_contact_attribute_map_take_sliced_gvalue (attributes,
          contact,
          TP_TOKEN_CONNECTION_INTERFACE_PRESENCE1_PRESENCE,
          tp_g_value_slice_new_take_boxed (type, presence));
    }
}

static void
tp_tests_broken_client_types_connection_class_init (
    TpTestsBrokenClientTypesConnectionClass *klass)
{
  TpBaseConnectionClass *base_class = TP_BASE_CONNECTION_CLASS (klass);

  base_class->fill_contact_attributes = fill_contact_attributes;
}
