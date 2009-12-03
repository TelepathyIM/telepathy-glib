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

#include "empathy-log-store.h"

GType
empathy_log_store_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (EmpathyLogStoreInterface),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      NULL,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      0,
      0,      /* n_preallocs */
      NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_INTERFACE, "EmpathyLogStore",
        &info, 0);
  }
  return type;
}

const gchar *
empathy_log_store_get_name (EmpathyLogStore *self)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_name)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_name (self);
}

gboolean
empathy_log_store_exists (EmpathyLogStore *self,
                          TpAccount *account,
                          const gchar *chat_id,
                          gboolean chatroom)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->exists)
    return FALSE;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->exists (
      self, account, chat_id, chatroom);
}



gboolean
empathy_log_store_add_message (EmpathyLogStore *self,
                               const gchar *chat_id,
                               gboolean chatroom,
                               EmpathyMessage *message,
                               GError **error)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->add_message)
    return FALSE;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->add_message (
      self, chat_id, chatroom, message, error);
}

GList *
empathy_log_store_get_dates (EmpathyLogStore *self,
                             TpAccount *account,
                             const gchar *chat_id,
                             gboolean chatroom)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_dates)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_dates (
      self, account, chat_id, chatroom);
}

GList *
empathy_log_store_get_messages_for_date (EmpathyLogStore *self,
                                         TpAccount *account,
                                         const gchar *chat_id,
                                         gboolean chatroom,
                                         const gchar *date)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date (
      self, account, chat_id, chatroom, date);
}

GList *
empathy_log_store_get_last_messages (EmpathyLogStore *self,
                                     TpAccount *account,
                                     const gchar *chat_id,
                                     gboolean chatroom)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_last_messages)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_last_messages (
      self, account, chat_id, chatroom);
}

GList *
empathy_log_store_get_chats (EmpathyLogStore *self,
                             TpAccount *account)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_chats)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_chats (self, account);
}

GList *
empathy_log_store_search_new (EmpathyLogStore *self,
                              const gchar *text)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->search_new)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->search_new (self, text);
}

void
empathy_log_store_ack_message (EmpathyLogStore *self,
                               const gchar *chat_id,
                               gboolean chatroom,
                               EmpathyMessage *message)
{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->ack_message)
    return;

  EMPATHY_LOG_STORE_GET_INTERFACE (self)->ack_message (
      self, chat_id, chatroom, message);
}

GList *
empathy_log_store_get_filtered_messages (EmpathyLogStore *self,
                                         TpAccount *account,
                                         const gchar *chat_id,
                                         gboolean chatroom,
                                         guint num_messages,
                                         EmpathyLogMessageFilter filter,
                                         gpointer user_data)

{
  if (!EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages)
    return NULL;

  return EMPATHY_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages (
      self, account, chat_id, chatroom, num_messages, filter, user_data);
}
