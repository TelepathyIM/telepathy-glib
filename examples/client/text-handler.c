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

#include "config.h"

#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>

static void
echo_message (TpTextChannel *channel,
    TpMessage *message,
    gboolean pending)
{
  gchar *text;
  gchar *up;
  TpMessage *reply;
  TpChannelTextMessageFlags flags;
  const gchar *comment = "";

  text = tp_message_to_text (message, &flags);

  if (flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT)
    {
      comment = " (and some non-text content we don't understand)";
    }

  if (pending)
    g_print ("pending: '%s' %s\n", text, comment);
  else
    g_print ("received: '%s' %s\n", text, comment);

  up = g_ascii_strup (text, -1);
  g_print ("send: %s\n", up);

  reply = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, up);

  tp_text_channel_send_message_async (channel, reply, 0, NULL, NULL);

  g_free (text);
  g_free (up);
  g_object_unref (reply);
}

static void
message_received_cb (TpTextChannel *channel,
    TpMessage *message,
    gpointer user_data)
{
  echo_message (channel, message, FALSE);

  tp_text_channel_ack_message_async (channel, message, NULL, NULL);
}

static void
display_pending_messages (TpTextChannel *channel)
{
  GList *messages, *l;

  messages = tp_text_channel_dup_pending_messages (channel);

  for (l = messages; l != NULL; l = g_list_next (l))
    {
      TpMessage *msg = l->data;

      echo_message (channel, msg, TRUE);
    }

  tp_text_channel_ack_messages_async (channel, messages, NULL, NULL);

  g_list_free_full (messages, g_object_unref);
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

      /* The default channel factory used by the TpSimpleHandler has
       * already prepared TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES,
       * if possible. */
      display_pending_messages (text_chan);
    }

  tp_handle_channels_context_accept (context);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpAccountManager *manager;
  GError *error = NULL;
  TpBaseClient *handler;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  manager = tp_account_manager_dup ();
  handler = tp_simple_handler_new_with_am (manager, FALSE, FALSE,
      "ExampleHandler", FALSE, handle_channels_cb, NULL, NULL);

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
  g_object_unref (manager);
  g_object_unref (handler);

  return 0;
}
