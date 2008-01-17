/*
 * telepathy-example-inspect-connection - inspect a connection
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

void
got_channels (TpConnection *connection,
              const GPtrArray *channels,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  GMainLoop *mainloop = user_data;

  if (error == NULL)
    {
      guint i;

      for (i = 0; i < channels->len; i++)
        {
          GValueArray *channel = g_ptr_array_index (channels, i);

          printf ("Channel: %s\n",
              (const gchar *) g_value_get_boxed (channel->values));
          printf ("\tType: %s\n",
              g_value_get_string (channel->values + 1));
          printf ("\tHandle: type %u, #%u\n",
              g_value_get_uint (channel->values + 2),
              g_value_get_uint (channel->values + 3)
              );
        }
    }
  else
    {
      printf ("Error listing channels: %s", error->message);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *bus_name, *object_path;
  TpConnection *connection;
  GMainLoop *mainloop;
  TpDBusDaemon *daemon;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 2)
    {
      fputs ("Usage: one of\n"
          "    telepathy-example-inspect-connection BUS_NAME\n"
          "    telepathy-example-inspect-connection OBJECT_PATH\n"
          "    telepathy-example-inspect-connection BUS_NAME OBJECT_PATH\n",
          stderr);
      return 2;
    }

  mainloop = g_main_loop_new (NULL, FALSE);

  bus_name = argv[1];
  object_path = argv[2];    /* might be NULL */

  /* Cope with the arguments being a bus name, an object path or both */
  if (bus_name[0] == '/' && argc == 2)
    {
      object_path = bus_name;
      bus_name = NULL;
    }

  daemon = tp_dbus_daemon_new (tp_get_bus ());

  connection = tp_connection_new (daemon, bus_name, object_path, &error);

  /* for this example I assume it's an existing connection on which someone
   * else has called (or will call) Connect(), so we won't call Connect()
   * on it ourselves
   */
  if (connection == NULL ||
      !tp_connection_run_until_ready (connection, FALSE, &error, NULL))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      g_object_unref (daemon);

      if (connection != NULL)
        g_object_unref (connection);

      return 1;
    }

  printf ("Connection ready\n");

  /* An example non-blocking call */
  tp_cli_connection_call_list_channels (connection, -1,
      /* If ListChannels() needed any arguments, they'd go here */
      got_channels, g_main_loop_ref (mainloop),
      (GDestroyNotify) g_main_loop_unref, NULL);

  g_main_loop_run (mainloop);

  g_main_loop_unref (mainloop);
  g_object_unref (daemon);

  return 0;
}
