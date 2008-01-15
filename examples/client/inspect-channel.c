/*
 * telepathy-example-inspect-channel - inspect a channel
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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
channel_invalidated (TpChannel *channel,
                     guint domain,
                     gint code,
                     const gchar *message,
                     GMainLoop *mainloop)
{
  printf ("Channel invalidated before introspection finished: %s\n", message);
  g_main_loop_quit (mainloop);
}

void
channel_ready (TpChannel *channel,
               GParamSpec *unused,
               GMainLoop *mainloop)
{
  gchar *channel_type;
  gchar **interfaces, **iter;
  guint handle_type, handle;

  g_object_get (channel,
      "channel-type", &channel_type,
      "handle-type", &handle_type,
      "handle", &handle,
      "interfaces", &interfaces,
      NULL);

  printf ("Type: %s\n", channel_type);
  printf ("Handle: of type %u, #%u\n", handle_type, handle);
  puts ("Interfaces:");

  for (iter = interfaces; iter != NULL && *iter != NULL; iter++)
    {
      printf ("\t%s", *iter);
    }

  g_free (channel_type);
  g_strfreev (interfaces);

  if (tp_proxy_has_interface_by_id (channel,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP))
    {
      GArray *members;
      GError *error = NULL;

      printf ("Group members:\n");
      /* An example of a reentrant blocking call. This re-enters the
       * main loop and dispatches events until the call completes */
      if (tp_cli_channel_interface_group_run_get_members (channel, -1,
            /* If GetMembers had any "in" arguments they'd go here */
          &members, &error, NULL))
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
  TpDBusDaemon *daemon;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 3)
    return 2;

  mainloop = g_main_loop_new (NULL, FALSE);

  bus_name = argv[1];
  object_path = argv[2];

  daemon = tp_dbus_daemon_new (tp_get_bus ());

  channel = tp_channel_new (daemon, bus_name, object_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);

  if (channel == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      g_object_unref (daemon);
      return 1;
    }

  g_signal_connect (channel, "notify::channel-ready",
      G_CALLBACK (channel_ready), mainloop);
  g_signal_connect (channel, "invalidated", G_CALLBACK (channel_invalidated),
      mainloop);

  g_main_loop_run (mainloop);

  g_main_loop_unref (mainloop);
  g_object_unref (daemon);

  return 0;
}
