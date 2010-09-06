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
_channel_created (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel;
  GError *error = NULL;
  TpStreamTube *tube;

  channel = tp_account_channel_request_create_and_handle_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, NULL, &error);
  g_assert_no_error (error);

  g_debug ("Channel created: %s", tp_proxy_get_object_path (channel));

  tube = tp_stream_tube_new (tp_channel_borrow_connection (channel),
      tp_proxy_get_object_path (channel),
      tp_channel_borrow_immutable_properties (channel),
      &error);
  g_assert_no_error (error);

  g_object_unref (channel);

  g_signal_connect (tube, "incoming",
      G_CALLBACK (_incoming_iostream), NULL);

  tp_stream_tube_offer_async (tube, NULL, _tube_offered, NULL);
}

int
main (int argc,
    const char **argv)
{
  TpDBusDaemon *dbus;
  TpAccount *account;
  char *account_path;
  GError *error = NULL;
  TpAccountChannelRequest *req;
  GHashTable *request;

  g_assert (argc == 3);

  g_type_init ();

  dbus = tp_dbus_daemon_dup (&error);
  g_assert_no_error (error);

  account_path = g_strconcat (TP_ACCOUNT_OBJECT_PATH_BASE, argv[1], NULL);
  account = tp_account_new (dbus, account_path, &error);
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

  return 0;
}
