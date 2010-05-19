/* Feature test for contact lists
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>

#include "examples/cm/contactlist/conn.h"
#include "tests/lib/util.h"

typedef struct {
    TpDBusDaemon *dbus;
    ExampleContactListConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    GAsyncResult *prepare_result;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = test_dbus_daemon_dup_or_die ();

  test->service_conn = test_object_new_static_class (
        EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
        "account", "me@example.com",
        "protocol", "example-contact-list",
        NULL);
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "example",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  test_proxy_run_until_prepared (test->conn, features);

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));
}

static void
teardown (Test *test,
    gconstpointer data)
{
  TpConnection *conn;
  gboolean ok;
  GError *error = NULL;

  if (test->conn != NULL)
    {
      g_object_unref (test->conn);
      test->conn = NULL;
    }

  /* make a new TpConnection just to disconnect the underlying Connection,
   * so we don't leak it */
  conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);
  ok = tp_cli_connection_run_disconnect (conn, -1, &error, NULL);
  g_assert (ok);
  g_assert_no_error (error);
  g_assert (!tp_connection_run_until_ready (conn, FALSE, &error, NULL));
  g_assert_error (error, TP_ERRORS, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;
}

static void
test_nothing (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/contact-lists/prepare",
      Test, NULL, setup, test_nothing, teardown);

  return g_test_run ();
}
