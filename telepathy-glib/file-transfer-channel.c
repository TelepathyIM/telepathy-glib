/*
 * file-transfer-channel.h - high level API for Chan.I.FileTransfer
 *
 * Copyright (C) 2010-2011 Morten Mjelva <morten.mjelva@gmail.com>
 * Copyright (C) 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

/**
 * SECTION:file-transfer-channel
 * @title: TpFileTransferChannel
 * @short_description: proxy object for a file transfer channel
 *
 * #TpFileTransferChannel is a sub-class of #TpChannel providing convenient
 * API to send and receive files.
 */

/**
 * TpFileTransferChannel:
 *
 * Data structure representing a #TpFileTransferChannel.
 *
 * Since: 0.15.5
 */

/**
 * TpFileTransferChannelClass:
 *
 * The class of a #TpFileTransferChannel.
 *
 * Since: 0.15.5
 */

#include <config.h>

#include "telepathy-glib/file-transfer-channel.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/proxy-internal.h>
#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/automatic-client-factory-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "_gen/signals-marshal.h"

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#include <gio/gunixconnection.h>
#endif /* HAVE_GIO_UNIX */

G_DEFINE_TYPE (TpFileTransferChannel, tp_file_transfer_channel, TP_TYPE_CHANNEL)

struct _TpFileTransferChannelPrivate
{
    /* Exposed properties */
    const gchar *mime_type;
    GDateTime *date;
    const gchar *description;
    const gchar *filename;
    guint64 size;
    guint64 transferred_bytes;
    TpFileTransferState state;
    TpFileTransferStateChangeReason state_reason;
    GFile *file;

    /* Hidden properties */
    /* borrowed from the immutable properties GHashTable */
    GHashTable *available_socket_types;
    const gchar *content_hash;
    TpFileHashType content_hash_type;
    goffset initial_offset;

    /* Accepting side */
    GSocket*client_socket;
    /* The access_control_param we passed to Accept */
    GValue *access_control_param;

    /* Offering side */
    GSocketService *service;
    GSocketAddress *address;

    TpSocketAddressType socket_type;
    TpSocketAccessControl access_control;
    GSimpleAsyncResult *result;
};

enum /* properties */
{
  PROP_MIME_TYPE = 1,
  PROP_DATE,
  PROP_DESCRIPTION,
  PROP_FILENAME,
  PROP_SIZE,
  PROP_STATE,
  PROP_TRANSFERRED_BYTES,
  PROP_FILE,
  PROP_INITIAL_OFFSET,
  N_PROPS
};

