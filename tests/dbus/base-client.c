/* Tests of TpBaseClient
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

/* We include -internal headers of context to be able to easily access to
 * their semi-private attributes (connection, account, channels, etc). */
#include <telepathy-glib/add-dispatch-operation-context-internal.h>
#include <telepathy-glib/base-client.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/handle-channel-context-internal.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/observe-channel-context-internal.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/simple-channel-dispatch-operation.h"
#include "tests/lib/simple-channel-dispatcher.h"
#include "tests/lib/simple-channel-request.h"
#include "tests/lib/simple-client.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/echo-chan.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseClient *base_client;
    TpTestsSimpleClient *simple_client;
    TpBaseConnection *base_connection;
    TpTestsSimpleAccount *account_service;
    TpTestsEchoChannel *text_chan_service;
    TpTestsEchoChannel *text_chan_service_2;
    TpTestsSimpleChannelDispatchOperation *cdo_service;
    TpTestsSimpleChannelDispatcher *cd_service;

    /* Client side objects */
    TpClientFactory *factory;
    TpClient *client;
    TpConnection *connection;
    TpAccount *account;
    TpChannel *text_chan;
    TpChannel *text_chan_2;

    GError *error /* initialized where needed */;
    GStrv interfaces;
    gint wait;

    GPtrArray *delegated;
    GHashTable *not_delegated;
    guint nb_delegate_cb;
} Test;

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"
#define CDO_PATH "/whatever"

static void
setup (Test *test,
       gconstpointer data)
{
  gchar *chan_path;
  TpHandle handle;
  TpHandleRepoIface *contact_repo;
  GError *error = NULL;

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;
  test->interfaces = NULL;
  test->nb_delegate_cb = 0;

  test->factory = tp_client_factory_new (test->dbus);
  g_assert (test->factory != NULL);

  /* Claim AccountManager bus-name (needed as we're going to export an Account
   * object). */
  tp_dbus_daemon_request_name (test->dbus,
          TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  /* Create service-side Client object */
  test->simple_client = tp_tests_simple_client_new (test->factory,
      "Test", FALSE);
  g_assert (test->simple_client != NULL);
  test->base_client = TP_BASE_CLIENT (test->simple_client);

  /* Create service-side Account object */
  test->account_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
  tp_dbus_daemon_register_object (test->dbus, ACCOUNT_PATH,
      test->account_service);

  /* Create client-side Client object */
  test->client = tp_tests_object_new_static_class (TP_TYPE_CLIENT,
          "dbus-daemon", test->dbus,
          "bus-name", tp_base_client_get_bus_name (test->base_client),
          "object-path", tp_base_client_get_object_path (test->base_client),
          NULL);

  g_assert (test->client != NULL);

  /* Create client-side Account object */
  test->account = tp_client_factory_ensure_account (test->factory,
      ACCOUNT_PATH, NULL, &error);
  g_assert_no_error (error);
  g_assert (test->account != NULL);

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

  /* Create a second channel */
  chan_path = g_strdup_printf ("%s/Channel2",
      tp_proxy_get_object_path (test->connection));

  handle = tp_handle_ensure (contact_repo, "alice", NULL, &test->error);
  g_assert_no_error (test->error);

  test->text_chan_service_2 = TP_TESTS_ECHO_CHANNEL (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", test->base_connection,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  /* Create client-side text channel object */
  test->text_chan_2 = tp_tests_channel_new (test->connection, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);

  /* Create Service side ChannelDispatchOperation object */
  test->cdo_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCH_OPERATION,
      NULL);
  tp_dbus_daemon_register_object (test->dbus, CDO_PATH, test->cdo_service);

  tp_tests_simple_channel_dispatch_operation_set_conn_path (test->cdo_service,
      tp_proxy_get_object_path (test->connection));

  tp_tests_simple_channel_dispatch_operation_set_account_path (
      test->cdo_service, tp_proxy_get_object_path (test->account));

  tp_tests_simple_channel_dispatch_operation_set_channel (test->cdo_service,
      test->text_chan);

  g_assert (tp_dbus_daemon_request_name (test->dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL));

  /* Create and register CD */
  test->cd_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCHER,
      "connection", test->base_connection,
      NULL);

  tp_dbus_daemon_register_object (test->dbus, TP_CHANNEL_DISPATCHER_OBJECT_PATH,
      test->cd_service);
}

