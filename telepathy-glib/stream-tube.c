/*
 * stream-tube.c - high-level wrapper for a Stream Tube
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include "telepathy-glib/stream-tube.h"

#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#include "_gen/signals-marshal.h"

#include <stdio.h>
#include <glib/gstdio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

G_DEFINE_TYPE (TpStreamTube, tp_stream_tube, TP_TYPE_CHANNEL);

struct _TpStreamTubePrivate
{
  GSocketListener *listener;
  GSocketAddress *address;
};


enum /* signals */
{
  INCOMING,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };


static void
tp_stream_tube_finalize (GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;

  tp_clear_object (&self->priv->listener);

  if (self->priv->address != NULL)
    {
#ifdef HAVE_GIO_UNIX
      /* check if we need to remove the temporary file we created */
      if (G_IS_UNIX_SOCKET_ADDRESS (self->priv->address))
        {
          const gchar *path;

          path = g_unix_socket_address_get_path (
              G_UNIX_SOCKET_ADDRESS (self->priv->address));
          g_unlink (path);
        }
#endif /* HAVE_GIO_UNIX */

      g_object_unref (self->priv->address);
      self->priv->address = NULL;
    }

  G_OBJECT_CLASS (tp_stream_tube_parent_class)->finalize (obj);
}


static void
tp_stream_tube_class_init (TpStreamTubeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = tp_stream_tube_finalize;

  /**
   * TpStreamTube::incoming
   * @self: the #TpStreamTube
   * @contact: (transfer none): the #TpContact making the connection
   * @stream: (transfer none): the #GIOStream for the connection
   *
   * The ::incoming signal is emitted on offered Tubes when a new incoming
   * connection is made from a remote user (one accepting the Tube).
   *
   * Consumers of this signal must take their own references to @contact and
   * @stream.
   */
  _signals[INCOMING] = g_signal_new ("incoming",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tp_marshal_VOID__OBJECT_OBJECT,
      G_TYPE_NONE,
      2, TP_TYPE_CONTACT, G_TYPE_IO_STREAM);

  g_type_class_add_private (gobject_class, sizeof (TpStreamTubePrivate));
}


static void
tp_stream_tube_init (TpStreamTube *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_STREAM_TUBE,
      TpStreamTubePrivate);
}


/**
 * tp_stream_tube_new:
 * @channel: a #TpChannel that has had %TP_CHANNEL_FEATURE_CORE prepared
 *
 * Returns: a newly created #TpStreamTube
 */
TpStreamTube *
tp_stream_tube_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TP_TYPE_STREAM_TUBE,
      "connection", conn,
       "dbus-daemon", conn_proxy->dbus_daemon,
       "bus-name", conn_proxy->bus_name,
       "object-path", object_path,
       "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
       "channel-properties", immutable_properties,
       NULL);
}

static TpSocketAddressType
determine_socket_type (TpChannel *channel,
    GError **error)
{
  GHashTable *properties;
  GHashTable *supported_sockets;

  properties = tp_channel_borrow_immutable_properties (channel);

  supported_sockets = tp_asv_get_boxed (properties,
      TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SUPPORTED_SOCKET_TYPES,
      TP_HASH_TYPE_SUPPORTED_SOCKET_MAP);

#ifdef HAVE_GIO_UNIX
  if (g_hash_table_lookup (supported_sockets,
        GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_UNIX)) != NULL)
    {
      return TP_SOCKET_ADDRESS_TYPE_UNIX;
    }
  else
#endif /* HAVE_GIO_UNIX */
  if (g_hash_table_lookup (supported_sockets,
        GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_IPV4)) != NULL)
    {
      return TP_SOCKET_ADDRESS_TYPE_IPV4;
    }
  else
    {
      /* this should never happen */
      DEBUG ("Unable to find a supported socket type");

      g_set_error (error, TP_ERRORS,
          TP_ERROR_NOT_IMPLEMENTED, "No supported socket types");

      return 0;
    }
}


static void
_socket_connected (GObject *client,
    GAsyncResult *result,
    gpointer user_data)
{
  GSocketConnection *conn;
  GSimpleAsyncResult *simple_result = user_data;
  GError *error = NULL;

  conn = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), result,
      &error);
  if (error != NULL)
    {
      DEBUG ("Failed to connect socket: %s", error->message);

      g_simple_async_result_set_from_error (simple_result, error);
      g_simple_async_result_complete (simple_result);
      g_object_unref (simple_result);
      g_clear_error (&error);

      return;
    }

  DEBUG ("Stream Tube socket connected");

  g_simple_async_result_set_op_res_gpointer (simple_result, conn, NULL);
  g_simple_async_result_complete (simple_result);
  g_object_unref (simple_result);
  g_object_unref (client);
}


