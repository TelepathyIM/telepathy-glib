/* Tests of TpAccount channel request API
 *
 * Copyright © 2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"
#include "tests/lib/simple-channel-dispatcher.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsSimpleAccount *account_service;
    TpTestsSimpleChannelDispatcher *cd_service;

    /* Client side objects */
    TpConnection *connection;
    TpAccount *account;
    TpChannel *channel;

    GError *error /* initialized where needed */;
} Test;

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Claim AccountManager bus-name (needed as we're going to export an Account
   * object). */
  tp_dbus_daemon_request_name (test->dbus,
          TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  /* Create service-side Account object */
  test->account_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
  tp_dbus_daemon_register_object (test->dbus, ACCOUNT_PATH,
      test->account_service);

  /* Claim CD bus-name */
  tp_dbus_daemon_request_name (test->dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

    /* Create client-side Account object */
  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  /* Create and register CD */
  test->cd_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCHER,
      "connection", test->base_connection,
      NULL);

  tp_dbus_daemon_register_object (test->dbus, TP_CHANNEL_DISPATCHER_OBJECT_PATH,
      test->cd_service);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_dbus_daemon_unregister_object (test->dbus, test->account_service);
  g_object_unref (test->account_service);

  tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  tp_dbus_daemon_release_name (test->dbus, TP_CHANNEL_DISPATCHER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  tp_clear_object (&test->cd_service);

  tp_clear_object (&test->dbus);

  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->account);

  tp_clear_object (&test->channel);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  tp_clear_object (&test->connection);
  tp_clear_object (&test->base_connection);
}

static void
create_and_handle_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  Test *test = user_data;

  if (!tp_account_create_and_handle_channel_finish (TP_ACCOUNT (source),
        result, &test->channel, &test->error))
    goto out;

  g_assert (TP_IS_CHANNEL (test->channel));
  tp_clear_object (&test->channel);

out:
  g_main_loop_quit (test->mainloop);
}

static GHashTable *
create_request (void)
{
  return tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
        TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, "alice",
      NULL);
}

static void
test_create_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;

  request = create_request ();

  tp_account_create_and_handle_channel_async (test->account, request, 0,
      create_and_handle_cb, test);

  g_hash_table_unref (request);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

/* ChannelDispatcher.CreateChannel() call fails */
static void
test_create_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "CreateChannelFail", TRUE);

  tp_account_create_and_handle_channel_async (test->account, request, 0,
      create_and_handle_cb, test);

  g_hash_table_unref (request);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest.Proceed() call fails */
static void
test_proceed_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "ProceedFail", TRUE);

  tp_account_create_and_handle_channel_async (test->account, request, 0,
      create_and_handle_cb, test);

  g_hash_table_unref (request);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest fire the 'Failed' signal */
static void
test_cr_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;

  request = create_request ();

  /* Ask to the CR to fire the signal */
  tp_asv_set_boolean (request, "FireFailed", TRUE);

  tp_account_create_and_handle_channel_async (test->account, request, 0,
      create_and_handle_cb, test);

  g_hash_table_unref (request);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/account-channels/create-success", Test, NULL, setup,
      test_create_success, teardown);
  g_test_add ("/account-channels/create-fail", Test, NULL, setup,
      test_create_fail, teardown);
  g_test_add ("/account-channels/proceed-fail", Test, NULL, setup,
      test_proceed_fail, teardown);
  g_test_add ("/account-channels/cr-failed", Test, NULL, setup,
      test_cr_failed, teardown);

  return g_test_run ();
}