static void
teardown_channel_invalidated_cb (TpChannel *self,
  guint domain,
  gint code,
  gchar *message,
  Test *test)
{
  g_main_loop_quit (test->mainloop);
}

static void
teardown_run_close_channel (Test *test, TpChannel *channel)
{
  if (channel != NULL && tp_proxy_get_invalidated (channel) == NULL)
    {
      g_signal_connect (channel, "invalidated",
          G_CALLBACK (teardown_channel_invalidated_cb), test);
      tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
      g_main_loop_run (test->mainloop);
    }
}

static void
teardown (Test *test,
          gconstpointer data)
{
  teardown_run_close_channel (test, test->text_chan);
  teardown_run_close_channel (test, test->text_chan_2);

  g_clear_error (&test->error);

  g_strfreev (test->interfaces);

  g_object_unref (test->factory);

  tp_dbus_daemon_release_name (test->dbus, TP_CHANNEL_DISPATCHER_BUS_NAME,
      NULL);

  g_object_unref (test->base_client);
  g_object_unref (test->client);

  tp_dbus_daemon_unregister_object (test->dbus, test->account_service);
  g_object_unref (test->account_service);

  tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  g_object_unref (test->account);

  if (test->text_chan_service != NULL)
    g_object_unref (test->text_chan_service);
  g_object_unref (test->text_chan);

  if (test->text_chan_service_2 != NULL)
    g_object_unref (test->text_chan_service_2);
  g_object_unref (test->text_chan_2);

  tp_clear_object (&test->cdo_service);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->cd_service);

  tp_clear_pointer (&test->delegated, g_ptr_array_unref);
  tp_clear_pointer (&test->not_delegated, g_hash_table_unref);
}

/* Test Basis */

static void
test_basics (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpClientFactory *factory;
  TpDBusDaemon *dbus;
  gchar *name;
  gboolean unique;

  g_object_get (test->base_client,
      "factory", &factory,
      "dbus-daemon", &dbus,
      "name", &name,
      "uniquify-name", &unique,
      NULL);

  g_assert (test->factory == factory);
  g_assert (test->dbus == dbus);
  g_assert_cmpstr ("Test", ==, name);
  g_assert (!unique);

  g_assert (test->dbus == tp_base_client_get_dbus_daemon (test->base_client));
  g_assert_cmpstr ("Test", ==, tp_base_client_get_name (test->base_client));
  g_assert (!tp_base_client_get_uniquify_name (test->base_client));

  g_object_unref (dbus);
  g_free (name);
}

/* Test register */

static void
get_client_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 1);

  g_strfreev (test->interfaces);
  test->interfaces = g_strdupv ((GStrv) tp_asv_get_strv (
        properties, "Interfaces"));

out:
  g_main_loop_quit (test->mainloop);
}

static void
test_register (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_base_client_be_a_handler (test->base_client);

  /* no-op as the client is not registered yet */
  tp_base_client_unregister (test->base_client);

  /* Client is not registered yet */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_error (test->error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN);
  g_error_free (test->error);
  test->error = NULL;

  /* register the client */
  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);

  /* unregister the client */
  tp_base_client_unregister (test->base_client);
  tp_tests_proxy_run_until_dbus_queue_processed (test->client);

  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_error (test->error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN);
  g_error_free (test->error);
  test->error = NULL;

  /* re-register the client */
  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

