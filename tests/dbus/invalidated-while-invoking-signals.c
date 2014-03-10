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

typedef struct {
    TpBaseConnection *service;
    TpConnection *client;
    GMainLoop *mainloop;
} Fixture;

static void
on_status_changed (TpConnection *connection,
                   guint status,
                   guint reason,
                   gpointer user_data,
                   GObject *weak_object)
{
  Fixture *f = user_data;

  MYASSERT (status == TP_CONNECTION_STATUS_DISCONNECTED, "%u", status);

  MYASSERT (f->client == connection, "%p vs %p", f->client, connection);
  g_object_unref (f->client);
  f->client = NULL;
}

static gboolean
disconnect (gpointer data)
{
  Fixture *f = data;

  tp_tests_simple_connection_inject_disconnect (
      TP_TESTS_SIMPLE_CONNECTION (f->service));

  return FALSE;
}

static void
on_shutdown_finished (TpBaseConnection *base_conn,
                      gpointer user_data)
{
  Fixture *f = user_data;

  g_main_loop_quit (f->mainloop);
}

static void
setup (Fixture *f,
    gconstpointer data)
{
  f->mainloop = g_main_loop_new (NULL, FALSE);

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@example.com", &f->service, &f->client);
}

static void
test_invalidated_while_invoking_signals (Fixture *f,
      gconstpointer data)
{
  g_signal_connect (f->service, "shutdown-finished",
      G_CALLBACK (on_shutdown_finished), f);

  MYASSERT (tp_cli_connection_connect_to_status_changed (f->client,
        on_status_changed, f, NULL, NULL, NULL), "");

  g_idle_add (disconnect, f);

  g_main_loop_run (f->mainloop);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  g_object_unref (f->service);
  g_object_unref (f->client);
  g_main_loop_unref (f->mainloop);
}

int
main (int argc,
    char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/invalidated-while-invoking-signals/dispose",
      Fixture, NULL, setup, test_invalidated_while_invoking_signals, teardown);

  return tp_tests_run_with_bus ();
}
