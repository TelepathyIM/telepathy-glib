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

#ifndef __TPL_LOG_STORE_FACTORY_H__
#define __TPL_LOG_STORE_FACTORY_H__

#include <glib-object.h>

#include <telepathy-logger/log-store-internal.h>

typedef TplLogStore* (*TplLogStoreConstructor) (const gchar *name,
    gboolean write_access, gboolean read_access);
typedef TplLogStore* (*TplLogStoreFactory) (const gchar *logstore_type,
    const gchar *name, gboolean write_access, gboolean read_access);

void _tpl_log_store_factory_init (void);
void _tpl_log_store_factory_deinit (void);
void _tpl_log_store_factory_add (const gchar *logstore_type,
    TplLogStoreConstructor constructor);
TplLogStoreConstructor _tpl_log_store_factory_lookup (const gchar *logstore_type);
TplLogStore * _tpl_log_store_factory_build (const gchar *logstore_type,
    const gchar *name, gboolean write_access, gboolean read_access);

#endif /* __TPL_LOG_STORE_FACTORY_H__ */
