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
#include "log-store-factory-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

static GHashTable *logstores_table = NULL;

void
_tpl_log_store_factory_init (void)
{
  g_return_if_fail (logstores_table == NULL);

  logstores_table = g_hash_table_new_full (g_str_hash,
      (GEqualFunc) g_str_equal, g_free, NULL);
}


void
_tpl_log_store_factory_add (const gchar *logstore_type,
    TplLogStoreConstructor constructor)
{
  gchar *key;

  g_return_if_fail (!TPL_STR_EMPTY (logstore_type));
  g_return_if_fail (constructor != NULL);
  g_return_if_fail (logstores_table != NULL);

  key = g_strdup (logstore_type);

  if (g_hash_table_lookup (logstores_table, logstore_type) != NULL)
    {
      g_warning ("Type %s already mapped. replacing constructor.",
          logstore_type);
      g_hash_table_replace (logstores_table, key, constructor);
    }
  else
    g_hash_table_insert (logstores_table, key, constructor);
}


TplLogStoreConstructor
_tpl_log_store_factory_lookup (const gchar *logstore_type)
{
  g_return_val_if_fail (!TPL_STR_EMPTY (logstore_type), NULL);
  g_return_val_if_fail (logstores_table != NULL, NULL);

  return g_hash_table_lookup (logstores_table, logstore_type);
}

void
_tpl_log_store_factory_deinit (void)
{
  g_return_if_fail (logstores_table != NULL);

  g_hash_table_unref (logstores_table);
  logstores_table = NULL;
}

TplLogStore *
_tpl_log_store_factory_build (const gchar *logstore_type,
    const gchar *name,
    gboolean write_access,
    gboolean read_access)
{
  TplLogStoreConstructor constructor;

  g_return_val_if_fail (logstores_table != NULL, NULL);

  constructor = _tpl_log_store_factory_lookup (logstore_type);
  if (constructor == NULL)
    {
      DEBUG ("%s: log store type not handled by this logger", logstore_type);
      return NULL;
    }

  return constructor (name, write_access, read_access);
}
