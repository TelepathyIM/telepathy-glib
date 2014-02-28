/* Tests of TpSimpleHandler
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/simple-handler.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/echo-chan.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseClient *simple_handler;
    TpBaseConnection *base_connection;
    TpTestsSimpleAccount *account_service;
    TpTestsEchoChannel *text_chan_service;

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
  test->account = tp_tests_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  /* Create service-side text channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_ENTITY_TYPE_CONTACT);
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
      TP_ENTITY_TYPE_CONTACT, handle, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);
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

  g_clear_error (&test->error);

  g_object_unref (test->simple_handler);
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
create_simple_handler (Test *test,
    gboolean bypass_approval,
    gboolean requests,
    TpSimpleHandlerHandleChannelImpl impl)
{
  /* Create service-side Client object */
  test->simple_handler = tp_tests_object_new_static_class (
      TP_TYPE_SIMPLE_HANDLER,
      "dbus-daemon", test->dbus,
      "bypass-approval", bypass_approval,
      "requests", requests,
      "name", "MySimpleHandler",
      "uniquify-name", FALSE,
      "callback", impl,
      "user-data", test,
      "destroy", NULL,
      NULL);
  g_assert (test->simple_handler != NULL);

 /* Create client-side Client object */
  test->client = tp_tests_object_new_static_class (TP_TYPE_CLIENT,
          "dbus-daemon", test->dbus,
          "bus-name", tp_base_client_get_bus_name (test->simple_handler),
          "object-path", tp_base_client_get_object_path (test->simple_handler),
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
  g_assert_cmpint (g_strv_length ((GStrv) interfaces), ==, 2);
  g_assert (tp_strv_contains (interfaces, TP_IFACE_CLIENT_HANDLER));
  g_assert (tp_strv_contains (interfaces, TP_IFACE_CLIENT_INTERFACE_REQUESTS));

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
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1);
  g_assert_cmpuint (tp_asv_get_uint32 (filter,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, NULL), ==, TP_ENTITY_TYPE_CONTACT);
}

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
  g_assert (!bypass);

  capabilities = tp_asv_get_strv (properties, "Capabilities");
  g_assert_cmpint (g_strv_length ((GStrv) capabilities), ==, 0);

  handled = tp_asv_get_boxed (properties, "HandledChannels",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  g_assert (handled != NULL);
  g_assert_cmpint (handled->len, ==, 0);

out:
  g_main_loop_quit (test->mainloop);
}

static void
handle_channel_success (
    TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    GList *requests_satisified,
    gint64 user_action_time,
    TpHandleChannelContext *context,
    gpointer user_data)
{
  GVariant *info;
  guint u = 0;

  info = tp_handle_channel_context_dup_handler_info (context);
  g_assert (info != NULL);
  g_assert (g_variant_is_of_type (info, G_VARIANT_TYPE_VARDICT));
  g_variant_lookup (info, "badger", "u", &u);
  g_assert_cmpuint (u, ==, 42);
  g_variant_unref (info);

  tp_handle_channel_context_accept (context);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_handler (test, FALSE, TRUE, handle_channel_success);

  tp_base_client_add_handler_filter (test->simple_handler,
      g_variant_new_parsed ("{ %s: <%s> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_TEXT));

  tp_base_client_add_handler_filter (test->simple_handler,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT));

  tp_base_client_register (test->simple_handler, &test->error);
  g_assert_no_error (test->error);

  /* Check Client properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Check Handler properties */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT_HANDLER, get_handler_prop_cb, test, NULL, NULL);

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
call_handle_channel (Test *test)
{
  GHashTable *requests_satisfied, *info,* chan_props;
  int i;

  requests_satisfied = g_hash_table_new (NULL, NULL);
  info = tp_asv_new (
      "badger", G_TYPE_UINT, 42,
      NULL);
  chan_props = tp_tests_dup_channel_props_asv (test->text_chan);

  tp_proxy_add_interface_by_id (TP_PROXY (test->client),
      TP_IFACE_QUARK_CLIENT_HANDLER);

  for (i = 0 ; i < 10 ; i ++)
    {
      tp_cli_client_handler_call_handle_channel (test->client, -1,
          tp_proxy_get_object_path (test->account),
          tp_proxy_get_object_path (test->connection),
          tp_proxy_get_object_path (test->text_chan), chan_props,
          requests_satisfied, 0, info,
          no_return_cb, test, NULL, NULL);

      g_main_loop_run (test->mainloop);
    }

  g_hash_table_unref (requests_satisfied);
  g_hash_table_unref (info);
  g_hash_table_unref (chan_props);
}

/* HandleChannel returns immediately */
static void
test_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_handler (test, FALSE, FALSE, handle_channel_success);

  tp_base_client_add_handler_filter (test->simple_handler,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_handler, &test->error);
  g_assert_no_error (test->error);

  call_handle_channel (test);
  g_assert_no_error (test->error);
}

/* HandleChannel returns in an async way */
static gboolean
accept_idle_cb (gpointer data)
{
  TpHandleChannelContext *context = data;

  tp_handle_channel_context_accept (context);
  g_object_unref (context);
  return FALSE;
}

static void
handle_channel_async (
    TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    GList *requests_satisified,
    gint64 user_action_time,
    TpHandleChannelContext *context,
    gpointer user_data)
{
  g_idle_add (accept_idle_cb, g_object_ref (context));

  tp_handle_channel_context_delay (context);
}

static void
test_delayed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_handler (test, FALSE, FALSE, handle_channel_async);

  tp_base_client_add_handler_filter (test->simple_handler,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_handler, &test->error);
  g_assert_no_error (test->error);

  call_handle_channel (test);
  g_assert_no_error (test->error);
}

/* HandleChannel fails */
static void
handle_channel_fail (
    TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    GList *requests_satisified,
    gint64 user_action_time,
    TpHandleChannelContext *context,
    gpointer user_data)
{
  GError error = { TP_ERROR, TP_ERROR_NOT_AVAILABLE,
      "No HandleChannel for you!" };

  tp_handle_channel_context_fail (context, &error);
}

static void
test_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_simple_handler (test, FALSE, FALSE, handle_channel_fail);

  tp_base_client_add_handler_filter (test->simple_handler,
      g_variant_new_parsed ("@a{sv} {}"));

  tp_base_client_register (test->simple_handler, &test->error);
  g_assert_no_error (test->error);

  call_handle_channel (test);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/simple-handler/properties", Test, NULL, setup, test_properties,
      teardown);
  g_test_add ("/simple-handler/success", Test, NULL, setup, test_success,
      teardown);
  g_test_add ("/simple-handler/delayed", Test, NULL, setup, test_delayed,
      teardown);
  g_test_add ("/simple-handler/fail", Test, NULL, setup, test_fail,
      teardown);

  return tp_tests_run_with_bus ();
}
