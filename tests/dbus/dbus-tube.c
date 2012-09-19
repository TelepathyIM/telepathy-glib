/* Tests of TpDbusTubeChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <string.h>

#include <telepathy-glib/dbus-tube-channel.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/dbus.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/dbus-tube-chan.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsDBusTubeChannel *tube_chan_service;
    TpHandleRepoIface *contact_repo;
    TpHandleRepoIface *room_repo;

    /* Client side objects */
    TpConnection *connection;
    TpDBusTubeChannel *tube;

    GDBusConnection *tube_conn;
    GDBusConnection *cm_conn;
    GVariant *call_result;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  g_clear_object (&test->tube_conn);
  g_clear_object (&test->cm_conn);
  tp_clear_pointer (&test->call_result, g_variant_unref);
}

static void
create_tube_service (Test *test,
    gboolean requested,
    gboolean contact)
{
  gchar *chan_path;
  TpHandle handle, alf_handle;
  GHashTable *props;
  GType type;
  TpSimpleClientFactory *factory;

  /* If previous tube is still preparing, refs are kept on it. We want it to be
   * destroyed now otherwise it will get reused by the factory. */
  if (test->tube != NULL)
    tp_tests_proxy_run_until_prepared (test->tube, NULL);

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  test->room_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_ROOM);
  g_assert (test->room_repo != NULL);

  if (contact)
    {
      handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
      type = TP_TESTS_TYPE_CONTACT_DBUS_TUBE_CHANNEL;
    }
  else
    {
      handle = tp_handle_ensure (test->room_repo, "#test", NULL, &test->error);
      type = TP_TESTS_TYPE_ROOM_DBUS_TUBE_CHANNEL;
    }

  g_assert_no_error (test->error);

  alf_handle = tp_handle_ensure (test->contact_repo, "alf", NULL, &test->error);
  g_assert_no_error (test->error);

  test->tube_chan_service = g_object_new (
      type,
      "connection", test->base_connection,
      "handle", handle,
      "requested", requested,
      "object-path", chan_path,
      "initiator-handle", alf_handle,
      NULL);

  /* Create client-side tube channel object */
  g_object_get (test->tube_chan_service, "channel-properties", &props, NULL);

  factory = tp_proxy_get_factory (test->connection);
  test->tube = (TpDBusTubeChannel *) tp_simple_client_factory_ensure_channel (
      factory, test->connection, chan_path, props, &test->error);
  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));

  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (props);

  if (contact)
    tp_handle_unref (test->contact_repo, handle);
  else
    tp_handle_unref (test->room_repo, handle);
}

/* Test Basis */

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_tube_service (test, TRUE, TRUE);

  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));

  create_tube_service (test, FALSE, FALSE);

  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));
}

static void
check_parameters (GHashTable *parameters)
{
  g_assert (parameters != NULL);

  g_assert_cmpuint (g_hash_table_size (parameters), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (parameters, "badger", NULL), ==, 42);
}

