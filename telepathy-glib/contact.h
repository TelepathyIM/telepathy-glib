/* Object representing a Telepathy contact
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef __TP_CONTACT_H__
#define __TP_CONTACT_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/capabilities.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/handle.h>

#include <telepathy-glib/_gen/genums.h>

G_BEGIN_DECLS

/* TpContact is forward-declared in connection.h */
typedef struct _TpContactClass TpContactClass;
typedef struct _TpContactPrivate TpContactPrivate;

GType tp_contact_get_type (void) G_GNUC_CONST;

#define TP_TYPE_CONTACT \
  (tp_contact_get_type ())
#define TP_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CONTACT, \
                               TpContact))
#define TP_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CONTACT, \
                            TpContactClass))
#define TP_IS_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CONTACT))
#define TP_IS_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CONTACT))
#define TP_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CONTACT, \
                              TpContactClass))

#define TP_CONTACT_FEATURE_ALIAS \
  (tp_contact_get_feature_quark_alias ())
GQuark tp_contact_get_feature_quark_alias (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_PRESENCE \
  (tp_contact_get_feature_quark_presence ())
GQuark tp_contact_get_feature_quark_presence (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_LOCATION \
  (tp_contact_get_feature_quark_location ())
GQuark tp_contact_get_feature_quark_location (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_CAPABILITIES \
  (tp_contact_get_feature_quark_capabilities ())
GQuark tp_contact_get_feature_quark_capabilities (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_AVATAR \
  (tp_contact_get_feature_quark_avatar ())
GQuark tp_contact_get_feature_quark_avatar (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_CONTACT_INFO \
  (tp_contact_get_feature_quark_contact_info ())
GQuark tp_contact_get_feature_quark_contact_info (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_CLIENT_TYPES \
  (tp_contact_get_feature_quark_client_types ())
GQuark tp_contact_get_feature_quark_client_types (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_SUBSCRIPTION_STATES \
  (tp_contact_get_feature_quark_subscription_states ())
GQuark tp_contact_get_feature_quark_subscription_states (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_CONTACT_GROUPS \
  (tp_contact_get_feature_quark_contact_groups ())
GQuark tp_contact_get_feature_quark_contact_groups (void) G_GNUC_CONST;

#define TP_CONTACT_FEATURE_CONTACT_BLOCKING \
  (tp_contact_get_feature_quark_contact_blocking ())
GQuark tp_contact_get_feature_quark_contact_blocking (void) G_GNUC_CONST;

/* Basic functionality, always available */
_TP_AVAILABLE_IN_0_20
TpAccount *tp_contact_get_account (TpContact *self);
TpConnection *tp_contact_get_connection (TpContact *self);
TpHandle tp_contact_get_handle (TpContact *self);
const gchar *tp_contact_get_identifier (TpContact *self);
gboolean tp_contact_has_feature (TpContact *self, GQuark feature);

/* TP_CONTACT_FEATURE_ALIAS */
const gchar *tp_contact_get_alias (TpContact *self);

/* TP_CONTACT_FEATURE_PRESENCE */
TpConnectionPresenceType tp_contact_get_presence_type (TpContact *self);
const gchar *tp_contact_get_presence_status (TpContact *self);
const gchar *tp_contact_get_presence_message (TpContact *self);

/* TP_CONTACT_FEATURE_LOCATION */
GHashTable *tp_contact_get_location (TpContact *self);
_TP_AVAILABLE_IN_0_20
GVariant *tp_contact_dup_location (TpContact *self);

/* TP_CONTACT_FEATURE_CAPABILITIES */
TpCapabilities *tp_contact_get_capabilities (TpContact *self);

/* TP_CONTACT_FEATURE_AVATAR */
GFile *tp_contact_get_avatar_file (TpContact *self);

/* TP_CONTACT_FEATURE_INFO */
#ifndef TP_DISABLE_DEPRECATED
_TP_DEPRECATED_IN_0_20_FOR (tp_contact_dup_contact_info)
GList *tp_contact_get_contact_info (TpContact *self);
#endif

_TP_AVAILABLE_IN_0_20
GList *tp_contact_dup_contact_info (TpContact *self);

void tp_contact_request_contact_info_async (TpContact *self,
    GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_contact_request_contact_info_finish (TpContact *self,
    GAsyncResult *result, GError **error);

void tp_connection_refresh_contact_info (TpConnection *self,
    guint n_contacts, TpContact * const *contacts);

/* TP_CONTACT_FEATURE_CLIENT_TYPES */
const gchar * const *
/* this comment stops gtkdoc denying that this function exists */
tp_contact_get_client_types (TpContact *self);

/* TP_CONTACT_FEATURE_SUBSCRIPTION_STATES */
TpSubscriptionState tp_contact_get_subscribe_state (TpContact *self);
TpSubscriptionState tp_contact_get_publish_state (TpContact *self);
const gchar *tp_contact_get_publish_request (TpContact *self);

/* TP_CONTACT_FEATURE_CONTACT_GROUPS */
const gchar * const *
/* this comment stops gtkdoc denying that this function exists */
tp_contact_get_contact_groups (TpContact *self);
void tp_contact_set_contact_groups_async (TpContact *self,
    gint n_groups, const gchar * const *groups, GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_set_contact_groups_finish (TpContact *self,
    GAsyncResult *result, GError **error);

TpContact *tp_connection_dup_contact_if_possible (TpConnection *connection,
    TpHandle handle, const gchar *identifier);

_TP_AVAILABLE_IN_0_20
void tp_connection_dup_contact_by_id_async (TpConnection *self,
    const gchar *id,
    const GQuark *features,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_0_20
TpContact *tp_connection_dup_contact_by_id_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

_TP_AVAILABLE_IN_0_20
void tp_connection_upgrade_contacts_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    const GQuark *features,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_0_20
gboolean tp_connection_upgrade_contacts_finish (TpConnection *self,
    GAsyncResult *result,
    GPtrArray **contacts,
    GError **error);

/* TP_CONTACT_FEATURE_CONTACT_BLOCKING */

_TP_AVAILABLE_IN_0_18
gboolean tp_contact_is_blocked (TpContact *self);

G_END_DECLS

#endif
