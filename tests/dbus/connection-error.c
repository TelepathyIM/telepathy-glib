/* Feature test for ConnectionError signal emission
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
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
  MYASSERT_SAME_STRING (error, "com.example.DomainSpecificError");
  MYASSERT_SAME_UINT (g_hash_table_size (details), 0);
}

static void
on_status_changed (TpConnection *conn,
                   guint status,
                   guint reason,
                   gpointer user_data,
                   GObject *weak_object)
{
  MYASSERT_SAME_UINT (status, TP_CONNECTION_STATUS_DISCONNECTED);
  MYASSERT_SAME_UINT (reason, TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);
  g_main_loop_quit (user_data);
}

typedef enum
{
  DOMAIN_SPECIFIC_ERROR = 0,
} ExampleError;

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

      tp_verify_statement (sizeof (GType) <= sizeof (gsize));

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

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *dbus;
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error = NULL;
  TpConnection *conn;
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);
  dbus = tp_dbus_daemon_new (tp_get_bus ());

  tp_proxy_subclass_add_error_mapping (TP_TYPE_CONNECTION,
      "com.example", example_com_error_quark (), example_com_error_get_type ());

  service_conn = SIMPLE_CONNECTION (g_object_new (
        SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  connection_errors = 0;
  tp_cli_connection_connect_to_connection_error (conn, on_connection_error,
      NULL, NULL, NULL, NULL);
  tp_cli_connection_connect_to_status_changed (conn, on_status_changed,
      mainloop, NULL, NULL, NULL);

  tp_base_connection_disconnect_with_dbus_error (service_conn_as_base,
      "com.example.DomainSpecificError", NULL,
      TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);

  g_main_loop_run (mainloop);

  MYASSERT_SAME_UINT (connection_errors, 1);

  MYASSERT (!tp_connection_run_until_ready (conn, FALSE, &error, NULL), "");
  MYASSERT_SAME_STRING (g_quark_to_string (error->domain),
      g_quark_to_string (example_com_error_quark ()));
  MYASSERT_SAME_UINT (error->code, DOMAIN_SPECIFIC_ERROR);
  g_error_free (error);
  error = NULL;

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);
  g_main_loop_unref (mainloop);

  return 0;
}
