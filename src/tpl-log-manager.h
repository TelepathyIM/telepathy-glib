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


#include <glib-object.h>

#include <telepathy-glib/account.h>

#include <tpl-log-entry.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_MANAGER		(tpl_log_manager_get_type ())
#define TPL_LOG_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TPL_TYPE_LOG_MANAGER, TplLogManager))
#define TPL_LOG_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))
#define TPL_IS_LOG_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TPL_TYPE_LOG_MANAGER))
#define TPL_IS_LOG_MANAGER_CLASS(k) 	(G_TYPE_CHECK_CLASS_TYPE ((k), TPL_TYPE_LOG_MANAGER))
#define TPL_LOG_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))

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
	gchar     *chat_id;
	gboolean   is_chatroom;
	gchar     *filename;
	gchar     *date;
} TplLogSearchHit;

typedef gboolean (*TplLogMessageFilter) (TplLogEntry *message,
		gpointer user_data);

GType tpl_log_manager_get_type (void);

TplLogManager *tpl_log_manager_dup_singleton (void);

gboolean tpl_log_manager_exists (TplLogManager *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom);

GList *tpl_log_manager_get_dates (TplLogManager *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom);

GList *tpl_log_manager_get_messages_for_date (TplLogManager *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom, const gchar *date);

GList *tpl_log_manager_get_filtered_messages (TplLogManager *manager,
		TpAccount *account, const gchar *chat_id, gboolean chatroom,
		guint num_messages, TplLogMessageFilter filter,
		gpointer user_data);

GList *tpl_log_manager_get_chats (TplLogManager *manager,
		TpAccount *account);

GList *tpl_log_manager_search_in_identifier_chats_new(
		TplLogManager *manager, TpAccount *account,
		gchar const* identifier, const gchar *text);

GList *tpl_log_manager_search_new (TplLogManager *manager,
		const gchar *text);

void tpl_log_manager_search_free (GList *hits);

gchar *tpl_log_manager_get_date_readable (const gchar *date);

void tpl_log_manager_search_hit_free (TplLogSearchHit *hit);

G_END_DECLS

#endif /* __TPL_LOG_MANAGER_H__ */
