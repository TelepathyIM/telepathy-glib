/* Basic introspection on a channel (template for further regression tests)
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
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"

static int fail = 0;
static GError *invalidated = NULL;
static GMainLoop *mainloop;

static void
myassert_failed (void)
{
  fail = 1;
}

static void
channel_ready (TpChannel *channel,
               const GError *error,
               gpointer user_data)
{
  gboolean *set = user_data;

  *set = TRUE;

  if (error == NULL)
    {
      g_message ("channel %p ready", channel);
    }
  else
    {
      g_message ("channel %p invalidated: %s #%u \"%s\"", channel,
          g_quark_to_string (error->domain), error->code, error->message);

      invalidated = g_error_copy (error);
    }

  if (mainloop != NULL)
    g_main_loop_quit (mainloop);
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
  gboolean was_ready;
  GError invalidated_for_test = { TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "No channel for you!" };

  g_type_init ();
  tp_debug_set_flags ("all");

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

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);

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

  mainloop = g_main_loop_new (NULL, FALSE);

  /* Channel becomes invalid while we wait */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);
  tp_proxy_invalidate ((TpProxy *) chan, &invalidated_for_test);

  MYASSERT (!tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT (error != NULL, "");
  MYASSERT_SAME_ERROR (&invalidated_for_test, error);
  g_error_free (error);
  error = NULL;

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes invalid and we are called back synchronously */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  tp_proxy_invalidate ((TpProxy *) chan, &invalidated_for_test);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT (invalidated != NULL, "");
  MYASSERT_SAME_ERROR (&invalidated_for_test, invalidated);
  g_error_free (invalidated);
  invalidated = NULL;

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes ready while we wait */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes ready while we wait (in the case where we have to discover
   * the channel type) */

  chan = tp_channel_new (conn, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes ready while we wait (in the case where we have to discover
   * the handle type) */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes ready while we wait (in the case where we have to discover
   * the handle) */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, 0, &error);
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  g_object_unref (chan);
  chan = NULL;

  /* Channel becomes ready and we are called back */

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  MYASSERT_NO_ERROR (error);

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  g_message ("Entering main loop");
  g_main_loop_run (mainloop);
  g_message ("Leaving main loop");
  MYASSERT (was_ready == TRUE, "");
  MYASSERT_NO_ERROR (invalidated);

  /* ... keep the same channel for the next test */

  /* Channel already ready, so we are called back synchronously */

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT_NO_ERROR (invalidated);

  /* ... keep the same channel for the next test */

  /* Channel already dead, so we are called back synchronously */

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT (invalidated != NULL, "");
  MYASSERT (invalidated->domain == TP_ERRORS_DISCONNECTED,
      "%s", g_quark_to_string (invalidated->domain));
  MYASSERT (invalidated->code == TP_CONNECTION_STATUS_REASON_REQUESTED,
      "%u", invalidated->code);
  g_error_free (invalidated);
  invalidated = NULL;

  g_main_loop_unref (mainloop);
  mainloop = NULL;

  tp_handle_unref (contact_repo, handle);
  g_object_unref (chan);
  g_object_unref (conn);
  g_object_unref (service_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  return fail;
}