static void
_channel_accepted (TpChannel *channel,
    const GValue *addressv,
    const GError *in_error,
    gpointer user_data,
    GObject *self)
{
  GSimpleAsyncResult *result = user_data;
  TpSocketAddressType socket_type;
  GSocketAddress *address;
  GSocketClient *client;
  GError *error = NULL;

  if (in_error != NULL)
    {
      DEBUG ("Failed to Accept Stream Tube: %s", in_error->message);

      g_simple_async_result_set_from_error (result, in_error);
      g_simple_async_result_complete (result);
      g_object_unref (result);

      return;
    }

  socket_type = determine_socket_type (channel, &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      return;
    }

  address = tp_g_socket_address_from_variant (socket_type, addressv, &error);
  if (error != NULL)
    {
      DEBUG ("Failed to convert address: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      return;
    }

  client = g_socket_client_new ();
  g_socket_client_connect_async (client, G_SOCKET_CONNECTABLE (address),
      NULL, _socket_connected, result);

  g_object_unref (address);
}


/**
 * tp_stream_tube_accept_async:
 * @self:
 * @callback:
 * @user_data:
 */
void
tp_stream_tube_accept_async (TpStreamTube *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpSocketAddressType socket_type;
  GValue value = { 0, };
  GError *error = NULL;

  g_return_if_fail (TP_IS_STREAM_TUBE (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_stream_tube_accept_async);

  socket_type = determine_socket_type (TP_CHANNEL (self), &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      return;
    }

  DEBUG ("Using socket type %u", socket_type);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, 0);

  /* Call Accept */
  tp_cli_channel_type_stream_tube_call_accept (TP_CHANNEL (self), -1,
      socket_type, TP_SOCKET_ACCESS_CONTROL_LOCALHOST, &value,
      _channel_accepted, result, NULL, G_OBJECT (self));

  g_value_unset (&value);
}


/*
 * tp_stream_tube_accept_finish:
 * @self:
 * @result:
 * @error:
 *
 * Returns:
 */
GIOStream *
tp_stream_tube_accept_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_STREAM_TUBE (self), NULL);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), tp_stream_tube_accept_async), NULL);

  return g_simple_async_result_get_op_res_gpointer (simple);
}


static void
_new_remote_connection_with_contact (TpConnection *conn,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *in_error,
    gpointer user_data,
    GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;
  TpContact *contact;
  GSocketConnection *sockconn;
  GError *error = NULL;

  if (in_error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", in_error->message);
      return;
    }

  if (n_failed > 0)
    {
      DEBUG ("Failed to prepare TpContact (unspecified error)");
      return;
    }

  /* accept the incoming socket to bring up the connection */
  contact = contacts[0];

  DEBUG ("Accepting incoming GIOStream from %s",
      tp_contact_get_identifier (contact));

  sockconn = g_socket_listener_accept (self->priv->listener, NULL, NULL,
      &error);
  if (error != NULL)
    {
      DEBUG ("Failed to accept incoming socket: %s", error->message);
      return;
    }

  g_signal_emit (self, _signals[INCOMING], 0, contact, sockconn);

  /* anyone receiving the signal is required to hold their own reference */
  g_object_unref (sockconn);
}


static void
_new_remote_connection (TpChannel *channel,
    guint handle,
    const GValue *param,
    guint connection_id,
    gpointer user_data,
    GObject *self)
{
  /* look up the handle */
  tp_connection_get_contacts_by_handle (tp_channel_borrow_connection (channel),
      1, &handle, 0, NULL,
      _new_remote_connection_with_contact,
      NULL, NULL, self);
}


static void
_channel_offered (TpChannel *channel,
    const GError *in_error,
    gpointer user_data,
    GObject *self)
{
  GSimpleAsyncResult *result = user_data;

  if (in_error != NULL)
    {
      DEBUG ("Failed to Offer Stream Tube: %s", in_error->message);

      g_simple_async_result_set_from_error (result, in_error);
      g_simple_async_result_complete (result);
      g_object_unref (result);

      return;
    }

  DEBUG ("Stream Tube offered");

  g_simple_async_result_set_op_res_gboolean (result, TRUE);
  g_simple_async_result_complete (result);
  g_object_unref (result);
}


