/*
 * D-Bus error types used in telepathy
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

#include <telepathy-glib/errors.h>

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/util.h>

/**
 * TP_ERROR_PREFIX:
 *
 * The common prefix of Telepathy errors, as a string constant, without
 * the trailing '.' character.
 *
 * Since: 0.7.1
 */

/**
 * TP_ERRORS:
 *
 * The error domain for the D-Bus errors described in the Telepathy
 * specification. Error codes in this domain come from the #TpError
 * enumeration.
 *
 * This macro expands to a call to a function returning the Telepathy error
 * domain. Since 0.7.17, this function automatically registers the domain with
 * dbus-glib for server-side use (using dbus_g_error_domain_register()) when
 * called.
 */

/**
 * TP_TYPE_ERROR:
 *
 * The GType of the Telepathy error enumeration.
 */

/**
 * TpError:
 * @TP_ERROR_NETWORK_ERROR: org.freedesktop.Telepathy.Error.NetworkError:
 *     Raised when there is an error reading from or writing to the network.
 * @TP_ERROR_NOT_IMPLEMENTED: org.freedesktop.Telepathy.Error.NotImplemented:
 *     Raised when the requested method, channel, etc is not available on this
 *     connection.
 * @TP_ERROR_INVALID_ARGUMENT: org.freedesktop.Telepathy.Error.InvalidArgument:
 *     Raised when one of the provided arguments is invalid.
 * @TP_ERROR_NOT_AVAILABLE: org.freedesktop.Telepathy.Error.NotAvailable:
 *     Raised when the requested functionality is temporarily unavailable.
 * @TP_ERROR_PERMISSION_DENIED: org.freedesktop.Telepathy.Error.PermissionDenied:
 *     The user is not permitted to perform the requested operation.
 * @TP_ERROR_DISCONNECTED: org.freedesktop.Telepathy.Error.Disconnected:
 *     The connection is not currently connected and cannot be used.
 *     This error may also be raised when operations are performed on a
 *     Connection for which StatusChanged has signalled status Disconnected
 *     for reason None.
 * @TP_ERROR_INVALID_HANDLE: org.freedesktop.Telepathy.Error.InvalidHandle:
 *     An identifier being converted to a handle was syntactically invalid,
 *     or an invalid handle was used.
 * @TP_ERROR_CHANNEL_BANNED: org.freedesktop.Telepathy.Error.Channel.Banned:
 *     You are banned from the channel.
 * @TP_ERROR_CHANNEL_FULL: org.freedesktop.Telepathy.Error.Channel.Full:
 *     The channel is full.
 * @TP_ERROR_CHANNEL_INVITE_ONLY: org.freedesktop.Telepathy.Error.Channel.InviteOnly:
 *     The requested channel is invite-only.
 * @TP_ERROR_NOT_YOURS: org.freedesktop.Telepathy.Error.NotYours:
 *     The requested channel or other resource already exists, and another
 *     client is responsible for it
 * @TP_ERROR_CANCELLED: org.freedesktop.Telepathy.Error.Cancelled:
 *     Raised by an ongoing request if it is cancelled by user request before
 *     it has completed, or when operations are performed on an object which
 *     the user has asked to close (for instance, a Connection where the user
 *     has called Disconnect, or a Channel where the user has called Close).
 * @TP_ERROR_AUTHENTICATION_FAILED: org.freedesktop.Telepathy.Error.AuthenticationFailed:
 *     Raised when authentication with a service was unsuccessful.
 * @TP_ERROR_ENCRYPTION_NOT_AVAILABLE: org.freedesktop.Telepathy.Error.EncryptionNotAvailable:
 *     Raised if a user request insisted that encryption should be used,
 *     but encryption was not actually available.
 * @TP_ERROR_ENCRYPTION_ERROR: org.freedesktop.Telepathy.Error.EncryptionError:
 *     Raised if encryption appears to be available, but could not actually be
 *     used (for instance if SSL/TLS negotiation fails).
 * @TP_ERROR_CERT_NOT_PROVIDED: org.freedesktop.Telepathy.Error.Cert.NotProvided:
 *     Raised if the server did not provide a SSL/TLS certificate.
 * @TP_ERROR_CERT_UNTRUSTED: org.freedesktop.Telepathy.Error.Cert.Untrusted:
 *     Raised if the server provided a SSL/TLS certificate signed by an
 *     untrusted certifying authority.
 * @TP_ERROR_CERT_EXPIRED: org.freedesktop.Telepathy.Error.Cert.Expired:
 *     Raised if the server provided an expired SSL/TLS certificate.
 * @TP_ERROR_CERT_NOT_ACTIVATED: org.freedesktop.Telepathy.Error.Cert.NotActivated:
 *     Raised if the server provided an SSL/TLS certificate that will become
 *     valid at some point in the future.
 * @TP_ERROR_CERT_FINGERPRINT_MISMATCH: org.freedesktop.Telepathy.Error.Cert.FingerprintMismatch:
 *     Raised if the server provided an SSL/TLS certificate that did not have
 *     the expected fingerprint.
 * @TP_ERROR_CERT_HOSTNAME_MISMATCH: org.freedesktop.Telepathy.Error.Cert.HostnameMismatch:
 *     Raised if the server provided an SSL/TLS certificate that did not
 *     match its hostname.
 * @TP_ERROR_CERT_SELF_SIGNED: org.freedesktop.Telepathy.Error.Cert.SelfSigned:
 *     Raised if the server provided an SSL/TLS certificate that is
 *     self-signed and untrusted.
 * @TP_ERROR_CERT_INVALID: org.freedesktop.Telepathy.Error.Cert.Invalid:
 *     Raised if the server provided an SSL/TLS certificate that is
 *     unacceptable in some way that does not have a more specific error.
 * @TP_ERROR_NOT_CAPABLE: org.freedesktop.Telepathy.Error.NotCapable:
 *     Raised when requested functionality is unavailable due to a contact
 *     not having the required capabilities.
 * @TP_ERROR_OFFLINE: org.freedesktop.Telepathy.Error.Offline:
 *     Raised when requested functionality is unavailable because a contact is
 *     offline.
 * @TP_ERROR_CHANNEL_KICKED: org.freedesktop.Telepathy.Error.Channel.Kicked:
 *     Used to represent a user being ejected from a channel by another user,
 *     for instance being kicked from a chatroom.
 * @TP_ERROR_BUSY: org.freedesktop.Telepathy.Error.Busy:
 *     Used to represent a user being removed from a channel because of a
 *     "busy" indication.
 * @TP_ERROR_NO_ANSWER: org.freedesktop.Telepathy.Error.NoAnswer:
 *     Used to represent a user being removed from a channel because they did
 *     not respond, e.g. to a StreamedMedia call.
 * @TP_ERROR_DOES_NOT_EXIST: org.freedesktop.Telepathy.Error.DoesNotExist:
 *     Raised when the requested user does not, in fact, exist.
 * @TP_ERROR_TERMINATED: org.freedesktop.Telepathy.Error.Terminated:
 *     Raised when a channel is terminated for an unspecified reason. In
 *     particular, this error SHOULD be used whenever normal termination of a
 *     1-1 StreamedMedia call by the remote user is represented as a D-Bus
 *     error name.
 * @TP_ERROR_CONNECTION_REFUSED: org.freedesktop.Telepathy.Error.ConnectionRefused:
 *     Raised when a connection is refused.
 * @TP_ERROR_CONNECTION_FAILED: org.freedesktop.Telepathy.Error.ConnectionFailed:
 *     Raised when a connection can't be established.
 * @TP_ERROR_CONNECTION_LOST: org.freedesktop.Telepathy.Error.ConnectionLost:
 *     Raised when a connection is broken.
 * @TP_ERROR_ALREADY_CONNECTED: org.freedesktop.Telepathy.Error.AlreadyConnected:
 *     Raised on attempts to connect again to an account that is already
 *     connected, if the protocol or server does not allow this.
 *     Since 0.7.34
 * @TP_ERROR_CONNECTION_REPLACED: org.freedesktop.Telepathy.Error.ConnectionReplaced:
 *     Used as disconnection reason for an existing connection if it is
 *     disconnected because a second connection to the same account is made.
 *     Since 0.7.34
 * @TP_ERROR_REGISTRATION_EXISTS: org.freedesktop.Telepathy.Error.RegistrationExists:
 *     Raised on attempts to register an account on a server when the account
 *     already exists.
 *     Since 0.7.34
 * @TP_ERROR_SERVICE_BUSY: org.freedesktop.Telepathy.Error.ServiceBusy:
 *     Raised when a server or other infrastructure rejects a request because
 *     it is too busy.
 *     Since 0.7.34
 * @TP_ERROR_RESOURCE_UNAVAILABLE: org.freedesktop.Telepathy.Error.ResourceUnavailable:
 *     Raised when a local process rejects a request because it does not have
 *     enough of a resource, such as memory.
 *     Since 0.7.34
 *
 * Enumerated type representing the Telepathy D-Bus errors.
 */

