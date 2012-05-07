/* Feature test for TpContact creation using a connection which doesn't
 * support Contacts.
 * Those tests are not updated any more as Contacts is now mandatory.
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

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

typedef struct {
    Result result;
    TpBaseConnection *base_connection;
    TpTestsContactsConnection *legacy_service_conn;
    TpConnection *legacy_client_conn;
    TpHandleRepoIface *service_repo;
} Fixture;

static void
reset_result (Result *result)
{
  tp_clear_pointer (&result->invalid, g_array_unref);
  if (result->contacts != NULL)
    g_ptr_array_foreach (result->contacts, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&result->contacts, g_ptr_array_unref);
  tp_clear_pointer (&result->good_ids, g_strfreev);
  tp_clear_pointer (&result->bad_ids, g_hash_table_unref);
  g_clear_error (&result->error);
}

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
test_by_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->legacy_service_conn;
  TpConnection *client_conn = f->legacy_client_conn;
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
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  g_assert_cmpuint (tp_contact_get_handle (contacts[0]), ==, handles[0]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  g_assert_cmpuint (tp_contact_get_handle (contacts[1]), ==, handles[1]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");
  contacts[3] = g_ptr_array_index (result.contacts, 2);
  g_assert_cmpuint (tp_contact_get_handle (contacts[3]), ==, handles[3]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[3]), ==, "chris");

  /* clean up before doing the second request */
  g_array_unref (result.invalid);
  result.invalid = NULL;
  g_ptr_array_unref (result.contacts);
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
  g_assert_no_error (result.error);

  /* 0, 1 and 3 we already have a reference to */
  MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
  g_object_unref (g_ptr_array_index (result.contacts, 0));
  MYASSERT (g_ptr_array_index (result.contacts, 1) == contacts[1], "");
  g_object_unref (g_ptr_array_index (result.contacts, 1));
  MYASSERT (g_ptr_array_index (result.contacts, 3) == contacts[3], "");
  g_object_unref (g_ptr_array_index (result.contacts, 3));

  /* 2 we don't */
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  g_assert_cmpuint (tp_contact_get_handle (contacts[2]), ==, handles[2]);
  g_assert_cmpstr (tp_contact_get_identifier (contacts[2]), ==, "dora");

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
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  /* unref all the handles we created service-side */
  tp_handle_unref (service_repo, handles[0]);
  tp_handle_unref (service_repo, handles[1]);
  tp_handle_unref (service_repo, handles[2]);
  tp_handle_unref (service_repo, handles[3]);

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_array_unref (result.invalid);
  g_ptr_array_unref (result.contacts);
  g_assert (result.error == NULL);
}

static void
test_no_features (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->legacy_service_conn;
  TpConnection *client_conn = f->legacy_client_conn;
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
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==, ids[i]);
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          tp_contact_get_identifier (contacts[i]));
      MYASSERT (tp_contact_get_avatar_token (contacts[i]) == NULL,
          ": %s", tp_contact_get_avatar_token (contacts[i]));
      g_assert_cmpuint (tp_contact_get_presence_type (contacts[i]), ==,
          TP_CONNECTION_PRESENCE_TYPE_UNSET);
      g_assert_cmpstr (tp_contact_get_presence_status (contacts[i]), ==, "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==, "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
    }

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
      tp_handle_unref (service_repo, handles[i]);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_array_unref (result.invalid);
  g_ptr_array_unref (result.contacts);
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
test_upgrade (Fixture *f,
    gconstpointer mode)
{
  TpTestsContactsConnection *service_conn = f->legacy_service_conn;
  TpConnection *client_conn = f->legacy_client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
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
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  TpContact *contacts[3];
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN, TP_CONTACT_FEATURE_PRESENCE };
  guint i;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_tests_contacts_connection_change_aliases (service_conn, 3, handles,
      aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 3, handles,
      tokens);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (tp_contact_get_connection (contacts[i]) == client_conn, "");
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==, ids[i]);
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          tp_contact_get_identifier (contacts[i]));
      MYASSERT (tp_contact_get_avatar_token (contacts[i]) == NULL,
          ": %s", tp_contact_get_avatar_token (contacts[i]));
      g_assert_cmpuint (tp_contact_get_presence_type (contacts[i]), ==,
          TP_CONNECTION_PRESENCE_TYPE_UNSET);
      g_assert_cmpstr (tp_contact_get_presence_status (contacts[i]), ==, "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==, "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
    }

  /* clean up before doing the second request */
  g_array_unref (result.invalid);
  result.invalid = NULL;
  g_ptr_array_unref (result.contacts);
  result.contacts = NULL;
  g_assert (result.error == NULL);

  if (!tp_strdiff (mode, "old"))
    {
      tp_connection_upgrade_contacts (client_conn,
          3, contacts,
          G_N_ELEMENTS (features), features,
          upgrade_cb,
          &result, finish, NULL);

      g_main_loop_run (result.loop);
    }
  else
    {
      GAsyncResult *res = NULL;

      tp_connection_upgrade_contacts_async (client_conn,
          3, contacts,
          G_N_ELEMENTS (features), features,
          tp_tests_result_ready_cb, &res);
      tp_tests_run_until_result (&res);

      tp_connection_upgrade_contacts_finish (client_conn, res,
          &result.contacts, &result.error);
    }

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid == NULL, "");
  g_assert_no_error (result.error);

  for (i = 0; i < 3; i++)
    {
      MYASSERT (g_ptr_array_index (result.contacts, 0) == contacts[0], "");
      g_object_unref (g_ptr_array_index (result.contacts, i));
    }

  for (i = 0; i < 3; i++)
    {
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==, ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==, aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          messages[i]);

      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "available");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==, "busy");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[2]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[2]), ==, "away");

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
      tp_handle_unref (service_repo, handles[i]);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_ptr_array_unref (result.contacts);
  g_assert (result.invalid == NULL);
  g_assert (result.error == NULL);
}

