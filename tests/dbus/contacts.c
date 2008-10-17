/* Feature test for TpContact creation.
 *
 * Code missing coverage in contact.c:
 * - all optional features
 * - connection becoming invalid
 * - fatal error on the connection
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static int fail = 0;

static void
myassert_failed (void)
{
  fail = 1;
}

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
    GPtrArray *contacts;
    GArray *invalid;
} Result;

static void
by_handle_cb (TpConnection *connection,
              guint n_contacts,
              TpContact * const *contacts,
              guint n_invalid,
              const TpHandle *invalid,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);

  if (error == NULL)
    {
      guint i;

      DEBUG ("got %u contacts and %u invalid", n_contacts, n_invalid);

      result->invalid = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle),
          n_invalid);
      g_array_append_vals (result->invalid, invalid, n_invalid);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          g_ptr_array_add (result->contacts, contact);
        }
    }
  else
    {
      DEBUG ("got an error: %s %u: %s", g_quark_to_string (error->domain),
          error->code, error->message);
      result->error = g_error_copy (error);
    }
}

static void
finish (gpointer r)
{
  Result *result = r;

  g_main_loop_quit (result->loop);
}

static void
test_by_handle (ContactsConnection *service_conn,
                TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[5] = { 0, 0, 0, 0, 0 };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[4];
  gpointer weak_pointers[4];
  guint i;

  g_message (G_STRFUNC);

  /* arrange for some handles to exist */
  handles[0] = tp_handle_ensure (service_repo, "alice", NULL, NULL);
  MYASSERT (handles[0] != 0, "");
  handles[1] = tp_handle_ensure (service_repo, "bob", NULL, NULL);
  MYASSERT (handles[1] != 0, "");
  /* randomly guess at a handle that shouldn't exist */
  handles[2] = 31337;
  MYASSERT (!tp_handle_is_valid (service_repo, 31337, NULL), "");
  /* another valid handle */
  handles[3] = tp_handle_ensure (service_repo, "chris", NULL, NULL);
  MYASSERT (handles[3] != 0, "");
  /* another invalid handle */
  handles[4] = 12345;
  MYASSERT (!tp_handle_is_valid (service_repo, 12345, NULL), "");

  /* Make a request for the following 5 contacts:
   * - alice
   * - bob
   * - invalid handle 31337
   * - chris
   * - invalid handle 12345
   */
  tp_connection_get_contacts_by_handle (client_conn,
      5, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 2, ": %u", result.invalid->len);
  MYASSERT_NO_ERROR (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[0]), handles[0]);
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[0]), "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[1]), handles[1]);
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[1]), "bob");
  contacts[3] = g_ptr_array_index (result.contacts, 2);
  MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[3]), handles[3]);
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[3]), "chris");

  /* clean up before doing the second request */
  g_array_free (result.invalid, TRUE);
  result.invalid = NULL;
  g_ptr_array_free (result.contacts, TRUE);
  result.contacts = NULL;
  g_assert (result.error == NULL);

  /* Replace one of the invalid handles with a valid one */
  handles[2] = tp_handle_ensure (service_repo, "dora", NULL, NULL);
  MYASSERT (handles[2] != 0, "");

  /* Make a request for the following 4 contacts:
   * - alice (TpContact exists)
   * - bob (TpContact exists)
   * - dora (TpContact needs to be created)
   * - chris (TpContact exists)
   */
  tp_connection_get_contacts_by_handle (client_conn,
      4, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  /* assert that we got the same contacts back */

  MYASSERT (result.contacts->len == 4, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  MYASSERT_NO_ERROR (result.error);

  /* 0, 1 and 3 we already have a reference to */
  MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
  g_object_unref (g_ptr_array_index (result.contacts, 0));
  MYASSERT (g_ptr_array_index (result.contacts, 1) == contacts[1], "");
  g_object_unref (g_ptr_array_index (result.contacts, 1));
  MYASSERT (g_ptr_array_index (result.contacts, 3) == contacts[3], "");
  g_object_unref (g_ptr_array_index (result.contacts, 3));

  /* 2 we don't */
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[2]), handles[2]);
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[2]), "dora");

  /* clean up refs to contacts and assert that they aren't leaked */

  for (i = 0; i < 4; i++)
    {
      weak_pointers[i] = contacts[i];
      g_object_add_weak_pointer ((GObject *) contacts[i],weak_pointers +i);
    }

  for (i = 0; i < 4; i++)
    {
      g_object_unref (contacts[i]);
      MYASSERT (weak_pointers[i] == NULL, ": %u", i);
    }

  /* wait for ReleaseHandles to run */
  test_connection_run_until_dbus_queue_processed (client_conn);

  /* unref all the handles we created service-side */
  tp_handle_unref (service_repo, handles[0]);
  MYASSERT (!tp_handle_is_valid (service_repo, handles[0], NULL), "");
  tp_handle_unref (service_repo, handles[1]);
  MYASSERT (!tp_handle_is_valid (service_repo, handles[1], NULL), "");
  tp_handle_unref (service_repo, handles[2]);
  MYASSERT (!tp_handle_is_valid (service_repo, handles[2], NULL), "");
  tp_handle_unref (service_repo, handles[3]);
  MYASSERT (!tp_handle_is_valid (service_repo, handles[3], NULL), "");

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_array_free (result.invalid, TRUE);
  g_ptr_array_free (result.contacts, TRUE);
  g_assert (result.error == NULL);
}

static void
test_no_features (ContactsConnection *service_conn,
                  TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  const gchar * const ids[] = { "alice", "bob", "chris" };
  TpHandle handles[3] = { 0, 0, 0 };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  MYASSERT_NO_ERROR (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[i]), handles[i]);
      MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[i]), ids[i]);
      MYASSERT_SAME_STRING (tp_contact_get_alias (contacts[i]),
          tp_contact_get_identifier (contacts[i]));
      MYASSERT (tp_contact_get_avatar_token (contacts[i]) == NULL,
          ": %s", tp_contact_get_avatar_token (contacts[i]));
      MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[i]),
          TP_CONNECTION_PRESENCE_TYPE_UNSET);
      MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[i]), "");
      MYASSERT_SAME_STRING (tp_contact_get_presence_message (contacts[i]), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
    }

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      test_connection_run_until_dbus_queue_processed (client_conn);
      tp_handle_unref (service_repo, handles[i]);
      MYASSERT (!tp_handle_is_valid (service_repo, handles[i], NULL), "");
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_array_free (result.invalid, TRUE);
  g_ptr_array_free (result.contacts, TRUE);
  g_assert (result.error == NULL);
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

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  MYASSERT_NO_ERROR (error);

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  MYASSERT_NO_ERROR (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);

  /* Tests */

  test_by_handle (service_conn, client_conn);
  test_no_features (service_conn, client_conn);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);

  return fail;
}
