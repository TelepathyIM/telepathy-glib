/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include "observer.h"

#include <glib.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-client.h>

#include <telepathy-logger/conf.h>
#include <telepathy-logger/channel.h>
#include <telepathy-logger/channel-factory.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/util.h>

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
 * tpl_observer_register_dbus is called, ensuring that the registration will
 * happen only once per singleton instance.
 *
 * Since: 0.1
 */

/**
 * TplObserverClass:
 *
 * The class of a #TplObserver.
 */

static void tpl_observer_finalize (GObject * obj);
static void tpl_observer_dispose (GObject * obj);
static void observer_iface_init (gpointer, gpointer);
static void got_tpl_channel_text_ready_cb (GObject *obj, GAsyncResult *result,
    gpointer user_data);
static TplChannelFactory tpl_observer_get_channel_factory (TplObserver *self);
static GHashTable *tpl_observer_get_channel_map (TplObserver *self);


#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplObserver)
struct _TplObserverPriv
{
    // mapping channel_path->TplChannel instances 
    GHashTable *channel_map;
    TplLogManager *logmanager;
    gboolean  dbus_registered;
    TplChannelFactory channel_factory;
};

typedef struct
{
  guint chan_n;
  DBusGMethodInvocation *dbus_ctx;
} ObservingContext;

static TplObserver *observer_singleton = NULL;

static const char *client_interfaces[] = {
  TP_IFACE_CLIENT_OBSERVER,
  NULL
};

enum
{
  PROP_0,
  PROP_INTERFACES,
  PROP_CHANNEL_FILTER,
  PROP_REGISTERED_CHANNELS
};

G_DEFINE_TYPE_WITH_CODE (TplObserver, tpl_observer, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT_OBSERVER, observer_iface_init);
    );


static void
tpl_observer_observe_channels (TpSvcClientObserver *self,
    const char *account,
    const char *connection,
    const GPtrArray *channels,
    const char *dispatch_op,
    const GPtrArray *requests_satisfied,
    GHashTable *observer_info,
    DBusGMethodInvocation *dbus_context)
{
  TpAccount *tp_acc;
  TpConnection *tp_conn;
  TpDBusDaemon *tp_bus_daemon;
  TplChannelFactory chan_factory;
  TplConf *conf;
  GError *error = NULL;
  ObservingContext *observing_ctx = NULL;
  const gchar *chan_type;

  g_return_if_fail (!TPL_STR_EMPTY (account) );
  g_return_if_fail (!TPL_STR_EMPTY (connection) );

  chan_factory = tpl_observer_get_channel_factory (TPL_OBSERVER (self));

  /* Check if logging if enabled globally and for the given account_path,
   * return imemdiatly if it's not */
  conf = tpl_conf_dup();
  if (!tpl_conf_is_globally_enabled(conf, &error))
    {
      if (error != NULL)
        g_debug ("%s", error->message);
      else
        g_debug ("Logging is globally disabled. Skipping channel logging.");
      return;
    }
  if (tpl_conf_is_account_ignored(conf, account, &error))
    {
      g_debug ("Logging is disabled for account %s. "
          "Channel associated to this account. "
          "Skipping this channel logging.", account);
      return;
    }

  /* Instantiating objects to pass to - or needed by them - the Tpl Channel Factory in order to
   * obtain a TplChannelXXX instance */
  tp_bus_daemon = tp_dbus_daemon_dup (&error);
  if (tp_bus_daemon == NULL)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      return;
    }

  tp_acc = tp_account_new (tp_bus_daemon, account, &error);
  if (tp_acc == NULL)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      g_object_unref (tp_bus_daemon);
      return;
    }

  tp_conn = tp_connection_new (tp_bus_daemon, NULL, connection, &error);
  if (tp_conn == NULL)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      g_object_unref (tp_bus_daemon);
      g_object_unref (tp_acc);
      return;
    }

  /* Parallelize TplChannel preparations, when the last one will be ready, the
   * counter will be 0 and tp_svc_client_observer_return_from_observe_channels
   * can be called */
  observing_ctx = g_slice_new0 (ObservingContext);
  observing_ctx->chan_n = channels->len;
  observing_ctx->dbus_ctx = dbus_context;

  /* channels is of type a(oa{sv}) */
  for (guint i = 0; i < channels->len; i++)
    {
      GValueArray *channel = g_ptr_array_index (channels, i);
      TplChannel *tpl_chan;

      gchar *path = g_value_get_boxed (g_value_array_get_nth (channel, 0));
      /* d.bus.propertyName.str/gvalue hash */
      GHashTable *map = g_value_get_boxed (g_value_array_get_nth (channel,
          1));

      chan_type = g_value_get_string (g_hash_table_lookup (map,
          TP_PROP_CHANNEL_CHANNEL_TYPE));
      tpl_chan = chan_factory (chan_type, tp_conn, path, map, tp_acc, &error);
      if (tpl_chan == NULL)
        {
          g_debug ("Creating TplChannel: %s", error->message);
          g_clear_error (&error);
          g_error_free (error);
          error = NULL;
          continue;
        }
      tpl_channel_call_when_ready (tpl_chan, got_tpl_channel_text_ready_cb,
          observing_ctx);
    }

  g_object_unref (tp_acc);
  g_object_unref (tp_conn);
  g_object_unref (tp_bus_daemon);
}


