/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "observer-internal.h"

#include <glib.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-client.h>
#include <telepathy-glib/account-manager.h>

#include <telepathy-logger/channel-internal.h>
#include <telepathy-logger/channel-factory-internal.h>
#include <telepathy-logger/log-manager.h>

#define DEBUG_FLAG TPL_DEBUG_OBSERVER
#include <telepathy-logger/action-chain-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:observer
 * @title: TplObserver
 * @short_description: TPL Observer main class, used to handle received
 * signals
 * @see_also: #TpSvcClientObserver
 *
 * The Telepathy Logger's Observer implements
 * org.freedesktop.Telepathy.Client.Observer DBus interface and is called by
 * the Channel Dispatcher when a new channel is created, in order to log
 * received signals.
 *
 * Since: 0.1
 */

/**
 * TplObserver:
 *
 * The Telepathy Logger's Observer implements
 * org.freedesktop.Telepathy.Client.Observer DBus interface and is called by
 * the Channel Dispatcher when a new channel is created, in order to log
 * received signals using its #LogManager.
 *
 * This object is a signleton, any call to tpl_observer_new will return the
 * same object with an incremented reference counter. One has to
 * unreference the object properly when the used reference is not used
 * anymore.
 *
 * This object will register to it's DBus interface when
 * tp_base_client_register is called, ensuring that the registration will
 * happen only once per singleton instance.
 *
 * Since: 0.1
 */

/**
 * TplObserverClass:
 *
 * The class of a #TplObserver.
 */

static void tpl_observer_dispose (GObject * obj);
static void channel_prepared_cb (GObject *obj,
    GAsyncResult *result,
    gpointer user_data);
static TplChannelFactory tpl_observer_get_channel_factory (TplObserver *self);

struct _TplObserverPriv
{
    /* Registered channels
     * channel path borrowed from the TplChannel => reffed TplChannel */
    GHashTable *channels;
    /* Channels that we'll register once they are registered
     * channel path borrowed from the TplChannel => reffed TplChannel */
    GHashTable *preparing_channels;
    TplLogManager *logmanager;
    gboolean  dbus_registered;
    TplChannelFactory channel_factory;
};

typedef struct
{
  TplObserver *self;
  guint chan_n;
  TpObserveChannelsContext *ctx;
} ObservingContext;

static gboolean observing_context_try_to_return (ObservingContext *ctx);

static TplObserver *observer_singleton = NULL;

enum
{
  PROP_0,
  PROP_REGISTERED_CHANNELS
};

G_DEFINE_TYPE (TplObserver, _tpl_observer, TP_TYPE_BASE_CLIENT)

static void
tpl_observer_observe_channels (TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context)
{
  TplObserver *self = TPL_OBSERVER (client);
  TplChannelFactory chan_factory;
  GError *error = NULL;
  ObservingContext *observing_ctx = NULL;
  const gchar *chan_type;
  GList *l;

  chan_factory = tpl_observer_get_channel_factory (self);

  /* Parallelize TplChannel preparations, when the last one will be ready, the
   * counter will be 0 and tp_svc_client_observer_return_from_observe_channels
   * can be called */
  observing_ctx = g_slice_new0 (ObservingContext);
  observing_ctx->self = TPL_OBSERVER (self);
  observing_ctx->chan_n = g_list_length (channels);
  observing_ctx->ctx = g_object_ref (context);

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *channel = l->data;
      TplChannel *tpl_chan;
      GHashTable *prop_map;
      const gchar *path;

      path = tp_proxy_get_object_path (channel);

      /* Ignore channel if we are already observing it */
      if (g_hash_table_lookup (self->priv->channels, path) != NULL ||
          g_hash_table_lookup (self->priv->preparing_channels, path) != NULL)
        {
          observing_ctx->chan_n--;
          continue;
        }

      /* d.bus.propertyName.str/gvalue hash */
      prop_map = tp_channel_borrow_immutable_properties (channel);
      chan_type = tp_channel_get_channel_type (channel);

      tpl_chan = chan_factory (chan_type, connection, path, prop_map, account,
          &error);
      if (tpl_chan == NULL)
        {
          DEBUG ("%s: %s", path, error->message);
          g_clear_error (&error);
          observing_ctx->chan_n--;
          continue;
        }
      PATH_DEBUG (tpl_chan, "Starting preparation for TplChannel instance %p",
          tpl_chan);

      /* Pass the reference on the TplChannel to the hash table */
      g_hash_table_insert (self->priv->preparing_channels,
          (gchar *) tp_proxy_get_object_path (tpl_chan), tpl_chan);

      _tpl_channel_prepare_async (tpl_chan, channel_prepared_cb,
          observing_ctx);
    }

  /* Arguably we shouldn't claim to have accepted the channels if an error
   * occurred above? */
  if (!observing_context_try_to_return (observing_ctx))
    tp_observe_channels_context_delay (context);
}