typedef struct
{
  gboolean alias_changed;
  gboolean avatar_token_changed;
  gboolean presence_type_changed;
  gboolean presence_status_changed;
  gboolean presence_msg_changed;
} notify_ctx;

static void
notify_ctx_init (notify_ctx *ctx)
{
  ctx->alias_changed = FALSE;
  ctx->avatar_token_changed = FALSE;
  ctx->presence_type_changed = FALSE;
  ctx->presence_status_changed = FALSE;
  ctx->presence_msg_changed = FALSE;
}

static gboolean
notify_ctx_is_fully_changed (notify_ctx *ctx)
{
  return ctx->alias_changed && ctx->avatar_token_changed &&
    ctx->presence_type_changed && ctx->presence_status_changed &&
    ctx->presence_msg_changed;
}

static gboolean
notify_ctx_is_changed (notify_ctx *ctx)
{
  return ctx->alias_changed || ctx->avatar_token_changed ||
    ctx->presence_type_changed || ctx->presence_status_changed ||
    ctx->presence_msg_changed;
}

static void
contact_notify_cb (TpContact *contact,
    GParamSpec *param,
    notify_ctx *ctx)
{
  if (!tp_strdiff (param->name, "alias"))
    ctx->alias_changed = TRUE;
  else if (!tp_strdiff (param->name, "avatar-token"))
    ctx->avatar_token_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-type"))
    ctx->presence_type_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-status"))
    ctx->presence_status_changed = TRUE;
  else if (!tp_strdiff (param->name, "presence-message"))
    ctx->presence_msg_changed = TRUE;
}

