#include <telepathy-glib/telepathy-glib.h>

static GMainLoop *loop = NULL;

static void
channel_closed_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel = TP_CHANNEL (object);
  GError *error = NULL;

  if (!tp_channel_close_finish (channel, result, &error))
    {
      g_debug ("Failed to close tube channel: %s", error->message);
      g_error_free (error);
      return;
    }

  g_debug ("Tube channel closed");
}

static void
tube_conn_closed_cb (TpStreamTubeConnection *conn,
    const GError *error,
    gpointer user_data)
{
  g_debug ("Tube connection has been closed: %s", error->message);
}

static void
_incoming_iostream (TpStreamTubeChannel *tube,
    TpStreamTubeConnection* tube_conn,
    gpointer user_data)
{
  GInputStream *in;
  GOutputStream *out;
  char buf[128] = { 0, };
  GError *error = NULL;
  TpContact *contact;
  GSocketConnection *conn;

  g_signal_connect (tube_conn, "closed",
      G_CALLBACK (tube_conn_closed_cb), NULL);

  contact = tp_stream_tube_connection_get_contact (tube_conn);

  g_debug ("Got IOStream from %s",
      tp_contact_get_identifier (contact));

  conn = tp_stream_tube_connection_get_socket_connection (tube_conn);

  in = g_io_stream_get_input_stream (G_IO_STREAM (conn));
  out = g_io_stream_get_output_stream (G_IO_STREAM (conn));

  /* this bit is not a good example */
  g_input_stream_read (in, &buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_debug ("Received: %s", buf);

  g_debug ("Sending: Pong");
  g_output_stream_write (out, "Pong\n", 5, NULL, &error);
  g_assert_no_error (error);

  tp_channel_close_async (TP_CHANNEL (tube), channel_closed_cb, NULL);

  g_object_unref (tube);
}

static void
_tube_offered (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;

  if (!tp_stream_tube_channel_offer_finish (TP_STREAM_TUBE_CHANNEL (tube), res, &error))
    {
      g_debug ("Failed to offer tube: %s", error->message);

      g_error_free (error);
      return;
    }

  g_debug ("Tube offered");
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
_channel_created (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel;
  GError *error = NULL;
  TpStreamTubeChannel *tube;

  channel = tp_account_channel_request_create_and_handle_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, NULL, &error);
  if (channel == NULL)
    {
      g_debug ("Failed to create channel: %s", error->message);

      g_error_free (error);
      return;
    }

  g_debug ("Channel created: %s", tp_proxy_get_object_path (channel));

  tube = TP_STREAM_TUBE_CHANNEL (channel);

  g_signal_connect (tube, "incoming",
      G_CALLBACK (_incoming_iostream), NULL);
  g_signal_connect (tube, "invalidated",
      G_CALLBACK (tube_invalidated_cb), NULL);

  tp_stream_tube_channel_offer_async (tube, NULL, _tube_offered, NULL);
}

int
main (int argc,
    const char **argv)
{
  TpSimpleClientFactory *factory;
  TpAccount *account;
  char *account_path;
  GError *error = NULL;
  TpAccountChannelRequest *req;
  GHashTable *request;

  g_assert (argc == 3);

  g_type_init ();

  factory = tp_simple_client_factory_new (NULL);

  account_path = g_strconcat (TP_ACCOUNT_OBJECT_PATH_BASE, argv[1], NULL);
  account = tp_simple_client_factory_ensure_account (factory, account_path,
      NULL, &error);
  g_assert_no_error (error);
  g_free (account_path);

  request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
      G_TYPE_STRING,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,

      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      G_TYPE_UINT,
      TP_HANDLE_TYPE_CONTACT,

      TP_PROP_CHANNEL_TARGET_ID,
      G_TYPE_STRING,
      argv[2],

      TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE,
      G_TYPE_STRING,
      "ExampleService",

      NULL);

  g_debug ("Offer channel to %s", argv[2]);

  req = tp_account_channel_request_new (account, request,
      TP_USER_ACTION_TIME_CURRENT_TIME);

  tp_account_channel_request_create_and_handle_channel_async (req, NULL,
      _channel_created, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_object_unref (account);
  g_object_unref (req);
  g_hash_table_unref (request);
  g_main_loop_unref (loop);
  g_object_unref (factory);

  return 0;
}
