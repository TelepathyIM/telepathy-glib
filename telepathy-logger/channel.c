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

#include <glib.h>

#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/observer.h>

G_DEFINE_TYPE (TplChannel, tpl_channel, G_TYPE_OBJECT)

struct
{
  TpChannel *channel;
  gchar *channel_path;
  gchar *channel_type;
  GHashTable *channel_properties;

  TpAccount *account;
  gchar *account_path;
  TpConnection *connection;
  gchar *connection_path;

  TpSvcClientObserver *observer;

} _TplChannelPriv;

static void tpl_channel_dispose (GObject * obj)
{
  TplChannel *self = TPL_CHANNEL (obj);

  tpl_object_unref_if_not_null (self->channel);
  self->channel = NULL;

  if (self->channel_properties != NULL)
    g_hash_table_unref (self->channel_properties);
  self->channel_properties = NULL;

  tpl_object_unref_if_not_null (self->account);
  self->account = NULL;

  tpl_object_unref_if_not_null (self->connection);
  self->connection = NULL;

  tpl_object_unref_if_not_null (self->observer);
  self->observer = NULL;

  G_OBJECT_CLASS (tpl_channel_parent_class)->dispose (obj);
}

static void
tpl_channel_finalize (GObject * obj)
{
  TplChannel *self = TPL_CHANNEL (obj);
  g_free (self->channel_path);
  g_free (self->channel_type);
  g_free (self->account_path);
  g_free (self->connection_path);

  G_OBJECT_CLASS (tpl_channel_parent_class)->finalize (obj);
}

static void
tpl_channel_class_init (TplChannelClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_channel_dispose;
  object_class->finalize = tpl_channel_finalize;
}


static void
tpl_channel_init (TplChannel * self)
{
}


TplChannel *
tpl_channel_new (TpSvcClientObserver * observer)
{
  g_return_val_if_fail (TP_IS_SVC_CLIENT_OBSERVER (observer), NULL);

  TplChannel *ret = g_object_new (TPL_TYPE_CHANNEL, NULL);
  tpl_channel_set_observer (ret, observer);
  return ret;
}

TpSvcClientObserver *
tpl_channel_get_observer (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->observer;
}

TpAccount *
tpl_channel_get_account (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->account;
}

const gchar *
tpl_channel_get_account_path (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->account_path;
}

TpConnection *
tpl_channel_get_connection (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->connection;
}

const gchar *
tpl_channel_get_connection_path (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->connection_path;
}

TpChannel *
tpl_channel_get_channel (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->channel;
}

const gchar *
tpl_channel_get_channel_path (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->channel_path;
}

const gchar *
tpl_channel_get_channel_type (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->channel_type;
}

GHashTable *
tpl_channel_get_channel_properties (TplChannel * self)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);
  return self->channel_properties;
}



void
tpl_channel_set_observer (TplChannel * self, TpSvcClientObserver * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_SVC_CLIENT_OBSERVER (data) || data == NULL);

  tpl_object_unref_if_not_null (self->observer);
  self->observer = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_account (TplChannel * self, TpAccount * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_ACCOUNT (data) || data == NULL);

  tpl_object_unref_if_not_null (self->account);
  self->account = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_account_path (TplChannel * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  g_free (self->account_path);
  self->account_path = g_strdup (data);
}

void
tpl_channel_set_connection (TplChannel * self, TpConnection * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_CONNECTION (data) || data == NULL);

  tpl_object_unref_if_not_null (self->connection);
  self->connection = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_connection_path (TplChannel * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  g_free (self->connection_path);
  self->connection_path = g_strdup (data);
}

void
tpl_channel_set_channel (TplChannel * self, TpChannel * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_CHANNEL (data) || data == NULL);

  tpl_object_unref_if_not_null (self->channel);
  self->channel = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_channel_set_channel_path (TplChannel * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  g_free (self->channel_path);
  self->channel_path = g_strdup (data);
}

void
tpl_channel_set_channel_type (TplChannel * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  g_free (self->channel_type);
  self->channel_type = g_strdup (data);
}

void
tpl_channel_set_channel_properties (TplChannel * self, GHashTable * data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  // TODO check validity of data

  if (self->channel_properties != NULL)
    g_hash_table_unref (self->channel_properties);
  self->channel_properties = data;
  if (data != NULL)
    g_hash_table_ref (data);
}


gboolean
tpl_channel_register_to_observer (TplChannel * self)
{
  TplObserver *obs = TPL_OBSERVER (tpl_channel_get_observer (self));
  GHashTable *glob_map = tpl_observer_get_channel_map (obs);
  gchar *key;

  g_return_val_if_fail (TPL_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  key = g_strdup (tpl_channel_get_channel_path (self));

  if (g_hash_table_lookup (glob_map, key) != NULL)
    {
      g_error ("Channel path found, replacing %s", key);
      g_hash_table_remove (glob_map, key);
    }
  else
    {
      g_debug ("Channel path not found, registering %s\n", key);
    }

  /* Instantiate and delegate channel handling to the right object */
  if (0 == g_strcmp0 (TP_IFACE_CHAN_TEXT,
		      tpl_channel_get_channel_type (self)))
    {
      /* when removed, automatically frees the Key and unrefs
	       its Value */
      TplTextChannel *chan_text = tpl_text_channel_new (self);
      g_hash_table_insert (glob_map, key, chan_text);
    }
  else
    {
      g_warning ("%s: channel type not handled by this logger",
		 tpl_channel_get_channel_type (self));
    }

  g_object_unref (self);

  return TRUE;
}

gboolean
tpl_channel_unregister_from_observer (TplChannel * self)
{
  TplObserver *obs = TPL_OBSERVER (tpl_channel_get_observer (self));
  GHashTable *glob_map = tpl_observer_get_channel_map (obs);
  const gchar *key;

  g_assert (TPL_IS_CHANNEL (self));
  g_assert (glob_map != NULL);
  g_return_val_if_fail (TPL_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (glob_map != NULL, FALSE);

  key = tpl_channel_get_channel_path (self);
  g_debug ("Unregistering channel path %s\n", key);

  /* this will destroy the associated value object: at this point
     the hash table reference should be the only one for the
     value's object
   */
  return g_hash_table_remove (glob_map, key);
}
