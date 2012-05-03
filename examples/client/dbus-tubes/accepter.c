#include <telepathy-glib/telepathy-glib.h>
#include "constants.h"

static GMainLoop *loop = NULL;

static void
dbus_connection_closed_cb (
    GDBusConnection *connection,
    gboolean remote_peer_vanished,
    GError *error,
    gpointer user_data)
{
  if (remote_peer_vanished)
    g_debug ("remote peer disconnected: %s", error->message);
  else if (error != NULL)
    g_debug ("remote peer sent broken data: %s", error->message);
  else
    g_debug ("supposedly we closed the connection locally?!");

  g_object_unref (connection);
}

static void
lucky_number_cb (
    GDBusConnection *connection,
    const gchar *sender_name,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *signal_name,
    GVariant *parameters,
    gpointer user_data)
{
  if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(u)")))
    {
      guint32 x;

      g_variant_get (parameters, "(u)", &x);
      g_debug ("My lucky number is: %u", x);
    }
  else
    {
      g_warning ("LuckyNumber's arguments were %s, not (u)",
          g_variant_get_type_string (parameters));
    }
}

static void
add_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GDBusConnection *conn = G_DBUS_CONNECTION (source);
  GVariant *ret;
  GError *error = NULL;

  ret = g_dbus_connection_call_finish (conn, result, &error);

  if (ret != NULL)
    {
      gint32 value;

      g_variant_get (ret, "(i)", &value);
      g_debug ("Adding my numbers together gave: %i", value);
      g_variant_unref (ret);
    }
  else
    {
      g_warning ("Add() failed: %s", error->message);
      g_clear_error (&error);
    }
}

static void
tube_accepted (GObject *tube,
    GAsyncResult *res,
    gpointer user_data)
{
  GDBusConnection *conn;
  GError *error = NULL;

  conn = tp_dbus_tube_channel_accept_finish (
      TP_DBUS_TUBE_CHANNEL (tube), res, &error);
  if (conn == NULL)
    {
      g_debug ("Failed to accept tube: %s", error->message);
      g_error_free (error);
      tp_channel_close_async (TP_CHANNEL (tube), NULL, NULL);
      return;
    }

  g_debug ("tube accepted");
  g_signal_connect (conn, "closed",
      G_CALLBACK (dbus_connection_closed_cb), NULL);

  g_dbus_connection_signal_subscribe (conn,
      /* since we only deal with 1-1 connections, no need to match sender */
      NULL,
      EXAMPLE_INTERFACE,
      "LuckyNumber",
      EXAMPLE_PATH,
      NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,
      lucky_number_cb,
      NULL, NULL);
  g_dbus_connection_call (conn,
      NULL,
      EXAMPLE_PATH,
      EXAMPLE_INTERFACE,
      "Add",
      g_variant_new ("(ii)", 45, 54),
      G_VARIANT_TYPE ("(i)"),
      G_DBUS_CALL_FLAGS_NONE,
      -1,
      NULL,
      add_cb,
      NULL);
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
handle_channels (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *conn,
    GList *channels,
    GList *requests,
    gint64 action_time,
    TpHandleChannelsContext *context,
    gpointer user_data)
{
  TpDBusTubeChannel *tube;
  GList *l;
  GError error = { TP_ERROR, TP_ERROR_NOT_AVAILABLE,
      "No channel to be handled" };

  g_debug ("Handling channels");

  for (l = channels; l != NULL; l = l->next)
    {
      TpDBusTubeChannel *channel = l->data;

      if (!TP_IS_DBUS_TUBE_CHANNEL (channel))
        continue;

      if (tp_strdiff (tp_dbus_tube_channel_get_service_name (channel),
            EXAMPLE_SERVICE_NAME))
        continue;

      g_debug ("Accepting tube");

      tube = g_object_ref (channel);

      g_signal_connect (tube, "invalidated",
          G_CALLBACK (tube_invalidated_cb), NULL);

      tp_dbus_tube_channel_accept_async (tube, tube_accepted, context);

      tp_handle_channels_context_accept (context);
      return;
    }

  g_debug ("Rejecting channels");
  tp_handle_channels_context_fail (context, &error);
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
      "ExampleServiceHandler", FALSE, handle_channels, NULL, NULL);

  tp_base_client_take_handler_filter (handler, tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
      G_TYPE_STRING,
      TP_IFACE_CHANNEL_TYPE_DBUS_TUBE,

      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      G_TYPE_UINT,
      TP_HANDLE_TYPE_CONTACT,

      TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME,
      G_TYPE_STRING,
      EXAMPLE_SERVICE_NAME,

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
