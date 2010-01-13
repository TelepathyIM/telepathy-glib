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

#include "log-store.h"

GType
tpl_log_store_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      static const GTypeInfo info = {
	sizeof (TplLogStoreInterface),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	NULL,			/* class_init */
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	0,
	0,			/* n_preallocs */
	NULL			/* instance_init */
      };
      type = g_type_register_static (G_TYPE_INTERFACE, "TplLogStore",
				     &info, 0);
    }
  return type;
}

const gchar *
tpl_log_store_get_name (TplLogStore * self)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_name)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_name (self);
}

gboolean
tpl_log_store_exists (TplLogStore * self,
		      TpAccount * account,
		      const gchar * chat_id, gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->exists)
    return FALSE;

  return TPL_LOG_STORE_GET_INTERFACE (self)->exists (self, account, chat_id,
						     chatroom);
}



gboolean
tpl_log_store_add_message (TplLogStore * self,
			   const gchar * chat_id,
			   gboolean chatroom,
			   TplLogEntry * message, GError ** error)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->add_message)
    {
      g_warning ("LogStore: add_message not implemented");
      return FALSE;
    }

  return TPL_LOG_STORE_GET_INTERFACE (self)->add_message (self, chat_id,
							  chatroom, message,
							  error);
}

GList *
tpl_log_store_get_dates (TplLogStore * self,
			 TpAccount * account,
			 const gchar * chat_id, gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_dates)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_dates (self, account,
							chat_id, chatroom);
}

GList *
tpl_log_store_get_messages_for_date (TplLogStore * self,
				     TpAccount * account,
				     const gchar * chat_id,
				     gboolean chatroom, const gchar * date)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_messages_for_date (self,
								    account,
								    chat_id,
								    chatroom,
								    date);
}

GList *
tpl_log_store_get_last_messages (TplLogStore * self,
				 TpAccount * account,
				 const gchar * chat_id, gboolean chatroom)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_last_messages)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_last_messages (self, account,
								chat_id,
								chatroom);
}

GList *
tpl_log_store_get_chats (TplLogStore * self, TpAccount * account)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_chats)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_chats (self, account);
}



GList *
tpl_log_store_search_in_identifier_chats_new (TplLogStore * self,
					      TpAccount * account,
					      gchar const *identifier,
					      const gchar * text)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->search_new)
    return NULL;

  return
    TPL_LOG_STORE_GET_INTERFACE (self)->search_in_identifier_chats_new (self,
									account,
									identifier,
									text);
}


GList *
tpl_log_store_search_new (TplLogStore * self, const gchar * text)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->search_new)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->search_new (self, text);
}

void
tpl_log_store_ack_message (TplLogStore * self,
			   const gchar * chat_id,
			   gboolean chatroom, TplLogEntry * message)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->ack_message)
    return;

  TPL_LOG_STORE_GET_INTERFACE (self)->ack_message (self, chat_id, chatroom,
						   message);
}

GList *
tpl_log_store_get_filtered_messages (TplLogStore * self,
				     TpAccount * account,
				     const gchar * chat_id,
				     gboolean chatroom,
				     guint num_messages,
				     TplLogMessageFilter filter,
				     gpointer user_data)
{
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_messages (self,
								    account,
								    chat_id,
								    chatroom,
								    num_messages,
								    filter,
								    user_data);
}