static gboolean
observing_context_try_to_return (ObservingContext *observing_ctx)
{
  if (observing_ctx->chan_n == 0)
    {
      DEBUG ("Returning from observe channels");
      tp_observe_channels_context_accept (observing_ctx->ctx);
      g_object_unref (observing_ctx->ctx);
      g_slice_free (ObservingContext, observing_ctx);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
_tpl_observer_register_channel (TplObserver *self,
    TplChannel *channel)
{
  gchar *key;

  g_return_val_if_fail (TPL_IS_OBSERVER (self), FALSE);
  g_return_val_if_fail (TPL_IS_CHANNEL (channel), FALSE);

  key = (char *) tp_proxy_get_object_path (G_OBJECT (channel));

  DEBUG ("Registering channel %s", key);

  g_hash_table_insert (self->priv->channels, key, g_object_ref (channel));
  g_object_notify (G_OBJECT (self), "registered-channels");

  return TRUE;
}


static void
channel_prepared_cb (GObject *obj,
    GAsyncResult *result,
    gpointer user_data)
{
  ObservingContext *observing_ctx = user_data;
  GError *error = NULL;

  if (_tpl_action_chain_new_finish (obj, result, &error))
    {
      PATH_DEBUG (obj, "channel prepared");
      _tpl_observer_register_channel (observing_ctx->self, TPL_CHANNEL (obj));
    }
  else
    {
      PATH_DEBUG (obj, "failed to prepare channel: %s", error->message);
      g_error_free (error);
    }

  g_hash_table_remove (observing_ctx->self->priv->preparing_channels,
      tp_proxy_get_object_path (obj));

  observing_ctx->chan_n -= 1;
  observing_context_try_to_return (observing_ctx);
}


static void
tpl_observer_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplObserver *self = TPL_OBSERVER (object);

  switch (property_id)
    {
      case PROP_REGISTERED_CHANNELS:
        {
          GPtrArray *array = g_ptr_array_new ();
          GList *keys, *ptr;

          keys = g_hash_table_get_keys (self->priv->channels);

          for (ptr = keys; ptr != NULL; ptr = ptr->next)
            {
              g_ptr_array_add (array, ptr->data);
            }

          g_value_set_boxed (value, array);

          g_ptr_array_unref (array);
          g_list_free (keys);

          break;
        }

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
        break;
    }
}

static void
_tpl_observer_class_init (TplObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseClientClass *base_clt_cls = TP_BASE_CLIENT_CLASS (klass);

  object_class->dispose = tpl_observer_dispose;
  object_class->get_property = tpl_observer_get_property;

  /**
   * TplObserver:registered-channels:
   *
   * A list of channel's paths currently registered to this object.
   *
   * One can receive change notifications on this property by connecting
   * to the #GObject::notify signal and using this property as the signal
   * detail.
   */
  g_object_class_install_property (object_class, PROP_REGISTERED_CHANNELS,
      g_param_spec_boxed ("registered-channels",
        "Registered Channels",
        "open TpChannels which the TplObserver is logging",
        TP_ARRAY_TYPE_OBJECT_PATH_LIST,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (object_class, sizeof (TplObserverPriv));

  tp_base_client_implement_observe_channels (base_clt_cls,
      tpl_observer_observe_channels);
}

static void
_tpl_observer_init (TplObserver *self)
{
  TplObserverPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_OBSERVER, TplObserverPriv);
  self->priv = priv;

  priv->channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, g_object_unref);

  priv->preparing_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, g_object_unref);

  priv->logmanager = tpl_log_manager_dup_singleton ();

  /* Observe contact text channels */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
           TP_IFACE_CHANNEL_TYPE_TEXT,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
           TP_HANDLE_TYPE_CONTACT,
      NULL));

  /* Observe room text channels */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
           TP_IFACE_CHANNEL_TYPE_TEXT,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
           TP_HANDLE_TYPE_ROOM,
      NULL));

  /* Observe contact stream media channels */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
           TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
           TP_HANDLE_TYPE_CONTACT,
      NULL));

  /* Observe contact call channels */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
           "org.freedesktop.Telepathy.Channel.Type.Call1",
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
           TP_HANDLE_TYPE_CONTACT,
      NULL));

  /* Observe room call channels */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
           "org.freedesktop.Telepathy.Channel.Type.Call1",
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
           TP_HANDLE_TYPE_ROOM,
      NULL));

  tp_base_client_set_observer_recover (TP_BASE_CLIENT (self), TRUE);
}