static void
test_features (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpTestsContactsConnection *service_conn = f->legacy_service_conn;
  TpConnection *client_conn = f->legacy_client_conn;
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handles[] = { 0, 0, 0 };
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
  static const gchar * const new_aliases[] = { "Alice [at a tea party]",
      "Bob the Plumber" };
  static const gchar * const new_tokens[] = { "AAAA", "BBBB" };
  static TpTestsContactsConnectionPresenceStatusIndex new_statuses[] = {
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY,
      TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE };
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
  notify_ctx notify_ctx_alice, notify_ctx_chris;

  g_message (G_STRFUNC);

  for (i = 0; i < 3; i++)
    handles[i] = tp_handle_ensure (service_repo, ids[i], NULL, NULL);

  tp_tests_contacts_connection_change_aliases (service_conn, 3, handles,
      aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 3, handles,
      statuses, messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 3, handles,
      tokens);

  tp_connection_get_contacts_by_handle (client_conn,
      3, handles,
      G_N_ELEMENTS (features), features,
      by_handle_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.contacts->len == 3, ": %u", result.contacts->len);
  MYASSERT (result.invalid->len == 0, ": %u", result.invalid->len);
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");

  for (i = 0; i < 3; i++)
    contacts[i] = g_ptr_array_index (result.contacts, i);

  for (i = 0; i < 3; i++)
    {
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==, ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==, aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          messages[i]);

      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "available");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==,
      "busy");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[2]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[2]), ==,
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
  g_assert_cmpuint (from_gobject.handle, ==, handles[0]);
  g_assert_cmpstr (from_gobject.identifier, ==, "alice");
  g_assert_cmpstr (from_gobject.alias, ==, "Alice in Wonderland");
  g_assert_cmpstr (from_gobject.avatar_token, ==, "aaaaa");
  g_assert_cmpuint (from_gobject.presence_type, ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (from_gobject.presence_status, ==, "available");
  g_assert_cmpstr (from_gobject.presence_message, ==, "");
  g_object_unref (from_gobject.connection);
  g_free (from_gobject.identifier);
  g_free (from_gobject.alias);
  g_free (from_gobject.avatar_token);
  g_free (from_gobject.presence_status);
  g_free (from_gobject.presence_message);

  notify_ctx_init (&notify_ctx_alice);
  g_signal_connect (contacts[0], "notify",
      G_CALLBACK (contact_notify_cb), &notify_ctx_alice);

  notify_ctx_init (&notify_ctx_chris);
  g_signal_connect (contacts[2], "notify",
      G_CALLBACK (contact_notify_cb), &notify_ctx_chris);

  /* Change Alice and Bob's contact info, leave Chris as-is */
  tp_tests_contacts_connection_change_aliases (service_conn, 2, handles,
      new_aliases);
  tp_tests_contacts_connection_change_presences (service_conn, 2, handles,
      new_statuses, new_messages);
  tp_tests_contacts_connection_change_avatar_tokens (service_conn, 2, handles,
      new_tokens);
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  g_assert (notify_ctx_is_fully_changed (&notify_ctx_alice));
  g_assert (!notify_ctx_is_changed (&notify_ctx_chris));

  for (i = 0; i < 2; i++)
    {
      g_assert_cmpuint (tp_contact_get_handle (contacts[i]), ==, handles[i]);
      g_assert_cmpstr (tp_contact_get_identifier (contacts[i]), ==,
          ids[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_ALIAS), "");
      g_assert_cmpstr (tp_contact_get_alias (contacts[i]), ==,
          new_aliases[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_AVATAR_TOKEN), "");
      g_assert_cmpstr (tp_contact_get_avatar_token (contacts[i]), ==,
          new_tokens[i]);

      MYASSERT (tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_PRESENCE), "");
      g_assert_cmpstr (tp_contact_get_presence_message (contacts[i]), ==,
          new_messages[i]);

      MYASSERT (!tp_contact_has_feature (contacts[i],
            TP_CONTACT_FEATURE_LOCATION), "");
    }

  g_assert_cmpuint (tp_contact_get_presence_type (contacts[0]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[0]), ==,
      "away");
  g_assert_cmpuint (tp_contact_get_presence_type (contacts[1]), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (tp_contact_get_presence_status (contacts[1]), ==,
      "available");

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
      tp_tests_proxy_run_until_dbus_queue_processed (client_conn);
      tp_handle_unref (service_repo, handles[i]);
    }

  /* remaining cleanup */
  g_main_loop_unref (result.loop);
  g_array_unref (result.invalid);
  g_ptr_array_unref (result.contacts);
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
test_by_id (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpConnection *client_conn = f->legacy_client_conn;
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
  g_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  g_ptr_array_unref (result.contacts);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_unref (result.bad_ids);
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
  g_assert_no_error (result.error);

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  g_assert_cmpstr (result.good_ids[0], ==, "Alice");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  g_assert_cmpstr (result.good_ids[1], ==, "Bob");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");

  for (i = 0; i < 2; i++)
    {
      g_object_unref (contacts[i]);
    }

  g_ptr_array_unref (result.contacts);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_unref (result.bad_ids);
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
  g_assert_no_error (result.error);

  e = g_hash_table_lookup (result.bad_ids, "Not valid");
  MYASSERT (e != NULL, "");

  e = g_hash_table_lookup (result.bad_ids, "not valid either");
  MYASSERT (e != NULL, "");

  MYASSERT (g_ptr_array_index (result.contacts, 0) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 1) != NULL, "");
  MYASSERT (g_ptr_array_index (result.contacts, 2) != NULL, "");
  contacts[0] = g_ptr_array_index (result.contacts, 0);
  g_assert_cmpstr (result.good_ids[0], ==, "Alice");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[0]), ==, "alice");
  contacts[1] = g_ptr_array_index (result.contacts, 1);
  g_assert_cmpstr (result.good_ids[1], ==, "Bob");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[1]), ==, "bob");
  contacts[2] = g_ptr_array_index (result.contacts, 2);
  g_assert_cmpstr (result.good_ids[2], ==, "Chris");
  g_assert_cmpstr (tp_contact_get_identifier (contacts[2]), ==, "chris");

  /* clean up refs to contacts */

  for (i = 0; i < 3; i++)
    {
      g_object_unref (contacts[i]);
    }

  /* wait for ReleaseHandles to run */
  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  /* remaining cleanup */
  g_main_loop_unref (result.loop);

  g_ptr_array_unref (result.contacts);
  result.contacts = NULL;
  g_strfreev (result.good_ids);
  result.good_ids = NULL;
  g_hash_table_unref (result.bad_ids);
  result.bad_ids = NULL;
}

