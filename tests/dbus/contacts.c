/* Feature test for TpContact creation.
 *
 * Code missing coverage in contact.c:
 * - connection becoming invalid
 * - fatal error on the connection
 * - inconsistent CM
 * - having to fall back to RequestAliases
 * - get_contacts_by_id with features (but it's trivial)
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

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
    GPtrArray *contacts;
    GArray *invalid;
    gchar **good_ids;
    GHashTable *bad_ids;
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
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

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
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
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
  test_assert_no_error (result.error);

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
  test_assert_no_error (result.error);

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
  test_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
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

static void
upgrade_cb (TpConnection *connection,
            guint n_contacts,
            TpContact * const *contacts,
            const GError *error,
            gpointer user_data,
            GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      guint i;

      DEBUG ("got %u contacts", n_contacts);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
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
test_upgrade (ContactsConnection *service_conn,
              TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static ContactsConnectionPresenceStatusIndex statuses[] = {
      CONTACTS_CONNECTION_STATUS_AVAILABLE, CONTACTS_CONNECTION_STATUS_BUSY,
      CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE };
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  contacts_connection_change_aliases (service_conn, 3, handles, aliases);
  contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  contacts_connection_change_avatar_tokens (service_conn, 3, handles, tokens);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  test_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
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

  /* clean up before doing the second request */
  g_array_free (result.invalid, TRUE);
  result.invalid = NULL;
  g_ptr_array_free (result.contacts, TRUE);
  result.contacts = NULL;
  g_assert (result.error == NULL);

  tp_connection_upgrade_contacts (client_conn,
      3, contacts,
      sizeof (features) / sizeof (features[0]), features,
      upgrade_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid == NULL, "");
  test_assert_no_error (result.error);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
      g_object_unref (g_ptr_array_index (result.contacts, i));
    }

  for (i = 0; i < 3; i++)
    {
      MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[i]), handles[i]);
      MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[i]), ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT_SAME_STRING (tp_contact_get_alias (contacts[i]), aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT_SAME_STRING (tp_contact_get_avatar_token (contacts[i]),
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT_SAME_STRING (tp_contact_get_presence_message (contacts[i]),
          messages[i]);
    }

  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[0]),
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[0]),
      "available");
  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[1]),
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[1]),
      "busy");
  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[2]),
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[2]),
      "away");

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      test_connection_run_until_dbus_queue_processed (client_conn);
      tp_handle_unref (service_repo, handles[i]);
      MYASSERT (!tp_handle_is_valid (service_repo, handles[i], NULL), "");
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_ptr_array_free (result.contacts, TRUE);
  g_assert (result.invalid == NULL);
  g_assert (result.error == NULL);
}

