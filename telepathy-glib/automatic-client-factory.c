/*
 * Factory for specialized TpChannel subclasses.
 *
 * Copyright © 2011 Collabora Ltd.
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
 * SECTION:automatic-client-factory
 * @title: TpAutomaticClientFactory
 * @short_description: Factory for specialized #TpChannel subclasses.
 * @see_also: #TpClientFactory
 *
 * This factory overrides some #TpClientFactory virtual methods to
 * create specialized #TpChannel subclasses.
 *
 * #TpAutomaticClientFactory will currently create #TpChannel objects
 * as follows:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>a #TpStreamTubeChannel, if the channel is of type
 *     %TP_IFACE_CHANNEL_TYPE_STREAM_TUBE;</para>
 *   </listitem>
 *   <listitem>
 *     <para>a #TpDBusTubeChannel, if the channel is of type
 *     %TP_IFACE_CHANNEL_TYPE_DBUS_TUBE;</para>
 *   </listitem>
 *   <listitem>
 *     <para>a #TpTextChannel, if the channel is of type
 *     %TP_IFACE_CHANNEL_TYPE_TEXT and implements
 *     %TP_IFACE_CHANNEL_INTERFACE_MESSAGES;</para>
 *   </listitem>
 *   <listitem>
 *     <para>a #TpFileTransferChannel, if the channel is of type
 *     %TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER;</para>
 *   </listitem>
 *   <listitem>
 *     <para>a #TpCallChannel, if the channel is of type
 *     %TP_IFACE_CHANNEL_TYPE_CALL;</para>
 *   </listitem>
 *   <listitem>
 *     <para>a plain #TpChannel, otherwise</para>
 *   </listitem>
 * </itemizedlist>
 *
 * It is guaranteed that the objects returned by future versions
 * will be either the class that is currently used, or a more specific
 * subclass of that class.
 *
 * This factory asks to prepare the following features:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>%TP_CHANNEL_FEATURE_CORE
 *     for all channel types.</para>
 *   </listitem>
 *   <listitem>
 *     <para>%TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE
 *     for #TpFileTransferChannel</para>
 *   </listitem>
 *   <listitem>
 *     <para>%TP_CALL_CHANNEL_FEATURE_CORE
 *     for #TpCallChannel</para>
 *   </listitem>
 *   <listitem>
 *     <para>%TP_DBUS_TUBE_CHANNEL_FEATURE_CORE
 *     for #TpDBusTubeChannel</para>
 *   </listitem>
 * </itemizedlist>
 *
 * Since: 0.99.1
 */

/**
 * TpAutomaticClientFactory:
 *
 * Data structure representing a #TpAutomaticClientFactory
 *
 * Since: 0.15.5
 */

/**
 * TpAutomaticClientFactoryClass:
 * @parent_class: the parent class
 *
 * The class of a #TpAutomaticClientFactory.
 *
 * Since: 0.15.5
 */

#include "config.h"

#include "telepathy-glib/automatic-client-factory.h"

#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/automatic-client-factory-internal.h"
#include "telepathy-glib/variant-util.h"

G_DEFINE_TYPE (TpAutomaticClientFactory, tp_automatic_client_factory,
    TP_TYPE_CLIENT_FACTORY)

#define chainup ((TpClientFactoryClass *) \
    tp_automatic_client_factory_parent_class)

typedef gboolean (*CheckPropertiesFunc) (
    const gchar *object_path,
    GVariant *properties);

typedef TpChannel *(*NewFunc) (
    TpClientFactory *client,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *properties,
    GError **error);

typedef struct {
    const gchar *channel_type;
    GType gtype;
    CheckPropertiesFunc check_properties;
    NewFunc new_func;
    /* 0-terminated. All of a sudden, 3 is not such a scary number. */
    GQuark features[3];
} ChannelTypeMapping;

static ChannelTypeMapping *channel_type_mapping = NULL;

