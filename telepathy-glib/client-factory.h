/*
 * A factory for TpContacts and plain subclasses of TpProxy
 *
 * Copyright © 2011 Collabora Ltd.
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

#ifndef __TP_CLIENT_FACTORY_H__
#define __TP_CLIENT_FACTORY_H__

#include <telepathy-glib/account.h>
#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/channel-dispatcher.h>
#include <telepathy-glib/channel-dispatch-operation.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus-daemon.h>
#include <telepathy-glib/protocol.h>
#include <telepathy-glib/tls-certificate.h>

G_BEGIN_DECLS

/* TpClientFactory is typedef'd in proxy.h */
typedef struct _TpClientFactoryPrivate TpClientFactoryPrivate;
typedef struct _TpClientFactoryClass TpClientFactoryClass;

struct _TpClientFactoryClass {
    /*<public>*/
    GObjectClass parent_class;

    /* TpAccount */
    TpAccount * (*create_account) (TpClientFactory *self,
        const gchar *object_path,
        GVariant *immutable_properties,
        GError **error);
    GArray * (*dup_account_features) (TpClientFactory *self,
        TpAccount *account);

    /* TpConnection */
    TpConnection * (*create_connection) (TpClientFactory *self,
        const gchar *object_path,
        GVariant *immutable_properties,
        GError **error);
    GArray * (*dup_connection_features) (TpClientFactory *self,
        TpConnection *connection);

    /* TpChannel */
    TpChannel * (*create_channel) (TpClientFactory *self,
        TpConnection *conn,
        const gchar *object_path,
        GVariant *immutable_properties,
        GError **error);
    GArray * (*dup_channel_features) (TpClientFactory *self,
        TpChannel *channel);

    /* TpContact */
    TpContact * (*create_contact) (TpClientFactory *self,
        TpConnection *connection,
        TpHandle handle,
        const gchar *identifier);
    GArray * (*dup_contact_features) (TpClientFactory *self,
        TpConnection *connection);

    /* TpProcotol */
    TpProtocol * (*create_protocol) (TpClientFactory *self,
        const gchar *cm_name,
        const gchar *protocol_name,
        GVariant *immutable_properties,
        GError **error);
    GArray * (*dup_protocol_features) (TpClientFactory *self,
        TpProtocol *protocol);

    /* TpTLSCertificate */
    TpTLSCertificate * (*create_tls_certificate) (TpClientFactory *self,
        TpProxy *conn_or_chan,
        const gchar *object_path,
        GError **error);
    GArray * (*dup_tls_certificate_features) (TpClientFactory *self,
        TpTLSCertificate *certificate);

    /*<private>*/
    GCallback padding[20];
};

struct _TpClientFactory {
    /*<private>*/
    GObject parent;
    TpClientFactoryPrivate *priv;
};

GType tp_client_factory_get_type (void);

#define TP_TYPE_CLIENT_FACTORY \
  (tp_client_factory_get_type ())
#define TP_CLIENT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CLIENT_FACTORY, \
                               TpClientFactory))
#define TP_CLIENT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CLIENT_FACTORY, \
                            TpClientFactoryClass))
#define TP_IS_CLIENT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CLIENT_FACTORY))
#define TP_IS_CLIENT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CLIENT_FACTORY))
#define TP_CLIENT_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CLIENT_FACTORY, \
                              TpClientFactoryClass))

TpClientFactory * tp_client_factory_new (TpDBusDaemon *dbus);
TpClientFactory * tp_client_factory_dup (GError **error);
void tp_client_factory_set_default (TpClientFactory *self);
gboolean tp_client_factory_can_set_default (void);

TpDBusDaemon *tp_client_factory_get_dbus_daemon (TpClientFactory *self);
GDBusConnection *tp_client_factory_get_dbus_connection (TpClientFactory *self);

TpAccountManager *tp_client_factory_ensure_account_manager (
    TpClientFactory *self);
TpChannelDispatcher *tp_client_factory_ensure_channel_dispatcher (
    TpClientFactory *self);

/* TpAccount */
TpAccount *tp_client_factory_ensure_account (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error);
GArray *tp_client_factory_dup_account_features (TpClientFactory *self,
    TpAccount *account);
void tp_client_factory_add_account_features (TpClientFactory *self,
    const GQuark *features);
void tp_client_factory_add_account_features_varargs (TpClientFactory *self,
    GQuark feature,
    ...);

/* TpConnection */
TpConnection *tp_client_factory_ensure_connection (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error);
GArray *tp_client_factory_dup_connection_features (TpClientFactory *self,
    TpConnection *connection);
void tp_client_factory_add_connection_features (TpClientFactory *self,
    const GQuark *features);
void tp_client_factory_add_connection_features_varargs (TpClientFactory *self,
    GQuark feature,
    ...);

/* TpChannel */
TpChannel *tp_client_factory_ensure_channel (TpClientFactory *self,
    TpConnection *connection,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error);
GArray *tp_client_factory_dup_channel_features (TpClientFactory *self,
    TpChannel *channel);
void tp_client_factory_add_channel_features (TpClientFactory *self,
    const GQuark *features);
void tp_client_factory_add_channel_features_varargs (TpClientFactory *self,
    GQuark feature,
    ...);

/* TpContact */
TpContact *tp_client_factory_ensure_contact (TpClientFactory *self,
    TpConnection *connection,
    TpHandle handle,
    const gchar *identifier);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_upgrade_contacts_async (TpClientFactory *self,
    TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_1_0
gboolean tp_client_factory_upgrade_contacts_finish (TpClientFactory *self,
    GAsyncResult *result,
    GPtrArray **contacts,
    GError **error);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_ensure_contact_by_id_async (TpClientFactory *self,
    TpConnection *connection,
    const gchar *identifier,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_1_0
TpContact *tp_client_factory_ensure_contact_by_id_finish (TpClientFactory *self,
    GAsyncResult *result,
    GError **error);
GArray *tp_client_factory_dup_contact_features (TpClientFactory *self,
    TpConnection *connection);
void tp_client_factory_add_contact_features (TpClientFactory *self,
    const GQuark *features);
void tp_client_factory_add_contact_features_varargs (TpClientFactory *self,
    GQuark feature,
    ...);

/* TpProtocol */
_TP_AVAILABLE_IN_1_0
TpProtocol *tp_client_factory_ensure_protocol (TpClientFactory *self,
    const gchar *cm_name,
    const gchar *protocol_name,
    GVariant *immutable_properties,
    GError **error);
_TP_AVAILABLE_IN_1_0
GArray *tp_client_factory_dup_protocol_features (TpClientFactory *self,
    TpProtocol *protocol);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_add_protocol_features (TpClientFactory *self,
    const GQuark *features);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_add_protocol_features_varargs (TpClientFactory *self,
    GQuark feature,
    ...);

/* TpTLSCertificate */
_TP_AVAILABLE_IN_1_0
TpTLSCertificate *tp_client_factory_ensure_tls_certificate (
    TpClientFactory *self,
    TpProxy *conn_or_chan,
    const gchar *object_path,
    GError **error);
_TP_AVAILABLE_IN_1_0
GArray *tp_client_factory_dup_tls_certificate_features (TpClientFactory *self,
    TpTLSCertificate *certificate);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_add_tls_certificate_features (TpClientFactory *self,
    const GQuark *features);
_TP_AVAILABLE_IN_1_0
void tp_client_factory_add_tls_certificate_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...);

G_END_DECLS

#endif