static void
operation_failed (TpFileTransferChannel *self,
    const GError *error)
{
  g_simple_async_result_set_from_error (self->priv->result, error);

  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

static void
incoming_splice_done_cb (GObject *output,
    GAsyncResult *result,
    gpointer user_data)
{
  gssize size;
  GError *error = NULL;

  size = g_output_stream_splice_finish (G_OUTPUT_STREAM (output), result,
      &error);
}

static void
client_socket_connected (TpFileTransferChannel *self)
{
  GSocketConnection *conn;
  GFileOutputStream *out;
  GInputStream *in;
  GError *error = NULL;

  conn = g_socket_connection_factory_create_connection (
      self->priv->client_socket);
  if (conn == NULL)
    {
      DEBUG ("Failed to create client connection: %s", error->message);
      operation_failed (self, error);
      return;
    }

  DEBUG ("File transfer socket connected");

#ifdef HAVE_GIO_UNIX
  if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
    {
      guchar byte;

      byte = g_value_get_uchar (self->priv->access_control_param);

      /* FIXME: we should an async version of this API (bgo #629503) */
      if (!tp_unix_connection_send_credentials_with_byte (
            conn, byte, NULL, &error))
        {
          DEBUG ("Failed to send credentials: %s", error->message);

          operation_failed (self, error);
          g_clear_error (&error);
          return;
        }
    }
#endif

  /* Get an input/output streams from the SocketConnection and the GFile */
  in = g_io_stream_get_input_stream (G_IO_STREAM (conn));

  out = g_file_replace (self->priv->file, NULL, FALSE,
      G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);
  if (out == NULL)
    {
      DEBUG ("Failed to get output stream: %s", error->message);
      operation_failed (self, error);
      return;
    }

  g_output_stream_splice_async (G_OUTPUT_STREAM (out), in,
      G_OUTPUT_STREAM_SPLICE_NONE, 0, NULL, incoming_splice_done_cb, self);

  DEBUG ("Leaving client_socket_connected");
}

static gboolean
client_socket_cb (GSocket *socket,
    GIOCondition condition,
    TpFileTransferChannel *self)
{
  GError *error = NULL;

  if (!g_socket_check_connect_result (socket, &error))
    {
      DEBUG ("Failed to connect to socket: %s", error->message);

      operation_failed (self, error);
      g_error_free (error);
      return FALSE;
    }

  DEBUG ("Client socket connected after pending");
  client_socket_connected (self);

  return FALSE;
}


/* Callbacks */

static void
tp_file_transfer_channel_state_changed_cb (TpChannel *proxy,
    guint state,
    guint reason,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;

  self->priv->state = state;
  self->priv->state_reason = reason;
  g_object_notify (G_OBJECT (self), "state");
}

static void
tp_file_transfer_channel_initial_offset_defined_cb (TpChannel *proxy,
    guint64 initial_offset,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;

  self->priv->initial_offset = initial_offset;
  g_object_notify (G_OBJECT (self), "initial-offset");
}

static void
tp_file_transfer_channel_transferred_bytes_changed_cb (TpChannel *proxy,
    guint64 count,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;

  self->priv->transferred_bytes = count;
  g_object_notify (G_OBJECT (self), "transferred-bytes");
}

static void
tp_file_transfer_channel_uri_defined_cb (TpChannel *proxy,
    const gchar *uri,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;

  self->priv->file = g_file_new_for_uri (uri);
  g_object_notify (G_OBJECT (self), "file");
}

static void
tp_file_transfer_channel_prepare_core_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;
  GSimpleAsyncResult *result = user_data;
  gboolean valid;
  const gchar *uri;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      goto out;
    }

  self->priv->state = tp_asv_get_uint32 (properties, "State", &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.State property",
          tp_proxy_get_object_path (self));
    }

  self->priv->transferred_bytes = tp_asv_get_uint64 (properties,
      "TransferredBytes", &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.TransferredBytes property",
          tp_proxy_get_object_path (self));
    }

  self->priv->initial_offset = tp_asv_get_uint64 (properties, "InitialOffset",
      &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.InitialOffset property",
          tp_proxy_get_object_path (self));
    }

  /* URI might already be set from immutable properties */
  uri = tp_asv_get_string (properties, "URI");
  if (self->priv->file == NULL && uri != NULL)
    self->priv->file = g_file_new_for_uri (uri);

out:
  g_simple_async_result_complete (result);
}

static void
accept_file_cb (TpChannel *proxy,
    const GValue *addressv,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;
  GSocketAddress *remote_address;
  GError *error = NULL;

  DEBUG ("enter");

  if (error != NULL)
    {
      DEBUG ("Failed to accept file: %s", error->message);

      operation_failed (self, error);
      return;
    }

  remote_address = tp_g_socket_address_from_variant (self->priv->socket_type,
      addressv, &error);
  if (remote_address == NULL)
    {
      DEBUG ("Failed to convert address: %s", error->message);
      operation_failed (self, error);
      g_error_free (error);
      return;
    }

  /* TODO: Async? */
  g_socket_set_blocking (self->priv->client_socket, FALSE);

  /* g_socket_connect returns true on successful connection */
  if (g_socket_connect (self->priv->client_socket, remote_address, NULL,
        &error))
    {
      DEBUG ("Client socket connected immediately");
      client_socket_connected (self);
      goto out;
    }
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PENDING))
    {
      /* The connection is pending */
      GSource *source;

      source = g_socket_create_source (self->priv->client_socket, G_IO_OUT,
          NULL);

      g_source_attach (source, g_main_context_get_thread_default ());
      g_source_set_callback (source, (GSourceFunc) client_socket_cb, self,
          NULL);

      g_error_free (error);
      g_source_unref (source);
    }
  else
    {
      DEBUG ("Failed to connect to socket: %s:", error->message);

      operation_failed (self, error);
      g_error_free (error);
    }

