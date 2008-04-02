/* Regression test for https://bugs.freedesktop.org/show_bug.cgi?id=14854
 * (the original bug involved a TpChannel, but the principle is the same)
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

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"

static int fail = 0;
static GMainLoop *mainloop;

static void
myassert_failed (void)
{
  fail = 1;
}

static void
on_status_changed (TpConnection *connection,
                   guint status,
                   guint reason,
                   gpointer user_data,
                   GObject *weak_object)
{
  TpConnection **client = user_data;

  MYASSERT (status == TP_CONNECTION_STATUS_DISCONNECTED, "%u", status);

  MYASSERT (*client == connection, "%p vs %p", *client, connection);
  g_object_unref (*client);
  *client = NULL;

  g_main_loop_quit (mainloop);
}

static gboolean
disconnect (gpointer data)
{
  simple_connection_inject_disconnect (data);

  return FALSE;
}

int
main (int argc,
      char **argv)
{
  SimpleConnection *service;
  TpBaseConnection *service_as_base;
  TpDBusDaemon *dbus;
  TpConnection *client;
  GError *error = NULL;
  gchar *name;
  gchar *path;

  g_type_init ();
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);

  service = SIMPLE_CONNECTION (g_object_new (SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_as_base = TP_BASE_CONNECTION (service);
  MYASSERT (service != NULL, "");
  MYASSERT (service_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_as_base, "simple",
        &name, &path, &error), "");
  MYASSERT_NO_ERROR (error);

  dbus = tp_dbus_daemon_new (tp_get_bus ());
  client = tp_connection_new (dbus, name, path, &error);
  MYASSERT (client != NULL, "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_connection_run_until_ready (client, TRUE, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_cli_connection_connect_to_status_changed (client,
        on_status_changed, &client, NULL, NULL, NULL), "");

  g_idle_add (disconnect, service);

  g_main_loop_run (mainloop);

  g_message ("Cleaning up");
  service_as_base = NULL;
  g_object_unref (service);
  g_object_unref (dbus);
  g_main_loop_unref (mainloop);
  g_free (name);
  g_free (path);

  return fail;
}
