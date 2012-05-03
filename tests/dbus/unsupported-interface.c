/* Regression test for unsupported interfaces on objects.
 *
 * Copyright © 2007-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved. No warranty.
 */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

typedef struct {
    TpDBusDaemon *dbus;
    TpTestsSimpleConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    guint wait;
    gboolean reentrant;
    gboolean freed;
    GError *error /* initialized by GTest */;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");
  f->dbus = tp_tests_dbus_daemon_dup_or_die ();

  f->service_conn = TP_TESTS_SIMPLE_CONNECTION (
    tp_tests_object_new_static_class (
        TP_TESTS_TYPE_CONTACTS_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple-protocol",
        NULL));
  f->service_conn_as_base = TP_BASE_CONNECTION (f->service_conn);
  g_assert (f->service_conn != NULL);
  g_assert (f->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (f->service_conn_as_base, "simple",
        &f->conn_name, &f->conn_path, &f->error));
  g_assert_no_error (f->error);

  f->conn = tp_connection_new (f->dbus, f->conn_name, f->conn_path,
      &f->error);
  g_assert_no_error (f->error);
}

static void
test_supported_run (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_cli_connection_run_connect (f->conn, -1, &f->error, NULL);
  g_assert_no_error (f->error);
  g_assert (ok);
}

static void
test_unsupported_run (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_cli_connection_interface_mail_notification_run_request_inbox_url (
      f->conn, -1, NULL /* "out" arg */, &f->error, NULL);
  g_assert_error (f->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_NO_INTERFACE);
  g_assert (!ok);
}

static void
pretend_to_free (gpointer p)
{
  Fixture *f = p;

  g_assert (!f->freed);
  f->freed = TRUE;
}

static void
connect_cb (TpConnection *conn,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Fixture *f = user_data;

  g_assert_no_error (f->error);
  g_assert (!f->reentrant);
  g_assert (!f->freed);

  if (error != NULL)
    f->error = g_error_copy (error);

  f->wait--;
}

static void
test_supported_async (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  TpProxyPendingCall *call;

  f->reentrant = TRUE;
  f->wait = 1;
  call = tp_cli_connection_call_connect (f->conn, -1, connect_cb,
      f, pretend_to_free, NULL);
  f->reentrant = FALSE;

  g_assert (call != NULL);
  g_assert (!f->freed);

  while (f->wait)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (f->error);
  g_assert (f->freed);
}

static void
inbox_url_cb (TpConnection *conn,
    const GValueArray *va,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Fixture *f = user_data;

  g_assert_no_error (f->error);
  /* Unsupported interfaces are signalled by a re-entrant callback in 0.x */
  g_assert (f->reentrant);
  g_assert (!f->freed);

  if (error != NULL)
    f->error = g_error_copy (error);

  f->wait--;
}

static void
test_unsupported_async (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  TpProxyPendingCall *call;

  f->reentrant = TRUE;
  f->wait = 1;
  call = tp_cli_connection_interface_mail_notification_call_request_inbox_url (
      f->conn, -1, inbox_url_cb, f, pretend_to_free, NULL);
  f->reentrant = FALSE;

  /* Unsupported interfaces are signalled by a re-entrant callback in 0.x */
  g_assert (call == NULL);
  g_assert (f->freed);

  while (f->wait)
    g_main_context_iteration (NULL, TRUE);

  g_assert_error (f->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_NO_INTERFACE);
}

static void
do_nothing (TpConnection *conn,
    ...)
{
}

static void
test_supported_signal (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  TpProxySignalConnection *sc;

  sc = tp_cli_connection_connect_to_status_changed (f->conn,
      (void (*)(TpConnection *, guint, guint, gpointer, GObject *)) do_nothing,
      f, pretend_to_free, NULL, &f->error);

  g_assert_no_error (f->error);
  g_assert (sc != NULL);
  g_assert (!f->freed);

  tp_proxy_signal_connection_disconnect (sc);
  g_assert (f->freed);
}

static void
test_unsupported_signal (Fixture *f,
    gconstpointer nil G_GNUC_UNUSED)
{
  TpProxySignalConnection *sc;

  sc = tp_cli_connection_interface_mail_notification_connect_to_mails_received (
      f->conn,
      (void (*)(TpConnection *, const GPtrArray *, gpointer, GObject *)) do_nothing,
      f, pretend_to_free, NULL, &f->error);

  g_assert_error (f->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_NO_INTERFACE);
  g_assert (sc == NULL);
  g_assert (f->freed);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  TpConnection *conn;

  g_clear_error (&f->error);

  g_clear_object (&f->conn);

  /* disconnect the connection so we don't leak it */
  conn = tp_connection_new (f->dbus, f->conn_name, f->conn_path,
      &f->error);
  g_assert (conn != NULL);
  g_assert_no_error (f->error);

  tp_tests_connection_assert_disconnect_succeeds (conn);
  tp_tests_proxy_run_until_prepared_or_failed (conn, NULL, &f->error);
  g_assert_error (f->error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&f->error);

  g_object_unref (conn);

  /* borrowed from service_conn */
  f->service_conn_as_base = NULL;

  g_clear_object (&f->service_conn);
  g_free (f->conn_name);
  g_free (f->conn_path);

  g_clear_object (&f->dbus);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add ("/supported/run", Fixture, NULL,
      setup, test_supported_run, teardown);
  g_test_add ("/supported/async", Fixture, NULL,
      setup, test_supported_async, teardown);
  g_test_add ("/supported/signal", Fixture, NULL,
      setup, test_supported_signal, teardown);
  g_test_add ("/unsupported/run", Fixture, NULL,
      setup, test_unsupported_run, teardown);
  g_test_add ("/unsupported/async", Fixture, NULL,
      setup, test_unsupported_async, teardown);
  g_test_add ("/unsupported/signal", Fixture, NULL,
      setup, test_unsupported_signal, teardown);

  return g_test_run ();
}
