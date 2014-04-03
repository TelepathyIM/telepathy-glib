/*
 * dbus.h - Header for D-Bus utilities
 *
 * Copyright (C) 2005-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2009 Nokia Corporation
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
 */

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TELEPATHY_DBUS_H__
#define __TELEPATHY_DBUS_H__
#define __TP_IN_DBUS_H__

#include <gio/gio.h>

#include <telepathy-glib/defs.h>

#include <telepathy-glib/_gen/genums.h>

G_BEGIN_DECLS

void tp_dbus_g_method_return_not_implemented (GDBusMethodInvocation *context);

typedef enum /*< flags >*/
{
  TP_DBUS_NAME_TYPE_UNIQUE = 1,
  TP_DBUS_NAME_TYPE_WELL_KNOWN = 2,
  TP_DBUS_NAME_TYPE_BUS_DAEMON = 4,
  TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON = TP_DBUS_NAME_TYPE_UNIQUE | TP_DBUS_NAME_TYPE_WELL_KNOWN,
  TP_DBUS_NAME_TYPE_ANY = TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON | TP_DBUS_NAME_TYPE_BUS_DAEMON
} TpDBusNameType;

gboolean tp_dbus_check_valid_bus_name (const gchar *name,
    TpDBusNameType allow_types, GError **error);

gboolean tp_dbus_check_valid_interface_name (const gchar *name,
    GError **error);

gboolean tp_dbus_check_valid_member_name (const gchar *name,
    GError **error);

gboolean tp_dbus_check_valid_object_path (const gchar *path,
    GError **error);

gboolean tp_dbus_connection_request_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name, gboolean idempotent, GError **error);
gboolean tp_dbus_connection_release_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name, GError **error);

void tp_dbus_connection_register_object (GDBusConnection *dbus_connection,
    const gchar *object_path, gpointer object);
gboolean tp_dbus_connection_try_register_object (
    GDBusConnection *dbus_connection,
    const gchar *object_path,
    gpointer object,
    GError **error);
void tp_dbus_connection_unregister_object (GDBusConnection *dbus_connection,
    gpointer object);

G_END_DECLS

#undef __TP_IN_DBUS_H__
#endif /* __TELEPATHY_DBUS_H__ */
