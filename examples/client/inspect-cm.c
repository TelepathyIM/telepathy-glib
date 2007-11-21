/*
 * telepathy-example-observe-cms - inspect running connection managers
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
  if (++counter >= 2)
    g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *cm_name;
  TpConnectionManager *cm;
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("TP_EXAMPLES_DEBUG"));

  if (argc < 2)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  cm_name = argv[1];

  cm = tp_connection_manager_new (tp_dbus_daemon_new (tp_get_bus ()),
      cm_name);
  g_signal_connect (cm, "got-info",
      G_CALLBACK (connection_manager_got_info), mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