/* TODO: Don't return until the file transfer completes */

out:
  g_object_unref (remote_address);
//  g_simple_async_result_complete (self->priv->result);
}

static void
provide_file_cb (TpChannel *proxy,
    const GValue *addressv,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;
  GSocketAddress *remote_address;
  GError *error = NULL;

  DEBUG ("enter");

  if (error != NULL)
    {
      DEBUG ("Failed to offer file: %s", error->message);

      operation_failed (self, error);
      return;
    }

  remote_address = tp_g_socket_address_from_variant (self->priv->socket_type,
      addressv, &error);
  /* FIXME: Isn't really offered (but at least we haven't crashed) */
  DEBUG ("File offered");

  g_simple_async_result_complete (self->priv->result);
  tp_clear_object (&self->priv->result);
}

static void
service_incoming_cb (GSocketService *service,
    GSocketConnection *conn,
    GObject *source_object,
    gpointer user_data)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) user_data;
  guchar byte = 0;

  DEBUG ("New incoming connection");

#ifdef HAVE_GIO_UNIX
  /* Check the credentials if needed */
  if (self->priv->access_control == TP_SOCKET_ACCESS_CONTROL_CREDENTIALS)
    {
      GCredentials *creds;
      uid_t uid;
      GError *error = NULL;

      /* Should be async */
      creds = tp_unix_connection_receive_credentials_with_byte (
          conn, &byte, NULL, &error);
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
#endif /* HAVE_GIO_UNIX */
}

/* Private methods */

static void
tp_file_transfer_channel_prepare_core_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;
  TpChannel *channel = (TpChannel *) proxy;
  GSimpleAsyncResult *result;
  GError *error = NULL;

  tp_cli_channel_type_file_transfer_connect_to_file_transfer_state_changed (
      channel, tp_file_transfer_channel_state_changed_cb,
      NULL, NULL, NULL, &error);
  if (error != NULL)
    {
      WARNING ("Failed to connect to StateChanged on %s: %s",
          tp_proxy_get_object_path (self), error->message);
      g_error_free (error);
    }

  tp_cli_channel_type_file_transfer_connect_to_initial_offset_defined (
      channel, tp_file_transfer_channel_initial_offset_defined_cb,
      NULL, NULL, NULL, &error);
  if (error != NULL)
    {
      WARNING ("Failed to connect to InitialOffsetDefined on %s: %s",
          tp_proxy_get_object_path (self), error->message);
      g_error_free (error);
    }

  tp_cli_channel_type_file_transfer_connect_to_transferred_bytes_changed (
      channel, tp_file_transfer_channel_transferred_bytes_changed_cb,
      NULL, NULL, NULL, &error);
  if (error != NULL)
    {
      WARNING ("Failed to connect to TransferredBytesChanged on %s: %s",
          tp_proxy_get_object_path (self), error->message);
      g_error_free (error);
    }

  tp_cli_channel_type_file_transfer_connect_to_uri_defined (
      channel, tp_file_transfer_channel_uri_defined_cb,
      NULL, NULL, NULL, &error);
  if (error != NULL)
    {
      WARNING ("Failed to connect to UriDefined on %s: %s",
          tp_proxy_get_object_path (self), error->message);
      g_error_free (error);
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_file_transfer_channel_prepare_core_async);

  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
      tp_file_transfer_channel_prepare_core_cb,
      result, g_object_unref,
      NULL);
}

