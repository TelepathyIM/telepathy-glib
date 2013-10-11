/* Feature test for TpContactsMixin
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>

#include "telepathy-glib/reentrants.h"

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static void
test_no_features (TpTestsContactsConnection *service_conn,
                  TpConnection *client_conn,
                  GArray *handles)
{
  GError *error = NULL;
  GHashTable *contacts;
  GHashTable *attrs;

  g_message (G_STRFUNC);

  MYASSERT (tp_cli_connection_interface_contacts_run_get_contact_attributes (
        client_conn, -1, handles, NULL, &contacts, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (contacts), ==, 3);

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 0)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "alice");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 1)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "bob");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 2)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "chris");

  g_hash_table_unref (contacts);
}

static void
test_features (TpTestsContactsConnection *service_conn,
               TpConnection *client_conn,
               GArray *handles)
{
  const gchar *interfaces[] = { TP_IFACE_CONNECTION,
      TP_IFACE_CONNECTION_INTERFACE_ALIASING1,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS1,
      TP_IFACE_CONNECTION_INTERFACE_PRESENCE1,
      NULL };
  GError *error = NULL;
  GHashTable *contacts;
  GHashTable *attrs;

  g_message (G_STRFUNC);

  MYASSERT (tp_cli_connection_interface_contacts_run_get_contact_attributes (
        client_conn, -1, handles, interfaces, &contacts, &error, NULL),
      "");
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (contacts), ==, 3);

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 0)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "alice");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING1 "/alias"), ==,
      "Alice in Wonderland");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS1 "/token"), ==,
      "aaaaa");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 1)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "bob");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING1 "/alias"), ==,
      "Bob the Builder");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS1 "/token"), ==,
      "bbbbb");

  attrs = g_hash_table_lookup (contacts,
      GUINT_TO_POINTER (g_array_index (handles, guint, 2)));
  MYASSERT (attrs != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (attrs, TP_IFACE_CONNECTION "/contact-id"), ==,
      "chris");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING1 "/alias"), ==,
      "Christopher Robin");
  g_assert_cmpstr (
      tp_asv_get_string (attrs,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS1 "/token"), ==,
      "ccccc");

  g_hash_table_unref (contacts);
}

int
main (int argc,
      char **argv)
{
  TpTestsContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *client_conn;
  GArray *handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), 3);
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static TpTestsContactsConnectionPresenceStatusIndex statuses[] = {
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  TpHandleRepoIface *service_repo;
  guint i;

  /* Setup */

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &client_conn);
  service_conn = TP_TESTS_CONTACTS_CONNECTION (service_conn_as_base);

  service_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);

  /* Set up some contacts */

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

      g_array_append_val (handles, handle);
    }

  tp_tests_contacts_connection_change_aliases (service_conn, 3,
      (const TpHandle *) handles->data, aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 3,
      (const TpHandle *) handles->data, statuses, messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 3,
      (const TpHandle *) handles->data, tokens);

  /* Tests */

  test_no_features (service_conn, client_conn, handles);
  test_features (service_conn, client_conn, handles);

  /* Teardown */

  tp_tests_connection_assert_disconnect_succeeds (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_array_unref (handles);

  return 0;
}
