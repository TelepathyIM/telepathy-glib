/* Tests of TpBaseClient
 *
 * Copyright © 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

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
disconnect_and_destroy_conn (Test *test)
{
  tp_tests_connection_assert_disconnect_succeeds (
      TP_CONNECTION (test->my_conn));

  tp_clear_object (&test->connection);
  tp_clear_object (&test->base_connection);
  tp_clear_object (&test->my_conn);

}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  disconnect_and_destroy_conn (test);
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
  /* Prepare capabilities on a new proxy. CORE should be automatically prepared
   * *before* checking if Requests is implemented as
   * tp_proxy_has_interface_by_id() can't work without CORE. */
  GQuark features[] = { TP_CONNECTION_FEATURE_CAPABILITIES, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_CONNECTION_FEATURE_CAPABILITIES));
}

static void
test_prepare_core (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Test than preparing the 'top' core feature prepare the other core
   * features as well */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn, TP_CONNECTION_FEATURE_CORE));

  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_CONNECTION_FEATURE_CAPABILITIES));
}

static void
test_depends (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Test if A is automatically prepared when preparing B */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_B, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_A));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_B));
}

static void
test_wrong_iface (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Feature can't be prepared because proxy doesn't support the right
   * interface */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_WRONG_IFACE, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_WRONG_IFACE));
}

static void
test_bad_dep (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Feature can't be prepared because it depends on an unpreparable
   * feature */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_BAD_DEP, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_WRONG_IFACE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_BAD_DEP));
}

static void
test_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Feature preparation fails */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_FAIL, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_FAIL));
}

static void
test_fail_dep (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Feature can't be prepared because its deps can't be prepared */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_FAIL_DEP, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_FAIL));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_FAIL_DEP));
}

static void
test_retry (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* We have the prepare the feature twice */
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY, 0 };

  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY));

  /* second attempt */
  test->my_conn->retry_feature_success = TRUE;
  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY));
}

static void
test_retry_dep (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* RETRY_DEP depends on a feature having can_retry and which failed, so
   * preparing RETRY_DEP will re-prepare it successfully */
  GQuark features_retry[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY, 0 };
  GQuark features_retry_dep[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY_DEP, 0 };

  /* Try preparing TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY, will fail */
  tp_proxy_prepare_async (test->my_conn, features_retry, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY));
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY_DEP));

  /* Try prepare TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY_DEP, will re-prepare
   * TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY */
  test->my_conn->retry_feature_success = TRUE;
  tp_proxy_prepare_async (test->my_conn, features_retry_dep, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY));
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_RETRY_DEP));
}

static void
recreate_connection (Test *test)
{
  gchar *name;
  gchar *conn_path;

  disconnect_and_destroy_conn (test);

  test->base_connection = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "account", "me@test.com",
      "protocol", "simple",
      NULL);
  g_assert (test->base_connection != NULL);

  g_assert (tp_base_connection_register (test->base_connection, "simple",
        &name, &conn_path, &test->error));
  g_assert_no_error (test->error);

  test->connection = tp_connection_new (test->dbus, name, conn_path,
      &test->error);
  g_assert_no_error (test->error);

  test->my_conn = g_object_new (TP_TESTS_TYPE_MY_CONN_PROXY,
      "dbus-daemon", test->dbus,
      "bus-name", tp_proxy_get_bus_name (test->connection),
      "object-path", tp_proxy_get_object_path (test->connection),
      NULL);
  g_assert (test->my_conn != NULL);

  g_free (name);
  g_free (conn_path);
}

static void
test_before_connected (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_BEFORE_CONNECTED, 0 };
  GQuark connected[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  /* We need a not yet connected connection */
  recreate_connection (test);

  g_assert_cmpuint (test->my_conn->before_connected_state, ==,
      BEFORE_CONNECTED_STATE_UNPREPARED);

  /* Connection is not yet connected, prepare the feature */
  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_BEFORE_CONNECTED));

  g_assert_cmpuint (test->my_conn->before_connected_state, ==,
      BEFORE_CONNECTED_STATE_NOT_CONNECTED);

  tp_cli_connection_call_connect (test->connection, -1, NULL, NULL, NULL, NULL);

  /* Wait that CONNECTED is announced */
  tp_proxy_prepare_async (test->my_conn, connected, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* state has been updated */
  g_assert_cmpuint (test->my_conn->before_connected_state, ==,
      BEFORE_CONNECTED_STATE_CONNECTED);
}

static void
test_interface_later (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TESTS_MY_CONN_PROXY_FEATURE_INTERFACE_LATER, 0 };
  GQuark connected[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  const gchar *interfaces[] = { TP_TESTS_MY_CONN_PROXY_IFACE_LATER, NULL };

  /* We need a not yet connected connection */
  recreate_connection (test);

  /* Try preparing before the connection is connected */
  tp_proxy_prepare_async (test->my_conn, features, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Feature isn't prepared */
  g_assert (!tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_INTERFACE_LATER));

  tp_cli_connection_call_connect (test->connection, -1, NULL, NULL, NULL, NULL);

  /* While connecting the interface is added */
  tp_base_connection_add_interfaces (test->base_connection, interfaces);

  /* Wait that CONNECTED is announced */
  tp_proxy_prepare_async (test->my_conn, connected, prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* The feature has been prepared now */
  g_assert (tp_proxy_is_prepared (test->my_conn,
        TP_TESTS_MY_CONN_PROXY_FEATURE_INTERFACE_LATER));
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/proxy-preparation/prepare-capabilities", Test, NULL, setup,
      test_prepare_capabilities, teardown);
  g_test_add ("/proxy-preparation/prepare-core", Test, NULL, setup,
      test_prepare_core, teardown);
  g_test_add ("/proxy-preparation/depends", Test, NULL, setup,
      test_depends, teardown);
  g_test_add ("/proxy-preparation/wrong-iface", Test, NULL, setup,
      test_wrong_iface, teardown);
  g_test_add ("/proxy-preparation/bad-dep", Test, NULL, setup,
      test_bad_dep, teardown);
  g_test_add ("/proxy-preparation/fail", Test, NULL, setup,
      test_fail, teardown);
  g_test_add ("/proxy-preparation/fail-dep", Test, NULL, setup,
      test_fail_dep, teardown);
  g_test_add ("/proxy-preparation/retry", Test, NULL, setup,
      test_retry, teardown);
  g_test_add ("/proxy-preparation/retry-dep", Test, NULL, setup,
      test_retry_dep, teardown);
  g_test_add ("/proxy-preparation/before-connected", Test, NULL, setup,
      test_before_connected, teardown);
  g_test_add ("/proxy-preparation/interface-later", Test, NULL, setup,
      test_interface_later, teardown);

  return g_test_run ();
}
