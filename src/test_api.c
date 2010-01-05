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
//#include <stdio.h>
//#include <dbus/dbus-glib-bindings.h>

//#include <tpl-log-manager.h>
//#include <telepathy-glib/account.h>
#include <telepathy-glib/dbus.h>

#include <tpl-dbus-service-client.h>
#include <tpl-dbus-service.h>
#include <tpl-time.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define CHAT_ID	"echo@test.collabora.co.uk"

static GMainLoop *loop = NULL;

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

	if (!org_freedesktop_Telepathy_TelepathyLoggerService_last_chats_async
			(proxy, ACCOUNT_PATH, CHAT_ID, FALSE, 11, cb, NULL))
	{
		g_warning ("Async Woops remote method failed: %s", error->message);
		g_error_free (error);
		return 1;
	}

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	return 0;
}
