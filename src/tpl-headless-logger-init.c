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


#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/channel-dispatch-operation.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>

#include <tpl-observer.h>
#include <tpl-log-store-empathy.h>

/* 
 * Initialization of TPL (TelePathy Logger), it futurely set all the
 * inernal structs. tpl_headless_logger_deinit will free/unref them
 *
 * TplObserver *observer
 */
void tpl_headless_logger_init(void)
{
	TplObserver *observer;
	DBusGConnection *bus;
	TpDBusDaemon *tp_bus;
	GError *error = NULL;

	bus = tp_get_bus();
	tp_bus = tp_dbus_daemon_new(bus);
	
	if ( tp_dbus_daemon_request_name (tp_bus, TPL_OBSERVER_WELL_KNOWN_BUS_NAME,
			TRUE, &error) ) {
		g_print("Well Known name requested successfully!\n");
	} else {
		g_print("Well Known name request error: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}


	observer = tpl_observer_new ();
	dbus_g_connection_register_g_object (bus,
			TPL_OBSERVER_OBJECT_PATH,
			G_OBJECT(observer));
}
