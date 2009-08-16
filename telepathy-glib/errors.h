/*
 * <telepathy-glib/errors.h> - Header for Telepathy error types
 * Copyright (C) 2005-2009 Collabora Ltd.
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

#ifndef __TP_ERRORS_H__
#define __TP_ERRORS_H__

#include <glib-object.h>

#include <telepathy-glib/_gen/error-str.h>

G_BEGIN_DECLS

GQuark tp_errors_quark (void);

#define TP_ERROR_PREFIX "org.freedesktop.Telepathy.Error"

#define TP_ERRORS (tp_errors_quark ())

void tp_g_set_error_invalid_handle_type (guint type, GError **error);
void tp_g_set_error_unsupported_handle_type (guint type, GError **error);

#define TP_TYPE_ERROR (tp_error_get_type())

GType tp_error_get_type (void);

typedef enum {
    TP_ERROR_NETWORK_ERROR,
    TP_ERROR_NOT_IMPLEMENTED,
    TP_ERROR_INVALID_ARGUMENT,
    TP_ERROR_NOT_AVAILABLE,
    TP_ERROR_PERMISSION_DENIED,
    TP_ERROR_DISCONNECTED,
    TP_ERROR_INVALID_HANDLE,
    TP_ERROR_CHANNEL_BANNED,
    TP_ERROR_CHANNEL_FULL,
    TP_ERROR_CHANNEL_INVITE_ONLY,
    TP_ERROR_NOT_YOURS,
    TP_ERROR_CANCELLED,
    TP_ERROR_AUTHENTICATION_FAILED,
    TP_ERROR_ENCRYPTION_NOT_AVAILABLE,
    TP_ERROR_ENCRYPTION_ERROR,
    TP_ERROR_CERT_NOT_PROVIDED,
    TP_ERROR_CERT_UNTRUSTED,
    TP_ERROR_CERT_EXPIRED,
    TP_ERROR_CERT_NOT_ACTIVATED,
    TP_ERROR_CERT_FINGERPRINT_MISMATCH,
    TP_ERROR_CERT_HOSTNAME_MISMATCH,
    TP_ERROR_CERT_SELF_SIGNED,
    TP_ERROR_CERT_INVALID,
    TP_ERROR_NOT_CAPABLE,
    TP_ERROR_OFFLINE,
    TP_ERROR_CHANNEL_KICKED,
    TP_ERROR_BUSY,
    TP_ERROR_NO_ANSWER,
    TP_ERROR_DOES_NOT_EXIST,
    TP_ERROR_TERMINATED,
    TP_ERROR_CONNECTION_REFUSED,
    TP_ERROR_CONNECTION_FAILED,
    TP_ERROR_CONNECTION_LOST,
    TP_ERROR_ALREADY_CONNECTED,
    TP_ERROR_CONNECTION_REPLACED,
    TP_ERROR_REGISTRATION_EXISTS,
    TP_ERROR_SERVICE_BUSY,
    TP_ERROR_RESOURCE_UNAVAILABLE,
} TpError;

const gchar *tp_error_get_dbus_name (TpError error);

G_END_DECLS

#endif
