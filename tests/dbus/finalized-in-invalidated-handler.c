/* Regression test for https://bugs.freedesktop.org/show_bug.cgi?id=15644
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/echo-chan.h"
#include "tests/lib/util.h"

static GMainLoop *mainloop;
gboolean shutdown_finished = FALSE;
gboolean invalidated = FALSE;

static void
on_invalidated (TpChannel *chan,
                guint domain,
                gint code,
                gchar *message,
                gpointer user_data)
{
  TpChannel **client = user_data;

  MYASSERT (domain == TP_ERROR, ": domain \"%s\"",
      g_quark_to_string (domain));
  MYASSERT (code == TP_ERROR_CANCELLED, ": code %u", code);

  MYASSERT (*client == chan, "%p vs %p", *client, chan);
  g_object_unref (*client);
  *client = NULL;

  invalidated = TRUE;

  if (shutdown_finished)
    g_main_loop_quit (mainloop);
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
  shutdown_finished = TRUE;

  if (invalidated)
    g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TpTestsEchoChannel *service_chan;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *chan_path;
  TpHandle handle;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &conn);
  service_conn = TP_TESTS_SIMPLE_CONNECTION (service_conn_as_base);

  g_signal_connect (service_conn, "shutdown-finished",
      G_CALLBACK (on_shutdown_finished), NULL);

  /* Paste on a channel */

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  handle = tp_handle_ensure (contact_repo, "them@example.org", NULL, &error);
  g_assert_no_error (error);
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (conn));

  service_chan = TP_TESTS_ECHO_CHANNEL (tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", service_conn,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  chan = tp_tests_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  g_signal_connect (chan, "invalidated", G_CALLBACK (on_invalidated),
      &chan);

  g_idle_add (disconnect, service_conn);

  g_main_loop_run (mainloop);

  g_message ("Cleaning up");

  g_object_unref (conn);
  g_assert (chan == NULL);

  g_object_unref (service_chan);
  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_main_loop_unref (mainloop);
  g_free (chan_path);

  return 0;
}