/* Test Observer */
static void
check_filters (GPtrArray *filters)
{
  GHashTable *filter;

  g_assert (filters != NULL);
  g_assert_cmpuint (filters->len, ==, 2);

  filter = g_ptr_array_index (filters, 0);
  g_assert_cmpuint (g_hash_table_size (filter), ==, 1);
  g_assert_cmpstr (tp_asv_get_string (filter, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);

  filter = g_ptr_array_index (filters, 1);
  g_assert_cmpuint (g_hash_table_size (filter), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (filter, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1);
  g_assert_cmpuint (tp_asv_get_uint32 (filter,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, NULL), ==, TP_HANDLE_TYPE_CONTACT);
}

static void
get_observer_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  GPtrArray *filters;
  gboolean recover, delay;
  gboolean valid;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 3);

  filters = tp_asv_get_boxed (properties, "ObserverChannelFilter",
      TP_ARRAY_TYPE_CHANNEL_CLASS_LIST);
  check_filters (filters);

  recover = tp_asv_get_boolean (properties, "Recover", &valid);
  g_assert (valid);
  g_assert (recover);

  delay = tp_asv_get_boolean (properties, "DelayApprovers", &valid);
  g_assert (valid);
  g_assert (delay);

out:
  g_main_loop_quit (test->mainloop);
}

static void
no_return_cb (TpClient *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;

  g_clear_error (&test->error);

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

out:
  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_observer (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *filter, *chan_props;
  GPtrArray *requests_satisified;
  GHashTable *info;
  TpChannel *chan;

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_observer_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_observer_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  tp_base_client_set_observer_recover (test->base_client, TRUE);
  tp_base_client_set_observer_delay_approvers (test->base_client, TRUE);

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
  g_assert_cmpint (g_strv_length (test->interfaces), ==, 1);
  g_assert (tp_strv_contains ((const gchar * const *) test->interfaces,
        TP_IFACE_CLIENT_OBSERVER));

  /* Check Observer properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_OBSERVER, get_observer_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);

  /* Call ObserveChannels */
  chan_props = tp_tests_dup_channel_props_asv (test->text_chan);

  requests_satisified = g_ptr_array_sized_new (0);
  info = tp_asv_new (
      "recovering", G_TYPE_BOOLEAN, TRUE,
      NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_OBSERVER);

  tp_cli_client_observer_call_observe_channel (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      tp_proxy_get_object_path (test->text_chan), chan_props,
      "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->observe_ctx != NULL);
  g_assert (tp_observe_channel_context_is_recovering (
        test->simple_client->observe_ctx));

  g_assert (test->simple_client->observe_ctx->account == test->account);

  /* Now call it with an invalid argument */
  tp_asv_set_boolean (info, "FAIL", TRUE);

  tp_cli_client_observer_call_observe_channel (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      tp_proxy_get_object_path (test->text_chan), chan_props,
      "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  /* The channel being observed is invalidated while preparing */
  g_hash_table_remove (info, "FAIL");

  tp_cli_client_observer_call_observe_channel (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      tp_proxy_get_object_path (test->text_chan), chan_props,
      "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  tp_base_channel_close ((TpBaseChannel *) test->text_chan_service);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  chan = test->simple_client->observe_ctx->channel;
  g_assert (TP_IS_CHANNEL (chan));
  g_assert (tp_proxy_get_invalidated (chan) != NULL);

  g_ptr_array_unref (requests_satisified);
  g_hash_table_unref (info);
  g_hash_table_unref (chan_props);
}

/* Test Approver */
static void
get_approver_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  GPtrArray *filters;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 1);

  filters = tp_asv_get_boxed (properties, "ApproverChannelFilter",
      TP_ARRAY_TYPE_CHANNEL_CLASS_LIST);
  check_filters (filters);

out:
  g_main_loop_quit (test->mainloop);
}

static void
test_approver (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *filter;
  GHashTable *chan_props;
  TpChannel *chan;
  GHashTable *properties;
  static const char *interfaces[] = { NULL };
  static const gchar *possible_handlers[] = {
    TP_CLIENT_BUS_NAME_BASE ".Badger", NULL, };

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_approver_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_approver_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
  g_assert_cmpint (g_strv_length (test->interfaces), ==, 1);
  g_assert (tp_strv_contains ((const gchar * const *) test->interfaces,
        TP_IFACE_CLIENT_APPROVER));

  /* Check Approver properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_APPROVER, get_approver_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Call AddDispatchOperation */
  chan_props = tp_tests_dup_channel_props_asv (test->text_chan);

  properties = tp_asv_new (
      TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
        G_TYPE_STRV, interfaces,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->connection),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->account),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
        G_TYPE_STRV, possible_handlers,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL, DBUS_TYPE_G_OBJECT_PATH,
        tp_proxy_get_object_path (test->text_chan),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL_PROPERTIES,
        TP_HASH_TYPE_STRING_VARIANT_MAP, chan_props,
      NULL);

  g_hash_table_unref (chan_props);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_APPROVER);

  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      CDO_PATH, properties, no_return_cb, test, NULL, NULL);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->add_dispatch_ctx != NULL);
  chan = tp_channel_dispatch_operation_get_channel (
      test->simple_client->add_dispatch_ctx->dispatch_operation);
  g_assert (TP_IS_CHANNEL (chan));
  g_assert_cmpstr (tp_proxy_get_object_path (chan), ==,
      tp_proxy_get_object_path (test->text_chan));

  /* Another call to AddDispatchOperation, the last channel will be
   * invalidated during the call */
  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      CDO_PATH, properties, no_return_cb, test, NULL, NULL);

  tp_base_channel_close ((TpBaseChannel *) test->text_chan_service);
  g_object_unref (test->text_chan_service);
  test->text_chan_service = NULL;

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_hash_table_unref (properties);
}

