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
#include <gio/gunixconnection.h>
#endif /* HAVE_GIO_UNIX */

G_DEFINE_TYPE (TpStreamTube, tp_stream_tube, TP_TYPE_CHANNEL);

/* Used to store the data of a NewRemoteConnection signal while we are waiting
 * for the TCP connection identified by this signal */
typedef struct
{
  TpHandle handle;
  GValue *param;
  guint connection_id;
} SigWaitingConn;

static SigWaitingConn *
sig_waiting_conn_new (TpHandle handle,
    const GValue *param,
    guint connection_id)
{
  SigWaitingConn *ret = g_slice_new0 (SigWaitingConn);

  ret->handle = handle;
  ret->param = tp_g_value_slice_dup (param);
  ret->connection_id = connection_id;
  return ret;
}

static void
sig_waiting_conn_free (SigWaitingConn *sig)
{
  g_assert (sig != NULL);

  tp_g_value_slice_free (sig->param);
  g_slice_free (SigWaitingConn, sig);
}

typedef struct
{
  GSocketConnection *conn;
  /* Used only with TP_SOCKET_ACCESS_CONTROL_CREDENTIALS to store the byte
   * read with the credentials. */
  guchar byte;
} ConnWaitingSig;

static ConnWaitingSig *
conn_waiting_sig_new (GSocketConnection *conn,
    guchar byte)
{
  ConnWaitingSig *ret = g_slice_new0 (ConnWaitingSig);

  ret->conn = g_object_ref (conn);
  ret->byte = byte;
  return ret;
}

static void
conn_waiting_sig_free (ConnWaitingSig *c)
{
  g_assert (c != NULL);

  g_object_unref (c->conn);
  g_slice_free (ConnWaitingSig, c);
}

struct _TpStreamTubePrivate
{
  GHashTable *parameters;

  /* Offering side */
  GSocketService *service;
  GSocketAddress *address;
  /* GSocketConnection we have accepted but are still waiting a
   * NewRemoteConnection to identify them. Owned ConnWaitingSig. */
  GSList *conn_waiting_sig;
  /* NewRemoteConnection signals we have received but didn't accept their TCP
   * connection yet. Owned SigWaitingConn. */
  GSList *sig_waiting_conn;

  TpSocketAddressType socket_type;
  TpSocketAccessControl access_control;
  GCancellable *cancellable;

  GSimpleAsyncResult *result;

  /* (guint) connection ID => weakly reffed GSocketConnection */
  GHashTable *remote_connections;
};

enum
{
  PROP_SERVICE = 1,
  PROP_PARAMETERS
};

enum /* signals */
{
  INCOMING,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };

static gboolean
is_connection (gpointer key,
    gpointer value,
    gpointer user_data)
{
  return value == user_data;
}

static void
remote_connection_destroyed_cb (gpointer user_data,
    GObject *conn)
{
  /* The GSocketConnection has been destroyed, removing it from the hash */
  TpStreamTube *self = user_data;

  g_hash_table_foreach_remove (self->priv->remote_connections, is_connection,
      conn);
}

static void
tp_stream_tube_dispose (GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;

  if (self->priv->service != NULL)
    {
      g_socket_service_stop (self->priv->service);

      tp_clear_object (&self->priv->service);
    }

  tp_clear_object (&self->priv->result);
  tp_clear_pointer (&self->priv->parameters, g_hash_table_unref);

  g_slist_foreach (self->priv->conn_waiting_sig, (GFunc) conn_waiting_sig_free,
      NULL);
  tp_clear_pointer (&self->priv->conn_waiting_sig, g_slist_free);

  g_slist_foreach (self->priv->sig_waiting_conn, (GFunc) sig_waiting_conn_free,
      NULL);
  tp_clear_pointer (&self->priv->sig_waiting_conn, g_slist_free);

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->cancellable);
      tp_clear_object (&self->priv->cancellable);
    }

  if (self->priv->remote_connections != NULL)
    {
      GHashTableIter iter;
      gpointer conn;

      g_hash_table_iter_init (&iter, self->priv->remote_connections);
      while (g_hash_table_iter_next (&iter, NULL, &conn))
        {
          g_object_weak_unref (conn, remote_connection_destroyed_cb, self);
        }

      g_hash_table_unref (self->priv->remote_connections);
      self->priv->remote_connections = NULL;
    }

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

  G_OBJECT_CLASS (tp_stream_tube_parent_class)->dispose (obj);
}

