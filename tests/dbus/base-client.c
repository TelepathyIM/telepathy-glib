/* Tests of TpBaseClient
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

/* We include -internal headers of context to be able to easily access to
 * their semi-private attributes (connection, account, channels, etc). */
#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/add-dispatch-operation-context-internal.h>
#include <telepathy-glib/base-client.h>
#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/handle-channels-context-internal.h>
#include <telepathy-glib/observe-channels-context-internal.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/simple-channel-dispatch-operation.h"
#include "tests/lib/simple-client.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseClient *base_client;
    TpTestsSimpleClient *simple_client;
    TpBaseConnection *base_connection;
    TpTestsSimpleAccount *account_service;
    TpTestsTextChannelNull *text_chan_service;
    TpTestsTextChannelNull *text_chan_service_2;
    TpTestsSimpleChannelDispatchOperation *cdo_service;

    /* Client side objects */
    TpAccountManager *account_mgr;
    TpClient *client;
    TpConnection *connection;
    TpAccount *account;
    TpChannel *text_chan;
    TpChannel *text_chan_2;

    GError *error /* initialized where needed */;
    GStrv interfaces;
    gint wait;
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

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;
  test->interfaces = NULL;

  /* The case of a non-shared TpAccountManager is tested in
   * simple-approver.c */
  test->account_mgr = tp_account_manager_dup ();
  g_assert (test->account_mgr != NULL);

  /* Claim AccountManager bus-name (needed as we're going to export an Account
   * object). */
  tp_dbus_daemon_request_name (test->dbus,
          TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  /* Create service-side Client object */
  test->simple_client = tp_tests_simple_client_new (test->dbus, "Test", FALSE);
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
  test->account = tp_account_manager_ensure_account (test->account_mgr,
      ACCOUNT_PATH);
  g_assert (test->account != NULL);
  g_object_ref (test->account);

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

  /* Create Service side ChannelDispatchOperation object */
  test->cdo_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCH_OPERATION,
      NULL);
  tp_dbus_daemon_register_object (test->dbus, CDO_PATH, test->cdo_service);

  tp_tests_simple_channel_dispatch_operation_set_conn_path (test->cdo_service,
      tp_proxy_get_object_path (test->connection));

  tp_tests_simple_channel_dispatch_operation_set_account_path (
      test->cdo_service, tp_proxy_get_object_path (test->account));

  tp_tests_simple_channel_dispatch_operation_add_channel (test->cdo_service,
      test->text_chan);
  tp_tests_simple_channel_dispatch_operation_add_channel (test->cdo_service,
      test->text_chan_2);

  g_assert (tp_dbus_daemon_request_name (test->dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL));
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  g_strfreev (test->interfaces);

  g_object_unref (test->account_mgr);

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

  g_object_unref (test->cdo_service);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

/* Test Basis */

static void
test_basics (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccountManager *account_manager;
  TpDBusDaemon *dbus;
  gchar *name;
  gboolean unique;

  g_object_get (test->base_client,
      "account-manager", &account_manager,
      "dbus-daemon", &dbus,
      "name", &name,
      "uniquify-name", &unique,
      NULL);

  g_assert (test->account_mgr == account_manager);
  g_assert (test->dbus == dbus);
  g_assert_cmpstr ("Test", ==, name);
  g_assert (!unique);

  g_assert (test->account_mgr == tp_base_client_get_account_manager (
        test->base_client));
  g_assert (test->dbus == tp_base_client_get_dbus_daemon (test->base_client));
  g_assert_cmpstr ("Test", ==, tp_base_client_get_name (test->base_client));
  g_assert (!tp_base_client_get_uniquify_name (test->base_client));

  g_object_unref (account_manager);
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
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE);
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
  gboolean recover;
  gboolean valid;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 2);

  filters = tp_asv_get_boxed (properties, "ObserverChannelFilter",
      TP_ARRAY_TYPE_CHANNEL_CLASS_LIST);
  check_filters (filters);

  recover = tp_asv_get_boolean (properties, "Recover", &valid);
  g_assert (valid);
  g_assert (recover);

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
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
add_channel_to_ptr_array (GPtrArray *arr,
    TpChannel *channel)
{
  GValueArray *tmp;

  g_assert (arr != NULL);
  g_assert (channel != NULL);

  tmp = tp_value_array_build (2,
      DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (channel),
      TP_HASH_TYPE_STRING_VARIANT_MAP, tp_channel_borrow_immutable_properties (
        channel),
      G_TYPE_INVALID);

  g_ptr_array_add (arr, tmp);
}

static void
free_channel_details (gpointer data,
    gpointer user_data)
{
  g_boxed_free (TP_STRUCT_TYPE_CHANNEL_DETAILS, data);
}