/* Test Handler */
static void
get_handler_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  GPtrArray *filters;
  gboolean bypass;
  gboolean valid;
  const gchar * const * capabilities;
  GPtrArray *handled;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 4);

  filters = tp_asv_get_boxed (properties, "HandlerChannelFilter",
      TP_ARRAY_TYPE_CHANNEL_CLASS_LIST);
  check_filters (filters);

  bypass = tp_asv_get_boolean (properties, "BypassApproval", &valid);
  g_assert (valid);
  g_assert (bypass);

  capabilities = tp_asv_get_strv (properties, "Capabilities");
  g_assert_cmpint (g_strv_length ((GStrv) capabilities), ==, 5);
  g_assert (tp_strv_contains (capabilities, "badger"));
  g_assert (tp_strv_contains (capabilities, "mushroom"));
  g_assert (tp_strv_contains (capabilities, "snake"));
  g_assert (tp_strv_contains (capabilities, "goat"));
  g_assert (tp_strv_contains (capabilities, "pony"));

  handled = tp_asv_get_boxed (properties, "HandledChannels",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  g_assert (handled != NULL);
  g_assert_cmpint (handled->len, ==, 0);

out:
  g_main_loop_quit (test->mainloop);
}

static void
channel_invalidated_cb (TpChannel *channel,
    guint domain,
    gint code,
    gchar *message,
    Test *test)
{
  g_main_loop_quit (test->mainloop);
}

static void
call_handle_channels (Test *test,
    TpChannel *channel,
    GPtrArray *requests_satisified,
    GHashTable *info)
{
  GHashTable *chan_props;

  if (requests_satisified == NULL)
    requests_satisified = g_ptr_array_sized_new (0);
  else
    g_ptr_array_ref (requests_satisified);

  if (info == NULL)
    info = g_hash_table_new (NULL, NULL);
  else
    g_hash_table_ref (info);

  chan_props = tp_tests_dup_channel_props_asv (channel);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_HANDLER);

  tp_cli_client_handler_call_handle_channel (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      tp_proxy_get_object_path (channel), chan_props,
      requests_satisified, 0, info,
      no_return_cb, test, NULL, NULL);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_ptr_array_unref (requests_satisified);
  g_hash_table_unref (info);
  g_hash_table_unref (chan_props);
}