static void
tpl_observer_dispose (GObject *obj)
{
  TplObserverPriv *priv = TPL_OBSERVER (obj)->priv;

  tp_clear_pointer (&priv->channels, g_hash_table_unref);
  tp_clear_pointer (&priv->preparing_channels, g_hash_table_unref);

  if (priv->logmanager != NULL)
    {
      g_object_unref (priv->logmanager);
      priv->logmanager = NULL;
    }

  G_OBJECT_CLASS (_tpl_observer_parent_class)->dispose (obj);
}


TplObserver *
_tpl_observer_dup (GError **error)
{
  /* WARNING Not thread safe */
  if (G_UNLIKELY (observer_singleton == NULL))
    {
      GError *dbus_error = NULL;
      TpDBusDaemon *dbus = tp_dbus_daemon_dup (&dbus_error);
      TpSimpleClientFactory *factory;

      if (dbus == NULL)
        {
          g_propagate_error (error, dbus_error);
          return NULL;
        }

      factory = (TpSimpleClientFactory *) tp_automatic_client_factory_new (dbus);

      /* Pre-select feature to be initialized. */
      tp_simple_client_factory_add_contact_features_varargs (factory,
          TP_CONTACT_FEATURE_ALIAS,
          TP_CONTACT_FEATURE_AVATAR_TOKEN,
          TP_CONTACT_FEATURE_INVALID);

      observer_singleton = g_object_new (TPL_TYPE_OBSERVER,
          "factory", factory,
          "name", "Logger",
          "uniquify-name", FALSE,
          NULL);

      g_object_add_weak_pointer (G_OBJECT (observer_singleton),
          (gpointer *) &observer_singleton);

      g_object_unref (dbus);
      g_object_unref (factory);
    }
  else
    {
      g_object_ref (observer_singleton);
    }

  return observer_singleton;
}

/**
 * _tpl_observer_unregister_channel:
 * @self: #TplObserver instance, cannot be %NULL.
 * @channel: a #TplChannel cast of a TplChannel subclass instance
 *
 * Un-registers a TplChannel subclass instance, i.e. TplTextChannel instance,
 * as TplChannel instance.
 * It is supposed to be called when the Closed signal for a channel is
 * emitted or when an un-recoverable error during the life or a TplChannel
 * happens.
 *
 * Every time that a channel is registered or unregistered, a notification is
 * sent for the 'registered-channels' property.
 *
 * Returns: %TRUE if @channel is registered and can thus be un-registered or
 * %FALSE if the @channel is not currently among registered channels and thus
 * cannot be un-registered.
 */
gboolean
_tpl_observer_unregister_channel (TplObserver *self,
    TplChannel *channel)
{
  gboolean retval;
  gchar *key;

  g_return_val_if_fail (TPL_IS_OBSERVER (self), FALSE);
  g_return_val_if_fail (TPL_IS_CHANNEL (channel), FALSE);

  key = (char *) tp_proxy_get_object_path (TP_PROXY (channel));

  DEBUG ("Unregistering channel path %s", key);

  /* this will destroy the associated value object: at this point
     the hash table reference should be the only one for the
     value's object
   */
  retval = g_hash_table_remove (self->priv->channels, key);

  if (retval)
    g_object_notify (G_OBJECT (self), "registered-channels");

  return retval;
}


static TplChannelFactory
tpl_observer_get_channel_factory (TplObserver *self)
{
  g_return_val_if_fail (TPL_IS_OBSERVER (self), NULL);

  return self->priv->channel_factory;
}


void
_tpl_observer_set_channel_factory (TplObserver *self,
    TplChannelFactory factory)
{
  g_return_if_fail (TPL_IS_OBSERVER (self));
  g_return_if_fail (factory != NULL);
  g_return_if_fail (self->priv->channel_factory == NULL);

  self->priv->channel_factory = factory;
}
