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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>,
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "log-store.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug.h>

/**
 * SECTION:log-store
 * @title: TplLogStore
 * @short_description: LogStore interface can register into #TplLogManager as
 * #TplLogStore:writable or #TplLogStore:readable log stores.
 * @see_also: #log-entry-text:TplLogEntryText and other subclasses when they'll exist
 *
 * The #TplLogStore defines all the public methods that a TPL Log Store has to
 * implement in order to be used into a #TplLogManager.
 */

/**
 * TplLogStore:writable:
 *
 * Defines wether the object is writable for a #TplLogManager.
 *
 * If an TplLogStore implementation is writable, the #TplLogManager will call
 * it's tpl_log_store_add_message() method every time a loggable even occurs,
 * i.e., everytime tpl_log_manager_add_message() is called.
 */

/**
 * TplLogStore:readable:
 *
 * Defines wether the object is readable for a #TplLogManager.
 *
 * If an TplLogStore implementation is readable, the #TplLogManager will
 * use the query methods against the instance (i.e., tpl_log_store_get_dates())
 * every time a #TplLogManager instance is queried (i.e.,
 * tpl_log_manager_get_date()).
 */


GType
tpl_log_store_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      static const GTypeInfo info = {
          sizeof (TplLogStoreInterface),
          NULL, /* base_init */
          NULL, /* base_finalize */
          NULL, /* class_init */
          NULL, /* class_finalize */
          NULL, /* class_data */
          0,
          0,    /* n_preallocs */
          NULL  /* instance_init */
      };
      type = g_type_register_static (G_TYPE_INTERFACE, "TplLogStore",
          &info, 0);
    }
  return type;
}


const gchar *
tpl_log_store_get_name (TplLogStore *self)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_name)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_name (self);
}


gboolean
tpl_log_store_exists (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->exists)
    return FALSE;

  return TPL_LOG_STORE_GET_INTERFACE (self)->exists (self, account, chat_id,
      chatroom);
}


gboolean
tpl_log_store_add_message (TplLogStore *self,
    TplLogEntry *message,
    GError **error)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->add_message)
    {
      g_warning ("LogStore: add_message not implemented");
      return FALSE;
    }

  return TPL_LOG_STORE_GET_INTERFACE (self)->add_message (self, message,
      error);
}


GList *
tpl_log_store_get_dates (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_dates)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_dates (self, account,
      chat_id, chatroom);
}


GList *
tpl_log_store_get_messages_for_date (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const gchar *date)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date (self,
      account, chat_id, chatroom, date);
}


GList *
tpl_log_store_get_recent_messages (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_recent_messages)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_recent_messages (self, account,
      chat_id, chatroom);
}


GList *
tpl_log_store_get_chats (TplLogStore *self,
    TpAccount *account)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_chats)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_chats (self, account);
}



GList *
tpl_log_store_search_in_identifier_chats_new (TplLogStore *self,
    TpAccount *account,
    gchar const *identifier,
    const gchar *text)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->search_new)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->search_in_identifier_chats_new (self,
      account, identifier, text);
}


GList *
tpl_log_store_search_new (TplLogStore *self,
    const gchar *text)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->search_new)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->search_new (self, text);
}


GList *
tpl_log_store_get_filtered_messages (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    guint num_messages,
    TplLogMessageFilter filter,
    gpointer user_data)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages (self,
      account, chat_id, chatroom, num_messages, filter, user_data);
}


gboolean
tpl_log_store_is_writable (TplLogStore *self)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->is_writable)
    return FALSE;

  return TPL_LOG_STORE_GET_INTERFACE (self)->is_writable (self);
}


gboolean
tpl_log_store_is_readable (TplLogStore *self)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->is_readable)
    return FALSE;

  return TPL_LOG_STORE_GET_INTERFACE (self)->is_readable (self);
}


