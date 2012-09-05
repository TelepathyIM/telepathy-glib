#include <telepathy-glib/telepathy-glib.h>

static GMainLoop *loop = NULL;

static void
tube_conn_closed_cb (TpStreamTubeConnection *conn,
    const GError *error,
    gpointer user_data)
{
  g_debug ("Tube connection has been closed: %s", error->message);
}

static void
_tube_accepted (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  TpHandleChannelsContext *context = user_data;
  TpStreamTubeConnection *tube_conn;
  GSocketConnection *conn;
  GInputStream *in;
  GOutputStream *out;
  char buf[128] = { 0, };
  GError *error = NULL;

  tube_conn = tp_stream_tube_channel_accept_finish (
      TP_STREAM_TUBE_CHANNEL (tube), res, &error);

  g_signal_connect (tube_conn, "closed",
      G_CALLBACK (tube_conn_closed_cb), NULL);

  if (error != NULL)
    {
      g_debug ("Can't accept the tube: %s", error->message);
      tp_handle_channels_context_fail (context, error);
      g_error_free (error);
      return;
    }

  tp_handle_channels_context_accept (context);
  g_object_unref (context);

  g_debug ("Tube open, have IOStream");

  conn = tp_stream_tube_connection_get_socket_connection (tube_conn);

  in = g_io_stream_get_input_stream (G_IO_STREAM (conn));
  out = g_io_stream_get_output_stream (G_IO_STREAM (conn));

  /* this bit is not a good example */
  g_debug ("Sending: Ping");
  g_output_stream_write (out, "Ping\n", 5, NULL, &error);
  g_assert_no_error (error);

  g_input_stream_read (in, &buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);

  g_debug ("Received: %s", buf);

  g_object_unref (tube_conn);
}

static void
tube_invalidated_cb (TpStreamTubeChannel *tube,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  g_debug ("Tube has been invalidated: %s", message);
  g_main_loop_quit (loop);
}

static void
_handle_channels (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *conn,
    GList *channels,
    GList *requests,
    gint64 action_time,
    TpHandleChannelsContext *context,
    gpointer user_data)
{
  gboolean delay = FALSE;
  GList *l;

  g_debug ("Handling channels");

  for (l = channels; l != NULL; l = l->next)
    {
      TpStreamTubeChannel *tube = l->data;

      if (!TP_IS_STREAM_TUBE_CHANNEL (tube))
        continue;

      g_debug ("Accepting tube");

      g_signal_connect (tube, "invalidated",
          G_CALLBACK (tube_invalidated_cb), NULL);

      tp_stream_tube_channel_accept_async (tube, _tube_accepted, context);

      delay = TRUE;
    }

  if (delay)
    {
      g_debug ("Delaying channel acceptance");

      tp_handle_channels_context_delay (context);
      g_object_ref (context);
    }
  else
    {
      GError *error;

      g_debug ("Rejecting channels");

      error = g_error_new (TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "No channels to be handled");
      tp_handle_channels_context_fail (context, error);

      g_error_free (error);
    }
}


int
main (int argc,
    const char **argv)
{
  TpAccountManager *manager;
  TpBaseClient *handler;
  GError *error = NULL;

  g_type_init ();

  manager = tp_account_manager_dup ();
  handler = tp_simple_handler_new_with_am (manager, FALSE, FALSE,
      "ExampleServiceHandler", FALSE, _handle_channels, NULL, NULL);

  tp_base_client_take_handler_filter (handler, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE,
        G_TYPE_STRING,
        TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,

        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
        G_TYPE_UINT,
        TP_HANDLE_TYPE_CONTACT,

        TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE,
        G_TYPE_STRING,
        "ExampleService",

        NULL));

  tp_base_client_register (handler, &error);
  g_assert_no_error (error);

  g_debug ("Waiting for tube offer");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (handler);
  g_object_unref (manager);

  return 0;
}