static void
build_channel_type_mapping (void)
{
  ChannelTypeMapping i_hate_c[] = {
      { TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1,
        TP_TYPE_STREAM_TUBE_CHANNEL,
        NULL,
        (NewFunc) _tp_stream_tube_channel_new,
        { 0 },
      },
      { TP_IFACE_CHANNEL_TYPE_DBUS_TUBE1,
        TP_TYPE_DBUS_TUBE_CHANNEL,
        NULL,
        (NewFunc) _tp_dbus_tube_channel_new,
        { TP_DBUS_TUBE_CHANNEL_FEATURE_CORE,
          0 },
      },
      { TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_TYPE_TEXT_CHANNEL,
        NULL,
        (NewFunc) _tp_text_channel_new,
        { 0 },
      },
      { TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1,
        TP_TYPE_FILE_TRANSFER_CHANNEL,
        NULL,
        (NewFunc) _tp_file_transfer_channel_new,
        { TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE,
          0 },
      },
      { TP_IFACE_CHANNEL_TYPE_CALL1,
        TP_TYPE_CALL_CHANNEL,
        NULL,
        (NewFunc) _tp_call_channel_new,
        { TP_CALL_CHANNEL_FEATURE_CORE,
          0 },
      },
      { NULL }
  };

  g_return_if_fail (channel_type_mapping == NULL);

  channel_type_mapping = g_memdup (i_hate_c, sizeof i_hate_c);
}

static TpChannel *
create_channel_impl (TpClientFactory *self,
    TpConnection *conn,
    const gchar *object_path,
    GVariant *properties,
    GError **error)
{
  const gchar *chan_type = NULL;
  ChannelTypeMapping *m;
  TpChannel *channel = NULL;

  g_variant_lookup (properties, TP_PROP_CHANNEL_CHANNEL_TYPE, "&s", &chan_type);

  for (m = channel_type_mapping; m->channel_type != NULL; m++)
    {
      GHashTable *asv;

      if (tp_strdiff (chan_type, m->channel_type))
        continue;

      if (m->check_properties != NULL &&
          !m->check_properties (object_path, properties))
        break;

      asv = tp_asv_from_vardict (properties);
      channel = m->new_func (self, conn, object_path, asv, error);
      g_hash_table_unref (asv);
      break;
    }

  if (channel == NULL)
    {
      /* Chainup on parent implementation as fallback */
      channel = chainup->create_channel (self, conn, object_path, properties,
          error);
    }

  return channel;
}

static GArray *
dup_channel_features_impl (TpClientFactory *self,
    TpChannel *channel)
{
  GArray *features;
  ChannelTypeMapping *m;

  /* Chainup to get desired features for all channel types */
  features = chainup->dup_channel_features (self, channel);

  for (m = channel_type_mapping; m->channel_type != NULL; m++)
    {
      if (G_TYPE_CHECK_INSTANCE_TYPE (channel, m->gtype))
        {
          guint j;
          for (j = 0; m->features[j] != 0; j++)
            g_array_append_val (features, m->features[j]);
          break;
        }
    }

  return features;
}

static void
tp_automatic_client_factory_init (TpAutomaticClientFactory *self)
{
}

static void
tp_automatic_client_factory_class_init (TpAutomaticClientFactoryClass *klass)
{
  TpClientFactoryClass *base_class = (TpClientFactoryClass *) klass;

  base_class->create_channel = create_channel_impl;
  base_class->dup_channel_features = dup_channel_features_impl;

  build_channel_type_mapping ();
}

/**
 * tp_automatic_client_factory_new:
 * @dbus: (allow-none): a #TpDBusDaemon, or %NULL
 *
 * Returns a new #TpAutomaticClientFactory instance. If @dbus is %NULL then
 * tp_dbus_daemon_dup() will be used.
 *
 * Returns: a new #TpAutomaticClientFactory
 *
 * Since: 0.99.1
 */
TpClientFactory *
tp_automatic_client_factory_new (TpDBusDaemon *dbus)
{
  g_return_val_if_fail (dbus == NULL || TP_IS_DBUS_DAEMON (dbus), NULL);

  return g_object_new (TP_TYPE_AUTOMATIC_CLIENT_FACTORY,
      "dbus-daemon", dbus,
      NULL);
}
