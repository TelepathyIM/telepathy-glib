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
#include <telepathy-glib/util.h>

void
channel_died (TpChannel *channel,
              GMainLoop *mainloop)
{
  puts ("Channel was destroyed before introspection finished");
  g_main_loop_quit (mainloop);
}

void
channel_ready (TpChannel *channel,
               const gchar *channel_type,
               guint handle_type,
               guint handle,
               const gchar * const * interfaces,
               GMainLoop *mainloop)
{
  const gchar * const * iter = interfaces;
  gboolean is_group;

  printf ("Type: %s\n", channel_type);
  printf ("Handle: of type %u, #%u\n", handle_type, handle);
  printf ("Interfaces:\n");

  for (; *iter != NULL; iter++)
    {
      printf ("\t%s\n", *iter);
      if (!tp_strdiff (*iter, TP_IFACE_CHANNEL_INTERFACE_GROUP))
        {
          is_group = TRUE;
        }
    }

  if (is_group)
    {
      GArray *members;
      GError *error = NULL;

      printf ("Group members:\n");
      /* An example of a blocking call */
      if (tp_cli_channel_interface_group_block_on_get_members (channel, -1,
            /* If GetMembers had any "in" arguments they'd go here */
          &members, &error))
        {
          guint i;

          for (i = 0; i < members->len; i++)
            {
              printf("\tcontact #%u\n", g_array_index (members, guint, i));
            }

          g_array_free (members, TRUE);
        }
      else
        {
          printf ("\t[error: %s]\n", error->message);
          g_error_free (error);
        }
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
        "dbus-connection", tp_get_bus (),
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

  g_signal_connect (channel, "channel-ready",
      G_CALLBACK (channel_ready), mainloop);
  g_signal_connect (channel, "destroyed", G_CALLBACK (channel_died), mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
