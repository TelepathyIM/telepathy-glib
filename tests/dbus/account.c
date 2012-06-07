/* A very basic feature test for TpAccount
 *
 * Copyright (C) 2009-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/account.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/svc-account.h>
#include <telepathy-glib/enums.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/util.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"
#define SUPERSEDED_PATH TP_ACCOUNT_OBJECT_PATH_BASE "super/seded/whatever"

static void
test_parse_failure (gconstpointer test_data)
{
  GError *error = NULL;

  g_assert (!tp_account_parse_object_path (test_data, NULL, NULL, NULL,
      &error));
  g_assert (error != NULL);
  g_error_free (error);
}

typedef struct {
    const gchar *path;
    const gchar *cm;
    const gchar *protocol;
    const gchar *account_id;
} TestParseData;

static TestParseData *
test_parse_data_new (const gchar *path,
    const gchar *cm,
    const gchar *protocol,
    const gchar *account_id)
{
  TestParseData *t = g_slice_new (TestParseData);

  t->path = path;
  t->cm = cm;
  t->protocol = protocol;
  t->account_id = account_id;

  return t;
}

static void
test_parse_success (gconstpointer test_data)
{
  TestParseData *t = (TestParseData *) test_data;
  gchar *cm, *protocol, *account_id;
  GError *error = NULL;

  g_assert (tp_account_parse_object_path (t->path, &cm, &protocol, &account_id,
      &error));
  g_assert_no_error (error);
  g_assert_cmpstr (cm, ==, t->cm);
  g_assert_cmpstr (protocol, ==, t->protocol);
  g_assert_cmpstr (account_id, ==, t->account_id);

  g_free (cm);
  g_free (protocol);
  g_free (account_id);

  g_slice_free (TestParseData, t);
}

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpAccount *account;
    gulong notify_id;
    /* g_strdup (property name) => GUINT_TO_POINTER (counter) */
    GHashTable *times_notified;
    GAsyncResult *result;
    GError *error /* initialized where needed */;

    /* initialized in prepare_service */
    TpTestsSimpleAccount *account_service;
    TpBaseConnection *conn1_service;
    TpBaseConnection *conn2_service;
    TpConnection *conn1;
    TpConnection *conn2;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->account = NULL;

  test->times_notified = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
}

static void
setup_service (Test *test,
    gconstpointer data)
{
  setup (test, data);

  tp_dbus_daemon_request_name (test->dbus,
      TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  test->account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
  tp_dbus_daemon_register_object (test->dbus, ACCOUNT_PATH,
      test->account_service);

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "what@ever", &test->conn1_service, &test->conn1);
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "what2@ever", &test->conn2_service, &test->conn2);
}

static guint
test_get_times_notified (Test *test,
    const gchar *property)
{
  return GPOINTER_TO_UINT (g_hash_table_lookup (test->times_notified,
        property));
}

static void
test_notify_cb (Test *test,
    GParamSpec *pspec,
    TpAccount *account)
{
  guint counter = test_get_times_notified (test, pspec->name);

  g_hash_table_insert (test->times_notified, g_strdup (pspec->name),
      GUINT_TO_POINTER (++counter));
}

static void
test_set_up_account_notify (Test *test)
{
  g_assert (test->account != NULL);

  g_hash_table_remove_all (test->times_notified);

  if (test->notify_id != 0)
    {
      g_signal_handler_disconnect (test->account, test->notify_id);
    }

  test->notify_id = g_signal_connect_swapped (test->account, "notify",
      G_CALLBACK (test_notify_cb), test);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->account != NULL)
    {
      tp_tests_proxy_run_until_dbus_queue_processed (test->account);

      if (test->notify_id != 0)
        {
          g_signal_handler_disconnect (test->account, test->notify_id);
        }

      g_object_unref (test->account);
      test->account = NULL;
    }

  g_hash_table_unref (test->times_notified);
  test->times_notified = NULL;

  /* make sure any pending calls on the account have happened, so it can die */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  g_clear_error (&test->error);
  tp_clear_object (&test->result);
}

