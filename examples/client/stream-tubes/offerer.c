#include <telepathy-glib/telepathy-glib.h>

static GMainLoop *loop = NULL;


static void
_incoming_iostream (TpStreamTube *tube,
    TpContact *contact,
    GIOStream *iostream,
    gpointer user_data)
{
  GInputStream *in;
  GOutputStream *out;
  char buf[128] = { 0, };
  GError *error = NULL;

  g_debug ("Got IOStream from %s",
      tp_contact_get_identifier (contact));

  in = g_io_stream_get_input_stream (iostream);
  out = g_io_stream_get_output_stream (iostream);

  /* this bit is not a good example */
  g_output_stream_write (out, "Pong", 4, NULL, &error);
  g_assert_no_error (error);

  g_input_stream_read (in, &buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);

  g_debug ("Send Pong got: %s", buf);

  // FIXME: close the channel

  g_object_unref (tube);

  g_main_loop_quit (loop);
}


static void
_tube_offered (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;

  tp_stream_tube_offer_finish (TP_STREAM_TUBE (tube), res, &error);
  g_assert_no_error (error);

  g_debug ("Tube offered");
}


static void
_channel_prepared (GObject *channel,
    GAsyncResult *res,
    gpointer user_data)
{
  TpStreamTube *tube;
  GError *error = NULL;

  tp_proxy_prepare_finish (channel, res, &error);
  g_assert_no_error (error);

  g_debug ("Channel prepared");

  tube = tp_stream_tube_new (TP_CHANNEL (channel));
  /* the TpStreamTube holds the only reference to @channel */
  g_object_unref (channel);

  g_signal_connect (tube, "incoming",
      G_CALLBACK (_incoming_iostream), NULL);

  tp_stream_tube_offer_async (tube, NULL, _tube_offered, NULL);
}


static void
_channel_created (TpConnection *conn,
    const gchar *channel_path,
    GHashTable *props,
    const GError *in_error,
    gpointer user_data,
    GObject *weak_obj)
{
  TpChannel *channel;
  GError *error = NULL;

  g_assert_no_error ((GError *) in_error);

  g_debug ("Channel created: %s", channel_path);

  channel = tp_channel_new_from_properties (conn, channel_path, props,
      &error);
  g_assert_no_error (error);

  tp_proxy_prepare_async (channel, NULL, _channel_prepared, NULL);
}


static void
_connection_prepared (GObject *conn,
    GAsyncResult *res,
    gpointer user_data)
{
  GHashTable *request;
  GError *error = NULL;

  tp_proxy_prepare_finish (conn, res, &error);
  g_assert_no_error (error);

  g_debug ("Connection prepared");
  g_debug ("Requesting channel");

  request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
      G_TYPE_STRING,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE,

      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      G_TYPE_UINT,
      TP_HANDLE_TYPE_CONTACT,

      TP_PROP_CHANNEL_TARGET_ID,
      G_TYPE_STRING,
      user_data,

      TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE,
      G_TYPE_STRING,
      "ExampleService",

      NULL);

  tp_cli_connection_interface_requests_call_create_channel (
      TP_CONNECTION (conn), -1,
      request,
      _channel_created,
      NULL, NULL, NULL);

  g_hash_table_destroy (request);
}


static void
_account_prepared (GObject *account,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;

  tp_proxy_prepare_finish (account, res, &error);
  g_assert_no_error (error);

  g_debug ("Account prepared");

  tp_proxy_prepare_async (tp_account_get_connection (TP_ACCOUNT (account)),
        NULL, _connection_prepared, user_data);
}


int
main (int argc,
    const char **argv)
{
  TpDBusDaemon *dbus;
  TpAccount *account;
  char *account_path;
  GError *error = NULL;

  g_assert (argc == 3);

  g_type_init ();

  dbus = tp_dbus_daemon_dup (&error);
  g_assert_no_error (error);

  account_path = g_strconcat (TP_ACCOUNT_OBJECT_PATH_BASE, argv[1], NULL);
  account = tp_account_new (dbus, account_path, &error);
  g_assert_no_error (error);
  g_free (account_path);

  tp_proxy_prepare_async (account, NULL, _account_prepared,
      (gpointer) argv[2]);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_object_unref (account);
  g_main_loop_unref (loop);

  return 0;
}
