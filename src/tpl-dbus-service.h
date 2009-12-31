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

#ifndef __TPL_DBUS_SERVICE_H__
#define __TPL_DBUS_SERVICE_H__

#define TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME \
  "org.freedesktop.Telepathy.TelepathyLoggerService"
#define TPL_DBUS_SRV_OBJECT_PATH \
  "/org/freedesktop/Telepathy/TelepathyLoggerService"

#include <glib-object.h>
#include <tpl-log-manager.h>

G_BEGIN_DECLS

#define TPL_TYPE_DBUS_SERVICE                  (tpl_dbus_service_get_type ())
#define TPL_DBUS_SERVICE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_DBUS_SERVICE, TplDBusService))
#define TPL_DBUS_SERVICE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_DBUS_SERVICE, TplDBusServiceClass))
#define TPL_IS_DBUS_SERVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_DBUS_SERVICE))
#define TPL_IS_DBUS_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_DBUS_SERVICE))
#define TPL_DBUS_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_DBUS_SERVICE, TplDBusServiceClass))

#define TPL_DBUS_SERVICE_ERROR tpl_dbus_service_error_quark ()
GQuark
tpl_dbus_service_error_quark (void);

typedef enum {
	TPL_DBUS_SERVICE_ERROR_GENERIC
} TplDBusServiceErrorCode;

typedef struct {
	GObject parent;

	/* Private */
	TplLogManager *manager;
} TplDBusService;

typedef struct {
	GObjectClass	parent_class;
} TplDBusServiceClass;

GType tpl_dbus_service_get_type (void);

TplDBusService *tpl_dbus_service_new (void);


G_END_DECLS

#endif // __TPL_DBUS_SERVICE_H__
