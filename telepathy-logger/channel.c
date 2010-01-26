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

#include "channel.h"

#include <string.h>

#include <glib.h>

#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/observer.h>

#define TPCHAN_PROP_PREFIX "org.freedesktop.Telepathy.Channel."
#define TPCHAN_PROP_PREFIX_LEN strlen(TPCHAN_PROP_PREFIX)

G_DEFINE_TYPE (TplChannel, tpl_channel, TP_TYPE_CHANNEL)

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplChannel)
struct _TplChannelPriv
{
  TpAccount *account;
  gchar *account_path;
  TplObserver *observer;
};

enum
{
  PROP0,
  PROP_OBSERVER,
  PROP_ACCOUNT
};

static void
get_prop (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplChannelPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_OBSERVER:
        g_value_set_object (value, priv->observer);
        break;
      case PROP_ACCOUNT:
        g_value_set_object (value, priv->account);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
set_prop (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplChannel *self = TPL_CHANNEL (object);

  switch (param_id) {
      case PROP_OBSERVER:
        tpl_channel_set_observer (self, g_value_get_object (value));
        break;
      case PROP_ACCOUNT:
        tpl_channel_set_account (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}

static void
tpl_channel_dispose (GObject *obj)
{
  TplChannelPriv *priv = GET_PRIV (obj);

  tpl_object_unref_if_not_null (priv->account);
  priv->account = NULL;

  tpl_object_unref_if_not_null (priv->observer);
  priv->observer = NULL;

  G_OBJECT_CLASS (tpl_channel_parent_class)->dispose (obj);
}

static void
tpl_channel_finalize (GObject *obj)
{
  TplChannelPriv *priv = GET_PRIV (obj);

  g_free (priv->account_path);

  G_OBJECT_CLASS (tpl_channel_parent_class)->finalize (obj);
}

static void
tpl_channel_class_init (TplChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_channel_dispose;
  object_class->finalize = tpl_channel_finalize;
  object_class->get_property = get_prop;
  object_class->set_property = set_prop;

  param_spec = g_param_spec_object ("observer",
      "Observer",
      "TplObserver instance owning the TplChannel",
      TPL_TYPE_OBSERVER,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBSERVER, param_spec);

  param_spec = g_param_spec_object ("account",
      "Account",
      "TpAccount instance associated with TplChannel",
      TP_TYPE_ACCOUNT,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  g_type_class_add_private (object_class, sizeof (TplChannelPriv));
}


static void
tpl_channel_init (TplChannel *self)
{
  TplChannelPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TPL_TYPE_CHANNEL,
      TplChannelPriv);
  self->priv = priv;
}


TplChannel *
tpl_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TplObserver *observer,
    GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;
  TplChannel *ret;

  /* Do what tp_channel_new_from_properties does + I set TplChannel
   * specific properties
   */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);
  g_return_val_if_fail (TPL_IS_OBSERVER (observer), NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  ret = g_object_new (TPL_TYPE_CHANNEL,
      /* TplChannel properties */
      "observer", observer,
      /* TpChannel properties */
      "connection", conn,
      "dbus-daemon", conn_proxy->dbus_daemon,
      "bus-name", conn_proxy->bus_name,
      "object-path", object_path,
      "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
      "channel-properties", tp_chan_props,
      NULL);

  return ret;
}

TplObserver *
tpl_channel_get_observer (TplChannel *self)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);

  return priv->observer;
}

TpAccount *
tpl_channel_get_account (TplChannel *self)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);

  return priv->account;
}

const gchar *
tpl_channel_get_account_path (TplChannel *self)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);

  return priv->account_path;
}


void
tpl_channel_set_observer (TplChannel *self,
    TplObserver *data)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_SVC_CLIENT_OBSERVER (data) || data == NULL);

  tpl_object_unref_if_not_null (priv->observer);
  priv->observer = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_account (TplChannel *self,
    TpAccount *data)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_ACCOUNT (data) || data == NULL);

  tpl_object_unref_if_not_null (priv->account);
  priv->account = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_account_path (TplChannel *self,
    const gchar *data)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  g_free (priv->account_path);
  priv->account_path = g_strdup (data);
}


gboolean
tpl_channel_register_to_observer (TplChannel *self)
{
  TplObserver *obs = TPL_OBSERVER (tpl_channel_get_observer (self));
  GHashTable *glob_map = tpl_observer_get_channel_map (obs);
  gchar *key;

  g_return_val_if_fail (TPL_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  /* 'key' will be freed by the hash table on key removal/destruction */
  g_object_get (G_OBJECT (tp_channel_borrow_connection (TP_CHANNEL (self))),
      "object-path", &key, NULL);

  if (g_hash_table_lookup (glob_map, key) != NULL)
    {
      g_error ("Channel path found, replacing %s", key);
      g_hash_table_remove (glob_map, key);
    }
  else
    {
      g_debug ("Channel path not found, registering %s", key);
    }


  /* Instantiate and delegate channel handling to the right object */
  if (0 == g_strcmp0 (TP_IFACE_CHAN_TEXT, tp_channel_get_channel_type (
          TP_CHANNEL (self))))
    {
      /* when removed, automatically frees the Key and unrefs
	       its Value */
      TplTextChannel *chan_text = tpl_text_channel_new (self);

      g_hash_table_insert (glob_map, key, chan_text);
    }
  else
    {
      g_warning ("%s: channel type not handled by this logger",
          tp_channel_get_channel_type ( TP_CHANNEL (self)));
    }

  g_object_unref (self);

  return TRUE;
}

gboolean
tpl_channel_unregister_from_observer (TplChannel *self)
{
  TplObserver *obs = TPL_OBSERVER (tpl_channel_get_observer (self));
  GHashTable *glob_map = tpl_observer_get_channel_map (obs);
  gboolean retval = FALSE;
  gchar *key;

  g_return_val_if_fail (TPL_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  g_object_get ( G_OBJECT (tp_channel_borrow_connection (TP_CHANNEL (self))),
      "object-path", &key, NULL);

  g_debug ("Unregistering channel path %s", key);

  /* this will destroy the associated value object: at this point
     the hash table reference should be the only one for the
     value's object
   */
  retval = g_hash_table_remove (glob_map, key);
  g_free (key);
  return retval;
}
