/*
 * dbus.h - Header for D-Bus helper functions for telepathy implementation
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#ifndef __TELEPATHY_DBUS_H__
#define __TELEPATHY_DBUS_H__

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

void tp_dbus_g_method_return_not_implemented (DBusGMethodInvocation *context);
DBusGConnection * tp_get_bus (void);
DBusGProxy * tp_get_bus_proxy (void);

typedef struct _TpDBusDaemonClass TpDBusDaemonClass;
GType tp_dbus_daemon_get_type (void);

#define TP_TYPE_DBUS_DAEMON \
  (tp_dbus_daemon_get_type ())
#define TP_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemon))
#define TP_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_DBUS_DAEMON, \
                           TpDBusDaemonClass))
#define TP_IS_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_DBUS_DAEMON))
#define TP_IS_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_DBUS_DAEMON))
#define TP_DBUS_DAEMON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemonClass))

TpDBusDaemon *tp_dbus_daemon_new (DBusGConnection *connection);

typedef void (*TpDBusDaemonNameOwnerChangedCb) (TpDBusDaemon *daemon,
    const gchar *name, const gchar *new_owner, gpointer user_data);

void tp_dbus_daemon_watch_name_owner (TpDBusDaemon *self,
    const gchar *name, TpDBusDaemonNameOwnerChangedCb callback,
    gpointer user_data, GDestroyNotify destroy);

gboolean tp_dbus_daemon_cancel_name_owner_watch (TpDBusDaemon *self,
    const gchar *name, TpDBusDaemonNameOwnerChangedCb callback,
    gconstpointer user_data);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-dbus-daemon.h>

#endif /* __TELEPATHY_DBUS_H__ */