static void
tp_file_transfer_channel_constructed (GObject *obj)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) obj;
  GHashTable *properties;
  gboolean valid;
  gint64 date;
  const gchar *uri;

  G_OBJECT_CLASS (tp_file_transfer_channel_parent_class)->constructed (obj);

  properties = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  self->priv->mime_type = tp_asv_get_string (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_CONTENT_TYPE);
  if (self->priv->mime_type == NULL)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.ContentType in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->filename = tp_asv_get_string (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_FILENAME);
  if (self->priv->filename == NULL)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.Filename in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->size = tp_asv_get_uint64 (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_SIZE, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.Size in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->content_hash_type = tp_asv_get_uint32 (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_CONTENT_HASH_TYPE, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.ContentHashType in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->content_hash = tp_asv_get_string (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_CONTENT_HASH);
  if (self->priv->content_hash == NULL)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.ContentHash in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->description = tp_asv_get_string (properties,
    TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_DESCRIPTION);
  if (self->priv->description == NULL)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.Description in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  date = tp_asv_get_int64 (properties, TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_DATE,
      &valid);

  if (!valid)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.Date in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }
  else
    {
      self->priv->date = g_date_time_new_from_unix_utc (date);
    }

  self->priv->available_socket_types = tp_asv_get_boxed (properties,
     TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_AVAILABLE_SOCKET_TYPES,
     TP_HASH_TYPE_SUPPORTED_SOCKET_MAP);
  if (self->priv->available_socket_types == NULL)
    {
      DEBUG ("Channel %s doesn't have FileTransfer.AvailableSocketTypes in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  /* URI might be immutable */
  uri = tp_asv_get_string (properties, TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_URI);
  if (uri != NULL)
    self->priv->file = g_file_new_for_uri (uri);
}

static void
tp_file_transfer_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) object;

  switch (property_id)
    {
      case PROP_MIME_TYPE:
        g_value_set_string (value, self->priv->mime_type);
        break;

      case PROP_DATE:
        g_value_set_boxed (value, self->priv->date);
        break;

      case PROP_DESCRIPTION:
        g_value_set_string (value, self->priv->description);
        break;

      case PROP_FILENAME:
        g_value_set_string (value, self->priv->filename);
        break;

      case PROP_SIZE:
        g_value_set_uint64 (value, self->priv->size);
        break;

      case PROP_STATE:
        g_value_set_uint (value, self->priv->state);
        break;

      case PROP_TRANSFERRED_BYTES:
        g_value_set_uint64 (value, self->priv->transferred_bytes);
        break;

      case PROP_FILE:
        g_value_set_object (value, self->priv->file);
        break;

      case PROP_INITIAL_OFFSET:
        g_value_set_uint64 (value, self->priv->initial_offset);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

enum /* features */
{
  FEAT_CORE,
  N_FEAT
};

static const TpProxyFeature *
tp_file_transfer_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
      return features;

  features[FEAT_CORE].name = TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE;
  features[FEAT_CORE].core = TRUE;
  features[FEAT_CORE].prepare_async =
    tp_file_transfer_channel_prepare_core_async;

  /* Assert that the terminator at the end is present */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_file_transfer_channel_dispose (GObject *obj)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) obj;

  tp_clear_pointer (&self->priv->date, g_date_time_unref);
  g_clear_object (&self->priv->file);

  if (self->priv->service != NULL)
    {
      g_socket_service_stop (self->priv->service);
      tp_clear_object (&self->priv->service);
    }

  if (self->priv->address != NULL)
    {
#ifdef HAVE_GIO_UNIX
      /* Check if we need to remove our temp file */
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

  tp_clear_pointer (&self->priv->access_control_param, tp_g_value_slice_free);
  tp_clear_object (&self->priv->client_socket);

  G_OBJECT_CLASS (tp_file_transfer_channel_parent_class)->dispose (obj);
}

static void
tp_file_transfer_channel_class_init (TpFileTransferChannelClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructed = tp_file_transfer_channel_constructed;
  object_class->get_property = tp_file_transfer_channel_get_property;
  object_class->dispose = tp_file_transfer_channel_dispose;

  proxy_class->list_features = tp_file_transfer_channel_list_features;

  /* Properties */

  /**
   * TpFileTransferChannel:mime-type:
   *
   * The MIME type of the file to be transferred.
   *
   * Since: 0.15.5
   */
  param_spec = g_param_spec_string ("mime-type",
      "ContentType",
      "The ContentType property of this channel",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MIME_TYPE,
      param_spec);

  /**
   * TpFileTransferChannel:date
   *
   * A #GDateTime representing the last modification time of the file to be
   * transferred.
   *
   * Since 0.15.5
   */
  param_spec = g_param_spec_boxed ("date",
      "Date",
      "The Date property of this channel",
      G_TYPE_DATE_TIME,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DATE,
      param_spec);

  /**
   * TpFileTransferChannel:description
   *
   * The description of the file transfer, defined by the sender when offering
   * the file.
   *
   * Since 0.15.5
   */
  param_spec = g_param_spec_string ("description",
      "Description",
      "The Description property of this channel",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DESCRIPTION,
      param_spec);

  /**
    * TpFileTransferChannel:file
    *
    * For incoming, this property may be set to the location where the file
    * will be saved once the transfer starts. The feature
    * %TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE must already be prepared for this
    * property to have a meaningful value, and to receive change notification.
    * Once the initial value is set, this can no be changed.
    *
    * For outgoing, this property may be set to the location of the file being
    * sent. The feature %TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE does not have
    * to be prepared and there is no change notification.
    *
    * Since: 0.15.UNRELEASED
    */
  param_spec = g_param_spec_object ("file" ,
      "File",
      "A GFile corresponding to the URI property of this channel",
      G_TYPE_FILE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILE,
      param_spec);

  /**
   * TpFileTransferChannel:filename
   *
   * The name of the file on the sender's side. This is therefore given as a
   * suggested filename for the receiver.
   *
   * Since 0.15.5
   */
  param_spec = g_param_spec_string ("filename",
      "Filename",
      "The Filename property of this channel",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILENAME,
      param_spec);

  /**
   * TpFileTransferChannel:size
   *
   * The size of the file to be transferred,
   * or %G_MAXUINT64 if not known.
   *
   * Since 0.15.5
   */
  param_spec = g_param_spec_uint64 ("size",
      "Size",
      "The Size property of this channel",
      0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIZE,
      param_spec);

  /**
   * TpFileTransferChannel:state
   *
   * A TpFileTransferState holding the state of the file transfer.
   *
   * Since 0.15.UNRELEASED
   */
  param_spec = g_param_spec_uint ("state",
      "State",
      "The TpFileTransferState of the channel",
      0, NUM_TP_FILE_TRANSFER_STATES, TP_FILE_TRANSFER_STATE_NONE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STATE,
      param_spec);

  /**
   * TpFileTransferChannel:transferred-bytes
   *
   * The number of bytes transferred so far in this
   * file transfer.
   *
   * The %TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE feature has to be
   * prepared for this property to be meaningful and kept up to date.
   *
   * Since: 0.15.5
   */
  param_spec = g_param_spec_uint64 ("transferred-bytes",
      "TransferredBytes",
      "The TransferredBytes property of this channel",
      0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TRANSFERRED_BYTES,
      param_spec);

  /**
   * TpFileTransferChannel:initial-offset
   *
   * The offset in bytes from where the file should be sent.
   *
   * The %TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE feature has to be
   * prepared for this property to be meaningful and kept up to date.
   *
   * Since: 0.15.UNRELEASED
   */
  param_spec = g_param_spec_uint64 ("initial-offset",
      "InitialOffset",
      "The InitialOffset property of this channel",
      0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIAL_OFFSET,
      param_spec);

  g_type_class_add_private (object_class, sizeof
      (TpFileTransferChannelPrivate));
}

static void
tp_file_transfer_channel_init (TpFileTransferChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
      TP_TYPE_FILE_TRANSFER_CHANNEL, TpFileTransferChannelPrivate);
}

