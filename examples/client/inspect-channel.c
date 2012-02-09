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
      const TpIntset *members = tp_channel_group_get_members (channel);
      TpIntsetFastIter group_iter;
      TpHandle member;

      printf ("Group members:\n");

      tp_intset_fast_iter_init (&group_iter, members);

      while (tp_intset_fast_iter_next (&group_iter, &member))
        {
          printf ("\tcontact #%u\n", member);
        }
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

  channel = tp_channel_new (connection, data->object_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);

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
  const gchar *conn_name;
  TpDBusDaemon *dbus = NULL;
  TpConnection *connection = NULL;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 3)
    {
      fputs ("Usage:\n"
          "    telepathy-example-inspect-channel CONN OBJECT_PATH\n"
          "CONN may either be a connection's well-known bus name or object\n"
          "path.\n",
          stderr);
      return 2;
    }

  conn_name = argv[1];
  data.object_path = argv[2];

  dbus = tp_dbus_daemon_dup (&error);

  if (dbus == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      data.exit_status = 1;
      goto out;
    }

  if (conn_name[0] == '/')
    connection = tp_connection_new (dbus, NULL, conn_name, &error);
  else
    connection = tp_connection_new (dbus, conn_name, NULL, &error);

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
  if (dbus != NULL)
    g_object_unref (dbus);

  if (data.main_loop != NULL)
    g_main_loop_unref (data.main_loop);

  if (connection != NULL)
    g_object_unref (connection);

  return data.exit_status;
}
