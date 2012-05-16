#include <telepathy-glib/telepathy-glib.h>
#include "constants.h"

static GMainLoop *loop = NULL;

static void
connection_closed_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GError *error = NULL;

  if (!g_dbus_connection_close_finish (connection, result, &error))
    {
      g_warning ("Couldn't close connection: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      g_debug ("Connection closed.");
    }

  tp_channel_close_async (TP_CHANNEL (user_data), NULL, NULL);
  g_object_unref (connection);
}

static void
handle_method_call (
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data)
{
  if (tp_strdiff (method_name, "Add"))
    {
      g_dbus_method_invocation_return_error (invocation,
          G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
          "Unknown method '%s' on interface " EXAMPLE_INTERFACE,
          method_name);
    }
  else if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ii)")))
    {
      g_dbus_method_invocation_return_error (invocation,
          G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Add takes two int32 parameters, not %s",
          g_variant_get_type_string (parameters));
    }
  else /* hooray! */
    {
      guint x, y;
      gboolean ret;

      g_variant_get (parameters, "(ii)", &x, &y);

      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(i)", x + y));

      ret = g_dbus_connection_emit_signal (connection,
          NULL, object_path, interface_name, "LuckyNumber",
          g_variant_new ("(u)", g_random_int ()),
          NULL);
      /* "This can only fail if 'parameters' is not compatible with the D-Bus
       * protocol."
       */
      g_return_if_fail (ret);

      g_dbus_connection_flush_sync (connection, NULL, NULL);
      g_dbus_connection_close (connection, NULL, connection_closed_cb, user_data);
    }
}

static void
register_object (GDBusConnection *connection,
    TpDBusTubeChannel *channel)
{
  GDBusNodeInfo *introspection_data;
  guint registration_id;
  static const GDBusInterfaceVTable interface_vtable =
  {
    handle_method_call,
    NULL,
    NULL,
  };
  static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='" EXAMPLE_INTERFACE "'>"
    "    <method name='Add'>"
    "      <arg type='i' name='x' direction='in'/>"
    "      <arg type='i' name='y' direction='in'/>"
    "      <arg type='i' name='result' direction='out'/>"
    "    </method>"
    "    <signal name='LuckyNumber'>"
    "      <arg type='u' name='number'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  registration_id = g_dbus_connection_register_object (connection,
      EXAMPLE_PATH, introspection_data->interfaces[0],
      &interface_vtable, g_object_ref (channel), g_object_unref, NULL);
  g_assert (registration_id > 0);

  g_dbus_node_info_unref (introspection_data);
}

static void
tube_offered (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  GDBusConnection *conn;

  conn = tp_dbus_tube_channel_offer_finish (TP_DBUS_TUBE_CHANNEL (tube), res,
      &error);
  if (conn == NULL)
    {
      g_debug ("Failed to offer tube: %s", error->message);
      g_error_free (error);
      tp_channel_close_async (TP_CHANNEL (tube), NULL, NULL);
      return;
    }

  g_debug ("Tube opened");
  register_object (conn, TP_DBUS_TUBE_CHANNEL (tube));
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
  g_object_unref (tube);
}

static void
channel_created (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *channel;
  GError *error = NULL;
  TpDBusTubeChannel *tube;

  channel = tp_account_channel_request_create_and_handle_channel_finish (
      TP_ACCOUNT_CHANNEL_REQUEST (source), result, NULL, &error);
  if (channel == NULL)
    {
      g_debug ("Failed to create channel: %s", error->message);
      g_error_free (error);
      g_main_loop_quit (loop);
      return;
    }

  g_debug ("Channel created: %s", tp_proxy_get_object_path (channel));

  tube = TP_DBUS_TUBE_CHANNEL (channel);

  g_signal_connect (tube, "invalidated",
      G_CALLBACK (tube_invalidated_cb), NULL);

  tp_dbus_tube_channel_offer_async (tube, NULL, tube_offered, NULL);
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

  g_type_init ();

  if (argc != 3)
    g_error ("Usage: offerer gabble/jabber/ladygaga t-pain@example.com");

  factory = tp_simple_client_factory_new (NULL);

  account_path = g_strconcat (TP_ACCOUNT_OBJECT_PATH_BASE, argv[1], NULL);
  account = tp_simple_client_factory_ensure_account (factory, account_path,
      NULL, &error);
  g_assert_no_error (error);
  g_free (account_path);

  request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
      G_TYPE_STRING,
      TP_IFACE_CHANNEL_TYPE_DBUS_TUBE,

      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      G_TYPE_UINT,
      TP_HANDLE_TYPE_CONTACT,

      TP_PROP_CHANNEL_TARGET_ID,
      G_TYPE_STRING,
      argv[2],

      TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME,
      G_TYPE_STRING,
      EXAMPLE_SERVICE_NAME,

      NULL);

  g_debug ("Offer channel to %s", argv[2]);

  req = tp_account_channel_request_new (account, request,
      TP_USER_ACTION_TIME_CURRENT_TIME);

  tp_account_channel_request_create_and_handle_channel_async (req, NULL,
      channel_created, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_object_unref (account);
  g_object_unref (req);
  g_hash_table_unref (request);
  g_main_loop_unref (loop);
  g_object_unref (factory);

  return 0;
}
