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

    /* Hidden properties */
    /* borrowed from the immutable properties GHashTable */
    GHashTable *available_socket_types;
    const gchar *content_hash;
    TpFileHashType content_hash_type;
    goffset initial_offset;
    TpFileTransferState state;
    TpFileTransferStateChangeReason state_reason;
};

enum /* properties */
{
  PROP_MIME_TYPE = 1,
  PROP_DATE,
  PROP_DESCRIPTION,
  PROP_FILENAME,
  PROP_SIZE,
  PROP_TRANSFERRED_BYTES,
  PROP_FILE,
  N_PROPS
};

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
}

static void
tp_file_transfer_channel_initial_offset_defined_cb (TpChannel *proxy,
    guint64 initial_offset,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;

  self->priv->initial_offset = initial_offset;
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
tp_file_transfer_channel_prepare_core_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpFileTransferChannel *self = (TpFileTransferChannel *) proxy;
  GSimpleAsyncResult *result = user_data;
  gboolean valid;

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

out:
  g_simple_async_result_complete (result);
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

      case PROP_TRANSFERRED_BYTES:
        g_value_set_uint64 (value, self->priv->transferred_bytes);
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
   * TpFileTransferChannel:transferred-bytes
   *
   * The number of bytes transferred so far in this
   * file transfer.
   *
   * The %TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE feature has to be prepared for
   * this property to be meaningful and kept up to date.
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

/* Property accessors */

/**
 * tp_file_transfer_channel_get_mime_type
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:mime-type property
 *
 * Returns: (transfer none): the value of the
 * #TpFileTransferChannel:mime-type property
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
 * tp_file_transfer_channel_get_date
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:date property
 *
 * Returns: (transfer none): the value of the #TpFileTransferChannel:date
 * property
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
 * tp_file_transfer_channel_get_description
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:description property
 *
 * Returns: (transfer none): the value of the #TpFileTransferChannel:description
 * property
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
 * tp_file_transfer_channel_get_filename
 * @self: a #TpFileTransferChannel
 *
 * Return the #TpFileTransferChannel:filename property
 *
 * Returns: (transfer none): the value of the #TpFileTransferChannel:filename
 * property
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
 * tp_file_transfer_channel_get_size
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
 * tp_file_transfer_channel_get_transferred_bytes
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