static void
test_handler (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *filter;
  const gchar *caps[] = { "mushroom", "snake", NULL };
  GList *chans;
  TpTestsSimpleClient *client_2;

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_handler_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_handler_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  tp_base_client_set_handler_bypass_approval (test->base_client, TRUE);

  tp_base_client_add_handler_capability (test->base_client, "badger");
  tp_base_client_add_handler_capabilities (test->base_client, caps);
  tp_base_client_add_handler_capabilities_varargs (test->base_client,
      "goat", "pony", NULL);

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
  g_assert_cmpint (g_strv_length (test->interfaces), ==, 1);
  g_assert (tp_strv_contains ((const gchar * const *) test->interfaces,
        TP_IFACE_CLIENT_HANDLER));

  /* Check Handler properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_HANDLER, get_handler_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  call_handle_channels (test, test->text_chan, NULL, NULL);
  call_handle_channels (test, test->text_chan_2, NULL, NULL);

  g_assert (test->simple_client->handle_channel_ctx != NULL);
  g_assert (test->simple_client->handle_channel_ctx->account == test->account);

  chans = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 2);
  g_list_free_full (chans, g_object_unref);

  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  /* One of the channel is closed */
  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);
  tp_base_channel_close ((TpBaseChannel *) test->text_chan_service);
  g_main_loop_run (test->mainloop);

  chans = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free_full (chans, g_object_unref);

  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  /* Create another client sharing the same unique name */
  client_2 = tp_tests_simple_client_new (NULL, "Test", TRUE);
  tp_base_client_be_a_handler (TP_BASE_CLIENT (client_2));
  tp_base_client_register (TP_BASE_CLIENT (client_2), &test->error);
  g_assert_no_error (test->error);

  chans = tp_base_client_dup_handled_channels (TP_BASE_CLIENT (client_2));
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free_full (chans, g_object_unref);

  g_assert (!tp_base_client_is_handling_channel (TP_BASE_CLIENT (client_2),
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (TP_BASE_CLIENT (client_2),
        test->text_chan_2));

  g_object_unref (client_2);
}

/* Test Requests interface on Handler */
static void
get_requests_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 0);

out:
  g_main_loop_quit (test->mainloop);
}