static void
_offer_with_address (TpStreamTube *self,
    GSimpleAsyncResult *result,
    GHashTable *params)
{
  TpSocketAddressType socket_type;
  GValue *addressv = NULL;
  GError *error = NULL;

  addressv = tp_address_variant_from_g_socket_address (self->priv->address,
      &socket_type, &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      goto finally;
    }

  /* Connect the NewRemoteConnection signal */
  tp_cli_channel_type_stream_tube_connect_to_new_remote_connection (
      TP_CHANNEL (self), _new_remote_connection,
      NULL, NULL, G_OBJECT (self), &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      goto finally;
    }

  if (params != NULL)
    g_hash_table_ref (params);
  else
    params = tp_asv_new (NULL, NULL);

  /* Call Offer */
  tp_cli_channel_type_stream_tube_call_offer (TP_CHANNEL (self), -1,
      socket_type, addressv, TP_SOCKET_ACCESS_CONTROL_LOCALHOST, params,
      _channel_offered, result, NULL, G_OBJECT (self));

  g_hash_table_unref (params);

finally:
  if (addressv != NULL)
    tp_g_value_slice_free (addressv);
}


/**
 * tp_stream_tube_offer_async:
 * @self:
 * @params: (allow none) (transfer none):
 * @callback:
 * @user_data:
 */
void
tp_stream_tube_offer_async (TpStreamTube *self,
    GHashTable *params,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpSocketAddressType socket_type;
  GError *error = NULL;

  g_return_if_fail (TP_IS_STREAM_TUBE (self));

  if (self->priv->listener != NULL)
    {
      g_critical ("Can't reoffer Tube!");
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_stream_tube_offer_async);

  socket_type = determine_socket_type (TP_CHANNEL (self), &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      g_clear_error (&error);

      return;
    }

  DEBUG ("Using socket type %u", socket_type);

  self->priv->listener = g_socket_listener_new ();

  switch (socket_type)
    {
#ifdef HAVE_GIO_UNIX
      case TP_SOCKET_ADDRESS_TYPE_UNIX:
        {
          guint i;

          /* why doesn't GIO provide a method to create a socket we don't
           * care about? Iterate until we find a valid temporary name.
           *
           * Try a maximum of 10 times to get a socket */
          for (i = 0; i < 10; i++)
            {
              self->priv->address = g_unix_socket_address_new (tmpnam (NULL));

              if (g_socket_listener_add_address (self->priv->listener,
                    self->priv->address, G_SOCKET_TYPE_STREAM,
                    G_SOCKET_PROTOCOL_DEFAULT,
                    NULL, NULL, &error))
                {
                  break;
                }
              else
                {
                  g_object_unref (self->priv->address);
                  g_clear_error (&error);
                }
            }

          /* check there wasn't an error on the final attempt */
          if (error != NULL)
            {
              g_simple_async_result_set_from_error (result, error);
              g_simple_async_result_complete (result);
              g_object_unref (result);
              g_clear_error (&error);

              return;
            }
        }

        break;
#endif /* HAVE_GIO_UNIX */

      case TP_SOCKET_ADDRESS_TYPE_IPV4:
        {
          GInetAddress *localhost;
          GSocketAddress *in_address;

          localhost = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
          in_address = g_inet_socket_address_new (localhost, 0);

          g_socket_listener_add_address (self->priv->listener, in_address,
              G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
              NULL, &self->priv->address, &error);

          g_object_unref (localhost);
          g_object_unref (in_address);

          if (error != NULL)
            {
              g_simple_async_result_set_from_error (result, error);
              g_simple_async_result_complete (result);
              g_object_unref (result);
              g_clear_error (&error);

              return;
            }

          break;
        }

      default:
        /* should have already errored */
        g_assert_not_reached ();
        break;
    }

  _offer_with_address (self, result, params);
}


/**
 * tp_stream_tube_offer_existing_async:
 * @self:
 * @params: (allow none) (transfer none):
 * @address: (tranfer none):
 * @callback:
 * @user_data:
 *
 * Offer an existing service over a Stream Tube.
 *
 * @address must be a valid GSocketAddress pointing to a service to be
 * offered.
 */
void
tp_stream_tube_offer_existing_async (TpStreamTube *self,
    GHashTable *params,
    GSocketAddress *address,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_STREAM_TUBE (self));
  g_return_if_fail (G_IS_SOCKET_ADDRESS (address));

  if (self->priv->listener != NULL)
    {
      g_critical ("Can't reoffer Tube!");
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_stream_tube_offer_existing_async);

  self->priv->address = g_object_ref (address);

  _offer_with_address (self, result, params);
}


/**
 * tp_stream_tube_offer_finish:
 * @self:
 * @result:
 * @error:
 *
 * Returns: %TRUE when a Tube has been successfully offered; %FALSE otherwise
 */
gboolean
tp_stream_tube_offer_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_STREAM_TUBE (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), tp_stream_tube_offer_async), FALSE);

  return g_simple_async_result_get_op_res_gboolean (simple);
}

gboolean
tp_stream_tube_offer_existing_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_STREAM_TUBE (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), tp_stream_tube_offer_existing_async), FALSE);

  return g_simple_async_result_get_op_res_gboolean (simple);
}
