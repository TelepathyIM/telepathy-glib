/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 */

#ifndef __TPL_LOG_STORE_H__
#define __TPL_LOG_STORE_H__

#include <glib-object.h>
#include <telepathy-glib/account.h>

#include <tpl-log-manager.h>
#include <tpl-log-entry-text.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_STORE (tpl_log_store_get_type ())
#define TPL_LOG_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_STORE, \
                               TplLogStore))
#define TPL_IS_LOG_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_STORE))
#define TPL_LOG_STORE_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), TPL_TYPE_LOG_STORE, \
                                  TplLogStoreInterface))

typedef struct _TplLogStore TplLogStore; /* dummy object */
typedef struct _TplLogStoreInterface TplLogStoreInterface;

struct _TplLogStoreInterface
{
  GTypeInterface parent;

  const gchar * (*get_name) (TplLogStore *self);
  gboolean (*exists) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  gboolean (*add_message) (TplLogStore *self, const gchar *chat_id,
      gboolean chatroom, TplLogEntryText *message, GError **error);
  GList * (*get_dates) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_messages_for_date) (TplLogStore *self,
      TpAccount *account, const gchar *chat_id, gboolean chatroom,
      const gchar *date);
  GList * (*get_last_messages) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_chats) (TplLogStore *self,
            TpAccount    *account);
  GList * (*search_new) (TplLogStore *self, const gchar *text);
  void (*ack_message) (TplLogStore *self, const gchar *chat_id,
      gboolean chatroom, TplLogEntryText *message);
  GList * (*get_filtered_messages) (TplLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom, guint num_messages,
      TplLogMessageFilter filter, gpointer user_data);
};

GType tpl_log_store_get_type (void);

const gchar *tpl_log_store_get_name (TplLogStore *self);
gboolean tpl_log_store_exists (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
gboolean tpl_log_store_add_message (TplLogStore *self,
    const gchar *chat_id, gboolean chatroom, TplLogEntryText *message,
    GError **error);
GList *tpl_log_store_get_dates (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *tpl_log_store_get_messages_for_date (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    const gchar *date);
GList *tpl_log_store_get_last_messages (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *tpl_log_store_get_chats (TplLogStore *self,
    TpAccount *account);
GList *tpl_log_store_search_new (TplLogStore *self,
    const gchar *text);
void tpl_log_store_ack_message (TplLogStore *self,
    const gchar *chat_id, gboolean chatroom, TplLogEntryText *message);
GList *tpl_log_store_get_filtered_messages (TplLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    guint num_messages, TplLogMessageFilter filter, gpointer user_data);

G_END_DECLS

#endif /* __TPL_LOG_STORE_H__ */
