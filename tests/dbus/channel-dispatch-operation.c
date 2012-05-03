/* A very basic feature test for TpChannelDispatchOperation
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/channel-dispatch-operation.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/svc-channel-dispatch-operation.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/simple-channel-dispatch-operation.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"
#include "tests/lib/util.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fake/fake/fake"
static const gchar *POSSIBLE_HANDLERS[] = {
    TP_CLIENT_BUS_NAME_BASE ".Badger", NULL, };

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    DBusGConnection *private_conn;
    TpDBusDaemon *private_dbus;
    TpTestsSimpleChannelDispatchOperation *cdo_service;
    TpTestsTextChannelNull *text_chan_service;
    TpTestsTextChannelNull *text_chan_service_2;

    TpChannelDispatchOperation *cdo;
    GError *error /* initialized where needed */;

    TpBaseConnection *base_connection;
    TpConnection *connection;
    TpChannel *text_chan;
    TpChannel *text_chan_2;

    guint sig;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  DBusConnection *libdbus;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  libdbus = dbus_bus_get_private (DBUS_BUS_STARTER, NULL);
  g_assert (libdbus != NULL);
  dbus_connection_setup_with_g_main (libdbus, NULL);
  dbus_connection_set_exit_on_disconnect (libdbus, FALSE);
  test->private_conn = dbus_connection_get_g_connection (libdbus);
  /* transfer ref */
  dbus_g_connection_ref (test->private_conn);
  dbus_connection_unref (libdbus);
  g_assert (test->private_conn != NULL);
  test->private_dbus = tp_dbus_daemon_new (test->private_conn);
  g_assert (test->private_dbus != NULL);

  test->cdo = NULL;

  test->cdo_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCH_OPERATION,
      NULL);
  tp_dbus_daemon_register_object (test->private_dbus, "/whatever",
      test->cdo_service);
 }

static void
setup_services (Test *test,
    gconstpointer data)
{
  gchar *chan_path;
  TpHandle handle;
  TpHandleRepoIface *contact_repo;

  setup (test, data);

 /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  /* Create service-side text channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (contact_repo != NULL);

  handle = tp_handle_ensure (contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  test->text_chan_service = TP_TESTS_TEXT_CHANNEL_NULL (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_TEXT_CHANNEL_NULL,
        "connection", test->base_connection,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  /* Create client-side text channel object */
  test->text_chan = tp_channel_new (test->connection, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  tp_handle_unref (contact_repo, handle);
  g_free (chan_path);

  /* Create a second channel */
  chan_path = g_strdup_printf ("%s/Channel2",
      tp_proxy_get_object_path (test->connection));

  handle = tp_handle_ensure (contact_repo, "alice", NULL, &test->error);
  g_assert_no_error (test->error);

  test->text_chan_service_2 = TP_TESTS_TEXT_CHANNEL_NULL (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_TEXT_CHANNEL_NULL,
        "connection", test->base_connection,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  /* Create client-side text channel object */
  test->text_chan_2 = tp_channel_new (test->connection, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  tp_handle_unref (contact_repo, handle);
  g_free (chan_path);


  /* Configure fake ChannelDispatchOperation service */
  tp_tests_simple_channel_dispatch_operation_set_conn_path (test->cdo_service,
      tp_proxy_get_object_path (test->connection));

  tp_tests_simple_channel_dispatch_operation_add_channel (test->cdo_service,
      test->text_chan);

  tp_tests_simple_channel_dispatch_operation_add_channel (test->cdo_service,
      test->text_chan_2);

  tp_tests_simple_channel_dispatch_operation_set_account_path (test->cdo_service,
       ACCOUNT_PATH);

  g_assert (tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL));
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->cdo != NULL)
    {
      g_object_unref (test->cdo);
      test->cdo = NULL;
    }

  tp_dbus_daemon_release_name (test->dbus, TP_CHANNEL_DISPATCHER_BUS_NAME,
      NULL);

  if (test->private_dbus != NULL)
    {
      tp_dbus_daemon_release_name (test->private_dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

      g_object_unref (test->private_dbus);
      test->private_dbus = NULL;
    }

  g_object_unref (test->cdo_service);
  test->cdo_service = NULL;

  if (test->private_conn != NULL)
    {
      dbus_connection_close (dbus_g_connection_get_connection (
            test->private_conn));

      dbus_g_connection_unref (test->private_conn);
      test->private_conn = NULL;
    }

  /* make sure any pending things have happened */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
teardown_services (Test *test,
          gconstpointer data)
{
  g_object_unref (test->text_chan);
  if (test->text_chan_service != NULL)
    g_object_unref (test->text_chan_service);

  g_object_unref (test->text_chan_2);
  if (test->text_chan_service_2 != NULL)
    g_object_unref (test->text_chan_service_2);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  teardown (test, data);
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  /* CD not running */
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cdo == NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "not even syntactically valid", NULL, NULL);
  g_assert (test->cdo == NULL);

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cdo != NULL);
}

static void
test_crash (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cdo = tp_channel_dispatch_operation_new (test->dbus, "/whatever",
      NULL, NULL);
  g_assert (test->cdo != NULL);
  g_assert (tp_proxy_get_invalidated (test->cdo) == NULL);

  tp_dbus_daemon_release_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cdo);

  g_assert (tp_proxy_get_invalidated (test->cdo) == NULL);

  dbus_connection_close (dbus_g_connection_get_connection (
        test->private_conn));
  dbus_g_connection_unref (test->private_conn);
  test->private_conn = NULL;

  while (tp_proxy_get_invalidated (test->cdo) == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_get_invalidated (test->cdo)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cdo)->code ==
      TP_DBUS_ERROR_NAME_OWNER_LOST);
}