/**
 * TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core"
 * feature on a #TpFileTransferChannel.
 *
 * When this feature is prepared, the #TpFileTransferChannel:transferred-bytes
 * property has been retrieved and will be updated.
 *
 * One can ask for a feature to be prepared using the tp_proxy_prepare_async()
 * function, and waiting for it to trigger the callback.
 *
 * Since: 0.15.5
 */

GQuark
tp_file_transfer_channel_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-file-transfer-channel-feature-core");
}


/* Public methods */

/**
 * tp_file_transfer_channel_new:
 * @conn: a #TpConnection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @immutable_properties: (transfer none) (element-type utf8 GObject.Value):
 *  the immutable properties of the channel,
 *  as signalled by the NewChannel D-Bus signal or returned by the
 *  CreateChannel and EnsureChannel D-Bus methods: a mapping from
 *  strings (D-Bus interface name + "." + property name) to #GValue instances
 * @error: used to indicate the error if %NULL is returned
 *
 * Convenient function to create a new #TpFileTransferChannel
 *
 * Returns: (transfer full): a newly created #TpFileTransferChannel
 *
 * Since: 0.15.5
 */
TpFileTransferChannel *
tp_file_transfer_channel_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  return _tp_file_transfer_channel_new_with_factory (NULL, conn, object_path,
      immutable_properties, error);
}

