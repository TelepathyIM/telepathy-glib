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
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

typedef struct {
    const gchar *mode;
    TpBaseConnection *service;
    TpConnection *client;
    gboolean shutdown_finished;
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

  if (!tp_strdiff (f->mode, "invalidate"))
    {
      GError e = { TP_ERROR, TP_ERROR_CANCELLED, "regression test" };

      tp_proxy_invalidate (TP_PROXY (f->client), &e);
    }
  else
    {
      /* The original test did this, and assumed that this was the
       * last-unref, and would cause invalidation. That was a failing
       * test-case for #14854 before it was fixed. However, the fix for
       * #14854 made that untrue, by taking a reference. */
      g_object_unref (f->client);
      f->client = NULL;
    }
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

  f->shutdown_finished = TRUE;
}

static void
setup (Fixture *f,
    gconstpointer data)
{
  f->mode = data;
  f->shutdown_finished = FALSE;

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

  if (!tp_strdiff (f->mode, "invalidate"))
    {
      while (tp_proxy_get_invalidated (f->client) == NULL ||
          !f->shutdown_finished)
        g_main_context_iteration (NULL, TRUE);
    }
  else
    {
      while (f->client != NULL || !f->shutdown_finished)
        g_main_context_iteration (NULL, TRUE);
    }
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  g_object_unref (f->service);
  g_clear_object (&f->client);
}

int
main (int argc,
    char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/invalidated-while-invoking-signals/dispose",
      Fixture, NULL, setup, test_invalidated_while_invoking_signals, teardown);
  g_test_add ("/invalidated-while-invoking-signals/invalidate",
      Fixture, "invalidate", setup, test_invalidated_while_invoking_signals,
      teardown);

  return tp_tests_run_with_bus ();
}
