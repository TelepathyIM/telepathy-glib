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
#include <glib/gprintf.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>

#include <tpl-dbus-service.h>
#include <tpl-log-manager.h>

static gboolean
tpl_dbus_service_last_messages (TplDBusService *self,
		gchar const* account_path, gchar const* identifier,
		gboolean is_chatroom, gchar **answer, GError **error);

#include <tpl-dbus-service-server.h>

G_DEFINE_TYPE (TplDBusService, tpl_dbus_service, G_TYPE_OBJECT)


static void
tpl_dbus_service_finalize (GObject *obj)
{
	G_OBJECT_CLASS (tpl_dbus_service_parent_class)->dispose (obj);
}

static void
tpl_dbus_service_dispose (GObject *obj)
{
	//TplDBusService *self = TPL_DBUS_SERVICE(obj);

	g_debug("TplDBusService: disposing\n");

	G_OBJECT_CLASS (tpl_dbus_service_parent_class)->finalize (obj);

	g_debug("TplDBusService: disposed\n");
}


static void
tpl_dbus_service_class_init(TplDBusServiceClass* klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = tpl_dbus_service_finalize;
	object_class->dispose = tpl_dbus_service_dispose;

	dbus_g_object_type_install_info (TPL_TYPE_DBUS_SERVICE,
			&dbus_glib_tpl_dbus_service_object_info);

}

static void
tpl_dbus_service_init(TplDBusService* self)
{
	self->manager = tpl_log_manager_dup_singleton ();
}


TplDBusService *tpl_dbus_service_new (void)
{
	return g_object_new(TPL_TYPE_DBUS_SERVICE, NULL);
}


static gboolean
tpl_dbus_service_last_messages (TplDBusService *self,
		gchar const* account_path, gchar const* identifier,
		gboolean is_chatroom, gchar **answer, GError **error)
{
	TpAccount *account;
	DBusGConnection *dbus;
	TpDBusDaemon *tp_dbus;
	GList *ret=NULL;
	guint lines = 0;

	g_message("TPL DBUS A: %s %s\n", account_path, identifier);

	dbus = tp_get_bus();
        tp_dbus = tp_dbus_daemon_new(dbus);

	account = tp_account_new(tp_dbus, account_path, error);
	if (!account) {
		g_error("during TpAccount creation: %s\n",
				(*error)->message);
		g_object_unref(tp_dbus);
		return FALSE;
	}


	GList *dates = tpl_log_manager_get_dates(self->manager, account, identifier, is_chatroom);
	if(!dates) {
		g_set_error(error,
				TPL_DBUS_SERVICE_ERROR,
				TPL_DBUS_SERVICE_ERROR_GENERIC,
				"Error during date list retrieving");
		return FALSE;
	}

	for(guint i=g_list_length(dates); i>0 && lines<=5; --i) {
		gchar *date = g_list_nth_data(dates, i-1);
		g_message("%d: %s\n", i, date);
		GList *messages = tpl_log_manager_get_messages_for_date(self->manager,
				account, identifier, is_chatroom, date);
		gint msgs_len = g_list_length(messages);
		// get the last 5 messages or less if lentgh<5
		gint msg_guard = msgs_len>=6 ? 6 : 0;
		for(gint m=msgs_len-1; m>=msg_guard && lines<=5; --m) {
			TplLogEntry *entry = g_list_nth_data(messages, m);
			g_message("CONSIDERING: %s\n", tpl_log_entry_text_get_message(entry->entry.text));
			ret = g_list_append(ret, entry);
			lines+=1;
		}
	}

	for(guint i=0;i<g_list_length(ret);++i) {
		TplLogEntry *entry = g_list_nth_data (ret, i);
		g_message("RET %d: %s\n", i, tpl_log_entry_text_get_message( entry->entry.text));
	}
	*answer = g_strdup_printf("%s %s %s %s %s",
			tpl_log_entry_text_get_message(((TplLogEntry*) g_list_nth_data(ret,0))->entry.text ),
			tpl_log_entry_text_get_message(((TplLogEntry*) g_list_nth_data(ret,1))->entry.text ),
			tpl_log_entry_text_get_message(((TplLogEntry*) g_list_nth_data(ret,2))->entry.text ),
			tpl_log_entry_text_get_message(((TplLogEntry*) g_list_nth_data(ret,3))->entry.text ),
			tpl_log_entry_text_get_message(((TplLogEntry*) g_list_nth_data(ret,4))->entry.text )
			);
	return TRUE;
}

GQuark
tpl_dbus_service_error_quark (void)
{
  return g_quark_from_static_string ("tpl-dbus-service-error-quark");
}
