/*
 * Factory creating higher level proxy objects
 *
 * Copyright © 2010 Collabora Ltd.
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
 * SECTION:automatic-proxy-factory
 * @title: TpAutomaticProxyFactory
 * @short_description: factory creating higher level proxy objects
 *
 * This factory implements the #TpClientChannelFactoryInterface interface to
 * create specialized #TpChannel subclasses.
 *
 * The current version of #TpAutomaticProxyFactory guarantees to create the
 * following objects:
 *  - if channel is of type TP_IFACE_CHANNEL_TYPE_STREAM_TUBE, a
 *  #TpStreamTubeChannel
 *  - for all the other channel types, a #TpChannel
 */

/**
 * TpAutomaticProxyFactory:
 *
 * Data structure representing a #TpAutomaticProxyFactory
 *
 * Since: 0.13.UNRELEASED
 */

/**
 * TpAutomaticProxyFactoryClass:
 * @parent_class: the parent class
 *
 * The class of a #TpAutomaticProxyFactory.
 *
 * Since: 0.13.UNRELEASED
 */

#include "telepathy-glib/automatic-proxy-factory.h"

#include <telepathy-glib/client-channel-factory.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/stream-tube-channel.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

static void client_proxy_factory_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(TpAutomaticProxyFactory,
    tp_automatic_proxy_factory, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CLIENT_CHANNEL_FACTORY,
      client_proxy_factory_iface_init))

static void
tp_automatic_proxy_factory_init (TpAutomaticProxyFactory *self)
{
}

static void
tp_automatic_proxy_factory_class_init (TpAutomaticProxyFactoryClass *cls)
{
}

static TpChannel *
tp_automatic_proxy_factory_create_channel (
    TpClientChannelFactoryInterface *factory,
    TpConnection *conn,
    const gchar *path,
    GHashTable *properties,
    GError **error)
{
  const gchar *chan_type;

  chan_type = tp_asv_get_string (properties, TP_PROP_CHANNEL_CHANNEL_TYPE);

  if (!tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_STREAM_TUBE))
    return TP_CHANNEL (tp_stream_tube_channel_new (conn, path, properties,
          error));

  return tp_channel_new_from_properties (conn, path, properties, error);
}

static void
client_proxy_factory_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
  TpClientChannelFactoryInterface *iface = g_iface;

  iface->create_channel = tp_automatic_proxy_factory_create_channel;
}

/**
 * tp_automatic_proxy_factory_new:
 *
 * Convenient function to create a new #TpAutomaticProxyFactory instance.
 *
 * Returns: a new #TpAutomaticProxyFactory
 *
 * Since: 0.13.UNRELEASED
 */
TpAutomaticProxyFactory *
tp_automatic_proxy_factory_new (void)
{
  return g_object_new (TP_TYPE_AUTOMATIC_PROXY_FACTORY,
      NULL);
}