static void
got_tpl_channel_text_ready_cb (GObject *obj,
    GAsyncResult *result,
    gpointer user_data)
{
  ObservingContext *observing_ctx = user_data;
  DBusGMethodInvocation *dbus_ctx = observing_ctx->dbus_ctx;

  observing_ctx->chan_n -= 1;
  if (observing_ctx->chan_n == 0)
    {
      tp_svc_client_observer_return_from_observe_channels (dbus_ctx);
      g_slice_free (ObservingContext, observing_ctx);
    }
}


static void
get_prop (GObject *self,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  GPtrArray *array;
  GList *key_list;
  GHashTable *map;

  switch (property_id)
    {
      case PROP_INTERFACES:
        g_value_set_boxed (value, client_interfaces);
        break;

      case PROP_CHANNEL_FILTER:
        /* create an empty filter - which means all channels */
        array = g_ptr_array_new ();
        map = g_hash_table_new (NULL, NULL);
        g_ptr_array_add (array, map);
        g_value_set_boxed (value, array);
        break;

      case PROP_REGISTERED_CHANNELS:
        array = g_ptr_array_new ();
        key_list = g_hash_table_get_keys (tpl_observer_get_channel_map (
              TPL_OBSERVER (self)));
        g_ptr_array_add (array, key_list);
        g_value_set_boxed (value, array);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
        break;
    }
}

/* Singleton Constructor */
static GObject *
tpl_observer_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval;

  if (observer_singleton)
    retval = g_object_ref (observer_singleton);
  else
    {
      retval = G_OBJECT_CLASS (tpl_observer_parent_class)->constructor (type,
          n_props, props);

      observer_singleton = TPL_OBSERVER (retval);
      g_object_add_weak_pointer (retval, (gpointer *) & observer_singleton);
    }

  return retval;
}


