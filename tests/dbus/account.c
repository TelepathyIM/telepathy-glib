/* A very basic feature test for TpAccount
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/account.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/svc-account.h>
#include <telepathy-glib/enums.h>

#include "tests/lib/simple-account.h"
#include "tests/lib/util.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"
#define CONN1_PATH TP_CONN_OBJECT_PATH_BASE "what/ev/er"
#define CONN2_PATH TP_CONN_OBJECT_PATH_BASE "what/ev/s"
#define CONN1_BUS_NAME TP_CONN_BUS_NAME_BASE "what.ev.er"
#define CONN2_BUS_NAME TP_CONN_BUS_NAME_BASE "what.ev.s"

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

    TpTestsSimpleAccount *account_service /* initialized in prepare_service */;
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

  tp_dbus_daemon_request_name (test->dbus,
      CONN1_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  tp_dbus_daemon_request_name (test->dbus,
      CONN2_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  test->account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
  tp_dbus_daemon_register_object (test->dbus, ACCOUNT_PATH,
      test->account_service);
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

  g_hash_table_destroy (test->times_notified);
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
  tp_dbus_daemon_release_name (test->dbus, CONN1_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);
  tp_dbus_daemon_release_name (test->dbus, CONN2_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  tp_dbus_daemon_unregister_object (test->dbus, test->account_service);

  g_object_unref (test->account_service);
  test->account_service = NULL;

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

static void
get_storage_specific_info_cb (GObject *account,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  GHashTable *info;
  GError *error = NULL;

  info = tp_account_get_storage_specific_information_finish (
      TP_ACCOUNT (account), result, &error);
  g_assert_no_error (error);

  g_assert_cmpuint (g_hash_table_size (info), ==, 3);

  g_assert_cmpint (tp_asv_get_int32 (info, "one", NULL), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (info, "two", NULL), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (info, "marco"), ==, "polo");

  g_main_loop_quit (test->mainloop);
}

static void
test_prepare_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE,
      TP_ACCOUNT_FEATURE_STORAGE, 0 };
  TpConnectionStatusReason reason;
  gchar *status = NULL;
  gchar *message = NULL;
  const GHashTable *details = GUINT_TO_POINTER (666);

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  /* the obvious accessors */
  g_assert (tp_account_is_prepared (test->account, TP_ACCOUNT_FEATURE_CORE));
  g_assert (tp_account_is_enabled (test->account));
  g_assert (tp_account_is_valid (test->account));
  g_assert_cmpstr (tp_account_get_display_name (test->account), ==,
      "Fake Account");
  g_assert_cmpstr (tp_account_get_nickname (test->account), ==, "badger");
  g_assert_cmpuint (tp_asv_size (tp_account_get_parameters (test->account)),
      ==, 0);
  g_assert (!tp_account_get_connect_automatically (test->account));
  g_assert (tp_account_get_has_been_online (test->account));
  g_assert_cmpint (tp_account_get_connection_status (test->account, NULL),
      ==, TP_CONNECTION_STATUS_CONNECTED);
  g_assert_cmpint (tp_account_get_connection_status (test->account, &reason),
      ==, TP_CONNECTION_STATUS_CONNECTED);
  g_assert_cmpint (reason, ==, TP_CONNECTION_STATUS_REASON_REQUESTED);
  g_assert_cmpstr (tp_account_get_detailed_error (test->account, NULL), ==,
      NULL);
  g_assert_cmpstr (tp_account_get_detailed_error (test->account, &details), ==,
      NULL);
  /* this is documented to be untouched */
  g_assert_cmpuint (GPOINTER_TO_UINT (details), ==, 666);

  /* the CM and protocol come from the object path */
  g_assert_cmpstr (tp_account_get_connection_manager (test->account),
      ==, "what");
  g_assert_cmpstr (tp_account_get_protocol (test->account), ==, "ev");

  /* the icon name in SimpleAccount is "", so we guess based on the protocol */
  g_assert_cmpstr (tp_account_get_icon_name (test->account), ==, "im-ev");

  /* RequestedPresence is (Available, "available", "") */
  g_assert_cmpint (tp_account_get_requested_presence (test->account, NULL,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpint (tp_account_get_requested_presence (test->account, &status,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (status, ==, "available");
  g_free (status);
  g_assert_cmpint (tp_account_get_requested_presence (test->account, NULL,
        &message), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (message, ==, "");
  g_free (message);

  /* CurrentPresence is the same as RequestedPresence */
  g_assert_cmpint (tp_account_get_current_presence (test->account, NULL,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpint (tp_account_get_current_presence (test->account, &status,
        NULL), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (status, ==, "available");
  g_free (status);
  g_assert_cmpint (tp_account_get_current_presence (test->account, NULL,
        &message), ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (message, ==, "");
  g_free (message);

  /* NormalizedName and AutomaticPresence aren't available yet */

  /* test Acct.I.Storage features */
  g_assert_cmpstr (tp_account_get_storage_provider (test->account), ==,
      "org.freedesktop.Telepathy.glib.test");
  g_assert_cmpstr (
      g_value_get_string (tp_account_get_storage_identifier (test->account)),
      ==, "unique-identifier");
  g_assert_cmpuint (tp_account_get_storage_restrictions (test->account), ==,
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_ENABLED |
      TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS);

  /* request the StorageSpecificProperties hash */
  tp_account_get_storage_specific_information_async (test->account,
      get_storage_specific_info_cb, test);
  g_main_loop_run (test->mainloop);
}

static void
test_connection (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  GHashTable *change = tp_asv_new (NULL, NULL);
  TpConnection *conn;
  const GHashTable *details;

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_proxy_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_account_is_prepared (test->account, TP_ACCOUNT_FEATURE_CORE));

  /* a connection turns up */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", CONN1_PATH);
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
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, CONN1_PATH);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  g_assert_cmpstr (tp_account_get_detailed_error (test->account, NULL), ==,
      TP_ERROR_STR_CANCELLED);

  /* ensure the same connection - no change notification */

  test_set_up_account_notify (test);
  conn = tp_account_ensure_connection (test->account, CONN1_PATH);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, CONN1_PATH);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);

  /* a no-op "change" */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", CONN1_PATH);
  tp_asv_set_uint32 (change, "ConnectionStatus",
      TP_CONNECTION_STATUS_CONNECTING);
  tp_asv_set_uint32 (change, "ConnectionStatusReason",
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_svc_account_emit_account_property_changed (test->account_service, change);
  g_hash_table_remove_all (change);

  tp_tests_proxy_run_until_dbus_queue_processed (test->account);

  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);
  conn = tp_account_get_connection (test->account);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, CONN1_PATH);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 0);

  /* atomically flip from one connection to another (unlikely) */

  test_set_up_account_notify (test);
  tp_asv_set_object_path (change, "Connection", CONN2_PATH);
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
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, CONN2_PATH);
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
  tp_asv_set_object_path (change, "Connection", CONN1_PATH);
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

  /* staple on a Connection (this is intended for use in e.g. observers,
   * if they're told about a Connection that the Account hasn't told them
   * about yet) */

  test_set_up_account_notify (test);
  conn = tp_account_ensure_connection (test->account, CONN1_PATH);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==, CONN1_PATH);
  g_assert_cmpuint (test_get_times_notified (test, "connection"), ==, 1);

  g_hash_table_destroy (change);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
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

  g_test_add ("/account/prepare/success", Test, NULL, setup_service,
              test_prepare_success, teardown_service);

  g_test_add ("/account/connection", Test, NULL, setup_service,
              test_connection, teardown_service);

  return g_test_run ();
}
