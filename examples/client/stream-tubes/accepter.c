#include "config.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

static GMainLoop *loop = NULL;

static void
tube_conn_closed_cb (TpStreamTubeConnection *conn,
    const GError *error,
    gpointer user_data)
{
  g_message ("Tube connection has been closed: %s", error->message);
}

static void
_tube_accepted (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  TpHandleChannelContext *context = user_data;
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
      g_message ("Can't accept the tube: %s", error->message);
      tp_handle_channel_context_fail (context, error);
      g_error_free (error);
      return;
    }

  tp_handle_channel_context_accept (context);
  g_object_unref (context);

  g_message ("Tube open, have IOStream");

  conn = tp_stream_tube_connection_get_socket_connection (tube_conn);

  in = g_io_stream_get_input_stream (G_IO_STREAM (conn));
  out = g_io_stream_get_output_stream (G_IO_STREAM (conn));

  /* this bit is not a good example */
  g_message ("Sending: Ping");
  g_output_stream_write (out, "Ping\n", 5, NULL, &error);
  g_assert_no_error (error);

  g_input_stream_read (in, &buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);

  g_message ("Received: %s", buf);

  g_object_unref (tube_conn);
}

static void
tube_invalidated_cb (TpStreamTubeChannel *tube,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  g_message ("Tube has been invalidated: %s", message);
  g_main_loop_quit (loop);
}

static void
_handle_channel (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *conn,
    TpChannel *channel,
    GList *requests,
    gint64 action_time,
    TpHandleChannelContext *context,
    gpointer user_data)
{
  g_message ("Handling channel; accepting tube");

  g_signal_connect (channel, "invalidated",
      G_CALLBACK (tube_invalidated_cb), NULL);

  tp_stream_tube_channel_accept_async (TP_STREAM_TUBE_CHANNEL (channel),
      _tube_accepted, context);

  g_message ("Delaying channel acceptance");

  tp_handle_channel_context_delay (context);
  g_object_ref (context);
}


int
main (int argc,
    const char **argv)
{
  TpAccountManager *manager;
  TpBaseClient *handler;
  GError *error = NULL;

  manager = tp_account_manager_dup ();
  handler = tp_simple_handler_new_with_am (manager, FALSE, FALSE,
      "ExampleServiceHandler", FALSE, _handle_channel, NULL, NULL);

  tp_base_client_add_handler_filter (handler,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u>, %s: <%s> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT,
        TP_PROP_CHANNEL_TYPE_STREAM_TUBE1_SERVICE, "ExampleService"));

  tp_base_client_register (handler, &error);
  g_assert_no_error (error);

  g_message ("Waiting for tube offer");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (handler);
  g_object_unref (manager);

  return 0;
}