static void
tp_stream_tube_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamTube *self = (TpStreamTube *) object;

  switch (property_id)
    {
      case PROP_SERVICE:
        g_value_set_string (value, tp_stream_tube_get_service (self));
        break;

      case PROP_PARAMETERS:
        g_value_set_boxed (value, self->priv->parameters);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_stream_tube_constructed (GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;

   /*  Tube.Parameters is immutable for incoming tubes. For outgoing ones,
    *  it's defined when offering the tube. */
  if (!tp_channel_get_requested (TP_CHANNEL (self)))
    {
      GHashTable *props;
      GHashTable *params;

      props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

      params = tp_asv_get_boxed (props,
          TP_PROP_CHANNEL_INTERFACE_TUBE_PARAMETERS,
          TP_HASH_TYPE_STRING_VARIANT_MAP);

      if (params == NULL)
        {
          DEBUG ("Incoming tube doesn't have Tube.Parameters property");

          self->priv->parameters = tp_asv_new (NULL, NULL);
        }
      else
        {
          self->priv->parameters = g_boxed_copy (
              TP_HASH_TYPE_STRING_VARIANT_MAP, params);
        }
    }
}

static void
tp_stream_tube_class_init (TpStreamTubeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->constructed = tp_stream_tube_constructed;
  gobject_class->get_property = tp_stream_tube_get_property;
  gobject_class->dispose = tp_stream_tube_dispose;

  /**
   * TpStreamTube:service:
   *
   * A string representing the service name that will be used over the tube.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_string ("service", "Service",
      "The service of the stream tube",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SERVICE, param_spec);

  /**
   * TpStreamTube:parameters:
   *
   * A string to #GValue #GHashTable representing the parameters of the tube.
   *
   * Will be %NULL for outgoing tubes until the tube has been offered.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("parameters", "Parameters",
      "The parameters of the stream tube",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_PARAMETERS, param_spec);

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

  self->priv->remote_connections = g_hash_table_new (NULL, NULL);
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

/* Return the 'best' TpSocketAccessControl for the socket type, falling back
 * to TP_SOCKET_ACCESS_CONTROL_LOCALHOST if needed. */
static TpSocketAccessControl
find_best_access_control (GArray *arr,
    TpSocketAddressType socket_type,
    GError **error)
{
  gboolean support_localhost = FALSE;
  TpSocketAccessControl best = TP_SOCKET_ACCESS_CONTROL_LOCALHOST;
  guint i;

  switch (socket_type)
    {
    case TP_SOCKET_ADDRESS_TYPE_UNIX:
    case TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX:
      {
        for (i = 0; i < arr->len; i++)
          {
            TpSocketAccessControl _access = g_array_index (arr,
              TpSocketAccessControl, i);

            if (_access == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
              best = _access;
            else if (_access == TP_SOCKET_ACCESS_CONTROL_LOCALHOST)
              support_localhost = TRUE;
          }

        if (best == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
          return TP_SOCKET_ACCESS_CONTROL_CREDENTIALS;
      }
      break;

    case TP_SOCKET_ADDRESS_TYPE_IPV4:
    case TP_SOCKET_ADDRESS_TYPE_IPV6:
      {
        for (i = 0; i < arr->len; i++)
          {
            TpSocketAccessControl _access = g_array_index (arr,
              TpSocketAccessControl, i);

            if (_access == TP_SOCKET_ACCESS_CONTROL_PORT)
              best = _access;
            else if (_access == TP_SOCKET_ACCESS_CONTROL_LOCALHOST)
              support_localhost = TRUE;
          }

        if (best == TP_SOCKET_ACCESS_CONTROL_PORT)
          return TP_SOCKET_ACCESS_CONTROL_PORT;
      }
      break;
    }

  if (!support_localhost)
    {
      g_set_error (error, TP_ERRORS,
          TP_ERROR_NOT_IMPLEMENTED, "No supported access control");
    }

  return TP_SOCKET_ACCESS_CONTROL_LOCALHOST;
}

static TpSocketAddressType
determine_socket_type (TpStreamTube *self,
    GError **error)
{
  GHashTable *properties;
  GHashTable *supported_sockets;
  GArray *arr;

  properties = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  supported_sockets = tp_asv_get_boxed (properties,
      TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SUPPORTED_SOCKET_TYPES,
      TP_HASH_TYPE_SUPPORTED_SOCKET_MAP);

#ifdef HAVE_GIO_UNIX
  arr = g_hash_table_lookup (supported_sockets,
      GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_UNIX));

  if (arr != NULL)
    {
      self->priv->access_control = find_best_access_control (arr,
          TP_SOCKET_ADDRESS_TYPE_UNIX, error);

      return TP_SOCKET_ADDRESS_TYPE_UNIX;
    }
#endif /* HAVE_GIO_UNIX */

  arr = g_hash_table_lookup (supported_sockets,
      GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_IPV4));
  if (arr != NULL)
    {
      self->priv->access_control = find_best_access_control (arr,
          TP_SOCKET_ADDRESS_TYPE_IPV4, error);

      return TP_SOCKET_ADDRESS_TYPE_IPV4;
    }

  arr = g_hash_table_lookup (supported_sockets,
      GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_IPV6));
  if (arr != NULL)
    {
      self->priv->access_control = find_best_access_control (arr,
          TP_SOCKET_ADDRESS_TYPE_IPV6, error);

      return TP_SOCKET_ADDRESS_TYPE_IPV6;
    }

  /* this should never happen */
  DEBUG ("Unable to find a supported socket type");

  g_set_error (error, TP_ERRORS,
      TP_ERROR_NOT_IMPLEMENTED, "No supported socket types");

  return 0;
}

static void
operation_failed (TpStreamTube *self,
    const GError *error)
{
  g_simple_async_result_set_from_error (self->priv->result, error);

  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

static void
complete_accept_operation (TpStreamTube *self,
    GSocketConnection *conn)
{
  g_simple_async_result_set_op_res_gpointer (self->priv->result, conn, NULL);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

static void
_socket_connected (GObject *client,
    GAsyncResult *result,
    gpointer user_data)
{
  TpStreamTube *self = user_data;
  GSocketConnection *conn;
  GError *error = NULL;

  conn = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), result,
      &error);
  if (error != NULL)
    {
      DEBUG ("Failed to connect socket: %s", error->message);

      operation_failed (self, error);
      g_clear_error (&error);
      return;
    }

  DEBUG ("Stream Tube socket connected");

#ifdef HAVE_GIO_UNIX
  if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
    {
      if (!g_unix_connection_send_credentials (G_UNIX_CONNECTION (conn), NULL,
            &error))
        {
          DEBUG ("Failed to send credentials: %s", error->message);

          operation_failed (self, error);
          g_clear_error (&error);
          return;
        }
    }
#endif

  complete_accept_operation (self, conn);
  g_object_unref (client);
}


static void
_channel_accepted (TpChannel *channel,
    const GValue *addressv,
    const GError *in_error,
    gpointer user_data,
    GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;
  GSocketAddress *address;
  GSocketClient *client;
  GError *error = NULL;

  if (in_error != NULL)
    {
      DEBUG ("Failed to Accept Stream Tube: %s", in_error->message);

      operation_failed (self, in_error);
      return;
    }

  address = tp_g_socket_address_from_variant (self->priv->socket_type,
      addressv, &error);
  if (error != NULL)
    {
      DEBUG ("Failed to convert address: %s", error->message);

      operation_failed (self, in_error);

      g_clear_error (&error);
      return;
    }

  client = g_socket_client_new ();
  g_socket_client_connect_async (client, G_SOCKET_CONNECTABLE (address),
      self->priv->cancellable, _socket_connected, self);

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
  GValue value = { 0, };
  GError *error = NULL;

  g_return_if_fail (TP_IS_STREAM_TUBE (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_stream_tube_accept_async);

  self->priv->socket_type = determine_socket_type (self, &error);
  if (error != NULL)
    {
      operation_failed (self, error);

      g_clear_error (&error);
      return;
    }

  DEBUG ("Using socket type %u with access control %u", self->priv->socket_type,
      self->priv->access_control);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, 0);

  /* Call Accept */
  tp_cli_channel_type_stream_tube_call_accept (TP_CHANNEL (self), -1,
      self->priv->socket_type, self->priv->access_control, &value,
      _channel_accepted, NULL, NULL, G_OBJECT (self));

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
  GSocketConnection *sockconn = user_data;

  if (in_error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", in_error->message);
      goto out;
    }

  if (n_failed > 0)
    {
      DEBUG ("Failed to prepare TpContact (unspecified error)");
      goto out;
    }

  /* accept the incoming socket to bring up the connection */
  contact = contacts[0];

  DEBUG ("Accepting incoming GIOStream from %s",
      tp_contact_get_identifier (contact));

  g_signal_emit (self, _signals[INCOMING], 0, contact, sockconn);

  /* anyone receiving the signal is required to hold their own reference */
out:
  g_object_unref (sockconn);
}

static gboolean
sig_match_conn (TpStreamTube *self,
    SigWaitingConn *sig,
    ConnWaitingSig *c)
{
  if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_PORT)
    {
      /* Use the port to identify the connection */
      guint port;
      GSocketAddress *address;
      GError *error = NULL;

      address = g_socket_connection_get_remote_address (c->conn, &error);
      if (address == NULL)
        {
          DEBUG ("Failed to get connection address: %s", error->message);

          g_error_free (error);
          return FALSE;
        }

      dbus_g_type_struct_get (sig->param, 1, &port, G_MAXINT);

      if (port == g_inet_socket_address_get_port (
            G_INET_SOCKET_ADDRESS (address)))
        {
          DEBUG ("Identified connection %u using port %u",
              port, sig->connection_id);
          return TRUE;
        }

      g_object_unref (address);
    }
  else if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
    {
      guchar byte;

      byte = g_value_get_uchar (sig->param);

      return byte == c->byte;
    }
  else
    {
      DEBUG ("Can't properly identify connection as we are using "
          "access control %u. Assume it's the head of the list",
          self->priv->access_control);

      return TRUE;
    }

  return FALSE;
}

