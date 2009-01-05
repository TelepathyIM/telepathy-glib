/* Test TpChannel's group code.
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
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/util.h"

static int fail = 0;
static GMainLoop *mainloop;
TestTextChannelGroup *service_chan_1, *service_chan_2;
TpHandleRepoIface *contact_repo;
TpHandle self_handle;

static void
myassert_failed (void)
{
  fail = 1;
}

int
main (int argc,
      char **argv)
{
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path_1, *chan_path_2;

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
  self_handle = tp_handle_ensure (contact_repo, "me@example.com", NULL, NULL);

  chan_path_1 = g_strdup_printf ("%s/Channel1", conn_path);
  service_chan_1 = TEST_TEXT_CHANNEL_GROUP (g_object_new (
        TEST_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", chan_path_1,
        "detailed", FALSE,
        NULL));

  chan_path_2 = g_strdup_printf ("%s/Channel2", conn_path);
  service_chan_2 = TEST_TEXT_CHANNEL_GROUP (g_object_new (
        TEST_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", chan_path_2,
        "detailed", TRUE,
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  /* Tests go here.
   *
   *  - check that group-members-changed and group-members-changed-detailed
   *    both fire once per change on a channel without the Detailed flag.
   *  - ditto for a channel with the detailed flag.
   *  - in both cases, check that the client-side cache matches reality.
   *  - if feeling very keen, emit spurious MembersChanged signals on a channel
   *    without Detailed set (and vice versa), and check they're ignored.
   */

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  /* clean up */

  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);
  g_object_unref (service_chan_2);
  g_object_unref (service_chan_1);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path_1);
  g_free (chan_path_2);

  return fail;
}
