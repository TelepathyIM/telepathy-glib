/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=15300
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/util.h"

typedef struct {
    TpDBusDaemon *dbus;
    SimpleConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    gboolean cwr_ready;
    GError *cwr_error /* initialized in setup */;
} Test;

static GError invalidated_for_test = { 0, TP_ERROR_PERMISSION_DENIED,
      "No connection for you!" };

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;

  invalidated_for_test.domain = TP_ERRORS;

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = test_dbus_daemon_dup_or_die ();

  test->service_conn = SIMPLE_CONNECTION (g_object_new (
        SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple-protocol",
        NULL));
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "simple",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;
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

  /* disconnect the connection so we don't leak it */
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
test_run_until_invalid (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);
  tp_proxy_invalidate ((TpProxy *) test->conn, &invalidated_for_test);

  MYASSERT (!tp_connection_run_until_ready (test->conn, TRUE, &error, NULL),
      "");
  g_assert (error != NULL);
  MYASSERT_SAME_ERROR (&invalidated_for_test, error);
  g_error_free (error);
}

static void
test_run_until_ready (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  MYASSERT (tp_connection_run_until_ready (test->conn, TRUE, &error, NULL),
      "");
  g_assert_no_error (error);
}

static void
conn_ready (TpConnection *connection,
            const GError *error,
            gpointer user_data)
{
  Test *test = user_data;

  test->cwr_ready = TRUE;

  if (error == NULL)
    {
      gboolean parsed;
      gchar *proto = NULL;
      gchar *cm_name = NULL;

      g_message ("connection %p ready", connection);
      parsed = tp_connection_parse_object_path (connection, &proto, &cm_name);
      g_assert (parsed);
      g_assert_cmpstr (proto, ==, "simple-protocol");
      g_assert_cmpstr (cm_name, ==, "simple");
      g_free (proto);
      g_free (cm_name);
    }
  else
    {
      g_message ("connection %p invalidated: %s #%u \"%s\"", connection,
          g_quark_to_string (error->domain), error->code, error->message);

      test->cwr_error = g_error_copy (error);
    }
}

static void
test_call_when_ready (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);

  tp_connection_call_when_ready (test->conn, conn_ready, test);

  while (!test->cwr_ready)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (test->cwr_error);

  /* Connection already ready here, so we are called back synchronously */

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;
  tp_connection_call_when_ready (test->conn, conn_ready, test);
  g_assert_cmpint (test->cwr_ready, ==, TRUE);
  g_assert_no_error (test->cwr_error);
}

static void
test_call_when_invalid (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  /* Connection becomes invalid, so we are called back synchronously */

  tp_connection_call_when_ready (test->conn, conn_ready, test);
  tp_proxy_invalidate ((TpProxy *) test->conn, &invalidated_for_test);
  g_assert_cmpint (test->cwr_ready, ==, TRUE);
  MYASSERT_SAME_ERROR (&invalidated_for_test, test->cwr_error);
  g_clear_error (&test->cwr_error);

  /* Connection already invalid, so we are called back synchronously */

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;
  tp_connection_call_when_ready (test->conn, conn_ready, test);
  g_assert (test->cwr_ready);
  MYASSERT_SAME_ERROR (&invalidated_for_test, test->cwr_error);
  g_error_free (test->cwr_error);
  test->cwr_error = NULL;
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/conn/run_until_invalid", Test, NULL, setup,
      test_run_until_invalid, teardown);
  g_test_add ("/conn/run_until_ready", Test, NULL, setup,
      test_run_until_ready, teardown);
  g_test_add ("/conn/call_when_ready", Test, NULL, setup,
      test_call_when_ready, teardown);
  g_test_add ("/conn/call_when_invalid", Test, NULL, setup,
      test_call_when_invalid, teardown);

  return g_test_run ();
}
