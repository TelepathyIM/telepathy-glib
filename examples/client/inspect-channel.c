/*
 * telepathy-example-inspect-channel - inspect a channel
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>

void
channel_died (TpChannel *channel,
              GMainLoop *mainloop)
{
  puts ("Channel was destroyed before introspection finished");
  g_main_loop_quit (mainloop);
}

void
channel_introspected (TpChannel *channel,
                      const gchar *channel_type,
                      guint handle_type,
                      guint handle,
                      const gchar * const * interfaces,
                      GMainLoop *mainloop)
{
  const gchar * const * iter = interfaces;

  printf ("Type: %s\n", channel_type);
  printf ("Handle: of type %u, #%u\n", handle_type, handle);
  printf ("Interfaces:\n");

  for (; *iter != NULL; iter++)
    {
      printf ("\t%s\n", *iter);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *bus_name, *object_path;
  TpChannel *channel;
  GMainLoop *mainloop;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("TP_EXAMPLES_DEBUG"));

  if (argc < 3)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  bus_name = argv[1];
  object_path = argv[2];

  channel = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "connection", tp_get_bus (),
        "name", bus_name,
        "path", object_path,
        "interface", TP_IFACE_CHANNEL,
        NULL));

  g_signal_connect (channel, "introspected",
      G_CALLBACK (channel_introspected), mainloop);
  g_signal_connect (channel, "destroy", G_CALLBACK (channel_died), mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