static void
tpl_observer_class_init (TplObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = tpl_observer_constructor;
  object_class->finalize = tpl_observer_finalize;
  object_class->dispose = tpl_observer_dispose;

  /* D-Bus properties are exposed as GObject properties through the
   * TpDBusPropertiesMixin properties on the Client interface */
  static TpDBusPropertiesMixinPropImpl client_props[] = {
    {"Interfaces", "interfaces", NULL},
    {NULL}
  };

  /* properties on the Client.Observer interface */
  static TpDBusPropertiesMixinPropImpl client_observer_props[] = {
    {"ObserverChannelFilter", "channel-filter", NULL},
    {NULL}
  };

  /* complete list of interfaces with properties */
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
    {TP_IFACE_CLIENT,
     tp_dbus_properties_mixin_getter_gobject_properties,
     NULL,
     client_props},
    {TP_IFACE_CLIENT_OBSERVER,
     tp_dbus_properties_mixin_getter_gobject_properties,
     NULL,
     client_observer_props},
    {NULL}
  };

  object_class->get_property = get_prop;

  /**
   * TplObserver:interfaces:
   *
   * Interfaces implemented by this object.
   *
   * Since: 0.1.0
   */
  g_object_class_install_property (object_class, PROP_INTERFACES,
      g_param_spec_boxed ("interfaces", "Interfaces",
        "Available D-Bus Interfaces", G_TYPE_STRV, G_PARAM_READABLE |
        G_PARAM_STATIC_STRINGS));

  /**
   * TplObserver:interfaces:
   *
   * Channels that this object will accept and manage from the Channel
   * Dispatcher
   *
   * Since: 0.1.0
   */
  g_object_class_install_property (object_class, PROP_CHANNEL_FILTER,
      g_param_spec_boxed ("channel-filter",
        "Channel Filter",
        "Filter for channels we observe",
        TP_ARRAY_TYPE_CHANNEL_CLASS_LIST, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * TplObserver:interfaces:
   *
   * A list of channel's paths currently registered to this object.
   *
   * One can receive change notifications on this property by connecting
   * to the #GObject::notify signal and using this property as the signal
   * detail.
   *
   * Since: 0.1.0
   */
  g_object_class_install_property (object_class, PROP_REGISTERED_CHANNELS,
      g_param_spec_boxed ("registered-channels",
        "Registered Channels",
        "open TpChannels which the TplObserver is logging",
        TP_ARRAY_TYPE_CHANNEL_CLASS_LIST, G_PARAM_READABLE |
        G_PARAM_STATIC_STRINGS));

  /* call our mixin class init */
  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class, G_STRUCT_OFFSET (
      TplObserverClass, dbus_props_class));

  g_type_class_add_private (object_class, sizeof (TplObserverPriv));
}


static void
tpl_observer_init (TplObserver *self)
{
  TplObserverPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_OBSERVER, TplObserverPriv);
  self->priv = priv;

  priv->channel_map = g_hash_table_new_full (g_str_hash,
      (GEqualFunc) g_str_equal, g_free, g_object_unref);
  priv->logmanager = tpl_log_manager_dup_singleton ();
}


/**
 * tpl_observer_register_dbus:
 * @self: #TplObserver instance, cannot be %NULL.
 * @error: Used to raise an error if DBus registration fails
 *
 * Registers the object using #TPL_OBSERVER_WELL_KNOWN_BUS_NAME well known
 * name.
 *
 * Returns: %TRUE if registration suceeds or if the object is already
 * registered to DBus. or %FALSE if the object is not alerady registered and
 * the registration failed, in which case @error is set.
 */
gboolean
tpl_observer_register_dbus (TplObserver *self,
    GError **error)
{
  TplObserverPriv* priv = GET_PRIV (self);
  DBusGConnection *bus;
  TpDBusDaemon *tp_bus;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_OBSERVER (self), FALSE);

  /* just return TRUE if the Observer interface is actually already registered
   * to DBus */
  if (priv->dbus_registered)
    return TRUE;

  bus = tp_get_bus ();
  tp_bus = tp_dbus_daemon_new (bus);

  if (!tp_dbus_daemon_request_name (tp_bus, TPL_OBSERVER_WELL_KNOWN_BUS_NAME,
        TRUE, error))
    {
      g_assert (error == NULL || *error != NULL);
      return FALSE;
    }

  priv->dbus_registered = TRUE;
  g_debug ("%s DBus well known name registered",
      TPL_OBSERVER_WELL_KNOWN_BUS_NAME);

  dbus_g_connection_register_g_object (bus, TPL_OBSERVER_OBJECT_PATH,
      G_OBJECT (self));

  return TRUE;
}


static void
observer_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TpSvcClientObserverClass *klass = (TpSvcClientObserverClass *) g_iface;

  tp_svc_client_observer_implement_observe_channels (klass,
      tpl_observer_observe_channels);
}


