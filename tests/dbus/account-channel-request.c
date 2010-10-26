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
#include "tests/lib/simple-channel-request.h"

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

    GCancellable *cancellable;

    gint count;

    GError *error /* initialized where needed */;
} Test;

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->cancellable = g_cancellable_new ();

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

  tp_clear_object (&test->cancellable);
}

/* Request and handle tests */

static void
create_and_handle_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  Test *test = user_data;
  TpHandleChannelsContext *context = NULL;

  test->channel = tp_account_channel_request_create_and_handle_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, &context, &test->error);
  if (test->channel == NULL)
    goto out;

  g_assert (TP_IS_CHANNEL (test->channel));
  tp_clear_object (&test->channel);

  g_assert (TP_IS_HANDLE_CHANNELS_CONTEXT (context));
  g_object_unref (context);

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
test_handle_create_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_and_handle_channel_async (req,
      NULL, create_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

/* ChannelDispatcher.CreateChannel() call fails */
static void
test_handle_create_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "CreateChannelFail", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_and_handle_channel_async (req,
      NULL, create_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest.Proceed() call fails */
static void
test_handle_proceed_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "ProceedFail", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_and_handle_channel_async (req,
      NULL, create_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest fire the 'Failed' signal */
static void
test_handle_cr_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CR to fire the signal */
  tp_asv_set_boolean (request, "FireFailed", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_and_handle_channel_async (req,
      NULL, create_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

static void
ensure_and_handle_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  Test *test = user_data;

  test->channel = tp_account_channel_request_ensure_and_handle_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, NULL, &test->error);
  if (test->channel == NULL)
    goto out;

  g_assert (TP_IS_CHANNEL (test->channel));

out:
  test->count--;
  if (test->count <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_handle_ensure_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_ensure_and_handle_channel_async (req,
      NULL, ensure_and_handle_cb, test);

  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Try again, now it will fail as the channel already exist */
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_ensure_and_handle_channel_async (req,
      NULL, ensure_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_NOT_YOURS);

  tp_clear_object (&test->channel);
}

/* Cancel the operation before starting it */
static void
test_handle_cancel_before (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  g_cancellable_cancel (test->cancellable);

  tp_account_channel_request_ensure_and_handle_channel_async (req,
      test->cancellable, create_and_handle_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

/* Cancel the operation after the channel request has been created */
static void
channel_request_created_cb (TpTestsSimpleChannelDispatcher *dispatcher,
    TpTestsSimpleChannelRequest *request,
    Test *test)
{
  g_cancellable_cancel (test->cancellable);
}

static void
test_handle_cancel_after_create (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_ensure_and_handle_channel_async (req,
      test->cancellable, create_and_handle_cb, test);

  g_signal_connect (test->cd_service, "channel-request-created",
      G_CALLBACK (channel_request_created_cb), test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_CANCELLED);
}

/* Test if re-handled is properly fired when a channel is
 * re-handled */
static void
re_handled_cb (TpAccountChannelRequest *req,
    TpChannel *channel,
    gint64 timestamp,
    TpHandleChannelsContext *context,
    Test *test)
{
  g_assert (TP_IS_CHANNEL (channel));
  g_assert_cmpint (timestamp, ==, 666);
  g_assert (TP_IS_HANDLE_CHANNELS_CONTEXT (context));

  g_print ("sig fired\n");
  test->count--;
  if (test->count <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_handle_re_handle (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req, *req2;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_ensure_and_handle_channel_async (req,
      NULL, ensure_and_handle_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_signal_connect (req, "re-handled",
      G_CALLBACK (re_handled_cb), test);

  /* Ensure the same channel to re-handle it */
  req2 = tp_account_channel_request_new (test->account, request, 666);

  tp_account_channel_request_ensure_and_handle_channel_async (req2,
      NULL, ensure_and_handle_cb, test);

  /* Wait that the operation finished and the sig has been fired */
  test->count = 2;
  g_main_loop_run (test->mainloop);

  g_hash_table_unref (request);
  g_object_unref (req);
  g_object_unref (req2);
}

/* Request and forget tests */

static void
create_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  Test *test = user_data;

  tp_account_channel_request_create_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, &test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_forget_create_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_channel_async (req, "Fake", NULL, create_cb,
      test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

static void
ensure_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  Test *test = user_data;

  tp_account_channel_request_ensure_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, &test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_forget_ensure_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_ensure_channel_async (req, "Fake", NULL, ensure_cb,
      test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

/* ChannelDispatcher.CreateChannel() call fails */
static void
test_forget_create_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "CreateChannelFail", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_channel_async (req, "Fake", NULL, create_cb,
      test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest.Proceed() call fails */
static void
test_forget_proceed_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CD to fail */
  tp_asv_set_boolean (request, "ProceedFail", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_channel_async (req, "Fake", NULL, create_cb,
      test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* ChannelRequest fire the 'Failed' signal */
static void
test_forget_cr_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();

  /* Ask to the CR to fire the signal */
  tp_asv_set_boolean (request, "FireFailed", TRUE);

  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_channel_async (req, "Fake", NULL, create_cb,
      test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_assert (test->channel == NULL);
}

/* Cancel the operation before starting it */
static void
test_forget_cancel_before (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  g_cancellable_cancel (test->cancellable);

  tp_account_channel_request_create_channel_async (req, "Fake",
      test->cancellable, create_cb, test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
}

static void
test_forget_cancel_after_create (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;
  TpAccountChannelRequest *req;

  request = create_request ();
  req = tp_account_channel_request_new (test->account, request, 0);

  tp_account_channel_request_create_channel_async (req, "Fake",
      test->cancellable, create_cb, test);

  g_signal_connect (test->cd_service, "channel-request-created",
      G_CALLBACK (channel_request_created_cb), test);

  g_hash_table_unref (request);
  g_object_unref (req);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_CANCELLED);
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

  /* Request and handle tests */
  g_test_add ("/account-channels-request-handle/create-success", Test, NULL,
      setup, test_handle_create_success, teardown);
  g_test_add ("/account-channels-request-handle/create-fail", Test, NULL,
      setup, test_handle_create_fail, teardown);
  g_test_add ("/account-channels-request-handle/proceed-fail", Test, NULL,
      setup, test_handle_proceed_fail, teardown);
  g_test_add ("/account-channels-request-handle/cr-failed", Test, NULL,
      setup, test_handle_cr_failed, teardown);
  g_test_add ("/account-channels-request-handle/ensure-success", Test, NULL,
      setup, test_handle_ensure_success, teardown);
  g_test_add ("/account-channels-request-handle/cancel-before", Test, NULL,
      setup, test_handle_cancel_before, teardown);
  g_test_add ("/account-channels-request-handle/after-create", Test, NULL,
      setup, test_handle_cancel_after_create, teardown);
  g_test_add ("/account-channels-request-handle/re-handle", Test, NULL,
      setup, test_handle_re_handle, teardown);

  /* Request and handle tests */
  g_test_add ("/account-channels-request-forget/create-success", Test, NULL,
      setup, test_forget_create_success, teardown);
  g_test_add ("/account-channels-request-forget/create-fail", Test, NULL,
      setup, test_forget_create_fail, teardown);
  g_test_add ("/account-channels-request-foget/proceed-fail", Test, NULL,
      setup, test_forget_proceed_fail, teardown);
  g_test_add ("/account-channels-request-forget/cr-failed", Test, NULL,
      setup, test_forget_cr_failed, teardown);
  g_test_add ("/account-channels-request-forget/ensure-success", Test, NULL,
      setup, test_forget_ensure_success, teardown);
  g_test_add ("/account-channels-request-forget/cancel-before", Test, NULL,
      setup, test_forget_cancel_before, teardown);
  g_test_add ("/account-channels-request-forget/after-create", Test, NULL,
      setup, test_forget_cancel_after_create, teardown);

  return g_test_run ();
}
