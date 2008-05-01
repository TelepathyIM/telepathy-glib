/* Regression test for https://bugs.freedesktop.org/show_bug.cgi?id=15644
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"

static int fail = 0;
static GMainLoop *mainloop;

static void
myassert_failed (void)
{
  fail = 1;
}

static void
on_invalidated (TpChannel *chan,
                guint domain,
                gint code,
                gchar *message,
                gpointer user_data)
{
  TpChannel **client = user_data;

  MYASSERT (domain == TP_ERRORS_DISCONNECTED, ": domain \"%s\"",
      g_quark_to_string (domain));
  MYASSERT (code == TP_CONNECTION_STATUS_REASON_REQUESTED, ": code %u", code);

  MYASSERT (*client == chan, "%p vs %p", *client, chan);
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
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TestTextChannelNull *service_chan;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;
  TpHandle handle;

  g_type_init ();
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);

  service_conn = SIMPLE_CONNECTION (g_object_new (SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  MYASSERT_NO_ERROR (error);

  dbus = tp_dbus_daemon_new (tp_get_bus ());
  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  /* Paste on a channel */

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  handle = tp_handle_ensure (contact_repo, "them@example.org", NULL, &error);
  MYASSERT_NO_ERROR (error);
  chan_path = g_strdup_printf ("%s/Channel", conn_path);

  service_chan = TEST_TEXT_CHANNEL_NULL (g_object_new (
        TEST_TYPE_TEXT_CHANNEL_NULL,
        "connection", service_conn,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);

  tp_channel_run_until_ready (chan, &error, NULL);
  MYASSERT_NO_ERROR (error);

  g_signal_connect (chan, "invalidated", G_CALLBACK (on_invalidated),
      &chan);

  g_idle_add (disconnect, service_conn);

  g_main_loop_run (mainloop);

  g_message ("Cleaning up");

  tp_handle_unref (contact_repo, handle);
  g_object_unref (conn);
  g_assert (chan == NULL);

  g_object_unref (service_chan);
  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_main_loop_unref (mainloop);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  return fail;
}
