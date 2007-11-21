/*
 * telepathy-example-inspect-cm - inspect a connection manager
 *
 * Usage:
 *
 * telepathy-example-inspect-cm gabble
 *    Inspect the Gabble connection manager, by reading the installed
 *    .manager file if available, and introspecting the running CM if not
 *
 * telepathy-example-inspect-cm gabble data/gabble.manager
 *    As above, but assume the given filename is correct
 *
 * telepathy-example-inspect-cm gabble ""
 *    Don't read any .manager file, just introspect the running CM
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

static void
connection_manager_got_info (TpConnectionManager *cm,
                             guint source,
                             GMainLoop *mainloop)
{
  static int counter = 0;

  g_message ("Emitted got-info (source=%d)", source);

  if (source > 0 || ++counter >= 2)
    g_main_loop_quit (mainloop);
}

gboolean
time_out (gpointer mainloop)
{
  g_message ("Timed out");
  g_main_loop_quit (mainloop);
  return FALSE;
}

int
main (int argc,
      char **argv)
{
  const gchar *cm_name, *manager_file;
  TpConnectionManager *cm;
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("TP_EXAMPLES_DEBUG"));

  if (argc < 2)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  cm_name = argv[1];
  manager_file = argv[2];   /* possibly NULL */

  cm = tp_connection_manager_new (tp_dbus_daemon_new (tp_get_bus ()),
      cm_name, manager_file);
  g_signal_connect (cm, "got-info",
      G_CALLBACK (connection_manager_got_info), mainloop);

  g_timeout_add (5000, time_out, mainloop);

  g_main_loop_run (mainloop);

  g_object_unref (cm);
  g_main_loop_unref (mainloop);
  return 0;
}
