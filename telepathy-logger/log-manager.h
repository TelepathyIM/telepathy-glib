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

#ifndef __TPL_LOG_MANAGER_H__
#define __TPL_LOG_MANAGER_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/log-entry.h>

G_BEGIN_DECLS
#define TPL_TYPE_LOG_MANAGER  (tpl_log_manager_get_type ())
#define TPL_LOG_MANAGER(o)  (G_TYPE_CHECK_INSTANCE_CAST ((o), TPL_TYPE_LOG_MANAGER, TplLogManager))
#define TPL_LOG_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_CAST ((k), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))
#define TPL_IS_LOG_MANAGER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), TPL_TYPE_LOG_MANAGER))
#define TPL_IS_LOG_MANAGER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), TPL_TYPE_LOG_MANAGER))
#define TPL_LOG_MANAGER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))

#define TPL_LOG_MANAGER_ERROR g_quark_from_static_string ("tpl-log-manager-error-quark")
#define TPL_LOG_MANAGER_LOG_STORE_DEFAULT "TpLogger"

typedef enum
{
  /* generic error */
  TPL_LOG_MANAGER_ERROR_FAILED,
  TPL_LOG_MANAGER_ERROR_ADD_MESSAGE,
  /* arg passed is not a valid GObject or in the expected format */
  TPL_LOG_MANAGER_ERROR_BAD_ARG
} TplLogManagerError;


typedef struct
{
  GObject parent;

  gpointer priv;
} TplLogManager;

typedef struct
{
  GObjectClass parent_class;
} TplLogManagerClass;

typedef struct
{
  TpAccount *account;
  gchar *chat_id;
  gboolean is_chatroom;
  gchar *filename;
  gchar *date;
} TplLogSearchHit;

typedef gboolean (*TplLogMessageFilter) (TplLogEntry *message,
    gpointer user_data);

GType tpl_log_manager_get_type (void);

TplLogManager *tpl_log_manager_dup_singleton (void);

gboolean tpl_log_manager_exists (TplLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);

gboolean tpl_log_manager_get_dates_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **dates,
    GError **error);

void tpl_log_manager_get_dates_async (TplLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean is_chatroom,
    GAsyncReadyCallback callback, gpointer user_data);

gboolean tpl_log_manager_get_messages_for_date_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **messages,
    GError **error);

void tpl_log_manager_get_messages_for_date_async (TplLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean is_chatroom,
    const gchar *date, GAsyncReadyCallback callback, gpointer user_data);

gboolean tpl_log_manager_get_filtered_messages_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **messages,
    GError **error);

void tpl_log_manager_get_filtered_messages_async (TplLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean is_chatroom,
    guint num_messages, TplLogMessageFilter filter, gpointer filter_user_data,
    GAsyncReadyCallback callback, gpointer user_data);

gboolean tpl_log_manager_get_chats_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error);

void tpl_log_manager_get_chats_async (TplLogManager *manager,
    TpAccount *account, GAsyncReadyCallback callback, gpointer user_data);

gboolean tpl_log_manager_search_new_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error);

void tpl_log_manager_search_new_async (TplLogManager *manager,
    const gchar *text, GAsyncReadyCallback callback, gpointer user_data);

void tpl_log_manager_search_free (GList *hits);

gchar *tpl_log_manager_get_date_readable (const gchar *date);

G_END_DECLS
#endif /* __TPL_LOG_MANAGER_H__ */
