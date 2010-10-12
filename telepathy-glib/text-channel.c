/*
 * text-channel.h - high level API for Text channels
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

/**
 * SECTION:text-channel
 * @title: TpTextChannel
 * @short_description: proxy object for a text channel
 *
 * #TpTextChannel is a sub-class of #TpChannel providing convenient API
 * to send and receive #TpMessage.
 */

/**
 * TpTextChannel:
 *
 * Data structure representing a #TpTextChannel.
 *
 * Since: 0.13.UNRELEASED
 */

/**
 * TpTextChannelClass:
 *
 * The class of a #TpTextChannel.
 *
 * Since: 0.13.UNRELEASED
 */

#include <config.h>

#include "telepathy-glib/text-channel.h"

#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#include "_gen/signals-marshal.h"

#include <stdio.h>
#include <glib/gstdio.h>

G_DEFINE_TYPE (TpTextChannel, tp_text_channel, TP_TYPE_CHANNEL)

struct _TpTextChannelPrivate
{
  GStrv supported_content_types;
  TpMessagePartSupportFlags message_part_support_flags;
  TpDeliveryReportingSupportFlags delivery_reporting_support;
};

enum
{
  PROP_SUPPORTED_CONTENT_TYPES = 1,
  PROP_MESSAGE_PART_SUPPORT_FLAGS,
  PROP_DELIVERY_REPORTING_SUPPORT
};

#if 0
enum /* signals */
{
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };
#endif

static void
tp_text_channel_dispose (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;

  tp_clear_pointer (&self->priv->supported_content_types, g_strfreev);

  G_OBJECT_CLASS (tp_text_channel_parent_class)->dispose (obj);
}

static void
tp_text_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTextChannel *self = (TpTextChannel *) object;

  switch (property_id)
    {
      case PROP_SUPPORTED_CONTENT_TYPES:
        g_value_set_boxed (value,
            tp_text_channel_get_supported_content_types (self));
        break;

      case PROP_MESSAGE_PART_SUPPORT_FLAGS:
        g_value_set_uint (value,
            tp_text_channel_get_message_part_support_flags (self));
        break;

      case PROP_DELIVERY_REPORTING_SUPPORT:
        g_value_set_uint (value,
            tp_text_channel_get_delivery_reporting_support (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_text_channel_constructed (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_text_channel_parent_class)->constructed;
  TpChannel *chan = (TpChannel *) obj;
  GHashTable *props;
  gboolean valid;

  if (chain_up != NULL)
    chain_up (obj);

  if (tp_channel_get_channel_type_id (chan) !=
      TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel is not of type Text" };

      DEBUG ("Channel is not of type Text: %s", tp_channel_get_channel_type (
            chan));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

  if (!tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES))
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel does not implement the Messages interface" };

      DEBUG ("Channel does not implement the Messages interface");

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;

    }

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  self->priv->supported_content_types = (GStrv) tp_asv_get_strv (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_SUPPORTED_CONTENT_TYPES);
  if (self->priv->supported_content_types == NULL)
    {
      DEBUG ("Channel doesn't have Messages.SupportedContentTypes in its "
          "immutable properties");
    }
  else
    {
      self->priv->supported_content_types = g_strdupv (
          self->priv->supported_content_types);
    }

  self->priv->message_part_support_flags = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_MESSAGE_PART_SUPPORT_FLAGS, &valid);
  if (!valid)
    {
      DEBUG ("Channel doesn't have Messages.MessagePartSupportFlags in its "
          "immutable properties");
    }

  self->priv->delivery_reporting_support = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_DELIVERY_REPORTING_SUPPORT, &valid);
  if (!valid)
    {
      DEBUG ("Channel doesn't have Messages.DeliveryReportingSupport in its "
          "immutable properties");
    }
}

static void
tp_text_channel_class_init (TpTextChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->constructed = tp_text_channel_constructed;
  gobject_class->get_property = tp_text_channel_get_property;
  gobject_class->dispose = tp_text_channel_dispose;

  /**
   * TpTextChannel:supported-content-types:
   *
   * A #GStrv containing the MIME types supported by this channel, with more
   * preferred MIME types appearing earlier in the array.
   *
   * Since: 0.13.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("supported-content-types",
      "SupportedContentTypes",
      "The Messages.SupportedContentTypes property of the channel",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SUPPORTED_CONTENT_TYPES,
      param_spec);

  /**
   * TpTextChannel:message-part-support-flags:
   *
   * A #TpMessagePartSupportFlags indicating the level of support for
   * message parts on this channel.
   *
   * Since: 0.13.UNRELEASED
   */
  param_spec = g_param_spec_uint ("message-part-support-flags",
      "MessagePartSupportFlags",
      "The Messages.MessagePartSupportFlags property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MESSAGE_PART_SUPPORT_FLAGS, param_spec);

  /**
   * TpTextChannel:delivery-reporting-support:
   *
   * A #TpDeliveryReportingSupportFlags indicating features supported
   * by this channel.
   *
   * Since: 0.13.UNRELEASED
   */
  param_spec = g_param_spec_uint ("delivery-reporting-support",
      "DeliveryReportingSupport",
      "The Messages.DeliveryReportingSupport property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_DELIVERY_REPORTING_SUPPORT, param_spec);

  g_type_class_add_private (gobject_class, sizeof (TpTextChannelPrivate));
}

static void
tp_text_channel_init (TpTextChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_TEXT_CHANNEL,
      TpTextChannelPrivate);
}


/**
 * tp_text_channel_new:
 * @conn: a #TpConnection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @immutable_properties: (transfer none) (element-type utf8 GObject.Value):
 *  the immutable properties of the channel,
 *  as signalled by the NewChannel D-Bus signal or returned by the
 *  CreateChannel and EnsureChannel D-Bus methods: a mapping from
 *  strings (D-Bus interface name + "." + property name) to #GValue instances
 * @error: used to indicate the error if %NULL is returned
 *
 * Convenient function to create a new #TpTextChannel
 *
 * Returns: (transfer full): a newly created #TpTextChannel
 *
 * Since: 0.13.UNRELEASED
 */
TpTextChannel *
tp_text_channel_new (TpConnection *conn,
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

  return g_object_new (TP_TYPE_TEXT_CHANNEL,
      "connection", conn,
       "dbus-daemon", conn_proxy->dbus_daemon,
       "bus-name", conn_proxy->bus_name,
       "object-path", object_path,
       "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
       "channel-properties", immutable_properties,
       NULL);
}

/**
 * tp_text_channel_get_supported_content_types: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:supported-content-types property
 *
 * Returns: (transfer none) :
 * the value of #TpTextChannel:supported-content-types
 *
 * Since: 0.13.UNRELEASED
 */
GStrv
tp_text_channel_get_supported_content_types (TpTextChannel *self)
{
  return self->priv->supported_content_types;
}

/**
 * tp_text_channel_get_message_part_support_flags: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:message-part-support-flags property
 *
 * Returns: the value of #TpTextChannel:message-part-support-flags
 *
 * Since: 0.13.UNRELEASED
 */
TpMessagePartSupportFlags
tp_text_channel_get_message_part_support_flags (
    TpTextChannel *self)
{
  return self->priv->message_part_support_flags;
}

/**
 * tp_text_channel_get_delivery_reporting_support: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:delivery-reporting-support property
 *
 * Returns: the value of #TpTextChannel:delivery-reporting-support property
 *
 * Since: 0.13.UNRELEASED
 */
TpDeliveryReportingSupportFlags
tp_text_channel_get_delivery_reporting_support (
    TpTextChannel *self)
{
  return self->priv->delivery_reporting_support;
}
