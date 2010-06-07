/*-*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_LOG_STORE_H__
#define __TPL_LOG_STORE_H__

#include <glib-object.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/entry.h>
#include <telepathy-logger/log-manager.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_STORE (_tpl_log_store_get_type ())
#define TPL_LOG_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
      TPL_TYPE_LOG_STORE, TplLogStore))
#define TPL_IS_LOG_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
      TPL_TYPE_LOG_STORE))
#define TPL_LOG_STORE_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ( \
      (inst), TPL_TYPE_LOG_STORE, TplLogStoreInterface))

#define TPL_LOG_STORE_ERROR g_quark_from_static_string ("tpl-log-store-error-quark")
typedef enum
{
  /* generic error */
  TPL_LOG_STORE_ERROR_FAILED,
  /* generic failure for add_message() method, when nothing else applies */
  TPL_LOG_STORE_ERROR_ADD_MESSAGE,
  /* data is already present in the LogStore */
  TPL_LOG_STORE_ERROR_PRESENT,
  /* data is not present in the LogStore */
  TPL_LOG_STORE_ERROR_NOT_PRESENT,
  /* to be used in TplLogStoreIndexError as first value, so that value won't
   * overlap */
  TPL_LOG_STORE_ERROR_LAST
} TplLogStoreError;

typedef struct _TplLogStore TplLogStore;  /*dummy object */

typedef struct
{
  GTypeInterface parent;

  const gchar * (*get_name) (TplLogStore *self);
  gboolean (*exists) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  gboolean (*add_message) (TplLogStore *self, TplEntry *message,
      GError **error);
  GList * (*get_dates) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_messages_for_date) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom, const GDate *date);
  GList * (*get_recent_messages) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_chats) (TplLogStore *self, TpAccount *account);
  GList * (*search_new) (TplLogStore *self, const gchar *text);
  GList * (*search_in_identifier_chats_new) (TplLogStore *self,
      TpAccount *account, const gchar *identifier, const gchar *text);
  GList * (*get_filtered_messages) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom, guint num_messages,
      TplLogMessageFilter filter, gpointer user_data);
} TplLogStoreInterface;

GType _tpl_log_store_get_type (void);

const gchar * _tpl_log_store_get_name (TplLogStore *self);
gboolean _tpl_log_store_exists (TplLogStore *self, TpAccount *account,
    const gchar *chat_id, gboolean chatroom);
gboolean _tpl_log_store_add_message (TplLogStore *self, TplEntry *message,
    GError **error);
GList * _tpl_log_store_get_dates (TplLogStore *self, TpAccount *account,
    const gchar *chat_id, gboolean chatroom);
GList * _tpl_log_store_get_messages_for_date (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    const GDate *date);
GList * _tpl_log_store_get_recent_messages (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList * _tpl_log_store_get_chats (TplLogStore *self, TpAccount *account);
GList * _tpl_log_store_search_in_identifier_chats_new (TplLogStore *self,
    TpAccount *account, const gchar *identifier, const gchar *text);
GList * _tpl_log_store_search_new (TplLogStore *self, const gchar *text);
GList * _tpl_log_store_get_filtered_messages (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    guint num_messages, TplLogMessageFilter filter, gpointer user_data);
gboolean _tpl_log_store_is_writable (TplLogStore *self);
gboolean _tpl_log_store_is_readable (TplLogStore *self);

G_END_DECLS

#endif /*__TPL_LOG_STORE_H__ */
