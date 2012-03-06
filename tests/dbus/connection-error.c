/* Feature test for ConnectionError signal emission
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

static int connection_errors;

static void
on_connection_error (TpConnection *conn,
                     const gchar *error,
                     GHashTable *details,
                     gpointer user_data,
                     GObject *weak_object)
{
  connection_errors++;
  g_assert_cmpstr (error, ==, "com.example.DomainSpecificError");
  g_assert_cmpuint (g_hash_table_size (details), ==, 0);
}

static void
on_status_changed (TpConnection *conn,
                   guint status,
                   guint reason,
                   gpointer user_data,
                   GObject *weak_object)
{
  g_assert_cmpuint (status, ==, TP_CONNECTION_STATUS_DISCONNECTED);
  g_assert_cmpuint (reason, ==, TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);
  g_main_loop_quit (user_data);
}

typedef enum
{
  DOMAIN_SPECIFIC_ERROR = 0,
} ExampleError;

/* example_com_error_get_type relies on this */
G_STATIC_ASSERT (sizeof (GType) <= sizeof (gsize));

static GType
example_com_error_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type))
    {
      static const GEnumValue values[] = {
            { DOMAIN_SPECIFIC_ERROR, "DOMAIN_SPECIFIC_ERROR",
              "DomainSpecificError" },
            { 0 }
      };
      GType gtype;

      gtype = g_enum_register_static ("ExampleError", values);
      g_once_init_leave (&type, gtype);
    }

  return (GType) type;
}

static GQuark
example_com_error_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string ("com.example");

      g_assert (sizeof (GQuark) <= sizeof (gsize));

      g_type_init ();
      dbus_g_error_domain_register (domain, "com.example",
          example_com_error_get_type ());
      g_once_init_leave (&quark, domain);
    }

  return (GQuark) quark;
}

typedef struct {
  GMainLoop *mainloop;
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *conn;
} Test;

static void
global_setup (void)
{
  static gboolean done = FALSE;

  if (done)
    return;

  done = TRUE;

  g_type_init ();
  tp_debug_set_flags ("all");

  tp_proxy_subclass_add_error_mapping (TP_TYPE_CONNECTION,
      "com.example", example_com_error_quark (), example_com_error_get_type ());
}

static void
setup (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  global_setup ();

  test->mainloop = g_main_loop_new (NULL, FALSE);

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &test->service_conn_as_base, &test->conn);
  test->service_conn = TP_TESTS_SIMPLE_CONNECTION (test->service_conn_as_base);
}

static void
teardown (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GAsyncResult *result = NULL;

  tp_connection_disconnect_async (test->conn, tp_tests_result_ready_cb,
      &result);
  tp_tests_run_until_result (&result);
  /* Ignore success/failure: it might already have gone */
  g_object_unref (result);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_main_loop_unref (test->mainloop);
}

static void
test_registered_error (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  const GHashTable *asv;
  gboolean ok;

  asv = GUINT_TO_POINTER (0xDEADBEEF);
  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, NULL), ==,
      NULL);
  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, &asv), ==,
      NULL);
  g_assert_cmpuint (GPOINTER_TO_UINT (asv), ==, 0xDEADBEEF);

  connection_errors = 0;
  tp_cli_connection_connect_to_connection_error (test->conn,
      on_connection_error, NULL, NULL, NULL, NULL);
  tp_cli_connection_connect_to_status_changed (test->conn, on_status_changed,
      test->mainloop, NULL, NULL, NULL);

  tp_base_connection_disconnect_with_dbus_error (test->service_conn_as_base,
      "com.example.DomainSpecificError", NULL,
      TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);

  g_main_loop_run (test->mainloop);

  g_assert_cmpuint (connection_errors, ==, 1);

  ok = tp_tests_proxy_run_until_prepared_or_failed (test->conn, NULL, &error);
  g_assert_error (error, example_com_error_quark (), DOMAIN_SPECIFIC_ERROR);
  g_assert (!ok);

  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, NULL), ==,
      "com.example.DomainSpecificError");
  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, &asv), ==,
      "com.example.DomainSpecificError");
  g_assert (asv != NULL);

  g_assert_cmpstr (g_quark_to_string (error->domain), ==,
      g_quark_to_string (example_com_error_quark ()));
  g_assert_cmpuint (error->code, ==, DOMAIN_SPECIFIC_ERROR);
  g_error_free (error);
  error = NULL;
}

static void
on_unregistered_connection_error (TpConnection *conn,
    const gchar *error,
    GHashTable *details,
    gpointer user_data,
    GObject *weak_object)
{
  connection_errors++;
  g_assert_cmpstr (error, ==, "net.example.WTF");
  g_assert_cmpuint (g_hash_table_size (details), ==, 0);
}

static void
test_unregistered_error (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  const GHashTable *asv;
  gboolean ok;

  connection_errors = 0;
  tp_cli_connection_connect_to_connection_error (test->conn,
      on_unregistered_connection_error, NULL, NULL, NULL, NULL);
  tp_cli_connection_connect_to_status_changed (test->conn, on_status_changed,
      test->mainloop, NULL, NULL, NULL);

  tp_base_connection_disconnect_with_dbus_error (test->service_conn_as_base,
      "net.example.WTF", NULL,
      TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);

  g_main_loop_run (test->mainloop);

  g_assert_cmpuint (connection_errors, ==, 1);

  ok = tp_tests_proxy_run_until_prepared_or_failed (test->conn, NULL, &error);
  /* Because we didn't understand net.example.WTF as a GError, TpConnection
   * falls back to turning the Connection_Status_Reason into a GError. */
  g_assert_error (error, TP_ERRORS, TP_ERROR_NETWORK_ERROR);
  g_assert (!ok);

  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, NULL), ==,
      "net.example.WTF");
  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, &asv), ==,
      "net.example.WTF");
  g_assert (asv != NULL);

  g_error_free (error);
  error = NULL;
}

int
main (int argc,
      char **argv)
{
  tp_tests_abort_after (10);
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/connection/registered-error", Test, NULL, setup,
      test_registered_error, teardown);
  g_test_add ("/connection/unregistered-error", Test, NULL, setup,
      test_unregistered_error, teardown);

  return g_test_run ();
}
