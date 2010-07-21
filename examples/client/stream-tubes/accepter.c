#include <telepathy-glib/telepathy-glib.h>

static GMainLoop *loop = NULL;


static void
_tube_accepted (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  TpHandleChannelsContext *context = user_data;
  GIOStream *iostream;
  GInputStream *in;
  GOutputStream *out;
  char buf[128] = { 0, };
  GError *error = NULL;

  iostream = tp_stream_tube_accept_finish (TP_STREAM_TUBE (tube), res, &error);

  if (error != NULL)
    {
      tp_handle_channels_context_fail (context, error);
      g_error_free (error);
      return;
    }

  tp_handle_channels_context_accept (context);
  g_object_unref (context);

  g_debug ("Tube open, have IOStream");

  in = g_io_stream_get_input_stream (iostream);
  out = g_io_stream_get_output_stream (iostream);

  /* this bit is not a good example */
  g_output_stream_write (out, "Ping", 4, NULL, &error);
  g_assert_no_error (error);

  g_input_stream_read (in, &buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);

  g_debug ("Sent Ping got: %s", buf);

  // FIXME: close the channel

  g_object_unref (iostream);
  g_object_unref (tube);

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
  TpStreamTube *tube;
  gboolean delay = FALSE;
  GList *l;

  g_debug ("Handling channels");

  for (l = channels; l != NULL; l = l->next)
    {
      TpChannel *channel = l->data;
      GHashTable *props = tp_channel_borrow_immutable_properties (channel);

      if (tp_channel_get_channel_type_id (channel) !=
          TP_IFACE_QUARK_CHANNEL_TYPE_STREAM_TUBE)
        continue;

      if (tp_strdiff (
            tp_asv_get_string (props, TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE),
            "ExampleService"))
        continue;

      g_debug ("Accepting tube");

      /* the TpStreamTube holds the only ref to @channel */
      tube = tp_stream_tube_new (channel);
      tp_stream_tube_accept_async (tube, _tube_accepted, context);

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

      error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "No channels to be handled");
      tp_handle_channels_context_fail (context, error);

      g_error_free (error);
    }
}


int
main (int argc,
    const char **argv)
{
  TpDBusDaemon *dbus;
  TpBaseClient *handler;
  GError *error = NULL;

  g_type_init ();

  dbus = tp_dbus_daemon_dup (&error);
  g_assert_no_error (error);

  handler = tp_simple_handler_new (dbus, FALSE, FALSE, "ExampleServiceHandler",
      FALSE, _handle_channels, NULL, NULL);

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

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (handler);

  return 0;
}
