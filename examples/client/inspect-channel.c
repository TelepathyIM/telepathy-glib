/*
 * telepathy-example-inspect-channel - inspect a channel
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

typedef struct {
    int exit_status;
    GMainLoop *main_loop;
    const gchar *object_path;
} InspectChannelData;

static void
channel_ready_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel = TP_CHANNEL (source);
  InspectChannelData *data = user_data;
  guint handle_type, handle;
  gchar *channel_type;
  gchar **interfaces, **iter;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (channel, result, &error))
    {
      g_warning ("%s", error->message);
      data->exit_status = 1;
      g_main_loop_quit (data->main_loop);
      g_clear_error (&error);
      return;
    }

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
      printf ("\t%s\n", *iter);
    }

  g_free (channel_type);
  g_strfreev (interfaces);

  if (tp_proxy_has_interface_by_id (channel,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP))
    {
      GPtrArray *members = tp_channel_group_dup_members_contacts (channel);
      guint i;

      printf ("Group members:\n");

      for (i = 0; i < members->len; i++)
        {
          TpContact *member = g_ptr_array_index (members, i);

          printf ("\tcontact #%u %s\n",
              tp_contact_get_handle (member),
              tp_contact_get_identifier (member));
        }
      g_ptr_array_unref (members);
    }

  data->exit_status = 0;
  g_main_loop_quit (data->main_loop);
}

static void
connection_ready_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  InspectChannelData *data = user_data;
  GError *error = NULL;
  TpSimpleClientFactory *factory;
  TpConnection *connection = TP_CONNECTION (source);
  TpChannel *channel = NULL;


  if (!tp_proxy_prepare_finish (connection, result, &error))
    {
      g_warning ("%s", error->message);
      data->exit_status = 1;
      g_main_loop_quit (data->main_loop);
      g_clear_error (&error);
      return;
    }

  factory = tp_proxy_get_factory (connection);
  channel = tp_simple_client_factory_ensure_channel (factory, connection,
      data->object_path, NULL, &error);

  if (channel == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      data->exit_status = 1;
      g_main_loop_quit (data->main_loop);
      return;
    }

  tp_proxy_prepare_async (channel, NULL, channel_ready_cb, user_data);

  /* the channel will remain referenced as long as it has calls pending on
   * it */
  g_object_unref (channel);
}



int
main (int argc,
      char **argv)
{
  InspectChannelData data = { 1, NULL, NULL };
  TpSimpleClientFactory *factory;
  TpConnection *connection = NULL;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 3)
    {
      fputs ("Usage:\n"
          "    telepathy-example-inspect-channel CONN_PATH CHANNEL_PATH\n",
          stderr);
      return 2;
    }

  data.object_path = argv[2];
  factory = tp_simple_client_factory_new (NULL);
  connection = tp_simple_client_factory_ensure_connection (factory,
      argv[1], NULL, &error);

  if (connection == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      data.exit_status = 1;
      goto out;
    }

  data.main_loop = g_main_loop_new (NULL, FALSE);

  /* for this example I assume it's an existing connection on which someone
   * else has called (or will call) Connect(), so we won't call Connect()
   * on it ourselves
   */
  tp_proxy_prepare_async (connection, NULL, connection_ready_cb, &data);

  g_main_loop_run (data.main_loop);

out:
  if (data.main_loop != NULL)
    g_main_loop_unref (data.main_loop);

  if (connection != NULL)
    g_object_unref (connection);

  g_object_unref (factory);

  return data.exit_status;
}
