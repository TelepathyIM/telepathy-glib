/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=27835
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/simple-conn.h"
#include "tests/lib/util.h"

/* an almost-no-op subclass... */
typedef TpTestsSimpleConnection InterestedConnection;
typedef TpTestsSimpleConnectionClass InterestedConnectionClass;

static GType interested_connection_get_type (void);

G_DEFINE_TYPE_WITH_CODE (InterestedConnection,
    interested_connection,
    TP_TESTS_TYPE_SIMPLE_CONNECTION,
    G_STMT_START { } G_STMT_END)

/* Lord Pearson of Rannoch: My Lords, I beg leave to ask the Question
 * standing in my name on the Order Paper. In doing so, I declare an
 * interest as patron of the British Register of Chinese Herbal Medicine.
 * -- Hansard, 2010-02-01 */
#define SUPPORTED_TOKEN "com.example.rannoch/ChineseHerbalMedicine"

/* Lord Hoyle: My Lords, in thanking my noble friend for his Answer, I declare
 * an interest as the chairman and now president of Warrington Wolves Rugby
 * League Club. -- Hansard, 2010-01-11
 */
#define UNSUPPORTED_TOKEN "org.example.Warrington/Wolves"

static void
interested_connection_init (InterestedConnection *self G_GNUC_UNUSED)
{
}

static void
interested_connection_constructed (GObject *object)
{
  TpBaseConnection *base = (TpBaseConnection *) object;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) interested_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  tp_base_connection_add_possible_client_interest (base,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_LOCATION);
  tp_base_connection_add_possible_client_interest (base,
      g_quark_from_static_string (SUPPORTED_TOKEN));
}

static void
interested_connection_class_init (InterestedConnectionClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;

  object_class->constructed = interested_connection_constructed;
}

typedef struct {
    TpDBusDaemon *dbus;
    DBusConnection *client_libdbus;
    DBusGConnection *client_dbusglib;
    TpDBusDaemon *client_bus;
    TpTestsSimpleConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    gboolean cwr_ready;
    GError *cwr_error /* initialized in setup */;

    GAsyncResult *prepare_result;

    GPtrArray *log;
} Test;

static void
connection_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  Test *test = user_data;

  g_message ("%p prepared", object);
  g_assert (test->prepare_result == NULL);
  test->prepare_result = g_object_ref (res);
}

static void
interested_cb (TpBaseConnection *unused G_GNUC_UNUSED,
    const gchar *iface,
    Test *test)
{
  g_ptr_array_add (test->log, g_strdup_printf ("interested in %s", iface));
}