static void
connection_identified (TpStreamTube *self,
    GSocketConnection *conn,
    TpHandle handle,
    guint connection_id)
{
  g_hash_table_insert (self->priv->remote_connections,
      GUINT_TO_POINTER (connection_id), conn);

  g_object_weak_ref (G_OBJECT (conn), remote_connection_destroyed_cb, self);

  tp_connection_get_contacts_by_handle (
      tp_channel_borrow_connection (TP_CHANNEL (self)),
      1, &handle, 0, NULL,
      _new_remote_connection_with_contact,
      g_object_ref (conn), NULL, G_OBJECT (self));
}

static void
_new_remote_connection (TpChannel *channel,
    guint handle,
    const GValue *param,
    guint connection_id,
    gpointer user_data,
    GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;
  GSList *l;
  ConnWaitingSig *found_conn = NULL;
  SigWaitingConn *sig;

  sig = sig_waiting_conn_new (handle, param, connection_id);

  for (l = self->priv->conn_waiting_sig; l != NULL && found_conn == NULL;
      l = g_slist_next (l))
    {
      ConnWaitingSig *conn = l->data;

      if (sig_match_conn (self, sig, conn))
        found_conn = conn;

    }

  if (found_conn == NULL)
    {
      DEBUG ("Didn't find any connection for %u. Waiting for more",
          connection_id);

      /* Pass ownership of sig to the list */
      self->priv->sig_waiting_conn = g_slist_append (
          self->priv->sig_waiting_conn, sig);
      return;
    }

  /* We found a connection */
  self->priv->conn_waiting_sig = g_slist_remove (
      self->priv->conn_waiting_sig, found_conn);

  connection_identified (self, found_conn->conn, handle, connection_id);

  sig_waiting_conn_free (sig);
  conn_waiting_sig_free (found_conn);
}

