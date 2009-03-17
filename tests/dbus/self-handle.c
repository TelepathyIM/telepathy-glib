/* Feature test for the user's self-handle changing.
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
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static void
on_self_handle_changed (TpConnection *client_conn,
                        GParamSpec *param_spec G_GNUC_UNUSED,
                        gpointer user_data)
{
  guint *times = user_data;

  ++*times;
}

static void
test_self_handle (SimpleConnection *service_conn,
                  TpConnection *client_conn)
{
  TpBaseConnection *service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
      service_conn_as_base, TP_HANDLE_TYPE_CONTACT);
  TpHandle handle;
  guint times = 0;

  g_signal_connect (client_conn, "notify::self-handle",
      G_CALLBACK (on_self_handle_changed), &times);

  MYASSERT_SAME_STRING (tp_handle_inspect (contact_repo,
        tp_base_connection_get_self_handle (service_conn_as_base)),
      "me@example.com");

  MYASSERT_SAME_UINT (tp_connection_get_self_handle (client_conn),
      tp_base_connection_get_self_handle (service_conn_as_base));

  g_object_get (client_conn,
      "self-handle", &handle,
      NULL);
  MYASSERT_SAME_UINT (handle,
      tp_base_connection_get_self_handle (service_conn_as_base));

  MYASSERT_SAME_UINT (times, 0);

  /* similar to /nick in IRC */
  simple_connection_set_identifier (service_conn, "myself@example.org");
  test_connection_run_until_dbus_queue_processed (client_conn);
  MYASSERT_SAME_UINT (times, 1);

  MYASSERT_SAME_STRING (tp_handle_inspect (contact_repo,
        tp_base_connection_get_self_handle (service_conn_as_base)),
      "myself@example.org");

  MYASSERT_SAME_UINT (tp_connection_get_self_handle (client_conn),
      tp_base_connection_get_self_handle (service_conn_as_base));

  g_object_get (client_conn,
      "self-handle", &handle,
      NULL);
  MYASSERT_SAME_UINT (handle,
      tp_base_connection_get_self_handle (service_conn_as_base));
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
  TpConnection *client_conn;

  /* Setup */

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_dbus_daemon_new (tp_get_bus ());

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

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  /* Tests */

  test_self_handle (service_conn, client_conn);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  test_assert_no_error (error);
  g_object_unref (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);

  return 0;
}