static void
request_added_cb (TpBaseClient *client,
    TpAccount *account,
    TpChannelRequest *request,
    Test *test)
{
  GList *requests;

  g_assert (TP_IS_CHANNEL_REQUEST (request));
  g_assert (TP_IS_ACCOUNT (account));
  g_assert (tp_proxy_is_prepared (account, TP_ACCOUNT_FEATURE_CORE));

  requests = tp_base_client_dup_pending_requests (test->base_client);
  g_assert_cmpuint (g_list_length ((GList *) requests), ==, 1);
  g_assert (requests->data == request);
  g_list_free_full (requests, g_object_unref);

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
request_removed_cb (TpBaseClient *client,
    TpChannelRequest *request,
    const gchar *error,
    const gchar *reason,
    Test *test)
{
  g_assert (TP_IS_CHANNEL_REQUEST (request));

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_handler_requests (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties;
  GPtrArray *requests_satisified;
  GHashTable *request_props;
  GHashTable *info;
  TpChannelRequest *request;
  GList *requests;

  tp_base_client_take_handler_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  tp_base_client_set_handler_request_notification (test->base_client);

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
  g_assert_cmpint (g_strv_length (test->interfaces), ==, 2);
  g_assert (tp_strv_contains ((const gchar * const *) test->interfaces,
        TP_IFACE_CLIENT_HANDLER));
  g_assert (tp_strv_contains ((const gchar * const *) test->interfaces,
        TP_IFACE_CLIENT_INTERFACE_REQUESTS));

  /* Check Requests properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_INTERFACE_REQUESTS, get_requests_prop_cb,
      test, NULL, NULL);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_base_client_dup_pending_requests (test->base_client) == NULL);

  /* Call AddRequest */
  properties = tp_asv_new (
      TP_PROP_CHANNEL_REQUEST_ACCOUNT, DBUS_TYPE_G_OBJECT_PATH, ACCOUNT_PATH,
      NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_INTERFACE_REQUESTS);

  g_signal_connect (test->base_client, "request-added",
      G_CALLBACK (request_added_cb), test);

  tp_cli_client_interface_requests_call_add_request (test->client, -1,
      "/Request", properties,
      no_return_cb, test, NULL, NULL);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  requests = tp_base_client_dup_pending_requests (test->base_client);
  g_assert (requests != NULL);
  g_list_free_full (requests, g_object_unref);

  /* Call HandleChannel */
  requests_satisified = g_ptr_array_sized_new (1);
  g_ptr_array_add (requests_satisified, "/Request");

  request_props = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (request_props, "/Request", properties);
  info = tp_asv_new (
      "request-properties", TP_HASH_TYPE_OBJECT_IMMUTABLE_PROPERTIES_MAP,
          request_props,
      NULL);

  call_handle_channels (test, test->text_chan, requests_satisified, info);

  g_assert (test->simple_client->handle_channel_ctx != NULL);
  g_assert_cmpint (
      test->simple_client->handle_channel_ctx->requests_satisfied->len, ==, 1);
  request = g_ptr_array_index (
      test->simple_client->handle_channel_ctx->requests_satisfied, 0);
  requests = tp_base_client_dup_pending_requests (test->base_client);
  g_assert (requests->data == request);
  g_list_free_full (requests, g_object_unref);

  /* Call RemoveRequest */
  g_signal_connect (test->base_client, "request-removed",
      G_CALLBACK (request_removed_cb), test);

  tp_cli_client_interface_requests_call_remove_request (test->client, -1,
      "/Request", "Badger", "snake",
      no_return_cb, test, NULL, NULL);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_base_client_dup_pending_requests (test->base_client) == NULL);

  g_ptr_array_unref (requests_satisified);
  g_hash_table_unref (info);
  g_hash_table_unref (request_props);
}

static void
claim_with_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatch_operation_claim_with_finish (
      TP_CHANNEL_DISPATCH_OPERATION (source), result, &test->error);

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
cdo_finished_cb (TpTestsSimpleChannelDispatchOperation *cdo,
    const gchar *dbus_error,
    const gchar *message,
    Test *test)
{
  tp_clear_object (&test->cdo_service);
}

static void
test_channel_dispatch_operation_claim_with_async (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties, *chan_props;
  static const char *interfaces[] = { NULL };
  static const gchar *possible_handlers[] = {
    TP_CLIENT_BUS_NAME_BASE ".Badger", NULL, };
  TpChannelDispatchOperation *cdo;
  GList *handled;

  /* Register an Approver and Handler */
  tp_base_client_take_approver_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        NULL));

  tp_base_client_take_handler_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        NULL));

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  /* Call AddDispatchOperation */
  chan_props = tp_tests_dup_channel_props_asv (test->text_chan);

  properties = tp_asv_new (
      TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
        G_TYPE_STRV, interfaces,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->connection),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->account),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
        G_TYPE_STRV, possible_handlers,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL, DBUS_TYPE_G_OBJECT_PATH,
        tp_proxy_get_object_path (test->text_chan),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CHANNEL_PROPERTIES,
        TP_HASH_TYPE_STRING_VARIANT_MAP, chan_props,
      NULL);

  g_hash_table_unref (chan_props);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_APPROVER);

  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      CDO_PATH, properties, no_return_cb, test, NULL, NULL);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  cdo = test->simple_client->add_dispatch_ctx->dispatch_operation;
  g_assert (TP_IS_CHANNEL_DISPATCH_OPERATION (cdo));

  handled = tp_base_client_dup_handled_channels (test->base_client);
  g_assert (handled == NULL);

  /* Connect to CDO's Finished signal so we can remove it from the bus when
   * it's claimed as MC would do. */
  g_signal_connect (test->cdo_service, "finished",
      G_CALLBACK (cdo_finished_cb), test);

  /* Claim the CDO, as the client is also a Handler, it is now handling the
   * channel */
  tp_channel_dispatch_operation_claim_with_async (cdo, test->base_client,
      claim_with_cb, test);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  handled = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (handled), ==, 1);
  g_list_free_full (handled, g_object_unref);

  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  g_hash_table_unref (properties);
}