static void
teardown_service (Test *test,
    gconstpointer data)
{
  tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  tp_dbus_daemon_unregister_object (test->dbus, test->account_service);
  g_clear_object (&test->account_service);

  tp_tests_connection_assert_disconnect_succeeds (test->conn1);
  g_clear_object (&test->conn1);
  g_clear_object (&test->conn1_service);

  tp_tests_connection_assert_disconnect_succeeds (test->conn2);
  g_clear_object (&test->conn2);
  g_clear_object (&test->conn2_service);

  teardown (test, data);
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test->account = tp_account_new (test->dbus,
      "/secretly/not/an/object", NULL);
  g_assert (test->account == NULL);

  test->account = tp_account_new (test->dbus,
      "not even syntactically valid", NULL);
  g_assert (test->account == NULL);

  test->account = tp_account_new (test->dbus,
      "/org/freedesktop/Telepathy/Account/what/ev/er", NULL);
  g_assert (test->account != NULL);
}

static void
test_setters (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->account = tp_account_new (test->dbus,
      "/org/freedesktop/Telepathy/Account/what/ev/er", NULL);
  g_assert (test->account != NULL);

  tp_account_set_enabled_async (test->account, TRUE, tp_tests_result_ready_cb,
    &test->result);
  tp_tests_run_until_result (&test->result);
  tp_account_set_enabled_finish (test->account, test->result, &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);
  tp_clear_object (&test->result);
}

static void
test_reconnect (Test *test,
    gconstpointer data)
{
  GStrv reconnect_required;
  const gchar *unset[] = { "unset", NULL };

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  if (!tp_strdiff (data, "vardict"))
    {
      tp_account_update_parameters_vardict_async (test->account,
          g_variant_new_parsed ("{ 'set': <%s> }", "value"), unset,
          tp_tests_result_ready_cb, &test->result);
      tp_tests_run_until_result (&test->result);
      tp_account_update_parameters_vardict_finish (test->account, test->result,
          &reconnect_required, &test->error);
    }
  else
    {
      GHashTable *set = tp_asv_new (
          "set", G_TYPE_STRING, "value",
          NULL);

      tp_account_update_parameters_async (test->account, set, unset,
          tp_tests_result_ready_cb, &test->result);
      tp_tests_run_until_result (&test->result);
      tp_account_update_parameters_finish (test->account, test->result,
          &reconnect_required, &test->error);

      g_hash_table_unref (set);
    }

  g_assert_no_error (test->error);
  /* check that reconnect_required survives longer than result */
  tp_clear_object (&test->result);

  g_assert (reconnect_required != NULL);
  g_assert_cmpstr (reconnect_required[0], ==, "set");
  g_assert_cmpstr (reconnect_required[1], ==, "unset");
  g_assert_cmpstr (reconnect_required[2], ==, NULL);
  g_strfreev (reconnect_required);

  tp_account_reconnect_async (test->account, tp_tests_result_ready_cb,
      &test->result);
  tp_tests_run_until_result (&test->result);
  tp_account_reconnect_finish (test->account, test->result, &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);
  tp_clear_object (&test->result);
}

static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  GError *error = NULL;

  tp_proxy_prepare_finish (source, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (test->mainloop);
}

#define assert_strprop(self, prop, val) \
  {\
    gchar *_s; \
    \
    g_object_get (self, \
        prop, &_s, \
        NULL); \
    g_assert_cmpstr (_s, ==, val);\
    g_free (_s); \
  }
#define assert_uintprop(self, prop, val) \
  {\
    guint _u; \
    \
    g_object_get (self, \
        prop, &_u, \
        NULL); \
    g_assert_cmpuint (_u, ==, val);\
  }
#define assert_boolprop(self, prop, val) \
  {\
    gboolean _b; \
    \
    g_object_get (self, \
        prop, &_b, \
        NULL); \
    g_assert_cmpint (_b, ==, val);\
  }

