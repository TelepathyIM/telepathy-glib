/* Feature test for TpContactsMixin
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static void
test_no_features (ContactsConnection *service_conn,
                  TpConnection *client_conn,
                  GArray *handles)
{
  GError *error = NULL;
  GHashTable *contacts;
  GHashTable *attrs;

  g_message (G_STRFUNC);

  MYASSERT (tp_cli_connection_interface_contacts_run_get_contact_attributes (
        client_conn, -1, handles, NULL, FALSE, &contacts, &error, NULL), "");
  test_assert_no_error (error);
  MYASSERT_SAME_UINT (g_hash_table_size (contacts), 3);

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 0)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "alice");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 1)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "bob");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 2)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "chris");

  g_hash_table_destroy (contacts);
}

static void
test_features (ContactsConnection *service_conn,
               TpConnection *client_conn,
               GArray *handles)
{
  const gchar *interfaces[] = { TP_IFACE_CONNECTION,
      TP_IFACE_CONNECTION_INTERFACE_ALIASING,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS,
      TP_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE,
      NULL };
  GError *error = NULL;
  GHashTable *contacts;
  GHashTable *attrs;

  g_message (G_STRFUNC);

  MYASSERT (tp_cli_connection_interface_contacts_run_get_contact_attributes (
        client_conn, -1, handles, interfaces, FALSE, &contacts, &error, NULL),
      "");
  test_assert_no_error (error);
  MYASSERT_SAME_UINT (g_hash_table_size (contacts), 3);

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 0)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "alice");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING "/alias"),
      "Alice in Wonderland");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS "/token"),
      "aaaaa");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 1)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "bob");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING "/alias"),
      "Bob the Builder");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS "/token"),
      "bbbbb");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 2)));
  MYASSERT (attrs != NULL, "");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"),
      "chris");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING "/alias"),
      "Christopher Robin");
  MYASSERT_SAME_STRING (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS "/token"),
      "ccccc");

  g_hash_table_destroy (contacts);
}

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *dbus;
  ContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error = NULL;
  TpConnection *client_conn;
  GArray *handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), 3);
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static ContactsConnectionPresenceStatusIndex statuses[] = {
      CONTACTS_CONNECTION_STATUS_AVAILABLE, CONTACTS_CONNECTION_STATUS_BUSY,
      CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  TpHandleRepoIface *service_repo;
  guint i;

  /* Setup */

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_dbus_daemon_new (tp_get_bus ());

  service_conn = CONTACTS_CONNECTION (g_object_new (
        CONTACTS_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");
  service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  /* Set up some contacts */

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

      g_array_append_val (handles, handle);
    }

  contacts_connection_change_aliases (service_conn, 3,
      (const TpHandle *) handles->data, aliases);
  contacts_connection_change_presences (service_conn, 3,
      (const TpHandle *) handles->data, statuses, messages);
  contacts_connection_change_avatar_tokens (service_conn, 3,
      (const TpHandle *) handles->data, tokens);

  /* Tests */

  test_no_features (service_conn, client_conn, handles);
  test_features (service_conn, client_conn, handles);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  test_assert_no_error (error);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);
  g_array_free (handles, TRUE);

  return 0;
}
