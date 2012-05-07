/*
 * bug-19101-conn.c - a broken connection to reproduce bug #19101
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "bug-19101-conn.h"

#include <telepathy-glib/interfaces.h>

#include "debug.h"

static void contacts_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpTestsBug19101Connection,
    tp_tests_bug19101_connection, TP_TESTS_TYPE_CONTACTS_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACTS,
        contacts_iface_init);
    )

static void
tp_tests_bug19101_connection_init (TpTestsBug19101Connection *self)
{
}

static void
tp_tests_bug19101_connection_class_init (TpTestsBug19101ConnectionClass *klass)
{
}

/* A broken implementation of GetContactAttributes, which returns an empty dict
 * of attributes for each handle (other than the self-handle).
 */
static void
tp_tests_bug19101_connection_get_contact_attributes (
    TpSvcConnectionInterfaceContacts *iface,
    const GArray *handles,
    const char **interfaces,
    gboolean hold,
    DBusGMethodInvocation *context)
{
  TpBaseConnection *base_conn = TP_BASE_CONNECTION (iface);
  GHashTable *result;
  guint i;
  const gchar *assumed_interfaces[] = {
    TP_IFACE_CONNECTION,
    NULL
  };

  if (handles->len == 1 &&
      g_array_index (handles, TpHandle, 0) ==
      tp_base_connection_get_self_handle (base_conn))
    {
      DEBUG ("called for self-handle (during preparation), not being rubbish");
      /* strictly speaking this should hold the handles on behalf of the
       * sending process, but handles are immortal now anyway... */
      result = tp_contacts_mixin_get_contact_attributes ((GObject *) iface,
          handles, interfaces, assumed_interfaces, NULL);
      goto finally;
    }

  DEBUG ("called; returning rubbish");

  result = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_hash_table_unref);

  for (i = 0 ; i < handles->len ; i++)
    {
      TpHandle h= g_array_index (handles, TpHandle, i);
      GHashTable *attr_hash = g_hash_table_new_full (NULL, NULL, NULL, NULL);

      g_hash_table_insert (result, GUINT_TO_POINTER(h), attr_hash);
    }

finally:
  tp_svc_connection_interface_contacts_return_from_get_contact_attributes (
      context, result);
  g_hash_table_unref (result);
}

static void
contacts_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionInterfaceContactsClass *klass =
    (TpSvcConnectionInterfaceContactsClass *) g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_contacts_implement_##x ( \
    klass, tp_tests_bug19101_connection_##x)
  IMPLEMENT(get_contact_attributes);
#undef IMPLEMENT
}