static void
check_parameters_vardict (GVariant *parameters_vardict)
{
  guint32 badger_value;

  g_assert (parameters_vardict != NULL);

  g_assert (g_variant_lookup (parameters_vardict, "badger",
      "u", &badger_value));
  g_assert_cmpuint (badger_value, ==, 42);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gchar *service;
  GHashTable *parameters;
  GVariant *parameters_vardict;

  /* Outgoing tube */
  create_tube_service (test, TRUE, TRUE);

  /* Service */
  g_assert_cmpstr (tp_dbus_tube_channel_get_service_name (test->tube), ==,
      "com.test.Test");
  g_object_get (test->tube, "service-name", &service, NULL);
  g_assert_cmpstr (service, ==, "com.test.Test");
  g_free (service);

  /* Parameters */
  parameters = tp_dbus_tube_channel_get_parameters (test->tube);
  parameters_vardict = tp_dbus_tube_channel_dup_parameters_vardict (
      test->tube);
  /* NULL as the tube has not be offered yet */
  g_assert (parameters == NULL);
  g_object_get (test->tube, "parameters", &parameters, NULL);
  g_assert (parameters == NULL);
  g_assert (parameters_vardict == NULL);

  /* Incoming tube */
  create_tube_service (test, FALSE, FALSE);

  /* Parameters */
  parameters = tp_dbus_tube_channel_get_parameters (test->tube);
  check_parameters (parameters);
  g_object_get (test->tube, "parameters", &parameters, NULL);
  check_parameters (parameters);

  parameters_vardict = tp_dbus_tube_channel_dup_parameters_vardict (
      test->tube);
  check_parameters_vardict (parameters_vardict);

  g_hash_table_unref (parameters);
  g_variant_unref (parameters_vardict);

  g_object_get (test->tube,
      "parameters-vardict", &parameters_vardict,
      NULL);
  check_parameters_vardict (parameters_vardict);
  g_variant_unref (parameters_vardict);
}

static void
tube_offer_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_object (&test->tube_conn);

  test->tube_conn = tp_dbus_tube_channel_offer_finish (
      TP_DBUS_TUBE_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static gboolean
new_connection_cb (TpTestsDBusTubeChannel *chan,
    GDBusConnection *connection,
    Test *test)
{
  g_clear_object (&test->cm_conn);
  test->cm_conn = g_object_ref (connection);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);

  return TRUE;
}

static void
handle_double_call (GDBusConnection       *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data)
{
  if (!tp_strdiff (method_name, "Double"))
    {
      guint value;

      g_variant_get (parameters, "(i)", &value);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(i)", value * 2));
    }
}

static void
register_object (GDBusConnection *connection)
{
  GDBusNodeInfo *introspection_data;
  guint registration_id;
  static const GDBusInterfaceVTable interface_vtable =
  {
    handle_double_call,
    NULL,
    NULL,
  };
  static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.Example.TestInterface'>"
    "    <method name='Double'>"
    "      <arg type='i' name='value' direction='in'/>"
    "      <arg type='i' name='result' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  registration_id = g_dbus_connection_register_object (connection,
      "/org/Example/TestObject", introspection_data->interfaces[0],
      &interface_vtable, NULL, NULL, NULL);
  g_assert (registration_id > 0);

  g_dbus_node_info_unref (introspection_data);
}

static void
double_call_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_clear_pointer (&test->call_result, g_variant_unref);

  test->call_result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
      result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