static void
test_one_by_id (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  GAsyncResult *result = NULL;
  GError *error = NULL;
  TpContact *contact;

  tp_connection_dup_contact_by_id_async (f->legacy_client_conn,
      "Alice", 0, NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  contact = tp_connection_dup_contact_by_id_finish (f->legacy_client_conn,
      result, &error);

  g_assert_no_error (error);
  g_assert (TP_IS_CONTACT (contact));
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");

  g_clear_object (&result);
  g_clear_object (&contact);
}

static void
test_by_handle_again (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handle;
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      f->base_connection, TP_HANDLE_TYPE_CONTACT);
  TpContact *contact;
  gpointer weak_pointer;
  const gchar *alias = "Alice in Wonderland";
  TpContactFeature feature = TP_CONTACT_FEATURE_ALIAS;
  gboolean ok;

  g_test_bug ("25181");

  handle = tp_handle_ensure (service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);
  tp_tests_contacts_connection_change_aliases (f->legacy_service_conn, 1,
      &handle, &alias);

  tp_connection_get_contacts_by_handle (f->legacy_client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  /* clean up before doing the second request */
  reset_result (&result);
  g_assert (result.error == NULL);

  /* silently remove the object from D-Bus, so that if the second request
   * makes any D-Bus calls, it will fail (but the client conn isn't
   * invalidated) */
  tp_dbus_daemon_unregister_object (
      tp_base_connection_get_dbus_daemon (f->base_connection),
      f->base_connection);
  /* check that that worked */
  ok = tp_cli_connection_run_get_self_handle (f->legacy_client_conn, -1, NULL,
      &result.error, NULL);
  g_assert_error (result.error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD);
  g_assert (!ok);
  g_clear_error (&result.error);

  tp_connection_get_contacts_by_handle (f->legacy_client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) == contact);
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  /* OK, put it back so teardown() can use it */
  tp_dbus_daemon_register_object (
      tp_base_connection_get_dbus_daemon (f->base_connection),
      tp_base_connection_get_object_path (f->base_connection),
      f->base_connection);
  /* check that *that* worked */
  ok = tp_cli_connection_run_get_self_handle (f->legacy_client_conn, -1, NULL,
      &result.error, NULL);
  g_assert_no_error (result.error);
  g_assert (ok);

  g_assert (result.error == NULL);
  reset_result (&result);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (f->legacy_client_conn);
  g_main_loop_unref (result.loop);
}

static void
test_by_handle_upgrade (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  TpHandle handle;
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      f->base_connection, TP_HANDLE_TYPE_CONTACT);
  TpContact *contact;
  gpointer weak_pointer;
  const gchar *alias = "Alice in Wonderland";
  TpContactFeature feature = TP_CONTACT_FEATURE_ALIAS;

  g_test_bug ("32191");

  handle = tp_handle_ensure (service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (handle, !=, 0);
  tp_tests_contacts_connection_change_aliases (f->legacy_service_conn,
      1, &handle, &alias);

  tp_connection_get_contacts_by_handle (f->legacy_client_conn,
      1, &handle,
      0, NULL,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) != NULL);
  contact = g_object_ref (g_ptr_array_index (result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, "alice");
  /* fallback alias is still in effect */
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "alice");

  /* clean up before doing the second request */
  reset_result (&result);
  g_assert (result.error == NULL);

  /* the second request enables the Alias feature, so it must make more D-Bus
   * round trips */
  tp_connection_get_contacts_by_handle (f->legacy_client_conn,
      1, &handle,
      1, &feature,
      by_handle_cb,
      &result, finish, NULL);
  g_main_loop_run (result.loop);
  g_assert_cmpuint (result.contacts->len, ==, 1);
  g_assert_cmpuint (result.invalid->len, ==, 0);
  g_assert_no_error (result.error);

  g_assert (g_ptr_array_index (result.contacts, 0) == contact);
  g_assert_cmpstr (tp_contact_get_alias (contact), ==, "Alice in Wonderland");

  g_assert (result.error == NULL);
  reset_result (&result);

  weak_pointer = contact;
  g_object_add_weak_pointer ((GObject *) contact, &weak_pointer);
  g_object_unref (contact);
  g_assert (weak_pointer == NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (f->legacy_client_conn);
  g_main_loop_unref (result.loop);
}