TpFileTransferChannel *
_tp_file_transfer_channel_new_with_factory (
    TpSimpleClientFactory *factory,
    TpConnection *conn,
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

  return g_object_new (TP_TYPE_FILE_TRANSFER_CHANNEL,
      "connection", conn,
      "dbus-daemon", conn_proxy->dbus_daemon,
      "bus-name", conn_proxy->bus_name,
      "object-path", object_path,
      "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
      "channel-properties", immutable_properties,
      "factory", factory,
      NULL);
}

/**
 * tp_file_transfer_channel_accept_file_async:
 * @self: a #TpFileTransferChannel
 * @file: a #GFile
 * @offset: Offset from the start of @file where transfer begins
 * @callback: a callback to call when the transfer has been accepted
 * @user_data: data to pass to @callback
 *
 * Accept an offered file transfer. Once the accept has been processed,
 * @callback will be called. You can then call
 * tp_file_transfer_channel_accept_file_finish() to get the result of
 * the operation.
 *
 * Since: 0.15.UNRELEASED
 */
void
tp_file_transfer_channel_accept_file_async (TpFileTransferChannel *self,
    GFile *file,
    guint64 offset,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GHashTable *properties;
  GHashTable *supported_sockets;
  GError *error = NULL;

  g_return_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self));
  g_return_if_fail (G_IS_FILE (file));

  if (self->priv->access_control_param != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Can't accept already accepted transfer");

      return;
    }

  if (self->priv->state != TP_FILE_TRANSFER_STATE_PENDING)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Can't accept a transfer that isn't pending");

      return;
    }

  if (tp_channel_get_requested (TP_CHANNEL (self)))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
          user_data, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Can't accept outgoing transfer");

      return;
    }

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_file_transfer_channel_accept_file_async);

  properties = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));
  supported_sockets = tp_asv_get_boxed (properties,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_AVAILABLE_SOCKET_TYPES,
      TP_HASH_TYPE_SUPPORTED_SOCKET_MAP);

  if (!_tp_set_socket_address_type_and_access_control_type (supported_sockets,
      &self->priv->socket_type, &self->priv->access_control, &error))
    {
      operation_failed (self, error);

      g_clear_error (&error);
      return;
    }

  DEBUG ("Using socket type %u with access control %u",
      self->priv->socket_type, self->priv->access_control);

  self->priv->client_socket =
    _tp_create_client_socket (self->priv->socket_type, &error);
  if (self->priv->client_socket == NULL)
    {
      DEBUG ("Failed to create socket: %s", error->message);

      operation_failed (self, error);
      g_clear_error (&error);
      return;
    }

  switch (self->priv->access_control)
    {
      case TP_SOCKET_ACCESS_CONTROL_LOCALHOST:
        /* Dummy value */
        self->priv->access_control_param = tp_g_value_slice_new_uint (0);
        break;

      case TP_SOCKET_ACCESS_CONTROL_PORT:
        {
          GSocketAddress *addr;
          guint16 port;

          addr = g_socket_get_local_address (self->priv->client_socket,
              &error);
          if (addr == NULL)
            {
              DEBUG ("Failed to get address of local socket: %s",
                  error->message);

              operation_failed (self, error);
              g_error_free (error);
              return;
            }

          port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS
              (addr));
          self->priv->access_control_param = tp_g_value_slice_new_uint (port);

          g_object_unref (addr);
        }
        break;

      case TP_SOCKET_ACCESS_CONTROL_CREDENTIALS:
        self->priv->access_control_param = tp_g_value_slice_new_byte (
            g_random_int_range (0, G_MAXUINT8));
        break;

      default:
        g_assert_not_reached ();
    }

  /* FIXME: What if an approver already set a file? */
  g_clear_object (&self->priv->file);
  self->priv->file = g_object_ref (file);

  /* Call accept */
  tp_cli_channel_type_file_transfer_call_accept_file (TP_CHANNEL (self), -1,
      self->priv->socket_type,
      self->priv->access_control,
      self->priv->access_control_param,
      self->priv->initial_offset,
      accept_file_cb,
      NULL,
      NULL,
      G_OBJECT (self));
}

