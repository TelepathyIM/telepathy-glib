/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include <glib.h>
#include <stdio.h>

#include <tpl-log-manager.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/dbus.h>

//static GMainLoop *loop = NULL;

int main(int argc, char *argv[])
{
	TplLogManager *manager;
	//GList *l;
	TpAccount *acc;
	//DBusGConnection *bus;
	TpDBusDaemon *tp_bus;
	GError *err=NULL;
	g_type_init ();

	//tpl_headless_logger_init ();
	
	//bus = tp_get_bus();
	tp_bus = tp_dbus_daemon_dup(&err);
	acc = tp_account_new(tp_bus,
		"/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0",
		&err);

	if(err) {
		g_debug(err->message);
		return 0;
	}

	manager = tpl_log_manager_dup_singleton ();

	tpl_log_manager_search_in_identifier_chats_new(manager, 
		acc, "echo@test.collabora.co.uk", "foo");

	tpl_log_manager_search_new(manager, "foo");



/*
	l = tpl_log_manager_get_chats(manager, acc);
	int lenght = g_list_length(l);
	for(int i=0;i<lenght;++i) {
		TplLogSearchHit *hit = g_list_nth_data(l,i);
		g_debug("%d: %s\n", i, hit->filename);
		GList *gl;

		gl = tpl_log_manager_get_dates (manager, acc, hit->chat_id, hit->is_chatroom);

		for(guint ii=0;ii<g_list_length(gl);++ii) {
			GList *msgs;
			gchar *date = g_list_nth_data(gl, i);
			g_message(date);
			msgs = tpl_log_manager_get_messages_for_date(manager, acc, hit->chat_id,
					hit->is_chatroom, date);
			for(guint m=0;m<g_list_length(msgs);++m) {
				TplLogEntry *log = g_list_nth_data(msgs, m);
				TplLogEntryText *tlog = TPL_LOG_ENTRY_TEXT(tpl_log_entry_get_entry(log));
				
				g_message("BODY: %s\n", tpl_log_entry_text_get_message(tlog));
			}
		}

	}
*/
	//loop = g_main_loop_new (NULL, FALSE);
	//g_main_loop_run (loop);

	return 0;
}
