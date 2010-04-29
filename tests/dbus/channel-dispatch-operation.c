/* A very basic feature test for TpChannelDispatchOperation
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

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
    SimpleChannelDispatchOperation *cdo_service;
    TestTextChannelNull *text_chan_service;

    TpChannelDispatchOperation *cdo;
    GError *error /* initialized where needed */;

    TpBaseConnection *base_connection;
    TpConnection *connection;
    TpChannel *text_chan;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  DBusConnection *libdbus;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = test_dbus_daemon_dup_or_die ();

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

  test->cdo_service = test_object_new_static_class (
      SIMPLE_TYPE_CHANNEL_DISPATCH_OPERATION,
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
  test_create_and_connect_conn (SIMPLE_TYPE_CONNECTION, "me@test.com",
      &test->base_connection, &test->connection);

  /* Create service-side text channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (contact_repo != NULL);

  handle = tp_handle_ensure (contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  test->text_chan_service = TEST_TEXT_CHANNEL_NULL (
      test_object_new_static_class (
        TEST_TYPE_TEXT_CHANNEL_NULL,
        "connection", test->base_connection,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  /* Create client-side text channel object */
  test->text_chan = tp_channel_new (test->connection, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  /* Configure fake ChannelDispatchOperation service */
  simple_channel_dispatch_operation_set_conn_path (test->cdo_service,
      tp_proxy_get_object_path (test->connection));

  simple_channel_dispatch_operation_add_channel (test->cdo_service,
      test->text_chan);

  tp_handle_unref (contact_repo, handle);
  g_free (chan_path);
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
  test_proxy_run_until_dbus_queue_processed (test->dbus);

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
  g_object_unref (test->text_chan_service);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

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

  test_proxy_run_until_dbus_queue_processed (test->cdo);

  g_assert (tp_proxy_get_invalidated (test->cdo) == NULL);

  dbus_connection_close (dbus_g_connection_get_connection (
        test->private_conn));
  dbus_g_connection_unref (test->private_conn);
  test->private_conn = NULL;

  test_proxy_run_until_dbus_queue_processed (test->cdo);

  g_assert (tp_proxy_get_invalidated (test->cdo) != NULL);
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

  test_proxy_run_until_dbus_queue_processed (test->cdo);

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
      "channel-dispatch-operation-properties", &immutable_props,
      NULL);

  /* connection */
  g_assert (conn != NULL);
  g_assert (TP_IS_CONNECTION (conn));
  g_assert (tp_channel_dispatch_operation_borrow_connection (test->cdo)
      == conn);
  g_assert (!tp_strdiff (tp_proxy_get_object_path (conn),
        tp_proxy_get_object_path (test->connection)));
  g_object_unref (conn);

  /* account */
  g_assert (account != NULL);
  g_assert (TP_IS_ACCOUNT (account));
  g_assert (tp_channel_dispatch_operation_borrow_account (test->cdo)
      == account);
  g_assert (!tp_strdiff (tp_proxy_get_object_path (account),
        ACCOUNT_PATH));
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
test_properties_passed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  static const char *interfaces[] = { NULL };
  GHashTable *props;
  gboolean ok;
  GPtrArray *channels;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

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
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cdo/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/cdo/crash", Test, NULL, setup, test_crash, teardown);
  g_test_add ("/cdo/finished", Test, NULL, setup, test_finished, teardown);
  g_test_add ("/cdo/properties-passed", Test, NULL, setup_services,
      test_properties_passed, teardown_services);

  return g_test_run ();
}
