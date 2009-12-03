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

#ifndef __EMPATHY_LOG_STORE_H__
#define __EMPATHY_LOG_STORE_H__

#include <glib-object.h>

#include <telepathy-glib/account.h>

#include "empathy-message.h"
#include "empathy-log-manager.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_LOG_STORE (empathy_log_store_get_type ())
#define EMPATHY_LOG_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_LOG_STORE, \
                               EmpathyLogStore))
#define EMPATHY_IS_LOG_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_LOG_STORE))
#define EMPATHY_LOG_STORE_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EMPATHY_TYPE_LOG_STORE, \
                                  EmpathyLogStoreInterface))

typedef struct _EmpathyLogStore EmpathyLogStore; /* dummy object */
typedef struct _EmpathyLogStoreInterface EmpathyLogStoreInterface;

struct _EmpathyLogStoreInterface
{
  GTypeInterface parent;

  const gchar * (*get_name) (EmpathyLogStore *self);
  gboolean (*exists) (EmpathyLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  gboolean (*add_message) (EmpathyLogStore *self, const gchar *chat_id,
      gboolean chatroom, EmpathyMessage *message, GError **error);
  GList * (*get_dates) (EmpathyLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_messages_for_date) (EmpathyLogStore *self,
      TpAccount *account, const gchar *chat_id, gboolean chatroom,
      const gchar *date);
  GList * (*get_last_messages) (EmpathyLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_chats) (EmpathyLogStore *self,
            TpAccount    *account);
  GList * (*search_new) (EmpathyLogStore *self, const gchar *text);
  void (*ack_message) (EmpathyLogStore *self, const gchar *chat_id,
      gboolean chatroom, EmpathyMessage *message);
  GList * (*get_filtered_messages) (EmpathyLogStore *self, TpAccount *account,
      const gchar *chat_id, gboolean chatroom, guint num_messages,
      EmpathyLogMessageFilter filter, gpointer user_data);
};

GType empathy_log_store_get_type (void) G_GNUC_CONST;

const gchar *empathy_log_store_get_name (EmpathyLogStore *self);
gboolean empathy_log_store_exists (EmpathyLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
gboolean empathy_log_store_add_message (EmpathyLogStore *self,
    const gchar *chat_id, gboolean chatroom, EmpathyMessage *message,
    GError **error);
GList *empathy_log_store_get_dates (EmpathyLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_store_get_messages_for_date (EmpathyLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    const gchar *date);
GList *empathy_log_store_get_last_messages (EmpathyLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_store_get_chats (EmpathyLogStore *self,
    TpAccount *account);
GList *empathy_log_store_search_new (EmpathyLogStore *self,
    const gchar *text);
void empathy_log_store_ack_message (EmpathyLogStore *self,
    const gchar *chat_id, gboolean chatroom, EmpathyMessage *message);
GList *empathy_log_store_get_filtered_messages (EmpathyLogStore *self,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    guint num_messages, EmpathyLogMessageFilter filter, gpointer user_data);

G_END_DECLS

#endif /* __EMPATHY_LOG_STORE_H__ */