static void
test_prepare_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  TpConnectionStatusReason reason;
  gchar *status = NULL;
  gchar *message = NULL;
  const GHashTable *details = GUINT_TO_POINTER (666);
  GStrv strv;
  const gchar * const *cstrv;
  GVariant *variant;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  /* the obvious accessors */
  g_assert (tp_account_is_prepared (test->account, TP_ACCOUNT_FEATURE_CORE));
  g_assert (tp_account_is_enabled (test->account));
  assert_boolprop (test->account, "enabled", TRUE);
  g_assert (tp_account_is_valid (test->account));
  assert_boolprop (test->account, "valid", TRUE);
  g_assert_cmpstr (tp_account_get_display_name (test->account), ==,
      "Fake Account");
  assert_strprop (test->account, "display-name", "Fake Account");
  g_assert_cmpstr (tp_account_get_nickname (test->account), ==, "badger");
  assert_strprop (test->account, "nickname", "badger");
  g_assert_cmpuint (tp_asv_size (tp_account_get_parameters (test->account)),
      ==, 0);
  variant = tp_account_dup_parameters_vardict (test->account);
  g_assert_cmpstr (g_variant_get_type_string (variant), ==, "a{sv}");
  g_assert_cmpuint (g_variant_n_children (variant), ==, 0);
  g_variant_unref (variant);
  g_assert (!tp_account_get_connect_automatically (test->account));
  assert_boolprop (test->account, "connect-automatically", FALSE);
  g_assert (tp_account_get_has_been_online (test->account));
  assert_boolprop (test->account, "has-been-online", TRUE);
  g_assert_cmpint (tp_account_get_connection_status (test->account, NULL),
      ==, TP_CONNECTION_STATUS_CONNECTED);
  assert_uintprop (test->account, "connection-status",
      TP_CONNECTION_STATUS_CONNECTED);
  g_assert_cmpint (tp_account_get_connection_status (test->account, &reason),
      ==, TP_CONNECTION_STATUS_CONNECTED);
  g_assert_cmpint (reason, ==, TP_CONNECTION_STATUS_REASON_REQUESTED);
  assert_uintprop (test->account, "connection-status-reason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  g_assert_cmpstr (tp_account_get_detailed_error (test->account, NULL), ==,
      NULL);
  assert_strprop (test->account, "connection-error", NULL);
  g_assert_cmpstr (tp_account_get_detailed_error (test->account, &details), ==,
      NULL);
  /* this is documented to be untouched */
  g_assert_cmpuint (GPOINTER_TO_UINT (details), ==, 666);

  /* the CM and protocol come from the object path */
  g_assert_cmpstr (tp_account_get_cm_name (test->account),
      ==, "what");
  assert_strprop (test->account, "cm-name", "what");
  g_assert_cmpstr (tp_account_get_protocol_name (test->account), ==, "ev");
  assert_strprop (test->account, "protocol-name", "ev");

  /* the icon name in SimpleAccount is "", so we guess based on the protocol */
  g_assert_cmpstr (tp_account_get_icon_name (test->account), ==, "im-ev");
  assert_strprop (test->account, "icon-name", "im-ev");

  /* RequestedPresence */
  g_assert_cmpint (tp_account_get_requested_presence (test->account, NULL,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_BUSY);
  assert_uintprop (test->account, "requested-presence-type",
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpint (tp_account_get_requested_presence (test->account, &status,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (status, ==, "requesting");
  g_free (status);
  assert_strprop (test->account, "requested-status", "requesting");
  g_assert_cmpint (tp_account_get_requested_presence (test->account, NULL,
        &message), ==, TP_CONNECTION_PRESENCE_TYPE_BUSY);
  g_assert_cmpstr (message, ==, "this is my RequestedPresence");
  g_free (message);
  assert_strprop (test->account, "requested-status-message",
      "this is my RequestedPresence");

  /* CurrentPresence */
  g_assert_cmpint (tp_account_get_current_presence (test->account, NULL,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AWAY);
  assert_uintprop (test->account, "current-presence-type",
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpint (tp_account_get_current_presence (test->account, &status,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (status, ==, "currently-away");
  g_free (status);
  assert_strprop (test->account, "current-status", "currently-away");
  g_assert_cmpint (tp_account_get_current_presence (test->account, NULL,
        &message), ==, TP_CONNECTION_PRESENCE_TYPE_AWAY);
  g_assert_cmpstr (message, ==, "this is my CurrentPresence");
  g_free (message);
  assert_strprop (test->account, "current-status-message",
      "this is my CurrentPresence");

  /* AutomaticPresence */
  g_assert_cmpint (tp_account_get_automatic_presence (test->account, NULL,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  assert_uintprop (test->account, "automatic-presence-type",
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpint (tp_account_get_automatic_presence (test->account, &status,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (status, ==, "automatically-available");
  g_free (status);
  assert_strprop (test->account, "automatic-status",
      "automatically-available");
  g_assert_cmpint (tp_account_get_automatic_presence (test->account, NULL,
        &message), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (message, ==, "this is my AutomaticPresence");
  g_free (message);
  assert_strprop (test->account, "automatic-status-message",
      "this is my AutomaticPresence");

  /* NormalizedName */
  g_assert_cmpstr (tp_account_get_normalized_name (test->account), ==,
      "bob.mcbadgers@example.com");
  assert_strprop (test->account, "normalized-name",
      "bob.mcbadgers@example.com");

  g_object_get (test->account,
      "supersedes", &strv,
      NULL);
  g_assert_cmpstr (strv[0], ==, SUPERSEDED_PATH);
  g_assert_cmpstr (strv[1], ==, NULL);
  g_strfreev (strv);

  cstrv = tp_account_get_supersedes (test->account);
  g_assert_cmpstr (cstrv[0], ==, SUPERSEDED_PATH);
  g_assert_cmpstr (cstrv[1], ==, NULL);
}

static void
test_storage (Test *test,
    gconstpointer mode)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_STORAGE, 0 };
  GValue *gvalue;
  GVariant *gvariant;
  GHashTable *info;
  GError *error = NULL;
  gboolean found;
  gint32 i;
  guint32 u;
  const gchar *s;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  if (g_str_equal (mode, "later"))
    {
      /* prepare the core feature first */
      tp_proxy_prepare_async (test->account, NULL, account_prepare_cb, test);
      g_main_loop_run (test->mainloop);

      /* storage stuff doesn't work yet */
      g_assert_cmpstr (tp_account_get_storage_provider (test->account), ==,
          NULL);
      assert_strprop (test->account, "storage-provider", NULL);
      g_assert (tp_account_get_storage_identifier (test->account) == NULL);
      g_object_get (test->account,
          "storage-identifier", &gvalue,
          NULL);
      g_assert (gvalue == NULL);
      g_assert (tp_account_get_storage_identifier (test->account) == NULL);
      g_object_get (test->account,
          "storage-identifier", &gvariant,
          NULL);
      g_assert (gvariant == NULL);
      g_assert_cmpuint (tp_account_get_storage_restrictions (test->account), ==,
          0);
      assert_uintprop (test->account, "storage-restrictions", 0);
    }

  /* prepare the storage feature */
  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_cmpstr (tp_account_get_storage_provider (test->account), ==,
      "org.freedesktop.Telepathy.glib.test");
  assert_strprop (test->account, "storage-provider",
      "org.freedesktop.Telepathy.glib.test");

  g_assert_cmpstr (
      g_value_get_string (tp_account_get_storage_identifier (test->account)),
      ==, "unique-identifier");
  g_object_get (test->account,
      "storage-identifier", &gvalue,
      NULL);
  g_assert_cmpstr (g_value_get_string (gvalue), ==, "unique-identifier");
  g_boxed_free (G_TYPE_VALUE, gvalue);

  gvariant = tp_account_dup_storage_identifier_variant (test->account);
  g_assert_cmpstr (g_variant_get_type_string (gvariant), ==, "s");
  g_assert_cmpstr (g_variant_get_string (gvariant, NULL), ==,
      "unique-identifier");
  g_variant_unref (gvariant);
  g_object_get (test->account,
      "storage-identifier-variant", &gvariant,
      NULL);
  g_assert_cmpstr (g_variant_get_type_string (gvariant), ==, "s");
  g_assert_cmpstr (g_variant_get_string (gvariant, NULL), ==,
      "unique-identifier");
  g_variant_unref (gvariant);

  g_assert_cmpuint (tp_account_get_storage_restrictions (test->account), ==,
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_ENABLED |
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS);
  assert_uintprop (test->account, "storage-restrictions",
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_ENABLED |
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS);

  /* request the StorageSpecificProperties hash */
  tp_account_get_storage_specific_information_async (test->account,
      tp_tests_result_ready_cb, &test->result);
  tp_tests_run_until_result (&test->result);

  info = tp_account_get_storage_specific_information_finish (
      test->account, test->result, &error);
  g_assert_no_error (error);

  g_assert_cmpuint (g_hash_table_size (info), ==, 3);

  g_assert_cmpint (tp_asv_get_int32 (info, "one", NULL), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (info, "two", NULL), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (info, "marco"), ==, "polo");

  tp_clear_object (&test->result);

  /* the same, but with 100% more GVariant */
  tp_account_dup_storage_specific_information_vardict_async (test->account,
      tp_tests_result_ready_cb, &test->result);
  tp_tests_run_until_result (&test->result);

  gvariant = tp_account_dup_storage_specific_information_vardict_finish (
      test->account, test->result, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (g_variant_get_type_string (gvariant), ==, "a{sv}");
  found = g_variant_lookup (gvariant, "one", "i", &i);
  g_assert (found);
  g_assert_cmpint (i, ==, 1);
  found = g_variant_lookup (gvariant, "two", "u", &u);
  g_assert (found);
  g_assert_cmpint (u, ==, 2);
  found = g_variant_lookup (gvariant, "marco", "&s", &s);
  g_assert (found);
  g_assert_cmpstr (s, ==, "polo");
  found = g_variant_lookup (gvariant, "barisione", "&s", &s);
  g_assert (!found);

  g_variant_unref (gvariant);
  tp_clear_object (&test->result);
}

static void
test_addressing (Test *test,
    gconstpointer mode)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_ADDRESSING, 0 };
  const gchar * const *schemes;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  if (g_str_equal (mode, "later"))
    {
      /* prepare the core feature first */
      tp_proxy_prepare_async (test->account, NULL, account_prepare_cb, test);
      g_main_loop_run (test->mainloop);

      /* addressing stuff doesn't work yet */
      g_assert (tp_account_get_uri_schemes (test->account) == NULL);
      g_assert (!tp_account_associated_with_uri_scheme (test->account,
            "about"));
      g_assert (!tp_account_associated_with_uri_scheme (test->account,
            "telnet"));
      g_assert (!tp_account_associated_with_uri_scheme (test->account,
            "xmpp"));
    }

  /* prepare the addressing feature */
  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  schemes = tp_account_get_uri_schemes (test->account);
  g_assert (schemes != NULL);
  g_assert (tp_strv_contains (schemes, "about"));
  g_assert (tp_strv_contains (schemes, "telnet"));
  g_assert (schemes[2] == NULL);

  g_assert (tp_account_associated_with_uri_scheme (test->account,
        "about"));
  g_assert (tp_account_associated_with_uri_scheme (test->account,
        "telnet"));
  g_assert (!tp_account_associated_with_uri_scheme (test->account,
        "xmpp"));

}

static void
test_avatar (Test *test,
    gconstpointer mode)
{
  const GArray *blob;
  GError *error = NULL;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_proxy_prepare_async (test->account, NULL, account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  tp_account_get_avatar_async (test->account,
      tp_tests_result_ready_cb, &test->result);
  tp_tests_run_until_result (&test->result);

  blob = tp_account_get_avatar_finish (
      test->account, test->result, &error);
  g_assert_no_error (error);

  g_assert_cmpuint (blob->len, ==, 4);
  g_assert_cmpstr (((char *) blob->data), ==, ":-)");

  tp_clear_object (&test->result);
}

static void
test_connection (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const gchar *conn1_path = tp_proxy_get_object_path (test->conn1);
  const gchar *conn2_path = tp_proxy_get_object_path (test->conn2);
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  GHashTable *change = tp_asv_new (NULL, NULL);
  TpConnection *conn;
  const GHashTable *details;
  GVariant *details_v;
  gboolean found;
  gchar *s;
  guint32 u;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_account_is_prepared (test->account, TP_ACCOUNT_FEATURE_CORE));

  /* a connection turns up */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", conn1_path);
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_CONNECTING);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  while (test_get_times_notified (test, "connection") < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);
  conn = tp_account_get_connection (test->account);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, conn1_path);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  g_assert_cmpstr (tp_account_get_detailed_error (test->account, NULL), ==,
      TP_ERROR_STR_CANCELLED);

  /* ensure the same connection - no change notification */

  test_set_up_account_notify (test);
  conn = tp_account_ensure_connection (test->account, conn1_path);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, conn1_path);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);

  /* a no-op "change" */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", conn1_path);
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_CONNECTING);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  tp_tests_proxy_run_until_dbus_queue_processed (test->account);

  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);
  conn = tp_account_get_connection (test->account);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, conn1_path);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);

  /* atomically flip from one connection to another (unlikely) */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", conn2_path);
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_CONNECTED);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  while (test_get_times_notified (test, "connection") < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);
  conn = tp_account_get_connection (test->account);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, conn2_path);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  /* no more connection for you */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", "/");
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_DISCONNECTED);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  while (test_get_times_notified (test, "connection") < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);
  conn = tp_account_get_connection (test->account);
  g_assert (conn == NULL);

  g_assert_cmpstr (tp_account_get_detailed_error (test->account, NULL), ==,
      TP_ERROR_STR_ENCRYPTION_ERROR);

  /* another connection */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", conn1_path);
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_CONNECTING);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  tp_tests_proxy_run_until_dbus_queue_processed (test->account);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  /* lose the connection again */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", "/");
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_DISCONNECTED);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR);
  tp_asv_set_static_string (change, "ConnectionError",
      "org.debian.packages.OpenSSL.NotRandomEnough");
  tp_asv_take_boxed (change, "ConnectionErrorDetails",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      tp_asv_new (
        "bits-of-entropy", G_TYPE_UINT, 15,
        "debug-message", G_TYPE_STRING, "shiiiiii-",
        NULL));
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  tp_tests_proxy_run_until_dbus_queue_processed (test->account);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);
  g_assert_cmpuint (test_get_times_notified (test, "connection-error"), ==, 1);

  g_assert_cmpstr (tp_account_get_detailed_error (test->account, &details), ==,
      "org.debian.packages.OpenSSL.NotRandomEnough");
  g_assert_cmpuint (tp_asv_size (details), >=, 2);
  g_assert_cmpstr (tp_asv_get_string (details, "debug-message"), ==,
      "shiiiiii-");
  g_assert_cmpuint (tp_asv_get_uint32 (details, "bits-of-entropy", NULL), ==,
      15);

  s = tp_account_dup_detailed_error_vardict (test->account, &details_v);
  g_assert_cmpstr (s, ==, "org.debian.packages.OpenSSL.NotRandomEnough");
  g_free (s);
  g_assert_cmpuint (g_variant_n_children (details_v), >=, 2);
  g_assert_cmpstr (g_variant_get_type_string (details_v), ==, "a{sv}");
  found = g_variant_lookup (details_v, "debug-message", "s", &s);
  g_assert (found);
  g_assert_cmpstr (s, ==, "shiiiiii-");
  g_free (s);
  found = g_variant_lookup (details_v, "bits-of-entropy", "u", &u);
  g_assert (found);
  g_assert_cmpint (u, ==, 15);
  g_variant_unref (details_v);

  /* staple on a Connection (this is intended for use in e.g. observers,
   * if they're told about a Connection that the Account hasn't told them
   * about yet) */

  test_set_up_account_notify (test);
  conn = tp_account_ensure_connection (test->account, conn1_path);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, conn1_path);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  g_hash_table_unref (change);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_data_func ("/account/parse/spaces",
      "this is not an object path", test_parse_failure);
  g_test_add_data_func ("/account/parse/no-prefix",
      "/this/is/not/an/account/path", test_parse_failure);
  g_test_add_data_func ("/account/parse/too-few-components",
      "/org/freedesktop/Telepathy/Account/wrong", test_parse_failure);
  g_test_add_data_func ("/account/parse/too-many-components",
      "/org/freedesktop/Telepathy/Account/a/b/c/d", test_parse_failure);
  g_test_add_data_func ("/account/parse/illegal-components",
      "/org/freedesktop/Telepathy/Account/1/2/3", test_parse_failure);

  g_test_add_data_func ("/account/parse/legal",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/badgers",
          "gabble", "jabber", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/hyphenated-protocol",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_xmpp/badgers",
          "salut", "local-xmpp", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/wrongly-escaped-protocol",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_2dxmpp/badgers",
          "salut", "local-xmpp", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/wrongly-escaped-corner-case",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_2d/badgers",
          "salut", "local-", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/underscored-account",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "haze/msn/_thisseemsunlikely",
          "haze", "msn", "_thisseemsunlikely"),
      test_parse_success);

  g_test_add ("/account/new", Test, NULL, setup, test_new, teardown);

  g_test_add ("/account/setters", Test, NULL, setup_service, test_setters,
      teardown_service);

  g_test_add ("/account/reconnect", Test, NULL, setup_service, test_reconnect,
      teardown_service);
  g_test_add ("/account/reconnect", Test, "vardict", setup_service,
      test_reconnect, teardown_service);

  g_test_add ("/account/prepare/success", Test, NULL, setup_service,
              test_prepare_success, teardown_service);

  g_test_add ("/account/connection", Test, NULL, setup_service,
              test_connection, teardown_service);

  g_test_add ("/account/storage", Test, "first", setup_service, test_storage,
      teardown_service);
  g_test_add ("/account/storage", Test, "later", setup_service, test_storage,
      teardown_service);

  g_test_add ("/account/avatar", Test, NULL, setup_service, test_avatar,
      teardown_service);

  g_test_add ("/account/addressing", Test, "first", setup_service,
      test_addressing, teardown_service);
  g_test_add ("/account/addressing", Test, "later", setup_service,
      test_addressing, teardown_service);

  return g_test_run ();
}
