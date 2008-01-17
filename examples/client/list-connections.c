/*
 * telepathy-example-list-managers - list installed connection managers
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

void
got_connections (const gchar * const *bus_names,
                 gsize n,
                 const gchar * const *cms,
                 const gchar * const *protocols,
                 const GError *error,
                 gpointer user_data,
                 GObject *unused)
{
  GMainLoop *mainloop = user_data;

  if (error != NULL)
    {
      g_warning ("%s", error->message);
    }
  else
    {
      gsize i;

      g_message ("Found %" G_GSIZE_FORMAT " connections:", n);

      for (i = 0; i < n; i++)
        {
          g_message ("%s", bus_names[i]);
          g_message ("- CM %s, protocol %s", cms[i], protocols[i]);
        }

      /* all the arrays are also NULL-terminated */
      g_assert (bus_names[n] == NULL && cms[n] == NULL &&
          protocols[n] == NULL);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  mainloop = g_main_loop_new (NULL, FALSE);

  tp_list_connection_names (tp_dbus_daemon_new (tp_get_bus ()),
      got_connections, mainloop, NULL, NULL);

  g_main_loop_run (mainloop);

  g_main_loop_unref (mainloop);
  return 0;
}