static void
location_interested_cb (TpBaseConnection *unused G_GNUC_UNUSED,
    const gchar *iface,
    Test *test)
{
  g_assert_cmpstr (iface, ==, TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  g_ptr_array_add (test->log, g_strdup ("Location interested"));
}

static void
uninterested_cb (TpBaseConnection *unused G_GNUC_UNUSED,
    const gchar *iface,
    Test *test)
{
  g_ptr_array_add (test->log, g_strdup_printf ("uninterested in %s", iface));
}

static void
location_uninterested_cb (TpBaseConnection *unused G_GNUC_UNUSED,
    const gchar *iface,
    Test *test)
{
  g_assert_cmpstr (iface, ==, TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  g_ptr_array_add (test->log, g_strdup ("Location uninterested"));
}

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->client_libdbus = dbus_bus_get_private (DBUS_BUS_STARTER, NULL);
  g_assert (test->client_libdbus != NULL);
  dbus_connection_setup_with_g_main (test->client_libdbus, NULL);
  dbus_connection_set_exit_on_disconnect (test->client_libdbus, FALSE);
  test->client_dbusglib = dbus_connection_get_g_connection (
      test->client_libdbus);
  dbus_g_connection_ref (test->client_dbusglib);
  test->client_bus = tp_dbus_daemon_new (test->client_dbusglib);
  g_assert (test->client_bus != NULL);

  test->service_conn = tp_tests_object_new_static_class (
        interested_connection_get_type (),
        "account", "me@example.com",
        "protocol", "simple-protocol",
        NULL);
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "simple",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;

  test->conn = tp_connection_new (test->client_bus, test->conn_name,
      test->conn_path, &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  tp_proxy_prepare_async (test->conn, features, connection_prepared_cb, test);
  g_assert (test->prepare_result == NULL);

  while (test->prepare_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_prepare_finish (test->conn, test->prepare_result,
        &error));
  g_assert_no_error (error);
  g_object_unref (test->prepare_result);
  test->prepare_result = NULL;

  test->log = g_ptr_array_new_with_free_func (g_free);

  g_signal_connect (test->service_conn,
      "clients-interested", G_CALLBACK (interested_cb), test);
  g_signal_connect (test->service_conn,
      "clients-interested::" TP_IFACE_CONNECTION_INTERFACE_LOCATION,
      G_CALLBACK (location_interested_cb), test);

  g_signal_connect (test->service_conn,
      "clients-uninterested", G_CALLBACK (uninterested_cb), test);
  g_signal_connect (test->service_conn,
      "clients-uninterested::" TP_IFACE_CONNECTION_INTERFACE_LOCATION,
      G_CALLBACK (location_uninterested_cb), test);
}

static void
teardown (Test *test,
    gconstpointer data)
{
  TpConnection *conn;
  GError *error = NULL;

  if (test->conn != NULL)
    {
      g_object_unref (test->conn);
      test->conn = NULL;
    }

  /* disconnect the connection so we don't leak it */
  conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);

  tp_tests_connection_assert_disconnect_succeeds (conn);

  g_assert (!tp_connection_run_until_ready (conn, FALSE, &error, NULL));
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_object_unref (test->client_bus);
  test->client_bus = NULL;

  dbus_g_connection_unref (test->client_dbusglib);
  dbus_connection_close (test->client_libdbus);
  dbus_connection_unref (test->client_libdbus);

  g_ptr_array_unref (test->log);
}

static void
test_interested_client (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  guint i;

  tp_connection_add_client_interest_by_id (test->conn,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_LOCATION);
  tp_connection_add_client_interest_by_id (test->conn,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS);

  /* run until (after) the AddClientInterest calls have gone out */
  tp_tests_proxy_run_until_dbus_queue_processed (test->client_bus);

  /* we auto-release the Location client interest by disposing the client
   * connection */
  g_object_run_dispose ((GObject *) test->conn);
  g_object_unref (test->conn);
  test->conn = NULL;

  /* run until (after) the RemoveClientInterest call has gone out */
  tp_tests_proxy_run_until_dbus_queue_processed (test->client_bus);

  /* then, run until (after) the CM should have processed both ACI and RCI */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  i = 0;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "interested in " TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "Location interested");

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "uninterested in " TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "Location uninterested");

  i++;
  g_assert_cmpuint (test->log->len, ==, i);
}

static void
test_interest (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  static const gchar * telepathy[] = {
      TP_IFACE_CONNECTION_INTERFACE_LOCATION,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS,
      NULL
  };
  static const gchar * hansard[] = {
      SUPPORTED_TOKEN,
      UNSUPPORTED_TOKEN,
      NULL
  };
  GError *error = NULL;
  guint i;

  tp_cli_connection_run_add_client_interest (test->conn, -1, telepathy, &error,
      NULL);
  g_assert_no_error (error);

  tp_cli_connection_run_add_client_interest (test->conn, -1, hansard, &error,
      NULL);
  g_assert_no_error (error);

  tp_cli_connection_run_add_client_interest (test->conn, -1, telepathy, &error,
      NULL);
  g_assert_no_error (error);

  tp_cli_connection_run_remove_client_interest (test->conn, -1, telepathy,
      &error, NULL);
  g_assert_no_error (error);

  tp_cli_connection_run_remove_client_interest (test->conn, -1, hansard,
      &error, NULL);
  g_assert_no_error (error);

  /* we auto-release the Location client interest by dropping the client
   * connection */
  dbus_connection_flush (test->client_libdbus);
  dbus_connection_close (test->client_libdbus);

  while (test->log->len < 6)
    g_main_context_iteration (NULL, TRUE);

  i = 0;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "interested in " TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "Location interested");

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "interested in " SUPPORTED_TOKEN);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "uninterested in " SUPPORTED_TOKEN);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "uninterested in " TP_IFACE_CONNECTION_INTERFACE_LOCATION);

  i++;
  g_assert_cmpuint (test->log->len, >, i);
  g_assert_cmpstr (g_ptr_array_index (test->log, i), ==,
      "Location uninterested");

  i++;
  g_assert_cmpuint (test->log->len, ==, i);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add ("/conn/interest", Test, NULL, setup, test_interest, teardown);
  g_test_add ("/conn/interested-client", Test, NULL, setup,
      test_interested_client, teardown);

  return g_test_run ();
}