/**
 * tp_g_set_error_invalid_handle_type:
 * @type: An invalid handle type
 * @error: Either %NULL, or used to return an error (as for g_set_error)
 *
 * Set the error NotImplemented for an invalid handle type,
 * with an appropriate message.
 *
 * Changed in version 0.7.23: previously, the error was
 * InvalidArgument.
 */
void
tp_g_set_error_invalid_handle_type (guint type, GError **error)
{
  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "unsupported handle type %u", type);
}

/**
 * tp_g_set_error_unsupported_handle_type:
 * @type: An unsupported handle type
 * @error: Either %NULL, or used to return an error (as for g_set_error)
 *
 * Set the error NotImplemented for a handle type which is valid but is not
 * supported by this connection manager, with an appropriate message.
 *
 * Changed in version 0.7.23: previously, the error was
 * InvalidArgument.
 */
void
tp_g_set_error_unsupported_handle_type (guint type, GError **error)
{
  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "unsupported handle type %u", type);
}

/**
 * tp_error_get_dbus_name:
 * @error: a member of the #TpError enum.
 *
 * <!-- -->
 *
 * Returns: the D-Bus error name corresponding to @error.
 *
 * Since: 0.7.31
 */
/* tp_error_get_dbus_name is implemented in _gen/error-str.c by
 * tools/glib-errors-str-gen.py.
 */

