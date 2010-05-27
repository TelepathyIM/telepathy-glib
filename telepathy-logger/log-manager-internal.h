/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __TPL_LOG_MANAGER_PRIV_H__
#define __TPL_LOG_MANAGER_PRIV_H__

#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store-factory-internal.h>
#include <telepathy-logger/log-store-internal.h>

gboolean _tpl_log_manager_add_message (TplLogManager *manager,
    TplLogEntry *message, GError **error);

gboolean _tpl_log_manager_add_message_finish (TplLogManager *self,
    GAsyncResult *result,
    GError **error);

void _tpl_log_manager_add_message_async (TplLogManager *manager,
    TplLogEntry *message, GAsyncReadyCallback callback, gpointer user_data);

gboolean _tpl_log_manager_register_log_store (TplLogManager *self,
    TplLogStore *logstore);

GList * _tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom);

GList * _tpl_log_manager_get_messages_for_date (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const gchar *date);

GList * _tpl_log_manager_get_filtered_messages (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    guint num_messages,
    TplLogMessageFilter filter,
    gpointer user_data);

GList * _tpl_log_manager_get_chats (TplLogManager *manager,
    TpAccount *account);

GList * _tpl_log_manager_search_in_identifier_chats_new (TplLogManager *manager,
    TpAccount *account,
    gchar const *chat_id,
    const gchar *text);

GList * _tpl_log_manager_search_new (TplLogManager *manager,
    const gchar *text);

gboolean _tpl_log_manager_search_in_identifier_chats_new_finish (
    TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error);

void _tpl_log_manager_search_in_identifier_chats_new_async (
    TplLogManager *manager,
    TpAccount *account,
    gchar const *chat_id,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data);

void _tpl_log_manager_search_hit_free (TplLogSearchHit *hit);

gint _tpl_log_manager_search_hit_compare (TplLogSearchHit *a,
    TplLogSearchHit *b);

#endif /* __TPL_LOG_MANAGER_PRIV_H__ */
