/* Tests of TpSimpleObserver
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/simple-observer.h>
#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseClient *simple_observer;
    TpBaseConnection *base_connection;
    TpTestsSimpleAccount *account_service;
    TpTestsTextChannelNull *text_chan_service;

    /* Client side objects */
    TpClient *client;
    TpConnection *connection;
    TpAccount *account;
    TpChannel *text_chan;

    GError *error /* initialized where needed */;
} Test;

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

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

   /* Create client-side Account object */
  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION, "me@test.com",
      &test->base_connection, &test->connection);

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
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  g_object_unref (test->simple_observer);
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

  g_object_unref (test->text_chan_service);
  g_object_unref (test->text_chan);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
create_simple_observer (Test *test,
    gboolean recover,
    TpSimpleObserverObserveChannelsImpl impl)
{
  /* Create service-side Client object */
  test->simple_observer = tp_tests_object_new_static_class (
      TP_TYPE_SIMPLE_OBSERVER,
      "dbus-daemon", test->dbus,
      "recover", recover,
      "name", "MySimpleObserver",
      "uniquify-name", FALSE,
      "callback", impl,
      "user-data", test,
      "destroy", NULL,
      NULL);
  g_assert (test->simple_observer != NULL);

 /* Create client-side Client object */
  test->client = tp_tests_object_new_static_class (TP_TYPE_CLIENT,
          "dbus-daemon", test->dbus,
          "bus-name", tp_base_client_get_bus_name (test->simple_observer),
          "object-path", tp_base_client_get_object_path (test->simple_observer),
          NULL);

  g_assert (test->client != NULL);
}

static void
get_client_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  const gchar * const *interfaces;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 1);

  interfaces = tp_asv_get_strv (properties, "Interfaces");
  g_assert_cmpint (g_strv_length ((GStrv) interfaces), ==, 1);
  g_assert (tp_strv_contains (interfaces, TP_IFACE_CLIENT_OBSERVER));

out:
  g_main_loop_quit (test->mainloop);
}

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
  gboolean delay;

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
  g_assert (!delay);

out:
  g_main_loop_quit (test->mainloop);
}

static void
observe_channels_success (
    TpSimpleObserver *observer,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context,
    gpointer user_data)
{
  tp_observe_channels_context_accept (context);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_observer (test, TRUE, observe_channels_success);

  tp_base_client_add_observer_filter_vardict (test->simple_observer,
      g_variant_new_parsed ("{ %s: <%s> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_TEXT));

  tp_base_client_add_observer_filter_vardict (test->simple_observer,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, (guint32) TP_HANDLE_TYPE_CONTACT));

  tp_base_client_register (test->simple_observer, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Check Observer properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_OBSERVER, get_observer_prop_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
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
call_observe_channels (Test *test)
{
  GPtrArray *channels, *requests_satisified;
  GHashTable *info;

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

  g_ptr_array_foreach (channels, free_channel_details, NULL);
  g_ptr_array_unref (channels);
  g_ptr_array_unref (requests_satisified);
  g_hash_table_unref (info);
}

/* ObserveChannels returns immediately */
static void
test_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_observer (test, TRUE, observe_channels_success);

  tp_base_client_add_observer_filter_vardict (test->simple_observer,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_observer, &test->error);
  g_assert_no_error (test->error);

  call_observe_channels (test);
  g_assert_no_error (test->error);
}

/* ObserveChannels returns in an async way */
static gboolean
accept_idle_cb (gpointer data)
{
  TpObserveChannelsContext *context = data;

  tp_observe_channels_context_accept (context);
  g_object_unref (context);
  return FALSE;
}

static void
observe_channels_async (
    TpSimpleObserver *observer,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context,
    gpointer user_data)
{
  g_idle_add (accept_idle_cb, g_object_ref (context));

  tp_observe_channels_context_delay (context);
}

static void
test_delayed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_observer (test, TRUE, observe_channels_async);

  tp_base_client_add_observer_filter_vardict (test->simple_observer,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_observer, &test->error);
  g_assert_no_error (test->error);

  call_observe_channels (test);
  g_assert_no_error (test->error);
}

/* ObserveChannels fails */
static void
observe_channels_fail (
    TpSimpleObserver *observer,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context,
    gpointer user_data)
{
  GError error = { TP_ERROR, TP_ERROR_NOT_AVAILABLE,
      "No ObserveChannels for you!" };

  tp_observe_channels_context_fail (context, &error);
}

static void
test_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_observer (test, TRUE, observe_channels_fail);

  tp_base_client_add_observer_filter_vardict (test->simple_observer,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_observer, &test->error);
  g_assert_no_error (test->error);

  call_observe_channels (test);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/simple-observer/properties", Test, NULL, setup, test_properties,
      teardown);
  g_test_add ("/simple-observer/success", Test, NULL, setup, test_success,
      teardown);
  g_test_add ("/simple-observer/delayed", Test, NULL, setup, test_delayed,
      teardown);
  g_test_add ("/simple-observer/fail", Test, NULL, setup, test_fail,
      teardown);

  return g_test_run ();
}