/**
 * tp_file_transfer_channel_accept_file_finish:
 * @self: a #TpFileTransferChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes a file transfer accept.
 *
 * Returns: %TRUE if the accept operation was a success, or %FALSE
 *
 * Since: 0.15.UNRELEASED
 */
gboolean
tp_file_transfer_channel_accept_file_finish (TpFileTransferChannel *self,
    GAsyncResult *result,
    GError **error)
{
  DEBUG ("enter");
  _tp_implement_finish_void (self, tp_file_transfer_channel_accept_file_async)
}

/**
 * tp_file_transfer_channel_offer_file_async:
 * @self: a #TpFileTransferChannel
 * @file: a #GFile
 * @callback: a callback to call when the transfer has been accepted
 * @user_data: data to pass to @callback
 *
 * Offer a file transfer. Once the offer has been sent, @callback will be
 * called. You can then call tp_file_transfer_channel_offer_file_finish()
 * to get the result of the operation.
 *
 * Since: 0.15.UNRELEASED
 */
void
tp_file_transfer_channel_offer_file_async (TpFileTransferChannel *self,
    GFile *file,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GHashTable *properties;
  GHashTable *supported_sockets;
  GError *error = NULL;

  DEBUG ("enter");

  g_return_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (tp_channel_get_requested (TP_CHANNEL (self)));

  self->priv->file = g_object_ref (file);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_file_transfer_channel_offer_file_async);

  properties = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));
  supported_sockets = tp_asv_get_boxed (properties,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_AVAILABLE_SOCKET_TYPES,
      TP_HASH_TYPE_SUPPORTED_SOCKET_MAP);

  if (!_tp_set_socket_address_type_and_access_control_type (supported_sockets,
        &self->priv->socket_type, &self->priv->access_control, &error))
      {
        operation_failed (self, error);

        g_clear_error (&error);
        return;
      }

  DEBUG ("Using socket type %u with access control %u",
      self->priv->socket_type, self->priv->access_control);

  self->priv->service = g_socket_service_new ();

  switch (self->priv->socket_type)
    {
#ifdef HAVE_GIO_UNIX
    case TP_SOCKET_ADDRESS_TYPE_UNIX:
    case TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX:
        {
          self->priv->address = _tp_create_temp_unix_socket (
              self->priv->service, &error);

          if (self->priv->address == NULL)
            {
              operation_failed (self, error);

              g_clear_error (&error);
              return;
            }

        break;
        }
#endif /* HAVE_GIO_UNIX */

    case TP_SOCKET_ADDRESS_TYPE_IPV6:
    case TP_SOCKET_ADDRESS_TYPE_IPV4:
        {
          GInetAddress *localhost;
          GSocketAddress *inet_address;

          localhost = g_inet_address_new_loopback (
              self->priv->socket_type == TP_SOCKET_ADDRESS_TYPE_IPV4 ?
              G_SOCKET_FAMILY_IPV4 : G_SOCKET_FAMILY_IPV6);

          inet_address = g_inet_socket_address_new (localhost, 0);

          g_socket_listener_add_address (
              G_SOCKET_LISTENER (self->priv->service), inet_address,
              G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
              NULL, &self->priv->address, &error);

          g_object_unref (localhost);
          g_object_unref (inet_address);

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

  DEBUG ("Calling ProvideFile");

  /* Call provide */
  tp_cli_channel_type_file_transfer_call_provide_file (TP_CHANNEL (self), -1,
      self->priv->socket_type,
      self->priv->access_control,
      self->priv->access_control_param,
      provide_file_cb,
      NULL, NULL, NULL);
}

/**
 * tp_file_transfer_channel_offer_file_finish:
 * @self: a #TpFileTransferChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes a file transfer offer.
 *
 * Returns: %TRUE if the file has been successfully offered, or
 * %FALSE. This does not mean that the file transfer has completed or
 * has even started at all.
 *
 * Since: 0.15.UNRELEASED
 */
gboolean
tp_file_transfer_channel_offer_file_finish (TpFileTransferChannel *self,
    GAsyncResult *result,
    GError **error)
{
  DEBUG ("enter");
  _tp_implement_finish_void (self, tp_file_transfer_channel_offer_file_async)
}


/* Property accessors */

/**
 * tp_file_transfer_channel_get_mime_type:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:mime-type property
 *
 * Returns: (transfer none): the value of the
 *   #TpFileTransferChannel:mime-type property
 *
 * Since: 0.15.5
 */
const char *
tp_file_transfer_channel_get_mime_type (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), NULL);

  return self->priv->mime_type;
}

