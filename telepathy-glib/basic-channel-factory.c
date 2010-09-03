/*
 * Simple client channel factory creating TpChannel
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
 * SECTION:basic-channel-factory
 * @title: TpBasicChannelFactory
 * @short_description: channel factory creating TpChannel objects
 *
 * This factory implements the #TpClientChannelFactory interface to create
 * #TpChannel objects.
 *
 */

/**
 * TpBasicChannelFactory:
 *
 * Data structure representing a #TpBasicChannelFactory
 *
 * Since: 0.13.UNRELEASED
 */

/**
 * TpBasicChannelFactoryClass:
 * @parent_class: the parent class
 *
 * The class of a #TpBasicChannelFactory.
 *
 * Since: 0.13.UNRELEASED
 */

#include "telepathy-glib/basic-channel-factory.h"

#include <telepathy-glib/client-channel-factory.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

static void client_channel_factory_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(TpBasicChannelFactory, tp_basic_channel_factory, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CLIENT_CHANNEL_FACTORY,
      client_channel_factory_iface_init))

static void
tp_basic_channel_factory_init (TpBasicChannelFactory *self)
{
}

static void
tp_basic_channel_factory_class_init (TpBasicChannelFactoryClass *cls)
{
}

static void
client_channel_factory_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
  TpClientChannelFactoryInterface *iface = g_iface;

  /* We rely on the default implementation of create_channel */
  iface->create_channel = NULL;
}

/**
 * tp_basic_channel_factory_new:
 *
 * Convenient function to create a new #TpBasicChannelFactory instance.
 *
 * Returns: a new #TpBasicChannelFactory
 *
 * Since: 0.13.UNRELEASED
 */
TpBasicChannelFactory *
tp_basic_channel_factory_new (void)
{
  return g_object_new (TP_TYPE_BASIC_CHANNEL_FACTORY,
      NULL);
}