static void
test_finished (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cdo = tp_channel_dispatch_operation_new (test->dbus, "/whatever",
      NULL, NULL);
  g_assert (test->cdo != NULL);
  g_assert (tp_proxy_get_invalidated (test->cdo) == NULL);

  tp_svc_channel_dispatch_operation_emit_finished (test->cdo_service);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cdo);

  g_assert (tp_proxy_get_invalidated (test->cdo) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cdo)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cdo)->code ==
      TP_DBUS_ERROR_OBJECT_REMOVED);
}

static void
features_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (source, result, &test->error);
  g_assert_no_error (test->error);

  g_main_loop_quit (test->mainloop);
}

/* Test properties when passing the immutable properties to
 * tp_channel_dispatch_operation_new() */
static void
check_immutable_properties (Test *test)
{
  TpConnection *conn;
  TpAccount *account;
  GStrv possible_handlers;
  GHashTable *immutable_props;

  g_object_get (test->cdo,
      "connection", &conn,
      "account", &account,
      "possible-handlers", &possible_handlers,
      "cdo-properties", &immutable_props,
      NULL);

  /* connection */
  g_assert (conn != NULL);
  g_assert (TP_IS_CONNECTION (conn));
  g_assert (tp_channel_dispatch_operation_borrow_connection (test->cdo)
      == conn);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==,
        tp_proxy_get_object_path (test->connection));
  g_object_unref (conn);

  /* account */
  g_assert (account != NULL);
  g_assert (TP_IS_ACCOUNT (account));
  g_assert (tp_channel_dispatch_operation_borrow_account (test->cdo)
      == account);
  g_assert_cmpstr (tp_proxy_get_object_path (account), ==,
        ACCOUNT_PATH);
  g_object_unref (account);

  /* possible handlers */
  g_assert (possible_handlers != NULL);
  g_assert_cmpuint (g_strv_length (possible_handlers), ==, 1);
  g_assert (tp_strv_contains ((const gchar * const *) possible_handlers,
        POSSIBLE_HANDLERS[0]));
  g_strfreev (possible_handlers);

  possible_handlers = tp_channel_dispatch_operation_borrow_possible_handlers (
      test->cdo);
  g_assert_cmpuint (g_strv_length (possible_handlers), ==, 1);
  g_assert (tp_strv_contains ((const gchar * const *) possible_handlers,
        POSSIBLE_HANDLERS[0]));

  /* immutable properties */
  g_assert (tp_asv_get_object_path (immutable_props,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION) != NULL);
  g_assert (tp_asv_get_object_path (immutable_props,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT) != NULL);
  g_assert (tp_asv_get_strv (immutable_props,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS) != NULL);
  g_assert (tp_asv_get_strv (immutable_props,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES) != NULL);
  g_assert_cmpuint (g_hash_table_size (immutable_props), ==, 4);
  g_hash_table_unref (immutable_props);
  immutable_props = tp_channel_dispatch_operation_borrow_immutable_properties (
      test->cdo);
  g_assert_cmpuint (g_hash_table_size (immutable_props), ==, 4);
}

