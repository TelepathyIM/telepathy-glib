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

#ifndef __TPL_LOG_READER_H__
#define __TPL_LOG_READER_H__


#include <glib-object.h>
//TODO remove it
#include <tpl-log-manager.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_READER		(tpl_log_reader_get_type ())
#define TPL_LOG_READER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TPL_TYPE_LOG_READER, TplLogReader))
#define TPL_LOG_READER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), TPL_TYPE_LOG_READER, TplLogReaderClass))
#define TPL_IS_LOG_READER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TPL_TYPE_LOG_READER))
#define TPL_IS_LOG_READER_CLASS(k) 	(G_TYPE_CHECK_CLASS_TYPE ((k), TPL_TYPE_LOG_READER))
#define TPL_LOG_READER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TPL_TYPE_LOG_READER, TplLogReaderClass))

typedef struct 
{
	GObject parent;

	gpointer priv;
} TplLogReader;

typedef struct 
{
	GObjectClass parent_class;
} TplLogReaderClass;

GType tpl_log_reader_get_type (void);

TplLogReader *tpl_log_reader_dup_singleton (void);

gboolean tpl_log_reader_exists (TplLogReader *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom);

GList *tpl_log_reader_get_dates (TplLogReader *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom);

GList *tpl_log_reader_get_messages_for_date (TplLogReader *manager,
		TpAccount *account, const gchar *chat_id,
		gboolean chatroom, const gchar *date);

GList *tpl_log_reader_get_filtered_messages (TplLogReader *manager,
		TpAccount *account, const gchar *chat_id, gboolean chatroom,
		guint num_messages, TplLogMessageFilter filter,
		gpointer user_data);

GList *tpl_log_reader_get_chats (TplLogReader *manager,
		TpAccount *account);

GList *tpl_log_reader_search_new (TplLogReader *manager,
		const gchar *text);

void tpl_log_reader_search_free (GList *hits);

gchar *tpl_log_reader_get_date_readable (const gchar *date);

void tpl_log_reader_search_hit_free (TplLogSearchHit *hit);

//void tpl_log_reader_observe (TplLogReader *log_reader,
//    EmpathyDispatcher *dispatcher);

G_END_DECLS

#endif /* __TPL_LOG_READER_H__ */
