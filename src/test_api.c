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


static GMainLoop *loop = NULL;

static void cb (DBusGProxy *proxy, char *OUT_str_ret, GError *error, gpointer userdata) {
	if(error!=NULL) {
		g_error("ERROR: %s\n", error->message);
		return;
	}
	g_message("answer it: %s\n", OUT_str_ret);
}

int main(int argc, char *argv[])
{
	DBusGProxy *proxy;
	gchar *result;
	DBusGConnection *connection;
	GError *error=NULL;

	g_type_init ();
	//connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	connection = tp_get_bus();
	proxy = dbus_g_proxy_new_for_name (connection,
			TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME,
			TPL_DBUS_SRV_OBJECT_PATH,
			TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME);


	if (!org_freedesktop_Telepathy_TelepathyLoggerService_last_messages
			(proxy, "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0", "echo@test.collabora.co.uk", FALSE, &result, &error))
	{
		g_warning ("Woops remote method failed: %s", error->message);
		g_error_free (error);
		return 1;
	}
	g_message("RESULT: %s\n", result);

	if (!org_freedesktop_Telepathy_TelepathyLoggerService_last_messages_async
			(proxy, "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0", "echofoo", FALSE, cb, NULL))
	{
		g_warning ("Async Woops remote method failed: %s", error->message);
		g_error_free (error);
		return 1;
	}
//	g_object_unref (proxy);


	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

	return 0;
}
