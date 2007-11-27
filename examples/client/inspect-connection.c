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
                 GError *error,
                 GMainLoop *mainloop)
{
  printf ("Connection died before introspection finished: %s\n",
    error->message);
  g_main_loop_quit (mainloop);
}

void
got_channels (TpProxy *proxy,
              const GPtrArray *channels,
              const GError *error,
              gpointer user_data)
{
  TpConnection *connection = TP_CONNECTION (proxy);
  GMainLoop *mainloop = user_data;

  /* this is the connection - you can do more with it */
  (void) connection;

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

void
connection_ready (TpConnection *connection,
                  GMainLoop *mainloop)
{
  printf ("Connection ready\n");

  /* An example non-blocking call */
  if (tp_cli_connection_call_list_channels (connection, -1,
        /* If ListChannels() needed any arguments, they'd go here */
        got_channels, g_main_loop_ref (mainloop),
        (GDestroyNotify) g_main_loop_unref, NULL) != NULL)
    {
      return;
    }
  else
    {
      /* FIXME: this ought to come out in the callback */
      printf ("Connection claims to be unable to list channels");
      g_main_loop_quit (mainloop);
    }
}

int
main (int argc,
      char **argv)
{
  const gchar *bus_name, *object_path;
  TpConnection *connection;
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 3)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  bus_name = argv[1];
  object_path = argv[2];

  connection = TP_CONNECTION (g_object_new (TP_TYPE_CONNECTION,
        "dbus-connection", tp_get_bus (),
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

  g_signal_connect (connection, "connection-ready",
      G_CALLBACK (connection_ready), mainloop);
  g_signal_connect (connection, "destroyed", G_CALLBACK (connection_died),
      mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
