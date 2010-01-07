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
#include <gconf/gconf-client.h>
//#include <stdio.h>
//#include <dbus/dbus-glib-bindings.h>

//#include <telepathy-glib/account.h>
#include <telepathy-glib/dbus.h>

//#include <log-manager.h>
#include <dbus-service-client.h>
#include <dbus-service.h>
#include <datetime.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define CHAT_ID	"echo@test.collabora.co.uk"

#define GCONF_LIST "/apps/telepathy-logger/disabling/accounts/blocklist"


static GMainLoop *loop = NULL;
/*
static void
cb (DBusGProxy *proxy, GPtrArray *retval, GError *error,
    gpointer userdata)
{
	if(error!=NULL) {
		g_error("ERROR: %s\n", error->message);
		return;
	}
	for(guint i=0; i<retval->len; ++i) {
		GValueArray *values = g_ptr_array_index(retval, i);
		GValue *sender = g_value_array_get_nth(values, 0);
		GValue *message = g_value_array_get_nth(values, 1);
		GValue *timestamp = g_value_array_get_nth(values, 2);
		g_message("[%s] <%s> %s\n",
				tpl_time_to_string_local(g_value_get_uint(timestamp), "%Y-%m-%d %H:%M.%S"),
				g_value_get_string(sender),
				g_value_get_string(message));
	}
}
*/
int main(int argc, char *argv[])
{
	DBusGProxy *proxy;
//	gchar *result;
	DBusGConnection *connection;
	GError *error=NULL;

	g_type_init ();
	connection = tp_get_bus();
	proxy = dbus_g_proxy_new_for_name (connection,
			TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME,
			TPL_DBUS_SRV_OBJECT_PATH,
			TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME);
/*
	if (!org_freedesktop_Telepathy_TelepathyLoggerService_last_chats_async
			(proxy, ACCOUNT_PATH, CHAT_ID, FALSE, 11, cb, NULL))
	{
		g_warning ("Async Woops remote method failed: %s", error->message);
		g_clear_error (&error);
		g_error_free (error);
		return 1;
	}
*/
	GConfClient *client = gconf_client_get_default();
	GConfValue *val;
	GSList *lst=NULL;

	val = gconf_client_get (client, GCONF_LIST, &error);
	if (val==NULL) {
		g_message("NULL LIST\n");
	}
	else {
		lst = gconf_value_get_list(val);
		for(guint i=0; i<g_slist_length(lst);++i) {
			g_message("pre GLIST %d: %s\n",i, (gchar*)g_slist_nth_data(lst, i));
		}
	}

	lst = g_slist_prepend(lst, gconf_value_new_from_string(
		GCONF_VALUE_STRING, "FOO", NULL));

	val = gconf_value_new(GCONF_VALUE_LIST);
	gconf_value_set_list_type(val, GCONF_VALUE_STRING);
	gconf_value_set_list(val, lst);
	gconf_client_set(client, GCONF_LIST,
		val, NULL);

	val = gconf_client_get (client, GCONF_LIST, &error);
	lst = gconf_value_get_list(val);
	if (lst==NULL) g_message("NULL LIST\n");
	else {
		for(guint i=0; i<g_slist_length(lst);++i) {
			g_message("post GLIST %d: %s\n", i, (char*)g_slist_nth_data(lst, i));
		}
	}

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	return 0;
}