static void
_channel_offered (TpChannel *channel,
    const GError *in_error,
    gpointer user_data,
    GObject *obj)
{
  TpStreamTube *self = (TpStreamTube *) obj;

  if (in_error != NULL)
    {
      DEBUG ("Failed to Offer Stream Tube: %s", in_error->message);

      operation_failed (self, in_error);
      return;
    }

  DEBUG ("Stream Tube offered");

  g_simple_async_result_set_op_res_gboolean (self->priv->result, TRUE);
  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}


static void
_offer_with_address (TpStreamTube *self,
    GHashTable *params)
{
  GValue *addressv = NULL;
  GError *error = NULL;

  addressv = tp_address_variant_from_g_socket_address (self->priv->address,
      &self->priv->socket_type, &error);
  if (error != NULL)
    {
      operation_failed (self, error);

      g_clear_error (&error);
      goto finally;
    }

  /* Connect the NewRemoteConnection signal */
  tp_cli_channel_type_stream_tube_connect_to_new_remote_connection (
      TP_CHANNEL (self), _new_remote_connection,
      NULL, NULL, G_OBJECT (self), &error);
  if (error != NULL)
    {
      operation_failed (self, error);

      g_clear_error (&error);
      goto finally;
    }

  if (params != NULL)
    self->priv->parameters = g_hash_table_ref (params);
  else
    self->priv->parameters = tp_asv_new (NULL, NULL);

  g_object_notify (G_OBJECT (self), "parameters");

  /* Call Offer */
  tp_cli_channel_type_stream_tube_call_offer (TP_CHANNEL (self), -1,
      self->priv->socket_type, addressv, self->priv->access_control,
      self->priv->parameters, _channel_offered, NULL, NULL, G_OBJECT (self));

finally:
  if (addressv != NULL)
    tp_g_value_slice_free (addressv);
}