static void
delegate_channels_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_base_client_delegate_channels_finish (
      TP_BASE_CLIENT (source), result, &test->delegated, &test->not_delegated,
      &test->error);

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_delegate_channels (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GList *chans;
  GError *error = NULL;

  tp_base_client_be_a_handler (test->base_client);

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  call_handle_channels (test, test->text_chan, NULL, NULL);
  call_handle_channels (test, test->text_chan_2, NULL, NULL);

  /* The client is handling the 2 channels */
  chans = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 2);
  g_list_free_full (chans, g_object_unref);

  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  /* Try to delegate the first one */
  chans = g_list_append (NULL, test->text_chan);

  tp_base_client_delegate_channels_async (test->base_client,
      chans, TP_USER_ACTION_TIME_CURRENT_TIME, NULL,
      delegate_channels_cb, test);

  g_list_free (chans);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->delegated->len, ==, 1);
  g_assert (g_ptr_array_index (test->delegated, 0) == test->text_chan);
  g_assert_cmpuint (g_hash_table_size (test->not_delegated), ==, 0);

  /* Client is not handling the channel any more */
  chans = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free_full (chans, g_object_unref);

  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  /* Try delegating the second channel, but MC refuses */
  test->cd_service->refuse_delegate = TRUE;

  chans = g_list_append (NULL, test->text_chan_2);

  tp_base_client_delegate_channels_async (test->base_client,
      chans, TP_USER_ACTION_TIME_CURRENT_TIME, NULL,
      delegate_channels_cb, test);

  g_list_free (chans);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->delegated->len, ==, 0);
  g_assert_cmpuint (g_hash_table_size (test->not_delegated), ==, 1);
  error = g_hash_table_lookup (test->not_delegated, test->text_chan_2);
  g_assert_error (error, TP_ERROR, TP_ERROR_BUSY);

  /* Client is still handling the channel */
  chans = tp_base_client_dup_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free_full (chans, g_object_unref);

  g_assert (!tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));
}

static void
present_channel_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_dispatcher_present_channel_finish (
      TP_CHANNEL_DISPATCHER (source), result, &test->error);

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_present_channel (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpChannelDispatcher *cd;

  cd = tp_channel_dispatcher_new (test->dbus);

  tp_channel_dispatcher_present_channel_async (cd, test->text_chan,
      TP_USER_ACTION_TIME_CURRENT_TIME, present_channel_cb, test);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_object_unref (cd);
}

#define PREFERRED_HANDLER_NAME TP_CLIENT_BUS_NAME_BASE ".Badger"

static gboolean
channel_in_array (GPtrArray *array,
    TpChannel *channel)
{
  guint i;

  for (i = 0; i < array->len; i++)
    {
      TpChannel *c = g_ptr_array_index (array, i);

      if (!tp_strdiff (tp_proxy_get_object_path (channel),
            tp_proxy_get_object_path (c)))
        return TRUE;
    }

  return FALSE;
}

static void
delegated_channels_cb (TpBaseClient *client,
    GPtrArray *channels,
    gpointer user_data)
{
  Test *test = user_data;

  g_assert_cmpuint (channels->len, ==, 1);

  if (test->nb_delegate_cb == 0)
    g_assert (channel_in_array (channels, test->text_chan));
  else if (test->nb_delegate_cb == 1)
    g_assert (channel_in_array (channels, test->text_chan_2));
  else
    g_assert_not_reached ();

  test->nb_delegate_cb++;

  test->wait--;
  if (test->wait == 0)
    g_main_loop_quit (test->mainloop);
}

