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

#include "bug-19101-conn.h"

#include "debug.h"

static void contacts_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (Bug19101Connection, bug_19101_connection,
    CONTACTS_TYPE_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACTS,
        contacts_iface_init);
    )

static void
bug_19101_connection_init (Bug19101Connection *self)
{
}

static void
bug_19101_connection_class_init (Bug19101ConnectionClass *klass)
{
}

/* A broken implementation of GetContactAttributes, which returns an empty dict
 * of attributes for each handle.
 */
static void
bug_19101_connection_get_contact_attributes (
    TpSvcConnectionInterfaceContacts *iface,
    const GArray *handles,
    const char **interfaces,
    gboolean hold,
    DBusGMethodInvocation *context)
{
  GHashTable *result = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_hash_table_destroy);
  guint i;

  DEBUG ("called; returning rubbish");

  for (i = 0 ; i < handles->len ; i++)
    {
      TpHandle h= g_array_index (handles, TpHandle, i);
      GHashTable *attr_hash = g_hash_table_new_full (NULL, NULL, NULL, NULL);

      g_hash_table_insert (result, GUINT_TO_POINTER(h), attr_hash);
    }

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
    klass, bug_19101_connection_##x)
  IMPLEMENT(get_contact_attributes);
#undef IMPLEMENT
}