static void
test_dup_if_possible (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle alice_handle, bob_handle;
  TpContact *alice;
  TpContact *contact;

  alice_handle = tp_handle_ensure (f->service_repo, "alice", NULL, NULL);
  g_assert_cmpuint (alice_handle, !=, 0);
  bob_handle = tp_handle_ensure (f->service_repo, "bob", NULL, NULL);
  g_assert_cmpuint (bob_handle, !=, 0);

  tp_connection_get_contacts_by_handle (f->legacy_client_conn,
      1, &alice_handle,
      0, NULL,
      by_handle_cb,
      &f->result, finish, NULL);
  g_main_loop_run (f->result.loop);
  g_assert_cmpuint (f->result.contacts->len, ==, 1);
  g_assert_cmpuint (f->result.invalid->len, ==, 0);
  g_assert_no_error (f->result.error);

  g_assert (g_ptr_array_index (f->result.contacts, 0) != NULL);
  alice = g_object_ref (g_ptr_array_index (f->result.contacts, 0));
  g_assert_cmpuint (tp_contact_get_handle (alice), ==, alice_handle);
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");

  reset_result (&f->result);

  /* we already have a cached TpContact for Alice, so we can get another
   * copy of it synchronously */

  contact = tp_connection_dup_contact_if_possible (f->legacy_client_conn,
      alice_handle, "alice");
  g_assert (contact == alice);
  g_object_unref (contact);

  contact = tp_connection_dup_contact_if_possible (f->legacy_client_conn,
      alice_handle, NULL);
  g_assert (contact == alice);
  g_object_unref (contact);

  /* because this connection pretends not to have immortal handles, we can't
   * reliably get a contact for Bob synchronously, even if we supply his
   * identifier */

  contact = tp_connection_dup_contact_if_possible (f->legacy_client_conn,
      bob_handle, "bob");
  g_assert (contact == NULL);

  contact = tp_connection_dup_contact_if_possible (f->legacy_client_conn,
      bob_handle, NULL);
  g_assert (contact == NULL);
}

static void
setup (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_LEGACY_CONTACTS_CONNECTION,
      "me@test.com", &f->base_connection, &f->legacy_client_conn);

  f->legacy_service_conn = g_object_ref (TP_TESTS_CONTACTS_CONNECTION (
        f->base_connection));
  f->service_repo = tp_base_connection_get_handles (f->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  f->result.loop = g_main_loop_new (NULL, FALSE);
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  if (f->legacy_client_conn != NULL)
    tp_tests_connection_assert_disconnect_succeeds (f->legacy_client_conn);

  f->service_repo = NULL;
  tp_clear_object (&f->legacy_client_conn);
  tp_clear_object (&f->legacy_service_conn);
  tp_clear_object (&f->base_connection);
  reset_result (&f->result);
  tp_clear_pointer (&f->result.loop, g_main_loop_unref);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/contacts-slow-path/by-handle", Fixture, NULL, setup,
      test_by_handle, teardown);
  g_test_add ("/contacts-slow-path/no-features", Fixture, NULL, setup,
      test_no_features, teardown);
  g_test_add ("/contacts-slow-path/features", Fixture, NULL, setup,
      test_features, teardown);
  g_test_add ("/contacts-slow-path/upgrade/old", Fixture, "old", setup,
      test_upgrade, teardown);
  g_test_add ("/contacts-slow-path/upgrade", Fixture, "async", setup,
      test_upgrade, teardown);
  g_test_add ("/contacts-slow-path/by-id", Fixture, NULL, setup,
      test_by_id, teardown);
  g_test_add ("/contacts-slow-path/by-handle-again", Fixture, NULL, setup,
      test_by_handle_again, teardown);
  g_test_add ("/contacts-slow-path/by-handle-upgrade", Fixture, NULL, setup,
      test_by_handle_upgrade, teardown);
  g_test_add ("/contacts-slow-path/dup-if-possible", Fixture, NULL, setup,
      test_dup_if_possible, teardown);
  g_test_add ("/contacts-slow-path/one-by-id", Fixture, NULL, setup,
      test_one_by_id, teardown);

  return g_test_run ();
}
