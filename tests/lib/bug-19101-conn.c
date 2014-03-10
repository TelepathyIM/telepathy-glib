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

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "debug.h"

static void conn_iface_init (TpSvcConnectionClass *klass);

G_DEFINE_TYPE_WITH_CODE (TpTestsBug19101Connection,
    tp_tests_bug19101_connection, TP_TESTS_TYPE_CONTACTS_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION, conn_iface_init))

static void
tp_tests_bug19101_connection_init (TpTestsBug19101Connection *self)
{
}

static void
tp_tests_bug19101_connection_class_init (TpTestsBug19101ConnectionClass *klass)
{
}

/* A broken implementation of GetContactByID, which returns an empty dict
 * of attributes for each id.
 */
static void
tp_tests_bug19101_connection_get_contact_by_id (
    TpSvcConnection *iface,
    const gchar *id,
    const char **interfaces,
    GDBusMethodInvocation *context)
{
  TpBaseConnection *base_conn = TP_BASE_CONNECTION (iface);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
      base_conn, TP_ENTITY_TYPE_CONTACT);
  TpHandle handle;
  GHashTable *table;

  handle = tp_handle_ensure (contact_repo, id, NULL, NULL);
  table = g_hash_table_new (NULL, NULL);

  tp_svc_connection_return_from_get_contact_by_id (context, handle, table);

  g_hash_table_unref (table);
}

static void
conn_iface_init (TpSvcConnectionClass *klass)
{
#define IMPLEMENT(x) tp_svc_connection_implement_##x ( \
    klass, tp_tests_bug19101_connection_##x)
  IMPLEMENT(get_contact_by_id);
#undef IMPLEMENT
}