static SigWaitingConn *
find_sig_for_conn (TpStreamTube *self,
    ConnWaitingSig *c)
{
  GSList *l;

  for (l = self->priv->sig_waiting_conn; l != NULL; l = g_slist_next (l))
    {
      SigWaitingConn *sig = l->data;

      if (sig_match_conn (self, sig, c))
        return sig;
    }

  return NULL;
}

static void
service_incoming_cb (GSocketService *service,
    GSocketConnection *conn,
    GObject *source_object,
    gpointer user_data)
{
  TpStreamTube *self = user_data;
  SigWaitingConn *sig;
  ConnWaitingSig *c;
  guchar byte = 0;

  DEBUG ("New incoming connection");

#ifdef HAVE_GIO_UNIX
  /* Check the credentials if needed */
  if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
    {
      GCredentials *creds;
      uid_t uid;
      GError *error = NULL;

      creds = tp_unix_connection_receive_credentials_with_byte (
          G_UNIX_CONNECTION (conn), &byte, NULL, &error);
      if (creds == NULL)
        {
          DEBUG ("Failed to receive credentials: %s", error->message);

          g_error_free (error);
          return;
        }

      uid = g_credentials_get_unix_user (creds, &error);
      g_object_unref (creds);

      if (uid != geteuid ())
        {
          DEBUG ("Wrong credentials received (user: %u)", uid);
          return;
        }
    }
#endif

  c = conn_waiting_sig_new (conn, byte);

  sig = find_sig_for_conn (self, c);
  if (sig == NULL)
    {
      DEBUG ("Can't identify the connection, wait for NewRemoteConnection sig");

      /* Pass ownership to the list */
      self->priv->conn_waiting_sig = g_slist_append (
          self->priv->conn_waiting_sig, c);

      return;
    }

  /* Connection has been identified */
  self->priv->sig_waiting_conn = g_slist_remove (self->priv->sig_waiting_conn,
      sig);

  connection_identified (self, conn, sig->handle, sig->connection_id);

  sig_waiting_conn_free (sig);
  conn_waiting_sig_free (c);
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
  TpSocketAddressType socket_type;
  GError *error = NULL;

  g_return_if_fail (TP_IS_STREAM_TUBE (self));
  g_return_if_fail (self->priv->result == NULL);

  if (self->priv->service != NULL)
    {
      g_critical ("Can't reoffer Tube!");
      return;
    }

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_stream_tube_offer_async);

  socket_type = determine_socket_type (self, &error);
  if (error != NULL)
    {
      operation_failed (self, error);

      g_clear_error (&error);
      return;
    }

  DEBUG ("Using socket type %u with access control %u", self->priv->socket_type,
      self->priv->access_control);

  self->priv->service = g_socket_service_new ();

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

              if (g_socket_listener_add_address (
                    G_SOCKET_LISTENER (self->priv->service),
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
              operation_failed (self, error);

              g_clear_error (&error);
              return;
            }
        }

        break;