GQuark
tp_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string ("tp_errors");

      tp_verify_statement (sizeof (GQuark) <= sizeof (gsize));

      g_type_init ();
      dbus_g_error_domain_register (domain, TP_ERROR_PREFIX,
          TP_TYPE_ERROR);
      g_once_init_leave (&quark, domain);
    }

  return (GQuark) quark;
}

GType
tp_error_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0))
    {
      static const GEnumValue values[] = {
        { TP_ERROR_NETWORK_ERROR, "TP_ERROR_NETWORK_ERROR", "NetworkError" },
        { TP_ERROR_NOT_IMPLEMENTED, "TP_ERROR_NOT_IMPLEMENTED", "NotImplemented" },
        { TP_ERROR_INVALID_ARGUMENT, "TP_ERROR_INVALID_ARGUMENT", "InvalidArgument" },
        { TP_ERROR_NOT_AVAILABLE, "TP_ERROR_NOT_AVAILABLE", "NotAvailable" },
        { TP_ERROR_PERMISSION_DENIED, "TP_ERROR_PERMISSION_DENIED", "PermissionDenied" },
        { TP_ERROR_DISCONNECTED, "TP_ERROR_DISCONNECTED", "Disconnected" },
        { TP_ERROR_INVALID_HANDLE, "TP_ERROR_INVALID_HANDLE", "InvalidHandle" },
        { TP_ERROR_CHANNEL_BANNED, "TP_ERROR_CHANNEL_BANNED", "Channel.Banned" },
        { TP_ERROR_CHANNEL_FULL, "TP_ERROR_CHANNEL_FULL", "Channel.Full" },
        { TP_ERROR_CHANNEL_INVITE_ONLY, "TP_ERROR_CHANNEL_INVITE_ONLY", "Channel.InviteOnly" },
        { TP_ERROR_NOT_YOURS, "TP_ERROR_NOT_YOURS", "NotYours" },
        { TP_ERROR_CANCELLED, "TP_ERROR_CANCELLED", "Cancelled" },
        { TP_ERROR_AUTHENTICATION_FAILED, "TP_ERROR_AUTHENTICATION_FAILED", "AuthenticationFailed" },
        { TP_ERROR_ENCRYPTION_NOT_AVAILABLE, "TP_ERROR_ENCRYPTION_NOT_AVAILABLE", "EncryptionNotAvailable" },
        { TP_ERROR_ENCRYPTION_ERROR, "TP_ERROR_ENCRYPTION_ERROR", "EncryptionError" },
        { TP_ERROR_CERT_NOT_PROVIDED, "TP_ERROR_CERT_NOT_PROVIDED", "Cert.NotProvided" },
        { TP_ERROR_CERT_UNTRUSTED, "TP_ERROR_CERT_UNTRUSTED", "Cert.Untrusted" },
        { TP_ERROR_CERT_EXPIRED, "TP_ERROR_CERT_EXPIRED", "Cert.Expired" },
        { TP_ERROR_CERT_NOT_ACTIVATED, "TP_ERROR_CERT_NOT_ACTIVATED", "Cert.NotActivated" },
        { TP_ERROR_CERT_FINGERPRINT_MISMATCH, "TP_ERROR_CERT_FINGERPRINT_MISMATCH", "Cert.FingerprintMismatch" },
        { TP_ERROR_CERT_HOSTNAME_MISMATCH, "TP_ERROR_CERT_HOSTNAME_MISMATCH", "Cert.HostnameMismatch" },
        { TP_ERROR_CERT_SELF_SIGNED, "TP_ERROR_CERT_SELF_SIGNED", "Cert.SelfSigned" },
        { TP_ERROR_CERT_INVALID, "TP_ERROR_CERT_INVALID", "Cert.Invalid" },
        { TP_ERROR_NOT_CAPABLE, "TP_ERROR_NOT_CAPABLE", "NotCapable" },
        { TP_ERROR_OFFLINE, "TP_ERROR_OFFLINE", "Offline" },
        { TP_ERROR_CHANNEL_KICKED, "TP_ERROR_CHANNEL_KICKED", "Channel.Kicked" },
        { TP_ERROR_BUSY, "TP_ERROR_BUSY", "Busy" },
        { TP_ERROR_NO_ANSWER, "TP_ERROR_NO_ANSWER", "NoAnswer" },
        { TP_ERROR_DOES_NOT_EXIST, "TP_ERROR_DOES_NOT_EXIST", "DoesNotExist" },
        { TP_ERROR_TERMINATED, "TP_ERROR_TERMINATED", "Terminated" },
        { TP_ERROR_CONNECTION_REFUSED, "TP_ERROR_CONNECTION_REFUSED", "ConnectionRefused" },
        { TP_ERROR_CONNECTION_FAILED, "TP_ERROR_CONNECTION_FAILED", "ConnectionFailed" },
        { TP_ERROR_CONNECTION_LOST, "TP_ERROR_CONNECTION_LOST", "ConnectionLost" },
        /* new in telepathy-spec 0.17.27 / telepathy-glib 0.7.34 */
        { TP_ERROR_ALREADY_CONNECTED, "TP_ERROR_ALREADY_CONNECTED",
          "AlreadyConnected" },
        { TP_ERROR_CONNECTION_REPLACED, "TP_ERROR_CONNECTION_REPLACED",
          "ConnectionReplaced" },
        { TP_ERROR_REGISTRATION_EXISTS, "TP_ERROR_REGISTRATION_EXISTS",
          "RegistrationExists" },
        { TP_ERROR_SERVICE_BUSY, "TP_ERROR_SERVICE_BUSY", "ServiceBusy" },
        { TP_ERROR_RESOURCE_UNAVAILABLE, "TP_ERROR_RESOURCE_UNAVAILABLE",
          "ResourceUnavailable" },
        { 0 }
      };

      etype = g_enum_register_static ("TpError", values);
    }
  return etype;
}
