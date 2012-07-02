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
#include "channel-factory-internal.h"

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/text-channel-internal.h>

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

static GHashTable *channel_table = NULL;

void
_tpl_channel_factory_init (void)
{
  g_return_if_fail (channel_table == NULL);

  channel_table = g_hash_table_new_full (g_str_hash,
      (GEqualFunc) g_str_equal, g_free, NULL);
}


void
_tpl_channel_factory_add (const gchar *type,
    TplChannelConstructor constructor)
{
  gchar *key;

  g_return_if_fail (!TPL_STR_EMPTY (type));
  g_return_if_fail (constructor != NULL);
  g_return_if_fail (channel_table != NULL);

  key = g_strdup (type);

  if (g_hash_table_lookup (channel_table, type) != NULL)
    {
      g_warning ("Type %s already mapped. replacing constructor.", type);
      g_hash_table_replace (channel_table, key, constructor);
    }
  else
    g_hash_table_insert (channel_table, key, constructor);
}


TplChannelConstructor
_tpl_channel_factory_lookup (const gchar *type)
{
  g_return_val_if_fail (!TPL_STR_EMPTY (type), NULL);
  g_return_val_if_fail (channel_table != NULL, NULL);

  return g_hash_table_lookup (channel_table, type);
}

void
_tpl_channel_factory_deinit (void)
{
  g_return_if_fail (channel_table != NULL);

  g_hash_table_unref (channel_table);
  channel_table = NULL;
}

TplChannel *
_tpl_channel_factory_build (const gchar *channel_type,
    TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *tp_acc,
    GError **error)
{
  TplChannelConstructor chan_constructor;

  g_return_val_if_fail (channel_table != NULL, NULL);

  chan_constructor = _tpl_channel_factory_lookup (channel_type);
  if (chan_constructor == NULL)
    {
      g_set_error (error, TPL_CHANNEL_FACTORY_ERROR,
          TPL_CHANNEL_FACTORY_ERROR_CHANNEL_TYPE_NOT_HANDLED,
          "%s: channel type not handled by this logger", channel_type);
      return NULL;
    }

  return chan_constructor (conn, object_path, tp_chan_props, tp_acc, error);
}