#endif /* HAVE_GIO_UNIX */

      case TP_SOCKET_ADDRESS_TYPE_IPV4:
      case TP_SOCKET_ADDRESS_TYPE_IPV6:
        {
          GInetAddress *localhost;
          GSocketAddress *in_address;

          localhost = g_inet_address_new_loopback (
              socket_type == TP_SOCKET_ADDRESS_TYPE_IPV4 ?
              G_SOCKET_FAMILY_IPV4 : G_SOCKET_FAMILY_IPV6);
          in_address = g_inet_socket_address_new (localhost, 0);

          g_socket_listener_add_address (
              G_SOCKET_LISTENER (self->priv->service), in_address,
              G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
              NULL, &self->priv->address, &error);

          g_object_unref (localhost);
          g_object_unref (in_address);

          if (error != NULL)
            {
              operation_failed (self, error);

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

  tp_g_signal_connect_object (self->priv->service, "incoming",
      G_CALLBACK (service_incoming_cb), self, 0);

  g_socket_service_start (self->priv->service);

  _offer_with_address (self, params);
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
  g_return_if_fail (TP_IS_STREAM_TUBE (self));
  g_return_if_fail (G_IS_SOCKET_ADDRESS (address));
  g_return_if_fail (self->priv->result == NULL);

  if (self->priv->service != NULL)
    {
      g_critical ("Can't reoffer Tube!");
      return;
    }

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_stream_tube_offer_existing_async);

  self->priv->address = g_object_ref (address);

  _offer_with_address (self, params);
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

/**
 * tp_stream_tube_get_service: (skip)
 * @self: a #TpStreamTube
 *
 * Return the #TpStreamTube:service property
 *
 * Returns: (transfer none): the value of #TpStreamTube:service
 *
 * Since: 0.11.UNRELEASED
 */
const gchar *
tp_stream_tube_get_service (TpStreamTube *self)
{
  GHashTable *props;

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  return tp_asv_get_string (props, TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE);
}

/**
 * tp_stream_tube_get_parameters: (skip)
 * @self: a #TpStreamTube
 *
 * Return the #TpStreamTube:parameters property
 *
 * Returns: (transfer none) (element-type utf8 GObject.Value):
 * the value of #TpStreamTube:parameters
 *
 * Since: 0.11.UNRELEASED
 */
GHashTable *
tp_stream_tube_get_parameters (TpStreamTube *self)
{
  return self->priv->parameters;
}
