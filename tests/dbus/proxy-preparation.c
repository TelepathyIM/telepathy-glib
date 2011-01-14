/* Tests of TpBaseClient
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/my-conn-proxy.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;

    /* Client side objects */
    TpConnection *connection;
    TpTestsMyConnProxy *my_conn;

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

  test->my_conn = g_object_new (TP_TESTS_TYPE_MY_CONN_PROXY,
      "dbus-daemon", test->dbus,
      "bus-name", tp_proxy_get_bus_name (test->connection),
      "object-path", tp_proxy_get_object_path (test->connection),
      NULL);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
  g_object_unref (test->my_conn);
}

static void
prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (TP_PROXY (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_prepare_capabilities (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Prepare capabilities on a new proxy. Core should be prepared *before*
   * checking if Requests is implemented */
  GQuark features[] = { TP_CONNECTION_FEATURE_CAPABILITIES, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_CONNECTION_FEATURE_CAPABILITIES));
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/proxy-preparation/prepare-capabilities", Test, NULL, setup,
      test_prepare_capabilities, teardown);

  return g_test_run ();
}