/**
 * tp_file_transfer_channel_get_date:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:date property
 *
 * Returns: (transfer none): the value of the #TpFileTransferChannel:date
 *   property
 *
 * Since: 0.15.5
 */
GDateTime *
tp_file_transfer_channel_get_date (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), NULL);

  return self->priv->date;
}

/**
 * tp_file_transfer_channel_get_description:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:description property
 *
 * Returns: (transfer none): the value of the
 *   #TpFileTransferChannel:description property
 *
 * Since: 0.15.5
 */
const gchar *
tp_file_transfer_channel_get_description (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), NULL);

  return self->priv->description;
}

/**
 * tp_file_transfer_channel_get_filename:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:filename property
 *
 * Returns: (transfer none): the value of the
 *   #TpFileTransferChannel:filename property
 *
 * Since: 0.15.5
 */
const gchar *
tp_file_transfer_channel_get_filename (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), NULL);

  return self->priv->filename;
}

/**
 * tp_file_transfer_channel_get_size:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:size property
 *
 * Returns: the value of the #TpFileTransferChannel:size property
 *
 * Since: 0.15.5
 */
guint64
tp_file_transfer_channel_get_size (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), 0);

  return self->priv->size;
}

/**
 * tp_file_transfer_channel_get_state:
 * @self: a #TpFileTransferChannel
 * @reason: (out): a #TpFileTransferStateChangeReason, or %NULL
 *
 * If @reason is not %NULL it is set to the reason why
 * #TpFileTransferChannel:state changed to its current value.
 *
 * Returns: the value of the #TpFileTransferState:state property
 *
 * Since: 0.15.UNRELEASED
 */
TpFileTransferState
tp_file_transfer_channel_get_state (TpFileTransferChannel *self,
    TpFileTransferStateChangeReason *reason)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self),
      TP_FILE_TRANSFER_STATE_NONE);

  if (reason != NULL)
    *reason = self->priv->state_reason;

  return self->priv->state;
}

/**
 * tp_file_transfer_channel_get_transferred_bytes:
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:transferred-bytes property
 *
 * Returns: the value of the #TpFileTransferChannel:transferred-bytes property
 *
 * Since: 0.15.5
 */
guint64
tp_file_transfer_channel_get_transferred_bytes (TpFileTransferChannel *self)
{
  g_return_val_if_fail (TP_IS_FILE_TRANSFER_CHANNEL (self), 0);

  return self->priv->transferred_bytes;
}
