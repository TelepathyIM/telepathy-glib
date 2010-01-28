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
#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/log-manager.h>

// TODO move to a member of TplObserver
static TplLogManager *logmanager = NULL;

static void tpl_observer_finalize (GObject * obj);
static void tpl_observer_dispose (GObject * obj);
static void observer_iface_init (gpointer, gpointer);
static void got_tpl_channel_text_ready_cb (GObject *obj, GAsyncResult *result,
    gpointer user_data);


G_DEFINE_TYPE_WITH_CODE (TplObserver, tpl_observer, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
						tp_dbus_properties_mixin_iface_init);
			 G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT,
						NULL);
			 G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CLIENT_OBSERVER,
						observer_iface_init);
  );

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

static void
tpl_observer_observe_channels (TpSvcClientObserver *self,
    const char *account,
    const char *connection,
    const GPtrArray *channels,
    const char *dispatch_op,
    const GPtrArray *requests_satisfied,
    GHashTable *observer_info,
    DBusGMethodInvocation *context)
{
  TpAccount *tp_acc;
  TpConnection *tp_conn;
  TpDBusDaemon *tp_bus_daemon;
  TplConf *conf;
  GError *error = NULL;

  g_return_if_fail (!TPL_STR_EMPTY (account) );
  g_return_if_fail (!TPL_STR_EMPTY (connection) );

  /* Check if logging if enabled globally and for the given account_path,
   * return imemdiatly if it's not
   */

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
      g_debug("Logging is disabled for account %s. "
          "Skipping channel logging.", account);
      return;
    }

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

  /* channels is of type a(oa{sv}) */
  for (guint i = 0; i < channels->len; i++)
    {
      GValueArray *channel = g_ptr_array_index (channels, i);
      TplChannel *tpl_chan;

      gchar *path = g_value_get_boxed (g_value_array_get_nth (channel, 0));
      // d.bus.propertyName.str/gvalue hash
      GHashTable *map = g_value_get_boxed (g_value_array_get_nth (channel,
          1));

      //tpl_channel_factory (g_value_get_string (g_hash_table_lookup (map,
      //    TP_PROP_CHANNEL_CHANNEL_TYPE)));

      tpl_chan = TPL_CHANNEL (tpl_channel_text_new (tp_conn, path, map,
            tp_acc, &error));
      if (tpl_chan == NULL)
        {
          g_debug ("%s", error->message);
          g_clear_error (&error);
          g_error_free (error);
          error = NULL;
          continue;
        }
      tpl_channel_text_call_when_ready (TPL_CHANNEL_TEXT (tpl_chan),
          got_tpl_channel_text_ready_cb, context);
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
  DBusGMethodInvocation *context = user_data;
  tp_svc_client_observer_return_from_observe_channels (context);
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
    {
      retval = g_object_ref (observer_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (tpl_observer_parent_class)->constructor
	(type, n_props, props);

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
   * TpDBusPropertiesMixin */
  /* properties on the Client interface */
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

  g_object_class_install_property (object_class, PROP_INTERFACES,
      g_param_spec_boxed ("interfaces", "Interfaces",
        "Available D-Bus Interfaces", G_TYPE_STRV, G_PARAM_READABLE |
        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CHANNEL_FILTER,
      g_param_spec_boxed ("channel-filter", "Channel Filter",
        "Filter for channels we observe", TP_ARRAY_TYPE_CHANNEL_CLASS_LIST,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_REGISTERED_CHANNELS,
      g_param_spec_boxed ("registered-channels", "Registered Channels",
        "open TpChannels which the TplObserver is logging",
        TP_ARRAY_TYPE_CHANNEL_CLASS_LIST, G_PARAM_READABLE |
        G_PARAM_STATIC_STRINGS));

  /* call our mixin class init */
  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class, G_STRUCT_OFFSET (
      TplObserverClass, dbus_props_class));
}


static void
tpl_observer_init (TplObserver *self)
{
  DBusGConnection *bus;
  TpDBusDaemon *tp_bus;
  GError *error = NULL;

  self->channel_map = g_hash_table_new_full (g_str_hash, (GEqualFunc) tpl_strequal,
      g_free, g_object_unref);
  logmanager = tpl_log_manager_dup_singleton ();

  bus = tp_get_bus ();
  tp_bus = tp_dbus_daemon_new (bus);

  if (tp_dbus_daemon_request_name (tp_bus, TPL_OBSERVER_WELL_KNOWN_BUS_NAME,
      TRUE, &error))
    {
      g_debug ("%s DBus well known name registered",
          TPL_OBSERVER_WELL_KNOWN_BUS_NAME);
    }
  else
    {
      g_error ("Well Known name request error: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
    }

  dbus_g_connection_register_g_object (bus, TPL_OBSERVER_OBJECT_PATH,
      G_OBJECT (self));
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
  TplObserver *self = TPL_OBSERVER (obj);

  if (self->channel_map != NULL)
    {
      g_object_unref (self->channel_map);
      self->channel_map = NULL;
    }
  if (logmanager != NULL)
    {
      g_object_unref (logmanager);
      logmanager = NULL;
    }

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


GHashTable *
tpl_observer_get_channel_map (TplObserver *self)
{
  g_return_val_if_fail (TPL_IS_OBSERVER (self), NULL);

  return self->channel_map;
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
  g_object_get (G_OBJECT (tp_channel_borrow_connection (
      TP_CHANNEL (channel))), "object-path", &key, NULL);

  if (g_hash_table_lookup (glob_map, key) != NULL)
    {
      g_error ("Channel path found, replacing %s", key);
      g_hash_table_remove (glob_map, key);
    }
  else
    {
      g_debug ("Channel path not found, registering %s", key);
    }

  g_hash_table_insert (glob_map, key, channel);

  g_object_unref (channel);

  return TRUE;
}

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

  g_object_get (G_OBJECT (tp_channel_borrow_connection (
      TP_CHANNEL (channel))), "object-path", &key, NULL);

  g_debug ("Unregistering channel path %s", key);

  /* this will destroy the associated value object: at this point
     the hash table reference should be the only one for the
     value's object
   */
  retval = g_hash_table_remove (glob_map, key);
  g_free (key);
  return retval;
}
