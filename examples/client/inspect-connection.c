/*
 * telepathy-example-inspect-connection - inspect a connection
 *
 * Copyright (C) 2007 Collabora Ltd.
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
connection_died (TpConnection *connection,
                 GMainLoop *mainloop)
{
  puts ("Connection was destroyed before introspection finished");
  g_main_loop_quit (mainloop);
}

void
connection_ready (TpConnection *connection,
                  GMainLoop *mainloop)
{
  GPtrArray *channels;
  GError *error = NULL;

  printf ("Connection ready\n");

  /* An example blocking call */
  if (tp_cli_connection_block_on_list_channels (connection, &channels, &error))
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

          g_value_array_free (channel);
        }

      g_ptr_array_free (channels, TRUE);
    }
  else
    {
      printf ("Error listing channels: %s", error->message);
      g_error_free (error);
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

  g_type_init ();
  tp_debug_set_flags (g_getenv ("TP_EXAMPLES_DEBUG"));

  if (argc < 3)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  bus_name = argv[1];
  object_path = argv[2];

  connection = TP_CONNECTION (g_object_new (TP_TYPE_CONNECTION,
        "connection", tp_get_bus (),
        "name", bus_name,
        "path", object_path,
        "interface", TP_IFACE_CONNECTION,
        NULL));

  g_signal_connect (connection, "connection-ready",
      G_CALLBACK (connection_ready), mainloop);
  g_signal_connect (connection, "destroy", G_CALLBACK (connection_died),
      mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
