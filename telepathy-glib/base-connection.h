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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_BASE_CONNECTION_H__
#define __TP_BASE_CONNECTION_H__

#include <glib-object.h>

#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

/* The TpBaseConnection typedef is forward-declared in handle-repo.h */
typedef struct _TpBaseConnectionClass TpBaseConnectionClass;
typedef struct _TpBaseConnectionPrivate TpBaseConnectionPrivate;

typedef void (*TpBaseConnectionProc) (TpBaseConnection *self);

typedef gboolean (*TpBaseConnectionStartConnectingImpl) (
    TpBaseConnection *self, GError **error);

typedef void (*TpBaseConnectionCreateHandleReposImpl) (TpBaseConnection *self,
    TpHandleRepoIface *repos[TP_NUM_ENTITY_TYPES]);

typedef GPtrArray *(*TpBaseConnectionCreateChannelManagersImpl) (
    TpBaseConnection *self);

typedef gchar *(*TpBaseConnectionGetUniqueConnectionNameImpl) (
    TpBaseConnection *self);

struct _TpBaseConnection {
    /*<private>*/
    GDBusObjectSkeleton parent;

    TpBaseConnectionPrivate *priv;
};

struct _TpBaseConnectionClass {
    GDBusObjectSkeletonClass parent_class;

#ifdef __GI_SCANNER__
    /*<private>*/
    GCallback _internal_create_handle_repos;
    /*<public>*/
#else
    TpBaseConnectionCreateHandleReposImpl create_handle_repos;
#endif

    TpBaseConnectionGetUniqueConnectionNameImpl get_unique_connection_name;

    TpBaseConnectionProc connecting;
    TpBaseConnectionProc connected;
    TpBaseConnectionProc disconnected;

    TpBaseConnectionProc shut_down;

    TpBaseConnectionStartConnectingImpl start_connecting;

    TpBaseConnectionCreateChannelManagersImpl create_channel_managers;

    void (*fill_contact_attributes) (TpBaseConnection *self,
        const gchar *dbus_interface,
        TpHandle contact,
        GVariantDict *attributes);

    /*<private>*/
    GCallback _future[16];

    gpointer priv;
};

#   define TP_INTERNAL_CONNECTION_STATUS_NEW ((TpConnectionStatus)(-1))

GType tp_base_connection_get_type (void);

_TP_AVAILABLE_IN_0_20
const gchar *tp_base_connection_get_bus_name (TpBaseConnection *self);

_TP_AVAILABLE_IN_0_20
const gchar *tp_base_connection_get_object_path (TpBaseConnection *self);

_TP_AVAILABLE_IN_0_20
TpConnectionStatus tp_base_connection_get_status (TpBaseConnection *self);

_TP_AVAILABLE_IN_0_20
gboolean tp_base_connection_is_destroyed (TpBaseConnection *self);

_TP_AVAILABLE_IN_0_20
gboolean tp_base_connection_check_connected (TpBaseConnection *self,
    GError **error);

TpHandleRepoIface *tp_base_connection_get_handles (TpBaseConnection *self,
    TpEntityType entity_type);

gboolean tp_base_connection_register (TpBaseConnection *self,
    const gchar *cm_name, gchar **bus_name, gchar **object_path,
    GError **error);

/* FIXME: when dbus-glib exposes its GError -> D-Bus error name mapping,
we could also add:
void tp_base_connection_disconnect_with_error (TpBaseConnection *self,
    const GError *error, GHashTable *details, TpConnectionStatusReason reason);
*/

void tp_base_connection_disconnect_with_dbus_error (
    TpBaseConnection *self,
    const gchar *error_name,
    GVariant *details,
    TpConnectionStatusReason reason);

void tp_base_connection_change_status (TpBaseConnection *self,
    TpConnectionStatus status, TpConnectionStatusReason reason);

TpHandle tp_base_connection_get_self_handle (TpBaseConnection *self);

void tp_base_connection_set_self_handle (TpBaseConnection *self,
    TpHandle self_handle);

void tp_base_connection_finish_shutdown (TpBaseConnection *self);

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

#define TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED(conn, context) \
  G_STMT_START { \
    TpBaseConnection *c_ = (conn); \
    GError *e_ = NULL; \
    \
    if (!tp_base_connection_check_connected (c_, &e_)) \
      { \
        g_dbus_method_invocation_return_gerror ((context), e_); \
        g_error_free (e_); \
        return; \
      } \
  } G_STMT_END

GDBusConnection *tp_base_connection_get_dbus_connection (
    TpBaseConnection *self);

void tp_base_connection_add_client_interest (TpBaseConnection *self,
    const gchar *unique_name, const gchar *token,
    gboolean only_if_uninterested);

void tp_base_connection_add_possible_client_interest (TpBaseConnection *self,
    GQuark token);

_TP_AVAILABLE_IN_0_24
const gchar *tp_base_connection_get_account_path_suffix (
    TpBaseConnection *self);

GVariant *tp_base_connection_dup_contact_attributes (TpBaseConnection *self,
    TpHandleSet *handles,
    const gchar * const *interfaces,
    const gchar * const *assumed_interfaces);

G_END_DECLS

#endif /* #ifndef __TP_BASE_CONNECTION_H__*/