static void
delegate_to_preferred_handler (Test *test,
    gboolean supported)
{
  GPtrArray *requests_satisfied;
  GPtrArray *requests;
  GHashTable *request_props;
  GHashTable *info;
  TpTestsSimpleChannelRequest *cr;
  GHashTable *hints;

  tp_base_client_be_a_handler (test->base_client);

  if (supported)
    {
      tp_base_client_set_delegated_channels_callback (test->base_client,
          delegated_channels_cb, test, NULL);
    }

  tp_base_client_register (test->base_client, &test->error);
  g_assert_no_error (test->error);

  call_handle_channels (test, test->text_chan, NULL, NULL);
  call_handle_channels (test, test->text_chan_2, NULL, NULL);

  /* The client is handling the 2 channels */
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan));
  g_assert (tp_base_client_is_handling_channel (test->base_client,
        test->text_chan_2));

  /* Another client asks to dispatch the channel to it */
  requests = g_ptr_array_new ();

  hints = tp_asv_new (
      "im.telepathy.v1.ChannelRequest.DelegateToPreferredHandler",
        G_TYPE_BOOLEAN, TRUE,
      NULL);

  cr = tp_tests_simple_channel_request_new ("/CR",
      TP_TESTS_SIMPLE_CONNECTION (test->base_connection), ACCOUNT_PATH,
      TP_USER_ACTION_TIME_CURRENT_TIME, PREFERRED_HANDLER_NAME,
      requests, hints);

  requests_satisfied = g_ptr_array_sized_new (0);
  g_ptr_array_add (requests_satisfied, "/CR");

  request_props = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) g_hash_table_unref);

  g_hash_table_insert (request_props, "/CR",
      tp_tests_simple_channel_request_dup_immutable_props (cr));

  info = g_hash_table_new (NULL, NULL);
  tp_asv_set_boxed (info,
      "request-properties", TP_HASH_TYPE_OBJECT_IMMUTABLE_PROPERTIES_MAP,
      request_props);

  /* If we support the DelegateToPreferredHandler hint, we wait for
   * delegated_channels_cb to be called */
  if (supported)
    test->wait++;
  call_handle_channels (test, test->text_chan, requests_satisfied, info);

  if (supported)
    test->wait++;
  call_handle_channels (test, test->text_chan_2, requests_satisfied, info);

  g_assert_no_error (test->error);

  if (supported)
    {
      /* We are not handling the channels any more */
      g_assert (!tp_base_client_is_handling_channel (test->base_client,
            test->text_chan));
      g_assert (!tp_base_client_is_handling_channel (test->base_client,
            test->text_chan_2));
    }
  else
    {
      /* We are still handling the channels */
      g_assert (tp_base_client_is_handling_channel (test->base_client,
            test->text_chan));
      g_assert (tp_base_client_is_handling_channel (test->base_client,
            test->text_chan_2));
    }

  tp_base_client_unregister (test->base_client);

  g_object_unref (cr);
  g_ptr_array_unref (requests_satisfied);
  g_ptr_array_unref (requests);
  g_hash_table_unref (hints);
  g_hash_table_unref (request_props);
}

static void
test_delegate_to_preferred_handler_not_supported (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  delegate_to_preferred_handler (test, FALSE);
}

static void
test_delegate_to_preferred_handler_supported (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  delegate_to_preferred_handler (test, TRUE);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/base-client/basics", Test, NULL, setup, test_basics, teardown);
  g_test_add ("/base-client/register", Test, NULL, setup, test_register,
      teardown);
  g_test_add ("/base-client/observer", Test, NULL, setup, test_observer,
      teardown);
  g_test_add ("/base-client/approver", Test, NULL, setup, test_approver,
      teardown);
  g_test_add ("/base-client/handler", Test, NULL, setup, test_handler,
      teardown);
  g_test_add ("/base-client/handler-requests", Test, NULL, setup,
      test_handler_requests, teardown);
  g_test_add ("/cdo/claim_with", Test, NULL, setup,
      test_channel_dispatch_operation_claim_with_async, teardown);
  g_test_add ("/base-client/delegate-channels", Test, NULL, setup,
      test_delegate_channels, teardown);
  g_test_add ("/cd/present-channel", Test, NULL, setup,
      test_present_channel, teardown);
  g_test_add ("/cd/delegate-to-preferred-handler/not-supported", Test, NULL,
      setup, test_delegate_to_preferred_handler_not_supported, teardown);
  g_test_add ("/cd/delegate-to-preferred-handler/supported", Test, NULL,
      setup, test_delegate_to_preferred_handler_supported, teardown);

  return tp_tests_run_with_bus ();
}
