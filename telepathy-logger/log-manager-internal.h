/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2011 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __TPL_LOG_MANAGER_PRIV_H__
#define __TPL_LOG_MANAGER_PRIV_H__

#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store-factory-internal.h>
#include <telepathy-logger/log-store-internal.h>

gboolean _tpl_log_manager_add_event (TplLogManager *manager,
    TplEvent *event,
    GError **error);

gboolean _tpl_log_manager_register_log_store (TplLogManager *self,
    TplLogStore *logstore);

GList * _tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    gint type_mask);

GList * _tpl_log_manager_get_events_for_date (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    const GDate *date);

GList * _tpl_log_manager_get_filtered_events (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data);

GList * _tpl_log_manager_get_entities (TplLogManager *manager,
    TpAccount *account);

GList * _tpl_log_manager_search (TplLogManager *manager,
    const gchar *text,
    gint type_mask);

void _tpl_log_manager_clear (TplLogManager *self);

void _tpl_log_manager_clear_account (TplLogManager *self, TpAccount *account);

void _tpl_log_manager_clear_entity (TplLogManager *self, TpAccount *account,
    TplEntity *entity);

TplLogSearchHit * _tpl_log_manager_search_hit_new (TpAccount *account,
    TplEntity *target,
    GDate *date);

void _tpl_log_manager_search_hit_free (TplLogSearchHit *hit);

TplLogSearchHit * _tpl_log_manager_search_hit_copy (TplLogSearchHit *hit);

#endif /* __TPL_LOG_MANAGER_PRIV_H__ */