static void
check_channels (Test *test)
{
  GPtrArray *channels;
  TpChannel *channel;

  channels = tp_channel_dispatch_operation_borrow_channels (test->cdo);
  g_assert (channels != NULL);
  g_assert_cmpuint (channels->len, ==, 2);

  channel = g_ptr_array_index (channels, 0);
  g_assert (TP_IS_CHANNEL (channel));
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
        tp_proxy_get_object_path (test->text_chan));

  channel = g_ptr_array_index (channels, 1);
  g_assert (TP_IS_CHANNEL (channel));
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
        tp_proxy_get_object_path (test->text_chan_2));
}

static void
test_properties_passed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  static const char *interfaces[] = { NULL };
  GHashTable *props;
  GPtrArray *channels;
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };

  props = tp_asv_new (
      TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
        G_TYPE_STRV, interfaces,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->connection),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
        DBUS_TYPE_G_OBJECT_PATH, ACCOUNT_PATH,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
        G_TYPE_STRV, POSSIBLE_HANDLERS,
      NULL);

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", props, &test->error);
  g_assert_no_error (test->error);

  check_immutable_properties (test);

  g_object_get (test->cdo, "channels", &channels, NULL);

  /* Channels is not an immutable property so have to be fetched when
   * preparing the TpChannelDispatchOperation */
  g_assert (channels == NULL);
  g_assert (tp_channel_dispatch_operation_borrow_channels (test->cdo) == NULL);

  g_hash_table_unref (props);

  /* Prepare TpChannelDispatchOperation */
  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  /* Channels are now defined */
  check_immutable_properties (test);
  check_channels (test);
}

/* Don't pass immutable properties to tp_channel_dispatch_operation_new so
 * properties are fetched when preparing the core feature. */
static void
test_properties_fetched (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *props;
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  /* Properties are not defined yet */
  g_assert (tp_channel_dispatch_operation_borrow_connection (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_borrow_account (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_borrow_channels (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_borrow_possible_handlers (test->cdo)
      == NULL);
  props = tp_channel_dispatch_operation_borrow_immutable_properties (
        test->cdo);
  g_assert_cmpuint (g_hash_table_size (props), ==, 0);

  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  /* Immutable properties and Channels are now defined */
  check_immutable_properties (test);
  check_channels (test);
}

static void
channe_lost_cb (TpChannelDispatchOperation *cdo,
    TpChannel *channel,
    guint domain,
    gint code,
    gchar *message,
    Test *test)
{
  GError *error = g_error_new_literal (domain, code, message);

  if (test->text_chan_service_2 != NULL)
    {
      /* The second channel is still there so we removed the first one */
      g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
            tp_proxy_get_object_path (test->text_chan));
    }
  else
    {
      g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
            tp_proxy_get_object_path (test->text_chan_2));
    }

  g_assert_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);

  g_error_free (error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
invalidated_cb (TpProxy *self,
    guint domain,
    gint code,
    gchar *message,
    Test *test)
{
  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_channel_lost (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };
  GPtrArray *channels;
  TpChannel *channel;

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  check_channels (test);

  test->sig = 1;
  g_signal_connect (test->cdo, "channel-lost", G_CALLBACK (channe_lost_cb),
      test);

  /* First channel disappears and so is lost */
  tp_tests_text_channel_null_close (test->text_chan_service);

  g_object_unref (test->text_chan_service);
  test->text_chan_service = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan);
  g_main_loop_run (test->mainloop);

  channels = tp_channel_dispatch_operation_borrow_channels (test->cdo);
  g_assert (channels != NULL);
  /* Channel has  been removed */
  g_assert_cmpuint (channels->len, ==, 1);

  channel = g_ptr_array_index (channels, 0);
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
        tp_proxy_get_object_path (test->text_chan_2));
  /* Second channel disappears, Finished is emited and so the CDO is
   * invalidated */
  test->sig = 2;
  g_signal_connect (test->cdo, "invalidated", G_CALLBACK (invalidated_cb),
      test);

  tp_tests_text_channel_null_close (test->text_chan_service_2);

  g_object_unref (test->text_chan_service_2);
  test->text_chan_service_2 = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan_2);
  g_main_loop_run (test->mainloop);

  g_assert_cmpuint (channels->len, ==, 0);
}

