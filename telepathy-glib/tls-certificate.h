/*
 * TpTLSCertificate - a TpProxy for TLS certificates
 * Copyright © 2010 Collabora Ltd.
 *
 * Based on EmpathyTLSCertificate:
 * @author Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
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

#ifndef __TP_TLS_CERTIFICATE_H__
#define __TP_TLS_CERTIFICATE_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpTLSCertificate TpTLSCertificate;
typedef struct _TpTLSCertificateClass TpTLSCertificateClass;
typedef struct _TpTLSCertificatePrivate TpTLSCertificatePrivate;
typedef struct _TpTLSCertificateClassPrivate TpTLSCertificateClassPrivate;

struct _TpTLSCertificateClass {
    TpProxyClass parent_class;
    TpTLSCertificateClassPrivate *priv;
};

struct _TpTLSCertificate {
    TpProxy parent;
    TpTLSCertificatePrivate *priv;
};

GType tp_tls_certificate_get_type (void);

#define TP_TYPE_TLS_CERTIFICATE \
  (tp_tls_certificate_get_type ())
#define TP_TLS_CERTIFICATE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_TLS_CERTIFICATE, \
                               TpTLSCertificate))
#define TP_TLS_CERTIFICATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_TLS_CERTIFICATE, \
                            TpTLSCertificateClass))
#define TP_IS_TLS_CERTIFICATE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_TLS_CERTIFICATE))
#define TP_IS_TLS_CERTIFICATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_TLS_CERTIFICATE))
#define TP_TLS_CERTIFICATE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_TLS_CERTIFICATE, \
                              TpTLSCertificateClass))

TpTLSCertificate *tp_tls_certificate_new (TpDBusDaemon *dbus_daemon,
    const gchar *bus_name,
    const gchar *object_path,
    GError **error);

void tp_tls_certificate_prepare_async (TpTLSCertificate *self,
    GAsyncReadyCallback callback, gpointer user_data);
gboolean tp_tls_certificate_prepare_finish (TpTLSCertificate *self,
    GAsyncResult *result, GError **error);

void tp_tls_certificate_accept_async (TpTLSCertificate *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_tls_certificate_accept_finish (TpTLSCertificate *self,
    GAsyncResult *result,
    GError **error);

void tp_tls_certificate_reject_async (TpTLSCertificate *self,
    TpTLSCertificateRejectReason reason,
    GHashTable *details,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_tls_certificate_reject_finish (TpTLSCertificate *self,
    GAsyncResult *result,
    GError **error);

void tp_tls_certificate_init_known_interfaces (void);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-tls-cert.h>

#endif /* multiple-inclusion guard */
