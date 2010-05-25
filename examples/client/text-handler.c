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

gboolean got_pending_msg = FALSE;

static void
echo_message (TpChannel *channel,
    const gchar *text)
{
  gchar *msg;

  msg = g_ascii_strup (text, -1);
  g_print ("send: %s\n", msg);

  tp_cli_channel_type_text_call_send (channel, -1,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, msg, NULL, NULL, NULL, NULL);

  g_free (msg);
}

static void
message_received_cb (TpChannel *channel,
    guint id,
    guint timestamp,
    guint sender,
    guint type,
    guint flags,
    const gchar *text,
    gpointer user_data,
    GObject *weak_object)
{
  GArray *arr;

  /* Ignore messages if we didn't fetch pending messages yet */
  if (!got_pending_msg)
    return;

  g_print ("received: %s\n", text);

  echo_message (channel, text);

  arr = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (arr, id);

  tp_cli_channel_type_text_call_acknowledge_pending_messages (channel, -1,
      arr, NULL, NULL, NULL, NULL);

  g_array_free (arr, TRUE);
}

static void
got_pending_messages_cb (TpChannel *channel,
    const GPtrArray *messages,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  guint i;
  GArray *ids;

  got_pending_msg = TRUE;

  if (error != NULL)
    return;

  ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), messages->len);

  for (i = 0; i < messages->len; i++)
    {
      GValueArray *v = g_ptr_array_index (messages, i);
      guint id, timestamp, sender, type, flags;
      const gchar *text;

      tp_value_array_unpack (v, 6,
          &id, &timestamp, &sender, &type, &flags, &text);

      g_print ("pending: %s\n", text);

      echo_message (channel, text);
      g_array_append_val (ids, id);
    }

  tp_cli_channel_type_text_call_acknowledge_pending_messages (channel, -1,
      ids, NULL, NULL, NULL, NULL);

  g_array_free (ids, TRUE);
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
      GHashTable *props;
      gboolean requested;

      if (tp_strdiff (tp_channel_get_channel_type (channel),
            TP_IFACE_CHANNEL_TYPE_TEXT))
        continue;

      props = tp_channel_borrow_immutable_properties (channel);
      requested = tp_asv_get_boolean (props, TP_PROP_CHANNEL_REQUESTED, NULL);

      g_print ("Handling text channel with %s\n",
          tp_channel_get_identifier (channel));

      tp_cli_channel_type_text_connect_to_received (channel,
          message_received_cb, NULL, NULL, NULL, NULL);

      tp_cli_channel_type_text_call_list_pending_messages (channel, -1, FALSE,
          got_pending_messages_cb, NULL, NULL, NULL);
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