static void
handle_with_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_handle_with_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_handle_with (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_channel_dispatch_operation_handle_with_async (test->cdo,
      NULL, handle_with_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);

  tp_channel_dispatch_operation_handle_with_async (test->cdo,
      "FAIL", handle_with_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);
}

static void
claim_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_claim_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_claim (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_channel_dispatch_operation_claim_async (test->cdo, claim_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);

  /* tp_channel_dispatch_operation_claim_with_async() is tested in
   * tests/dbus/base-client.c */
}

static void
test_channel_lost_preparing (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };
  GPtrArray *channels;
  TpChannel *channel;

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);

  /* First channel disappears while preparing */
  tp_tests_text_channel_null_close (test->text_chan_service);

  g_object_unref (test->text_chan_service);
  test->text_chan_service = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan);

  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  channels = tp_channel_dispatch_operation_borrow_channels (test->cdo);
  g_assert (channels != NULL);
  /* Channel has  been removed */
  g_assert_cmpuint (channels->len, ==, 1);

  channel = g_ptr_array_index (channels, 0);
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
      tp_proxy_get_object_path (test->text_chan_2));
}

static void
features_not_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (source, result, &test->error);
  g_assert_error (test->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED);
  g_clear_error (&test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_finished_preparing (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };
  GPtrArray *channels;

  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_proxy_prepare_async (test->cdo, features, features_not_prepared_cb, test);

  /* The 2 channels are lost while preparing */
  tp_tests_text_channel_null_close (test->text_chan_service);

  g_object_unref (test->text_chan_service);
  test->text_chan_service = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan);

  tp_tests_text_channel_null_close (test->text_chan_service_2);

  g_object_unref (test->text_chan_service_2);
  test->text_chan_service_2 = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan_2);

  g_main_loop_run (test->mainloop);

  g_assert (!tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  channels = tp_channel_dispatch_operation_borrow_channels (test->cdo);
  g_assert (channels == NULL);
}

static void
handle_with_time_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_handle_with_time_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  g_main_loop_quit (test->mainloop);
}

static void
test_handle_with_time (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_channel_dispatch_operation_handle_with_time_async (test->cdo,
      NULL, 666, handle_with_time_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

static void
close_channels_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_close_channels_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
channel_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  Test *test = user_data;

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_close_channels (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);
  g_signal_connect (test->text_chan_2, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_close_channels_async (test->cdo,
      close_channels_cb, test);

  test->sig = 3;
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

static void
leave_channels_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_leave_channels_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_leave_channels (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);
  g_signal_connect (test->text_chan_2, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_leave_channels_async (test->cdo,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY, "Busy right now",
      leave_channels_cb, test);

  test->sig = 3;
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

static void
destroy_channels_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_destroy_channels_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_destroy_channels (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = tp_channel_dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);
  g_signal_connect (test->text_chan_2, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_destroy_channels_async (test->cdo,
      destroy_channels_cb, test);

  test->sig = 3;
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cdo/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/cdo/crash", Test, NULL, setup, test_crash, teardown);
  g_test_add ("/cdo/finished", Test, NULL, setup, test_finished, teardown);
  g_test_add ("/cdo/properties-passed", Test, NULL, setup_services,
      test_properties_passed, teardown_services);
  g_test_add ("/cdo/properties-fetched", Test, NULL, setup_services,
      test_properties_fetched, teardown_services);
  g_test_add ("/cdo/channel-lost", Test, NULL, setup_services,
      test_channel_lost, teardown_services);
  g_test_add ("/cdo/handle-with", Test, NULL, setup_services,
      test_handle_with, teardown_services);
  g_test_add ("/cdo/claim", Test, NULL, setup_services,
      test_claim, teardown_services);
  g_test_add ("/cdo/channel-lost-preparing", Test, NULL, setup_services,
      test_channel_lost_preparing, teardown_services);
  g_test_add ("/cdo/finished--preparing", Test, NULL, setup_services,
      test_finished_preparing, teardown_services);
  g_test_add ("/cdo/handle-with-time", Test, NULL, setup_services,
      test_handle_with_time, teardown_services);
  g_test_add ("/cdo/close-channels", Test, NULL, setup_services,
      test_close_channels, teardown_services);
  g_test_add ("/cdo/leave-channels", Test, NULL, setup_services,
      test_leave_channels, teardown_services);
  g_test_add ("/cdo/destroy-channels", Test, NULL, setup_services,
      test_destroy_channels, teardown_services);

  return g_test_run ();
}
