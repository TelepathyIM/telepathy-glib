/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
    TplEvent *event, GError **error);

gboolean _tpl_log_manager_register_log_store (TplLogManager *self,
    TplLogStore *logstore);

GList * _tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type);

GList * _tpl_log_manager_get_events_for_date (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    const GDate *date);

GList * _tpl_log_manager_get_filtered_events (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data);

GList * _tpl_log_manager_get_events (TplLogManager *manager,
    TpAccount *account);

GList * _tpl_log_manager_search (TplLogManager *manager,
    const gchar *text);

GList * _tpl_log_manager_search_in_identifier (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    TplEventSearchType type,
    const gchar *text);

gboolean _tpl_log_manager_search_in_identifier_finish (
    TplLogManager *self,
    GAsyncResult *result,
    GList **hits,
    GError **error);

void _tpl_log_manager_search_in_identifier_async (
    TplLogManager *manager,
    TpAccount *account,
    gchar const *id,
    TplEventSearchType type,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data);

TplLogSearchHit * _tpl_log_manager_search_hit_new (TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    const gchar *filename,
    GDate *date);

void _tpl_log_manager_search_hit_free (TplLogSearchHit *hit);

gint _tpl_log_manager_search_hit_compare (TplLogSearchHit *a,
    TplLogSearchHit *b);

TplLogSearchHit * _tpl_log_manager_search_hit_copy (TplLogSearchHit *hit);

#endif /* __TPL_LOG_MANAGER_PRIV_H__ */
