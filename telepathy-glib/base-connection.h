/*
 * base-connection.h - Header for TpBaseConnection
 *
 * Copyright (C) 2007-2008 Collabora Ltd.
 * Copyright (C) 2007-2008 Nokia Corporation
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

#ifndef __TP_BASE_CONNECTION_H__
#define __TP_BASE_CONNECTION_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/svc-connection.h>

G_BEGIN_DECLS

typedef struct _TpBaseConnection TpBaseConnection;
typedef struct _TpBaseConnectionClass TpBaseConnectionClass;
typedef struct _TpBaseConnectionPrivate TpBaseConnectionPrivate;

typedef void (*TpBaseConnectionProc) (TpBaseConnection *self);

typedef gboolean (*TpBaseConnectionStartConnectingImpl) (
    TpBaseConnection *self, GError **error);

typedef void (*TpBaseConnectionCreateHandleReposImpl) (TpBaseConnection *self,
    TpHandleRepoIface *repos[NUM_TP_HANDLE_TYPES]);


typedef GPtrArray *(*TpBaseConnectionCreateChannelFactoriesImpl) (
    TpBaseConnection *self);

typedef GPtrArray *(*TpBaseConnectionCreateChannelManagersImpl) (
    TpBaseConnection *self);

typedef gchar *(*TpBaseConnectionGetUniqueConnectionNameImpl) (
    TpBaseConnection *self);

struct _TpBaseConnectionClass {
    GObjectClass parent_class;

    TpBaseConnectionCreateHandleReposImpl create_handle_repos;

    TpBaseConnectionCreateChannelFactoriesImpl create_channel_factories;

    TpBaseConnectionGetUniqueConnectionNameImpl get_unique_connection_name;

    TpBaseConnectionProc connecting;
    TpBaseConnectionProc connected;
    TpBaseConnectionProc disconnected;

    TpBaseConnectionProc shut_down;

    TpBaseConnectionStartConnectingImpl start_connecting;

    const gchar **interfaces_always_present;

    TpBaseConnectionCreateChannelManagersImpl create_channel_managers;

    /*<private>*/
    gpointer _future2;
    gpointer _future3;
    gpointer _future4;

    gpointer priv;
};

#   define TP_INTERNAL_CONNECTION_STATUS_NEW ((TpConnectionStatus)(-1))

struct _TpBaseConnection {
    /*<public>*/
    GObject parent;

    gchar *bus_name;
    gchar *object_path;

    TpConnectionStatus status;

    TpHandle self_handle;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
    gpointer _future3;
    gpointer _future4;

    TpBaseConnectionPrivate *priv;
};

GType tp_base_connection_get_type (void);

TpHandleRepoIface *tp_base_connection_get_handles (TpBaseConnection *self,
    TpHandleType handle_type);

gboolean tp_base_connection_register (TpBaseConnection *self,
    const gchar *cm_name, gchar **bus_name, gchar **object_path,
    GError **error);

/* FIXME: when dbus-glib exposes its GError -> D-Bus error name mapping,
we could also add:
void tp_base_connection_disconnect_with_error (TpBaseConnection *self,
    const GError *error, GHashTable *details, TpConnectionStatusReason reason);
*/

void tp_base_connection_disconnect_with_dbus_error (TpBaseConnection *self,
    const gchar *error_name, GHashTable *details,
    TpConnectionStatusReason reason);

void tp_base_connection_change_status (TpBaseConnection *self,
    TpConnectionStatus status, TpConnectionStatusReason reason);

TpHandle tp_base_connection_get_self_handle (TpBaseConnection *self);

void tp_base_connection_set_self_handle (TpBaseConnection *self,
    TpHandle self_handle);

void tp_base_connection_finish_shutdown (TpBaseConnection *self);

void tp_base_connection_add_interfaces (TpBaseConnection *self,
    const gchar **interfaces);

void tp_base_connection_dbus_request_handles (TpSvcConnection *iface,
    guint handle_type, const gchar **names, DBusGMethodInvocation *context);

void tp_base_connection_register_with_contacts_mixin (TpBaseConnection *self);


typedef struct _TpChannelManagerIter TpChannelManagerIter;

struct _TpChannelManagerIter {
    /*<private>*/
    TpBaseConnection *self;
    guint index;
    gpointer _future[2];
};

void tp_base_connection_channel_manager_iter_init (TpChannelManagerIter *iter,
    TpBaseConnection *self);

gboolean tp_base_connection_channel_manager_iter_next (
    TpChannelManagerIter *iter, TpChannelManager **manager_out);


/* TYPE MACROS */
#define TP_TYPE_BASE_CONNECTION \
  (tp_base_connection_get_type ())
#define TP_BASE_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_BASE_CONNECTION, \
                              TpBaseConnection))
#define TP_BASE_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_BASE_CONNECTION, \
                           TpBaseConnectionClass))
#define TP_IS_BASE_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_BASE_CONNECTION))
#define TP_IS_BASE_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_BASE_CONNECTION))
#define TP_BASE_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_CONNECTION, \
                              TpBaseConnectionClass))

/* The cast of a string literal to (gchar *) is to keep C++ compilers happy */
#define TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED(conn, context) \
  G_STMT_START { \
    TpBaseConnection *c = (conn); \
    \
    if (c->status != TP_CONNECTION_STATUS_CONNECTED) \
      { \
        GError e = { TP_ERRORS, TP_ERROR_DISCONNECTED, \
            (gchar *) "Connection is disconnected" }; \
        \
        dbus_g_method_return_error ((context), &e); \
        return; \
      } \
  } G_STMT_END

G_END_DECLS

#endif /* #ifndef __TP_BASE_CONNECTION_H__*/