static void
test_observer (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *filter;
  GPtrArray *channels, *requests_satisified;
  GHashTable *info;
  TpChannel *chan;

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_observer_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_observer_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  tp_base_client_set_observer_recover (test->base_client, TRUE);

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
  channels = g_ptr_array_sized_new (1);
  add_channel_to_ptr_array (channels, test->text_chan);

  requests_satisified = g_ptr_array_sized_new (0);
  info = tp_asv_new (
      "recovering", G_TYPE_BOOLEAN, TRUE,
      NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_OBSERVER);

  tp_cli_client_observer_call_observe_channels (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      channels, "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->observe_ctx != NULL);
  g_assert (tp_observe_channels_context_is_recovering (
        test->simple_client->observe_ctx));

  g_assert (test->simple_client->observe_ctx->account == test->account);

  /* Now call it with an invalid argument */
  tp_asv_set_boolean (info, "FAIL", TRUE);

  tp_cli_client_observer_call_observe_channels (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      channels, "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  /* The channel being observed is invalidated while preparing */
  g_hash_table_remove (info, "FAIL");

  tp_cli_client_observer_call_observe_channels (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      channels, "/", requests_satisified, info,
      no_return_cb, test, NULL, NULL);

  tp_tests_text_channel_null_close (test->text_chan_service);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  chan = g_ptr_array_index (test->simple_client->observe_ctx->channels, 0);
  g_assert (TP_IS_CHANNEL (chan));
  g_assert (tp_proxy_get_invalidated (chan) != NULL);

  g_ptr_array_foreach (channels, free_channel_details, NULL);
  g_ptr_array_free (channels, TRUE);
  g_ptr_array_free (requests_satisified, TRUE);
  g_hash_table_unref (info);
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
  GPtrArray *channels, *chans;
  GHashTable *properties;
  static const char *interfaces[] = { NULL };
  static const gchar *possible_handlers[] = {
    TP_CLIENT_BUS_NAME_BASE ".Badger", NULL, };
  guint i;

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_approver_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_approver_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
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
  channels = g_ptr_array_sized_new (2);
  add_channel_to_ptr_array (channels, test->text_chan);
  add_channel_to_ptr_array (channels, test->text_chan_2);

  properties = tp_asv_new (
      TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
        G_TYPE_STRV, interfaces,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->connection),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
        DBUS_TYPE_G_OBJECT_PATH, tp_proxy_get_object_path (test->account),
      TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
        G_TYPE_STRV, possible_handlers,
      NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_APPROVER);

  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      channels, CDO_PATH, properties,
      no_return_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->add_dispatch_ctx != NULL);
  chans = tp_channel_dispatch_operation_borrow_channels (
      test->simple_client->add_dispatch_ctx->dispatch_operation);
  g_assert_cmpuint (chans->len, ==, 2);

  /* Check that we reuse existing proxies rather than creating new ones */
  g_assert (test->simple_client->add_dispatch_ctx->account == test->account);
  g_assert (tp_channel_dispatch_operation_borrow_account (
        test->simple_client->add_dispatch_ctx->dispatch_operation) ==
      test->account);

  g_assert (tp_channel_dispatch_operation_borrow_connection (
        test->simple_client->add_dispatch_ctx->dispatch_operation) ==
      test->simple_client->add_dispatch_ctx->connection);

  g_assert (chans->len == test->simple_client->add_dispatch_ctx->channels->len);
  for (i = 0; i < chans->len; i++)
    {
      g_assert (g_ptr_array_index (chans, i) ==
          g_ptr_array_index (test->simple_client->add_dispatch_ctx->channels,
            i));
    }

  /* Another call to AddDispatchOperation, the second channel will be
   * invalidated during the call */
  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      channels, CDO_PATH, properties,
      no_return_cb, test, NULL, NULL);

  tp_tests_text_channel_null_close (test->text_chan_service_2);

  g_object_unref (test->text_chan_service_2);
  test->text_chan_service_2 = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan_2);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->add_dispatch_ctx != NULL);
  /* The CDO only contain valid channels */
  chans = tp_channel_dispatch_operation_borrow_channels (
      test->simple_client->add_dispatch_ctx->dispatch_operation);
  g_assert_cmpuint (chans->len, ==, 1);
  /* But the context contains both */
  g_assert_cmpuint (test->simple_client->add_dispatch_ctx->channels->len,
      ==, 2);

  /* Another call to AddDispatchOperation, the last channel will be
   * invalidated during the call */
  tp_cli_client_approver_call_add_dispatch_operation (test->client, -1,
      channels, CDO_PATH, properties,
      no_return_cb, test, NULL, NULL);

  tp_tests_text_channel_null_close (test->text_chan_service);
  g_object_unref (test->text_chan_service);
  test->text_chan_service = NULL;

  tp_tests_simple_channel_dispatch_operation_lost_channel (test->cdo_service,
      test->text_chan);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_ptr_array_foreach (channels, free_channel_details, NULL);
  g_ptr_array_free (channels, TRUE);
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
test_handler (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *filter;
  const gchar *caps[] = { "mushroom", "snake", NULL };
  GPtrArray *channels;
  GPtrArray *requests_satisified;
  GHashTable *info;
  GList *chans;
  TpTestsSimpleClient *client_2;

  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      NULL);

  tp_base_client_add_handler_filter (test->base_client, filter);
  g_hash_table_unref (filter);

  tp_base_client_take_handler_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
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

  /* Call HandleChannels */
  channels = g_ptr_array_sized_new (2);
  add_channel_to_ptr_array (channels, test->text_chan);
  add_channel_to_ptr_array (channels, test->text_chan_2);

  requests_satisified = g_ptr_array_sized_new (0);
  info = g_hash_table_new (NULL, NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_HANDLER);

  tp_cli_client_handler_call_handle_channels (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      channels, requests_satisified, 0, info,
      no_return_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->handle_channels_ctx != NULL);
  g_assert (test->simple_client->handle_channels_ctx->account == test->account);

  chans = tp_base_client_get_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 2);
  g_list_free (chans);

  /* One of the channel is closed */
  g_signal_connect (test->text_chan, "invalidated",
      G_CALLBACK (channel_invalidated_cb), test);
  tp_tests_text_channel_null_close (test->text_chan_service);
  g_main_loop_run (test->mainloop);

  chans = tp_base_client_get_handled_channels (test->base_client);
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free (chans);

  /* Create another client sharing the same unique name */
  client_2 = tp_tests_simple_client_new (test->dbus, "Test", TRUE);
  tp_base_client_be_a_handler (TP_BASE_CLIENT (client_2));
  tp_base_client_register (TP_BASE_CLIENT (client_2), &test->error);
  g_assert_no_error (test->error);

  chans = tp_base_client_get_handled_channels (TP_BASE_CLIENT (client_2));
  g_assert_cmpuint (g_list_length (chans), ==, 1);
  g_list_free (chans);

  g_object_unref (client_2);

  g_ptr_array_foreach (channels, free_channel_details, NULL);
  g_ptr_array_free (channels, TRUE);
  g_ptr_array_free (requests_satisified, TRUE);
  g_hash_table_unref (info);
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

  requests = tp_base_client_get_pending_requests (test->base_client);
  g_assert_cmpuint (g_list_length ((GList *) requests), ==, 1);
  g_assert (requests->data == request);
  g_list_free (requests);

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
  GPtrArray *channels;
  GPtrArray *requests_satisified;
  GHashTable *info;
  TpChannelRequest *request;
  GList *requests;

  tp_base_client_take_handler_filter (test->base_client, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
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

  g_assert (tp_base_client_get_pending_requests (test->base_client) == NULL);

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

  requests = tp_base_client_get_pending_requests (test->base_client);
  g_assert (requests != NULL);
  g_list_free (requests);

  /* Call HandleChannels */
  channels = g_ptr_array_sized_new (2);
  add_channel_to_ptr_array (channels, test->text_chan);

  requests_satisified = g_ptr_array_sized_new (1);
  g_ptr_array_add (requests_satisified, "/Request");

  info = g_hash_table_new (NULL, NULL);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_HANDLER);

  tp_cli_client_handler_call_handle_channels (test->client, -1,
      tp_proxy_get_object_path (test->account),
      tp_proxy_get_object_path (test->connection),
      channels, requests_satisified, 0, info,
      no_return_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->simple_client->handle_channels_ctx != NULL);
  g_assert_cmpint (
      test->simple_client->handle_channels_ctx->requests_satisfied->len, ==, 1);
  request = g_ptr_array_index (
      test->simple_client->handle_channels_ctx->requests_satisfied, 0);
  requests = tp_base_client_get_pending_requests (test->base_client);
  g_assert (requests->data == request);
  g_list_free (requests);

  /* Call RemoveRequest */
  g_signal_connect (test->base_client, "request-removed",
      G_CALLBACK (request_removed_cb), test);

  tp_cli_client_interface_requests_call_remove_request (test->client, -1,
      "/Request", "Badger", "snake",
      no_return_cb, test, NULL, NULL);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_base_client_get_pending_requests (test->base_client) == NULL);

  g_hash_table_unref (properties);
  g_ptr_array_foreach (channels, free_channel_details, NULL);
  g_ptr_array_free (channels, TRUE);
  g_ptr_array_free (requests_satisified, TRUE);
  g_hash_table_unref (info);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
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

  return g_test_run ();
}