static void
test_features (ContactsConnection *service_conn,
               TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
  static const gchar * const ids[] = { "alice", "bob", "chris" };
  static const gchar * const aliases[] = { "Alice in Wonderland",
      "Bob the Builder", "Christopher Robin" };
  static const gchar * const tokens[] = { "aaaaa", "bbbbb", "ccccc" };
  static ContactsConnectionPresenceStatusIndex statuses[] = {
      CONTACTS_CONNECTION_STATUS_AVAILABLE, CONTACTS_CONNECTION_STATUS_BUSY,
      CONTACTS_CONNECTION_STATUS_AWAY };
  static const gchar * const messages[] = { "", "Fixing it",
      "GON OUT BACKSON" };
  static const gchar * const new_aliases[] = { "Alice [at a tea party]",
      "Bob the Plumber" };
  static const gchar * const new_tokens[] = { "AAAA", "BBBB" };
  static ContactsConnectionPresenceStatusIndex new_statuses[] = {
      CONTACTS_CONNECTION_STATUS_AWAY, CONTACTS_CONNECTION_STATUS_AVAILABLE };
  static const gchar * const new_messages[] = { "At the Mad Hatter's",
      "It'll cost you" };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE };
  guint i;
  struct {
      TpConnection *connection;
      TpHandle handle;
      gchar *identifier;
      gchar *alias;
      gchar *avatar_token;
      TpConnectionPresenceType presence_type;
      gchar *presence_status;
      gchar *presence_message;
  } from_gobject;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  contacts_connection_change_aliases (service_conn, 3, handles, aliases);
  contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  contacts_connection_change_avatar_tokens (service_conn, 3, handles, tokens);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      sizeof (features) / sizeof (features[0]), features,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  test_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[i]), handles[i]);
      MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[i]), ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT_SAME_STRING (tp_contact_get_alias (contacts[i]), aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT_SAME_STRING (tp_contact_get_avatar_token (contacts[i]),
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT_SAME_STRING (tp_contact_get_presence_message (contacts[i]),
          messages[i]);
    }

  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[0]),
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[0]),
      "available");
  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[1]),
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[1]),
      "busy");
  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[2]),
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[2]),
      "away");

  /* exercise GObject properties in a basic way */
  g_object_get (contacts[0],
      "connection", &from_gobject.connection,
      "handle", &from_gobject.handle,
      "identifier", &from_gobject.identifier,
      "alias", &from_gobject.alias,
      "avatar-token", &from_gobject.avatar_token,
      "presence-type", &from_gobject.presence_type,
      "presence-status", &from_gobject.presence_status,
      "presence-message", &from_gobject.presence_message,
      NULL);
  MYASSERT (from_gobject.connection == client_conn, "");
  MYASSERT_SAME_UINT (from_gobject.handle, handles[0]);
  MYASSERT_SAME_STRING (from_gobject.identifier, "alice");
  MYASSERT_SAME_STRING (from_gobject.alias, "Alice in Wonderland");
  MYASSERT_SAME_STRING (from_gobject.avatar_token, "aaaaa");
  MYASSERT_SAME_UINT (from_gobject.presence_type,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  MYASSERT_SAME_STRING (from_gobject.presence_status, "available");
  MYASSERT_SAME_STRING (from_gobject.presence_message, "");
  g_object_unref (from_gobject.connection);
  g_free (from_gobject.identifier);
  g_free (from_gobject.alias);
  g_free (from_gobject.avatar_token);
  g_free (from_gobject.presence_status);
  g_free (from_gobject.presence_message);

  /* Change Alice and Bob's contact info, leave Chris as-is */
  contacts_connection_change_aliases (service_conn, 2, handles, new_aliases);
  contacts_connection_change_presences (service_conn, 2, handles,
      new_statuses, new_messages);
  contacts_connection_change_avatar_tokens (service_conn, 2, handles,
      new_tokens);
  test_connection_run_until_dbus_queue_processed (client_conn);

  for (i = 0; i < 2; i++)
    {
      MYASSERT_SAME_UINT (tp_contact_get_handle (contacts[i]), handles[i]);
      MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[i]), ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT_SAME_STRING (tp_contact_get_alias (contacts[i]),
          new_aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT_SAME_STRING (tp_contact_get_avatar_token (contacts[i]),
          new_tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT_SAME_STRING (tp_contact_get_presence_message (contacts[i]),
          new_messages[i]);
    }

  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[0]),
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[0]),
      "away");
  MYASSERT_SAME_UINT (tp_contact_get_presence_type (contacts[1]),
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  MYASSERT_SAME_STRING (tp_contact_get_presence_status (contacts[1]),
      "available");

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

static void
by_id_cb (TpConnection *connection,
          guint n_contacts,
          TpContact * const *contacts,
          const gchar * const *good_ids,
          GHashTable *bad_ids,
          const GError *error,
          gpointer user_data,
          GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->invalid == NULL);
  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      GHashTableIter iter;
      gpointer key, value;
      guint i;

      DEBUG ("got %u contacts and %u bad IDs", n_contacts,
          g_hash_table_size (bad_ids));

      result->bad_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
          g_free, (GDestroyNotify) g_error_free);
      tp_g_hash_table_update (result->bad_ids, bad_ids,
          (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) g_error_copy);

      g_hash_table_iter_init (&iter, result->bad_ids);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          gchar *id = key;
          GError *e = value;

          DEBUG ("bad ID %s: %s %u: %s", id, g_quark_to_string (e->domain),
              e->code, e->message);
        }

      result->good_ids = g_strdupv ((GStrv) good_ids);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u we asked for ID %s", i, good_ids[i]);
          DEBUG ("contact #%u we got ID %s", i,
              tp_contact_get_identifier (contact));
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
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
test_by_id (TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE) };
  static const gchar * const ids[] = { "Alice", "Bob", "Not valid", "Chris",
      "not valid either", NULL };
  TpContact *contacts[3];
  guint i;
  GError *e /* no initialization needed */;

  g_message ("%s: all bad (fd.o #19688)", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      1, ids + 2,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 0, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 1, ": %u",
      g_hash_table_size (result.bad_ids));
  test_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  g_ptr_array_free (result.contacts, TRUE);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_destroy (result.bad_ids);
  result.bad_ids = NULL;

  g_message ("%s: all good", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      2, ids,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 2, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 0, ": %u",
      g_hash_table_size (result.bad_ids));
  test_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  MYASSERT_SAME_STRING (result.good_ids[0], "Alice");
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[0]), "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  MYASSERT_SAME_STRING (result.good_ids[1], "Bob");
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[1]), "bob");

  for (i = 0; i < 2; i++)
    {
      g_object_unref (contacts[i]);
    }

  g_ptr_array_free (result.contacts, TRUE);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_destroy (result.bad_ids);
  result.bad_ids = NULL;

  g_message ("%s: not all good", G_STRFUNC);

  tp_connection_get_contacts_by_id (client_conn,
      5, ids,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (g_hash_table_size (result.bad_ids) == 2, ": %u",
      g_hash_table_size (result.bad_ids));
  test_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  e = g_hash_table_lookup (result.bad_ids, "not valid either");
  MYASSERT (e != NULL, "");

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  MYASSERT_SAME_STRING (result.good_ids[0], "Alice");
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[0]), "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  MYASSERT_SAME_STRING (result.good_ids[1], "Bob");
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[1]), "bob");
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  MYASSERT_SAME_STRING (result.good_ids[2], "Chris");
  MYASSERT_SAME_STRING (tp_contact_get_identifier (contacts[2]), "chris");

  /* clean up refs to contacts */

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
    }

  /* wait for ReleaseHandles to run */
  test_connection_run_until_dbus_queue_processed (client_conn);

  /* remaining cleanup */
  g_main_loop_unref (result.loop);

  g_ptr_array_free (result.contacts, TRUE);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_destroy (result.bad_ids);
  result.bad_ids = NULL;
}

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *dbus;
  ContactsConnection *service_conn, *legacy_service_conn;
  TpBaseConnection *service_conn_as_base, *legacy_service_conn_as_base;
  gchar *name, *legacy_name;
  gchar *conn_path, *legacy_conn_path;
  GError *error = NULL;
  TpConnection *client_conn, *legacy_client_conn;

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

  legacy_service_conn = CONTACTS_CONNECTION (g_object_new (
        LEGACY_CONTACTS_TYPE_CONNECTION,
        "account", "legacy@example.com",
        "protocol", "simple",
        NULL));
  legacy_service_conn_as_base = TP_BASE_CONNECTION (legacy_service_conn);
  MYASSERT (legacy_service_conn != NULL, "");
  MYASSERT (legacy_service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  MYASSERT (tp_base_connection_register (legacy_service_conn_as_base, "simple",
        &legacy_name, &legacy_conn_path, &error), "");
  test_assert_no_error (error);

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  legacy_client_conn = tp_connection_new (dbus, legacy_name, legacy_conn_path,
      &error);
  MYASSERT (legacy_client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (legacy_client_conn, TRUE, &error,
        NULL), "");
  test_assert_no_error (error);

  /* Tests */

  test_by_handle (service_conn, client_conn);
  test_no_features (service_conn, client_conn);
  test_features (service_conn, client_conn);
  test_upgrade (service_conn, client_conn);
  test_by_id (client_conn);

  test_by_handle (legacy_service_conn, legacy_client_conn);
  test_no_features (legacy_service_conn, legacy_client_conn);
  test_features (legacy_service_conn, legacy_client_conn);
  test_upgrade (legacy_service_conn, legacy_client_conn);
  test_by_id (legacy_client_conn);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  test_assert_no_error (error);
  g_object_unref (client_conn);

  MYASSERT (tp_cli_connection_run_disconnect (legacy_client_conn, -1, &error,
        NULL), "");
  test_assert_no_error (error);
  g_object_unref (legacy_client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  legacy_service_conn_as_base = NULL;
  g_object_unref (legacy_service_conn);
  g_free (legacy_name);
  g_free (legacy_conn_path);

  g_object_unref (dbus);

  return 0;
}
