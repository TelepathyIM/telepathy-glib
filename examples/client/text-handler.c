/*
 * text-handler - Handle text channels
 *
 * Simple text channel handler echoing received message in upper case
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>

static void
echo_message (TpTextChannel *channel,
    const gchar *text)
{
  gchar *up;
  TpMessage *msg;

  up = g_ascii_strup (text, -1);
  g_print ("send: %s\n", up);

  msg = tp_client_message_text_new (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, up);

  tp_text_channel_send_message_async (channel, msg, 0, NULL, NULL);

  g_free (up);
  g_object_unref (msg);
}

static void
message_received_cb (TpTextChannel *channel,
    TpMessage *message,
    gpointer user_data)
{
  const GHashTable *part;
  const gchar *text;

  part = tp_message_peek (message, 1);
  text = tp_asv_get_string (part, "content");

  g_print ("received: %s\n", text);

  echo_message (channel, text);

  tp_text_channel_ack_message_async (channel, message, NULL, NULL);
}

static void
display_pending_messages (TpTextChannel *channel)
{
  GList *messages, *l;

  messages = tp_text_channel_get_pending_messages (channel);

  for (l = messages; l != NULL; l = g_list_next (l))
    {
      TpMessage *msg = l->data;
      const GHashTable *part = tp_message_peek (msg, 1);
      const gchar *text = tp_asv_get_string (part, "content");

      g_print ("pending: %s\n", text);

      echo_message (channel, text);
    }

  tp_text_channel_ack_messages_async (channel, messages, NULL, NULL);

  g_list_free (messages);
}

static void
handle_channels_cb (TpSimpleHandler *self,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests,
    gint64 user_action_time,
    TpHandleChannelsContext *context,
    gpointer user_data)
{
  GList *l;

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *channel = l->data;
      TpTextChannel *text_chan = l->data;

      if (!TP_IS_TEXT_CHANNEL (channel))
        continue;

      g_print ("Handling text channel with %s\n",
          tp_channel_get_identifier (channel));

      g_signal_connect (channel, "message-received",
          G_CALLBACK (message_received_cb), NULL);

      display_pending_messages (text_chan);
    }

  tp_handle_channels_context_accept (context);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpDBusDaemon *bus_daemon;
  GError *error = NULL;
  TpBaseClient *handler;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  bus_daemon = tp_dbus_daemon_dup (&error);

  if (bus_daemon == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return 1;
    }

  handler = tp_simple_handler_new (bus_daemon, FALSE, FALSE, "ExampleHandler",
      FALSE, handle_channels_cb, NULL, NULL);

  tp_base_client_take_handler_filter (handler, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
        NULL));

  if (!tp_base_client_register (handler, &error))
    {
      g_warning ("Failed to register Handler: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_print ("Waiting for channels\n");

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

out:
  g_object_unref (bus_daemon);
  g_object_unref (handler);

  return 0;
}
