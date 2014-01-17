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
#include <telepathy-glib/client-factory-internal.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel-dispatch-operation.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/simple-channel-dispatch-operation.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/echo-chan.h"
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
    TpTestsEchoChannel *text_chan_service;

    TpChannelDispatchOperation *cdo;
    GError *error /* initialized where needed */;

    TpBaseConnection *base_connection;
    TpConnection *connection;
    TpChannel *text_chan;

    guint sig;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  DBusConnection *libdbus;

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
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  /* Create service-side text channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (contact_repo != NULL);

  handle = tp_handle_ensure (contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  test->text_chan_service = TP_TESTS_ECHO_CHANNEL (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", test->base_connection,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  /* Create client-side text channel object */
  test->text_chan = tp_tests_channel_new (test->connection, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);

  /* Configure fake ChannelDispatchOperation service */
  tp_tests_simple_channel_dispatch_operation_set_conn_path (test->cdo_service,
      tp_proxy_get_object_path (test->connection));

  tp_tests_simple_channel_dispatch_operation_set_channel (test->cdo_service,
      test->text_chan);

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

  tp_tests_connection_assert_disconnect_succeeds (test->connection);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  teardown (test, data);
}

static TpChannelDispatchOperation *
dispatch_operation_new (TpDBusDaemon *bus_daemon,
    const gchar *object_path,
    GHashTable *immutable_properties,
    GError **error)
{
  TpChannelDispatchOperation *self;
  TpClientFactory *factory;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  if (immutable_properties == NULL)
    immutable_properties = tp_asv_new (NULL, NULL);
  else
    g_hash_table_ref (immutable_properties);

  factory = tp_client_factory_new (bus_daemon);
  self = _tp_client_factory_ensure_channel_dispatch_operation (factory,
      object_path, immutable_properties, error);

  g_object_unref (factory);
  g_hash_table_unref (immutable_properties);

  return self;
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  /* CD not running */
  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cdo == NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cdo = dispatch_operation_new (test->dbus,
      "not even syntactically valid", NULL, NULL);
  g_assert (test->cdo == NULL);

  test->cdo = dispatch_operation_new (test->dbus,
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

  test->cdo = dispatch_operation_new (test->dbus, "/whatever",
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

  test->cdo = dispatch_operation_new (test->dbus, "/whatever",
      NULL, NULL);
  g_assert (test->cdo != NULL);
  g_assert (tp_proxy_get_invalidated (test->cdo) == NULL);

  tp_svc_channel_dispatch_operation_emit_finished (test->cdo_service, "", "");

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
 * dispatch_operation_new() */
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
  g_assert (tp_channel_dispatch_operation_get_connection (test->cdo)
      == conn);
  g_assert_cmpstr (tp_proxy_get_object_path (conn), ==,
        tp_proxy_get_object_path (test->connection));
  g_object_unref (conn);

  /* account */
  g_assert (account != NULL);
  g_assert (TP_IS_ACCOUNT (account));
  g_assert (tp_channel_dispatch_operation_get_account (test->cdo)
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

  possible_handlers = tp_channel_dispatch_operation_get_possible_handlers (
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
  g_assert_cmpuint (g_hash_table_size (immutable_props), ==, 6);
  g_hash_table_unref (immutable_props);
}

static void
check_channel (Test *test)
{
  TpChannel *channel;

  channel = tp_channel_dispatch_operation_get_channel (test->cdo);
  g_assert (TP_IS_CHANNEL (channel));
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
        tp_proxy_get_object_path (test->text_chan));
}

static void
test_properties_passed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  static const char *interfaces[] = { NULL };
  GHashTable *props;
  TpChannel *channel;
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };
  GHashTable *chan_props;

  chan_props = tp_tests_dup_channel_props_asv (test->text_chan);

  props = tp_asv_new (
      TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
        G_TYPE_STRV, interfaces,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->connection),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
        DBUS_TYPE_G_OBJECT_PATH, ACCOUNT_PATH,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
        G_TYPE_STRV, POSSIBLE_HANDLERS,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->text_chan),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL_PROPERTIES,
        TP_HASH_TYPE_STRING_VARIANT_MAP, chan_props,
      NULL);

  g_hash_table_unref (chan_props);

  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", props, &test->error);
  g_assert_no_error (test->error);

  check_immutable_properties (test);

  g_object_get (test->cdo, "channel", &channel, NULL);

  g_assert (TP_IS_CHANNEL (test->text_chan));
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
      tp_proxy_get_object_path (test->text_chan));
  g_assert (tp_channel_dispatch_operation_get_channel (test->cdo) == channel);
  g_object_unref (channel);

  g_hash_table_unref (props);

  /* Prepare TpChannelDispatchOperation */
  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  /* Channels are now defined */
  check_immutable_properties (test);
  check_channel (test);
}

/* Don't pass immutable properties to dispatch_operation_new so
 * properties are fetched when preparing the core feature. */
static void
test_properties_fetched (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };

  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  /* Properties are not defined yet */
  g_assert (tp_channel_dispatch_operation_get_connection (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_get_account (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_get_channel (test->cdo)
      == NULL);
  g_assert (tp_channel_dispatch_operation_get_possible_handlers (test->cdo)
      == NULL);

  tp_proxy_prepare_async (test->cdo, features, features_prepared_cb, test);
  g_main_loop_run (test->mainloop);

  g_assert (tp_proxy_is_prepared (test->cdo,
        TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE));

  /* Immutable properties and Channels are now defined */
  check_immutable_properties (test);
  check_channel (test);
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
  test->cdo = dispatch_operation_new (test->dbus,
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
  test->cdo = dispatch_operation_new (test->dbus,
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

  tp_channel_dispatch_operation_close_channel_finish (
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
test_close_channel (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_close_channel_async (test->cdo,
      close_channels_cb, test);

  test->sig = 2;
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

static void
leave_channel_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_leave_channel_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_leave_channel (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_leave_channel_async (test->cdo,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY, "Busy right now",
      leave_channel_cb, test);

  test->sig = 2;
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

static void
destroy_channel_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_destroy_channel_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->sig--;
  if (test->sig == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_destroy_channel (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->cdo = dispatch_operation_new (test->dbus,
      "/whatever", NULL, &test->error);
  g_assert_no_error (test->error);

  tp_tests_proxy_run_until_prepared (test->cdo, NULL);

  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);

  tp_channel_dispatch_operation_destroy_channel_async (test->cdo,
      destroy_channel_cb, test);

  test->sig = 2;
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
  g_test_add ("/cdo/handle-with", Test, NULL, setup_services,
      test_handle_with, teardown_services);
  g_test_add ("/cdo/handle-with-time", Test, NULL, setup_services,
      test_handle_with_time, teardown_services);
  g_test_add ("/cdo/close-channel", Test, NULL, setup_services,
      test_close_channel, teardown_services);
  g_test_add ("/cdo/leave-channel", Test, NULL, setup_services,
      test_leave_channel, teardown_services);
  g_test_add ("/cdo/destroy-channel", Test, NULL, setup_services,
      test_destroy_channel, teardown_services);

  /* tp_channel_dispatch_operation_claim_with_async() is tested in
   * tests/dbus/base-client.c */

  return tp_tests_run_with_bus ();
}