static void
tpl_observer_dispose (GObject *obj)
{
  TplObserverPriv *priv = GET_PRIV (obj);

  if (priv->channel_map != NULL)
    {
      g_hash_table_unref (priv->channel_map);
      priv->channel_map = NULL;
    }
  tpl_object_unref_if_not_null (priv->logmanager);
  priv->logmanager = NULL;

  G_OBJECT_CLASS (tpl_observer_parent_class)->dispose (obj);
}


static void
tpl_observer_finalize (GObject *obj)
{
  G_OBJECT_CLASS (tpl_observer_parent_class)->finalize (obj);
}


TplObserver *
tpl_observer_new (void)
{
  return g_object_new (TPL_TYPE_OBSERVER, NULL);
}


static GHashTable *
tpl_observer_get_channel_map (TplObserver *self)
{
  g_return_val_if_fail (TPL_IS_OBSERVER (self), NULL);

  return GET_PRIV (self)->channel_map;
}


gboolean
tpl_observer_register_channel (TplObserver *self,
    TplChannel *channel)
{
  GHashTable *glob_map = tpl_observer_get_channel_map (self);
  gchar *key;

  g_return_val_if_fail (TPL_IS_OBSERVER (self), FALSE);
  g_return_val_if_fail (TPL_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  /* 'key' will be freed by the hash table on key removal/destruction */
  g_object_get (G_OBJECT (channel), "object-path", &key, NULL);

  if (g_hash_table_lookup (glob_map, key) != NULL)
    {
      g_error ("Channel path found, replacing %s", key);
      g_hash_table_remove (glob_map, key);
    }
  else
      g_debug ("Channel path not found, registering %s", key);

  g_hash_table_insert (glob_map, key, channel);
  g_object_notify (G_OBJECT(self), "registered-channels");

  g_object_unref (channel);

  return TRUE;
}


/**
 * tpl_observer_unregister_channel:
 * @self: #TplObserver instance, cannot be %NULL.
 * @channel: a #TplChannel cast of a TplChannel subclass instance
 *
 * Un-registers a TplChannel subclass instance, i.e. TplChannelText instance, as
 * TplChannel instance.
 * It is supposed to be called when the Closed signal for a channel is
 * emitted or when an un-recoverable error during the life or a TplChannel
 * happens.
 *
 * Every time that a channel is registered or unregistered, a notification is
 * sent for the 'registered-channels' property.
 *
 * Returns: %TRUE if @channel is registered and can thus be un-registered or %FALSE
 * if the @channel is not currently among registered channels and thus cannot
 * be un-registered.
 */


gboolean
tpl_observer_unregister_channel (TplObserver *self,
    TplChannel *channel)
{
  GHashTable *glob_map = tpl_observer_get_channel_map (self);
  gboolean retval;
  gchar *key;

  g_return_val_if_fail (TPL_IS_OBSERVER (self), FALSE);
  g_return_val_if_fail (TPL_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  g_object_get (G_OBJECT (channel), "object-path", &key, NULL);

  g_debug ("Unregistering channel path %s", key);

  /* this will destroy the associated value object: at this point
     the hash table reference should be the only one for the
     value's object
   */
  retval = g_hash_table_remove (glob_map, key);
  if (retval)
    g_object_notify (G_OBJECT(self), "registered-channels");
  g_free (key);
  return retval;
}


static TplChannelFactory
tpl_observer_get_channel_factory (TplObserver *self)
{
  g_return_val_if_fail (TPL_IS_OBSERVER (self), NULL);

  return GET_PRIV (self)->channel_factory;
}


void
tpl_observer_set_channel_factory (TplObserver *self,
    TplChannelFactory factory)
{
  TplObserverPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_OBSERVER (self));
  g_return_if_fail (factory != NULL);
  g_return_if_fail (factory != NULL);
  g_return_if_fail (priv->channel_factory == NULL);

  priv->channel_factory = factory;

}
