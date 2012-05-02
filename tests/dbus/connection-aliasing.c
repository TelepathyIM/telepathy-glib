/* Feature test for Conn.I.Aliasing
 *
 * Copyright © 2007-2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "examples/cm/contactlist/conn.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    DBusConnection *client_libdbus;
    DBusGConnection *client_dbusglib;
    TpDBusDaemon *client_bus;
    ExampleContactListConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    gboolean cwr_ready;
    GError *cwr_error /* initialized in setup */;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->error = NULL;

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
        EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
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
  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_BALANCE));

  tp_tests_proxy_run_until_prepared (test->conn, features);
}

static void
teardown (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpConnection *conn;
  GError *error = NULL;

  g_clear_error (&test->error);
  tp_clear_pointer (&test->mainloop, g_main_loop_unref);
  tp_clear_object (&test->conn);

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
}

static void
test_user_set (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_ALIASING, 0 };

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_ALIASING));
  g_assert (!tp_connection_can_set_contact_alias (test->conn));

  tp_tests_proxy_run_until_prepared (test->conn, features);

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_ALIASING));
  g_assert (tp_connection_can_set_contact_alias (test->conn));
}

int
main (int argc,
      char **argv)
{
  g_type_init ();

  tp_tests_abort_after (5);
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/conn/aliasing/user-set", Test, NULL,
      setup, test_user_set, teardown);

  return g_test_run ();
}