use_tube (Test *test,
    GDBusConnection *server_conn,
    GDBusConnection *client_conn)
{
  gint result;

  /* Server publishes an object on the tube */
  register_object (server_conn);

  /* Client calls a remote method */
  g_dbus_connection_call (client_conn, NULL, "/org/Example/TestObject",
      "org.Example.TestInterface", "Double",
      g_variant_new ("(i)", 42),
      G_VARIANT_TYPE ("(i)"), G_DBUS_CALL_FLAGS_NONE, -1,
      NULL, double_call_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_variant_get (test->call_result, "(i)", &result);
  g_assert_cmpuint (result, ==, 42 * 2);
}

static void
test_offer (Test *test,
    gconstpointer data)
{
  const TpTestsDBusTubeChannelOpenMode open_mode = GPOINTER_TO_UINT (data);
  GHashTable *params;

  /* Outgoing tube */
  create_tube_service (test, TRUE, TRUE);
  tp_tests_dbus_tube_channel_set_open_mode (test->tube_chan_service, open_mode);

  params = tp_asv_new ("badger", G_TYPE_UINT, 42, NULL);

  g_signal_connect (test->tube_chan_service, "new-connection",
      G_CALLBACK (new_connection_cb), test);

  tp_dbus_tube_channel_offer_async (test->tube, params, tube_offer_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  check_parameters (tp_dbus_tube_channel_get_parameters (test->tube));

  g_assert (G_IS_DBUS_CONNECTION (test->tube_conn));
  g_assert (G_IS_DBUS_CONNECTION (test->cm_conn));

  use_tube (test, test->tube_conn, test->cm_conn);
}

static void
test_offer_invalidated_before_open (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Outgoing tube */
  create_tube_service (test, TRUE, TRUE);
  tp_tests_dbus_tube_channel_set_open_mode (test->tube_chan_service,
      TP_TESTS_DBUS_TUBE_CHANNEL_NEVER_OPEN);

  tp_dbus_tube_channel_offer_async (test->tube, NULL, tube_offer_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  /* FIXME: this isn't a particularly good error… it's just what comes out when
   * the channel gets closed from under us, and there isn't really API on
   * DBusTube to give a better error.
   *
   * https://bugs.freedesktop.org/show_bug.cgi?id=48196
   */
  g_assert_error (test->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED);
}

static void
tube_accept_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  g_clear_object (&test->tube_conn);

  test->tube_conn = tp_dbus_tube_channel_accept_finish (
      TP_DBUS_TUBE_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_accept (Test *test,
    gconstpointer data)
{
  const TpTestsDBusTubeChannelOpenMode open_mode = GPOINTER_TO_UINT (data);

  /* Incoming tube */
  create_tube_service (test, FALSE, TRUE);
  tp_tests_dbus_tube_channel_set_open_mode (test->tube_chan_service, open_mode);

  g_signal_connect (test->tube_chan_service, "new-connection",
      G_CALLBACK (new_connection_cb), test);

  tp_dbus_tube_channel_accept_async (test->tube, tube_accept_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (G_IS_DBUS_CONNECTION (test->tube_conn));
  g_assert (G_IS_DBUS_CONNECTION (test->cm_conn));

  use_tube (test, test->cm_conn, test->tube_conn);
}

static void
test_accept_invalidated_before_open (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Incoming tube */
  create_tube_service (test, FALSE, TRUE);
  tp_tests_dbus_tube_channel_set_open_mode (test->tube_chan_service,
      TP_TESTS_DBUS_TUBE_CHANNEL_NEVER_OPEN);

  tp_dbus_tube_channel_accept_async (test->tube, tube_accept_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  /* FIXME: this isn't a particularly good error… it's just what comes out when
   * the channel gets closed from under us, and there isn't really API on
   * DBusTube to give a better error.
   *
   * https://bugs.freedesktop.org/show_bug.cgi?id=48196
   */
  g_assert_error (test->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/dbus-tube/creation", Test, NULL, setup, test_creation,
      teardown);
  g_test_add ("/dbus-tube/properties", Test, NULL, setup, test_properties,
      teardown);
  /* Han shot first. */
  g_test_add ("/dbus-tube/offer-open-first", Test,
      GUINT_TO_POINTER (TP_TESTS_DBUS_TUBE_CHANNEL_OPEN_FIRST),
      setup, test_offer, teardown);
  g_test_add ("/dbus-tube/offer-open-second", Test,
      GUINT_TO_POINTER (TP_TESTS_DBUS_TUBE_CHANNEL_OPEN_SECOND),
      setup, test_offer, teardown);
  g_test_add ("/dbus-tube/offer-invalidated-before-open", Test, NULL,
      setup, test_offer_invalidated_before_open, teardown);
  g_test_add ("/dbus-tube/accept-open-first", Test,
      GUINT_TO_POINTER (TP_TESTS_DBUS_TUBE_CHANNEL_OPEN_FIRST),
      setup, test_accept, teardown);
  g_test_add ("/dbus-tube/accept-open-second", Test,
      GUINT_TO_POINTER (TP_TESTS_DBUS_TUBE_CHANNEL_OPEN_SECOND),
      setup, test_accept, teardown);
  g_test_add ("/dbus-tube/accept-invalidated-before-open", Test, NULL,
      setup, test_accept_invalidated_before_open, teardown);

  return g_test_run ();
}
