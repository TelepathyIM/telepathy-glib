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

#include "config.h"

#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

static int exit_status = 1;

static void
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

static void
connection_ready_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GMainLoop *mainloop = user_data;
  GError *error = NULL;
  TpConnection *connection = TP_CONNECTION (source);

  if (!tp_proxy_prepare_finish (connection, result, &error))
    {
      g_warning ("%s", error->message);
      g_main_loop_quit (mainloop);
      g_clear_error (&error);
      return;
    }

  printf ("Connection ready\n");

  tp_cli_connection_call_list_channels (connection, -1,
      /* If ListChannels() needed any arguments, they'd go here */
      got_channels, g_main_loop_ref (mainloop),
      (GDestroyNotify) g_main_loop_unref, NULL);
}

int
main (int argc,
      char **argv)
{
  TpConnection *connection = NULL;
  GMainLoop *mainloop = NULL;
  TpSimpleClientFactory *factory;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 2)
    {
      fputs ("Usage:\n"
          "    telepathy-example-inspect-connection OBJECT_PATH\n",
          stderr);
      return 2;
    }

  mainloop = g_main_loop_new (NULL, FALSE);

  factory = tp_simple_client_factory_new (NULL);
  connection = tp_simple_client_factory_ensure_connection (factory,
      argv[1], NULL, &error);

  if (connection == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }

  /* for this example I assume it's an existing connection on which someone
   * else has called (or will call) Connect(), so we won't call Connect()
   * on it ourselves
   */
  tp_proxy_prepare_async (connection, NULL, connection_ready_cb, mainloop);

  g_main_loop_run (mainloop);

out:
  if (connection != NULL)
    g_object_unref (connection);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

  g_object_unref (factory);

  return exit_status;
}
