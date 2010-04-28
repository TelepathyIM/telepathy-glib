/*
 * connection.h - proxy for a Telepathy connection
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

#ifndef __TP_CONNECTION_H__
#define __TP_CONNECTION_H__

#include <telepathy-glib/capabilities.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpConnection TpConnection;
typedef struct _TpConnectionPrivate TpConnectionPrivate;
typedef struct _TpConnectionClass TpConnectionClass;

struct _TpConnectionClass {
    TpProxyClass parent_class;
    /*<private>*/
    GCallback _1;
    GCallback _2;
    GCallback _3;
    GCallback _4;
};

struct _TpConnection {
    /*<private>*/
    TpProxy parent;
    TpConnectionPrivate *priv;
};

typedef struct _TpAvatarRequirements TpAvatarRequirements;
struct _TpAvatarRequirements
{
  gchar **supported_mime_types;
  guint minimum_width;
  guint minimum_height;
  guint recommended_width;
  guint recommended_height;
  guint maximum_width;
  guint maximum_height;
  guint maximum_bytes;

  /*<private>*/
  gpointer _1;
  gpointer _2;
  gpointer _3;
  gpointer _4;
};

#define TP_TYPE_AVATAR_REQUIREMENTS (tp_avatar_requirements_get_type ())
GType tp_avatar_requirements_get_type (void);

TpAvatarRequirements * tp_avatar_requirements_new (
    GStrv supported_mime_types,
    guint minimum_width,
    guint minimum_height,
    guint recommended_width,
    guint recommended_height,
    guint maximum_width,
    guint maximum_height,
    guint maximum_bytes);

TpAvatarRequirements * tp_avatar_requirements_copy (
    TpAvatarRequirements *self);

void tp_avatar_requirements_destroy (TpAvatarRequirements *self);


GType tp_connection_get_type (void);

#define TP_ERRORS_DISCONNECTED (tp_errors_disconnected_quark ())
GQuark tp_errors_disconnected_quark (void);

#define TP_UNKNOWN_CONNECTION_STATUS ((TpConnectionStatus) -1)

/* TYPE MACROS */
#define TP_TYPE_CONNECTION \
  (tp_connection_get_type ())
#define TP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_CONNECTION, \
                              TpConnection))
#define TP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_CONNECTION, \
                           TpConnectionClass))
#define TP_IS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CONNECTION))
#define TP_IS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CONNECTION))
#define TP_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CONNECTION, \
                              TpConnectionClass))

TpConnection *tp_connection_new (TpDBusDaemon *dbus, const gchar *bus_name,
    const gchar *object_path, GError **error) G_GNUC_WARN_UNUSED_RESULT;

TpConnectionStatus tp_connection_get_status (TpConnection *self,
    TpConnectionStatusReason *reason);

TpHandle tp_connection_get_self_handle (TpConnection *self);

TpCapabilities * tp_connection_get_capabilities (TpConnection *self);

TpAvatarRequirements * tp_connection_get_avatar_requirements (
    TpConnection *self);

gboolean tp_connection_is_ready (TpConnection *self);

#ifndef TP_DISABLE_DEPRECATED
gboolean tp_connection_run_until_ready (TpConnection *self,
    gboolean connect, GError **error,
    GMainLoop **loop) _TP_GNUC_DEPRECATED;
#endif

typedef void (*TpConnectionWhenReadyCb) (TpConnection *connection,
    const GError *error, gpointer user_data);

void tp_connection_call_when_ready (TpConnection *self,
    TpConnectionWhenReadyCb callback, gpointer user_data);

typedef void (*TpConnectionNameListCb) (const gchar * const *names,
    gsize n, const gchar * const *cms, const gchar * const *protocols,
    const GError *error, gpointer user_data,
    GObject *weak_object);

void tp_list_connection_names (TpDBusDaemon *bus_daemon,
    TpConnectionNameListCb callback,
    gpointer user_data, GDestroyNotify destroy,
    GObject *weak_object);

void tp_connection_init_known_interfaces (void);

gint tp_connection_presence_type_cmp_availability (TpConnectionPresenceType p1,
  TpConnectionPresenceType p2);

gboolean tp_connection_parse_object_path (TpConnection *self, gchar **protocol,
    gchar **cm_name);

const gchar *tp_connection_get_detailed_error (TpConnection *self,
    const GHashTable **details);

#define TP_CONNECTION_FEATURE_CORE \
  (tp_connection_get_feature_quark_core ())
GQuark tp_connection_get_feature_quark_core (void) G_GNUC_CONST;

#define TP_CONNECTION_FEATURE_CONNECTED \
  (tp_connection_get_feature_quark_connected ())
GQuark tp_connection_get_feature_quark_connected (void) G_GNUC_CONST;

#define TP_CONNECTION_FEATURE_CAPABILITIES \
  (tp_connection_get_feature_quark_capabilities ())
GQuark tp_connection_get_feature_quark_capabilities (void) G_GNUC_CONST;

#define TP_CONNECTION_FEATURE_AVATAR_REQUIREMENTS \
  (tp_connection_get_feature_quark_avatar_requirements ())
GQuark tp_connection_get_feature_quark_avatar_requirements (void) G_GNUC_CONST;

/* connection-handles.c */

typedef void (*TpConnectionHoldHandlesCb) (TpConnection *connection,
    TpHandleType handle_type, guint n_handles, const TpHandle *handles,
    const GError *error, gpointer user_data, GObject *weak_object);

void tp_connection_hold_handles (TpConnection *self, gint timeout_ms,
    TpHandleType handle_type, guint n_handles, const TpHandle *handles,
    TpConnectionHoldHandlesCb callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

typedef void (*TpConnectionRequestHandlesCb) (TpConnection *connection,
    TpHandleType handle_type,
    guint n_handles, const TpHandle *handles, const gchar * const *ids,
    const GError *error, gpointer user_data, GObject *weak_object);

void tp_connection_request_handles (TpConnection *self, gint timeout_ms,
    TpHandleType handle_type, const gchar * const *ids,
    TpConnectionRequestHandlesCb callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

void tp_connection_unref_handles (TpConnection *self,
    TpHandleType handle_type, guint n_handles, const TpHandle *handles);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-connection.h>

G_BEGIN_DECLS

/* connection-handles.c again - this has to come after the auto-generated
 * stuff because it uses an auto-generated typedef */

void tp_connection_get_contact_attributes (TpConnection *self,
    gint timeout_ms, guint n_handles, const TpHandle *handles,
    const gchar * const *interfaces, gboolean hold,
    tp_cli_connection_interface_contacts_callback_for_get_contact_attributes callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

G_END_DECLS

#endif
