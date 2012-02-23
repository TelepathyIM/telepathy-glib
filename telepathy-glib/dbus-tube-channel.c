/*
 * dbus-tube-channel.h - high level API for DBusTube channels
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:dbus-tube-channel
 * @title: TpDBusTubeChannel
 * @short_description: proxy object for a dbus tube channel
 *
 * #TpDBusTubeChannel is a sub-class of #TpChannel providing convenient API
 * to offer and accept a dbus tube.
 *
 * Since: 0.15.6
 */

/**
 * TpDBusTubeChannel:
 *
 * Data structure representing a #TpDBusTubeChannel.
 *
 * Since: 0.15.6
 */

/**
 * TpDBusTubeChannelClass:
 *
 * The class of a #TpDBusTubeChannel.
 *
 * Since: 0.15.6
 */

#include "config.h"

#include "telepathy-glib/dbus-tube-channel.h"

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
#include "telepathy-glib/automatic-client-factory-internal.h"
#include "telepathy-glib/debug-internal.h"

#include <stdio.h>
#include <glib/gstdio.h>

G_DEFINE_TYPE (TpDBusTubeChannel, tp_dbus_tube_channel, TP_TYPE_CHANNEL)

struct _TpDBusTubeChannelPrivate
{
  GHashTable *parameters;
};

enum
{
  PROP_SERVICE_NAME = 1,
  PROP_PARAMETERS
};

static void
tp_dbus_tube_channel_dispose (GObject *obj)
{
  TpDBusTubeChannel *self = (TpDBusTubeChannel *) obj;

  tp_clear_pointer (&self->priv->parameters, g_hash_table_unref);

  G_OBJECT_CLASS (tp_dbus_tube_channel_parent_class)->dispose (obj);
}

static void
tp_dbus_tube_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpDBusTubeChannel *self = (TpDBusTubeChannel *) object;

  switch (property_id)
    {
      case PROP_SERVICE_NAME:
        g_value_set_string (value,
            tp_dbus_tube_channel_get_service_name (self));
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
tp_dbus_tube_channel_constructed (GObject *obj)
{
  TpDBusTubeChannel *self = (TpDBusTubeChannel *) obj;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_dbus_tube_channel_parent_class)->constructed;
  TpChannel *chan = (TpChannel *) obj;
  GHashTable *props;

  if (chain_up != NULL)
    chain_up (obj);

  if (tp_channel_get_channel_type_id (chan) !=
      TP_IFACE_QUARK_CHANNEL_TYPE_DBUS_TUBE)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel is not a D-Bus tube" };

      DEBUG ("Channel is not a D-Bus tube: %s", tp_channel_get_channel_type (
            chan));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  if (tp_asv_get_string (props, TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME)
      == NULL)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Tube doesn't have DBusTube.ServiceName property" };

      DEBUG ("%s", error.message);

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

   /*  Tube.Parameters is immutable for incoming tubes. For outgoing ones,
    *  it's defined when offering the tube. */
  if (!tp_channel_get_requested (TP_CHANNEL (self)))
    {
      GHashTable *params;

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
tp_dbus_tube_channel_class_init (TpDBusTubeChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->constructed = tp_dbus_tube_channel_constructed;
  gobject_class->get_property = tp_dbus_tube_channel_get_property;
  gobject_class->dispose = tp_dbus_tube_channel_dispose;

  /**
   * TpDBusTubeChannel:service-name:
   *
   * A string representing the service name that will be used over the tube.
   *
   * Since: 0.15.6
   */
  param_spec = g_param_spec_string ("service-name", "Service Name",
      "The service name of the dbus tube",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SERVICE_NAME,
      param_spec);

  /**
   * TpDBusTubeChannel:parameters:
   *
   * A string to #GValue #GHashTable representing the parameters of the tube.
   *
   * Will be %NULL for outgoing tubes until the tube has been offered.
   *
   * Since: 0.15.6
   */
  param_spec = g_param_spec_boxed ("parameters", "Parameters",
      "The parameters of the dbus tube",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_PARAMETERS, param_spec);

  g_type_class_add_private (gobject_class, sizeof (TpDBusTubeChannelPrivate));
}

static void
tp_dbus_tube_channel_init (TpDBusTubeChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_DBUS_TUBE_CHANNEL,
      TpDBusTubeChannelPrivate);
}

TpDBusTubeChannel *
_tp_dbus_tube_channel_new_with_factory (
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

  return g_object_new (TP_TYPE_DBUS_TUBE_CHANNEL,
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
 * tp_dbus_tube_channel_get_service_name: (skip)
 * @self: a #TpDBusTubeChannel
 *
 * Return the #TpDBusTubeChannel:service-name property
 *
 * Returns: (transfer none): the value of #TpDBusTubeChannel:service-name
 *
 * Since: 0.15.6
 */
const gchar *
tp_dbus_tube_channel_get_service_name (TpDBusTubeChannel *self)
{
  GHashTable *props;

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  return tp_asv_get_string (props, TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME);
}

/**
 * tp_dbus_tube_channel_get_parameters: (skip)
 * @self: a #TpDBusTubeChannel
 *
 * Return the #TpDBusTubeChannel:parameters property
 *
 * Returns: (transfer none) (element-type utf8 GObject.Value):
 * the value of #TpDBusTubeChannel:parameters
 *
 * Since: 0.15.6
 */
GHashTable *
tp_dbus_tube_channel_get_parameters (TpDBusTubeChannel *self)
{
  return self->priv->parameters;
}
