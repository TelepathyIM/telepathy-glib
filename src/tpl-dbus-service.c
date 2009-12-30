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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>

#include <tpl-dbus-service.h>

static gboolean
tpl_dbus_service_last_messages (TplDBusService *self,
		gchar const* account_path, gchar const* identifier,
		gboolean is_chatroom, gint lines, gchar **str_ret);

#include <tpl-dbus-service-server.h>

G_DEFINE_TYPE (TplDBusService, tpl_dbus_service, G_TYPE_OBJECT)


static gboolean
tpl_dbus_service_last_messages (TplDBusService *self,
		gchar const* account_path, gchar const* identifier,
		gboolean is_chatroom, gint lines, gchar **str_ret)
{
	TpAccount *account;
	DBusGConnection *dbus;
	TpDBusDaemon *tp_dbus;
	GError *error=NULL;

	g_message("TPL DBUS A: %s %s\n", account_path, identifier);

	dbus = tp_get_bus();
        tp_dbus = tp_dbus_daemon_new(dbus);

	account = tp_account_new(tp_dbus, account_path, &error);
	if (error!=NULL) {
		g_error("during TpAccount creation: %s\n",
				error->message);
		g_object_unref(tp_dbus);
		return FALSE;
	}

	str_ret = NULL;
	return TRUE;
}




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
}


TplDBusService *tpl_dbus_service_new (void)
{
	return g_object_new(TPL_TYPE_DBUS_SERVICE, NULL);
}


