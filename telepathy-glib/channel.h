/*
 * channel.h - proxy for a Telepathy channel
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#ifndef __TP_CHANNEL_H__
#define __TP_CHANNEL_H__

#include <telepathy-glib/connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpChannel TpChannel;
typedef struct _TpChannelPrivate TpChannelPrivate;
typedef struct _TpChannelClass TpChannelClass;

struct _TpChannelClass {
    TpProxyClass parent_class;
    /*<private>*/
    GCallback _1;
    GCallback _2;
    GCallback _3;
    GCallback _4;
};

struct _TpChannel {
    /*<private>*/
    TpProxy parent;

    TpChannelPrivate *priv;
};

GType tp_channel_get_type (void);

#define TP_ERRORS_REMOVED_FROM_GROUP (tp_errors_removed_from_group_quark ())
GQuark tp_errors_removed_from_group_quark (void);

/* TYPE MACROS */
#define TP_TYPE_CHANNEL \
  (tp_channel_get_type ())
#define TP_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_CHANNEL, \
                              TpChannel))
#define TP_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_CHANNEL, \
                           TpChannelClass))
#define TP_IS_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CHANNEL))
#define TP_IS_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CHANNEL))
#define TP_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CHANNEL, \
                              TpChannelClass))

_TP_DEPRECATED_IN_0_20_FOR(tp_simple_client_factory_ensure_channel)
TpChannel *tp_channel_new (TpConnection *conn,
    const gchar *object_path, const gchar *optional_channel_type,
    TpHandleType optional_handle_type, TpHandle optional_handle,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

_TP_DEPRECATED_IN_0_20_FOR(tp_simple_client_factory_ensure_channel)
TpChannel *tp_channel_new_from_properties (TpConnection *conn,
    const gchar *object_path, const GHashTable *immutable_properties,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

void tp_channel_init_known_interfaces (void);

TpConnection *tp_channel_borrow_connection (TpChannel *self);
GHashTable *tp_channel_borrow_immutable_properties (TpChannel *self);

void tp_channel_leave_async (TpChannel *self,
    TpChannelGroupChangeReason reason,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_channel_leave_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error);

void tp_channel_close_async (TpChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_channel_close_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error);

_TP_AVAILABLE_IN_0_16
void tp_channel_destroy_async (TpChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_0_16
gboolean tp_channel_destroy_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error);

gboolean tp_channel_get_requested (TpChannel *self);

#define TP_CHANNEL_FEATURE_CORE \
  tp_channel_get_feature_quark_core ()

GQuark tp_channel_get_feature_quark_core (void) G_GNUC_CONST;

const gchar *tp_channel_get_channel_type (TpChannel *self);
GQuark tp_channel_get_channel_type_id (TpChannel *self);
TpHandle tp_channel_get_handle (TpChannel *self, TpHandleType *handle_type);
const gchar *tp_channel_get_identifier (TpChannel *self);

_TP_AVAILABLE_IN_0_16
TpContact *tp_channel_get_target_contact (TpChannel *self);
_TP_AVAILABLE_IN_0_16
TpContact *tp_channel_get_initiator_contact (TpChannel *self);

#define TP_CHANNEL_FEATURE_GROUP \
  tp_channel_get_feature_quark_group ()
_TP_AVAILABLE_IN_UNRELEASED
GQuark tp_channel_get_feature_quark_group (void) G_GNUC_CONST;

_TP_AVAILABLE_IN_UNRELEASED
TpChannelGroupFlags tp_channel_group_get_flags (TpChannel *self);

_TP_AVAILABLE_IN_UNRELEASED
TpContact *tp_channel_group_get_self_contact (TpChannel *self);
_TP_AVAILABLE_IN_UNRELEASED
GPtrArray *tp_channel_group_dup_members (TpChannel *self);
_TP_AVAILABLE_IN_UNRELEASED
GPtrArray *tp_channel_group_dup_local_pending (TpChannel *self);
_TP_AVAILABLE_IN_UNRELEASED
GPtrArray *tp_channel_group_dup_remote_pending (TpChannel *self);
_TP_AVAILABLE_IN_UNRELEASED
gboolean tp_channel_group_get_local_pending_info (TpChannel *self,
    TpContact *local_pending, TpContact **actor,
    TpChannelGroupChangeReason *reason, const gchar **message);
_TP_AVAILABLE_IN_UNRELEASED
TpContact *tp_channel_group_get_contact_owner (TpChannel *self,
    TpContact *contact);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_join_async (TpChannel *self,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_UNRELEASED
gboolean tp_channel_join_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error);

/* Channel.Interface.Password */
#define TP_CHANNEL_FEATURE_PASSWORD \
  tp_channel_get_feature_quark_password ()
_TP_AVAILABLE_IN_0_16
GQuark tp_channel_get_feature_quark_password (void) G_GNUC_CONST;

_TP_AVAILABLE_IN_0_16
gboolean tp_channel_password_needed (TpChannel *self);

_TP_AVAILABLE_IN_0_16
void tp_channel_provide_password_async (TpChannel *self,
    const gchar *password,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_0_16
gboolean tp_channel_provide_password_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
