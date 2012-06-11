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

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

static GMainLoop *mainloop;

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
}

static gboolean
disconnect (gpointer data)
{
  tp_tests_simple_connection_inject_disconnect (data);

  return FALSE;
}

static void
on_shutdown_finished (TpBaseConnection *base_conn,
                      gpointer user_data)
{
  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  TpBaseConnection *service;
  TpConnection *client;

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@example.com", &service, &client);

  g_signal_connect (service, "shutdown-finished",
      G_CALLBACK (on_shutdown_finished), NULL);

  MYASSERT (tp_cli_connection_connect_to_status_changed (client,
        on_status_changed, &client, NULL, NULL, NULL), "");

  g_idle_add (disconnect, service);

  g_main_loop_run (mainloop);

  g_message ("Cleaning up");
  g_object_unref (service);
  g_object_unref (client);
  g_main_loop_unref (mainloop);

  return 0;
}
