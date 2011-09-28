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
  TpTubeChannelState state;
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
tube_state_changed_cb (TpChannel *channel,
    TpTubeChannelState state,
    gpointer user_data,
    GObject *weak_object)
{
  TpDBusTubeChannel *self = (TpDBusTubeChannel *) channel;

  self->priv->state = state;
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
get_state_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpDBusTubeChannel *self = (TpDBusTubeChannel *) proxy;
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to get Tube.State property: %s", error->message);

      g_simple_async_result_set_error (result, error->domain, error->code,
          "Failed to get Tube.State property: %s", error->message);
    }
  else
    {
      self->priv->state = g_value_get_uint (value);
    }

  g_simple_async_result_complete (result);
}

static void
tp_dbus_tube_channel_prepare_core_feature_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GError *error = NULL;
  TpChannel *chan = (TpChannel *) proxy;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_dbus_tube_channel_prepare_core_feature_async);

  if (tp_cli_channel_interface_tube_connect_to_tube_channel_state_changed (chan,
        tube_state_changed_cb, proxy, NULL, NULL, &error) == NULL)
    {
      WARNING ("Failed to connect to TubeChannelStateChanged on %s: %s",
          tp_proxy_get_object_path (proxy), error->message);
      g_error_free (error);
    }

  tp_cli_dbus_properties_call_get (proxy, -1,
      TP_IFACE_CHANNEL_INTERFACE_TUBE, "State",
      get_state_cb, result, g_object_unref, G_OBJECT (proxy));
}

enum {
    FEAT_CORE,
    N_FEAT
};

static const TpProxyFeature *
tp_dbus_tube_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name =
    TP_DBUS_TUBE_CHANNEL_FEATURE_CORE;
  features[FEAT_CORE].prepare_async =
    tp_dbus_tube_channel_prepare_core_feature_async;
  features[FEAT_CORE].core = TRUE;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_dbus_tube_channel_class_init (TpDBusTubeChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  gobject_class->constructed = tp_dbus_tube_channel_constructed;
  gobject_class->get_property = tp_dbus_tube_channel_get_property;
  gobject_class->dispose = tp_dbus_tube_channel_dispose;

  proxy_class->list_features = tp_dbus_tube_channel_list_features;

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

/**
 * TP_DBUS_TUBE_CHANNEL_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark representing the
 * core feature of a #TpDBusTubeChannel.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.UNRELEASED
 */
GQuark
tp_dbus_tube_channel_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-dbus-tube-channel-feature-core");
}
