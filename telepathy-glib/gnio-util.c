/*
 * gnio-util.c - Source for telepathy-glib GNIO utility functions
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 *   @author Danielle Madeley <danielle.madeley@collabora.co.uk>
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

/**
 * SECTION:gnio-util
 * @title: GNIO Utilities
 * @short_description: Telepathy/GNIO utility functions
 *
 * Utility functions for interacting between Telepathy and GNIO.
 *
 * Telepathy uses address variants stored in #GValue boxes for communicating
 * network socket addresses over D-Bus to and from the Connection Manager
 * (for instance when using the file transfer and stream tube APIs).
 *
 * This API provides translation between #GSocketAddress subtypes and a #GValue
 * that can be used by telepathy-glib.
 * #GInetSocketAddress is used for IPv4/IPv6 and #GUnixSocketAddress
 * for UNIX sockets (only available on platforms with gio-unix).
 */

#include <config.h>

#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/errors.h>

#include <string.h>

#include <dbus/dbus-glib.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

/**
 * tp_g_socket_address_from_variant:
 * @type: a Telepathy socket address type
 * @variant: an initialised #GValue containing an address variant
 * @error: return location for a #GError (or NULL)
 *
 * Converts an address variant stored in a #GValue into a #GSocketAddress that
 * can be used to make a socket connection with GIO.
 *
 * Returns: a newly allocated #GSocketAddress for the given variant, or NULL
 * on error
 */
GSocketAddress *
tp_g_socket_address_from_variant (TpSocketAddressType type,
    const GValue *variant,
    GError **error)
{
  GSocketAddress *addr;

  switch (type)
    {
#ifdef HAVE_GIO_UNIX
      case TP_SOCKET_ADDRESS_TYPE_UNIX:
        if (!G_VALUE_HOLDS (variant, DBUS_TYPE_G_UCHAR_ARRAY))
          {
            g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                "variant is %s not DBUS_TYPE_G_UCHAR_ARRAY",
                G_VALUE_TYPE_NAME (variant));

            return NULL;
          }
        else
          {
            GArray *address = g_value_get_boxed (variant);
            char path[address->len + 1];

            strncpy (path, address->data, address->len);
            path[address->len] = '\0';

            addr = g_unix_socket_address_new (path);
          }
        break;

      case TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX:
        if (!G_VALUE_HOLDS (variant, DBUS_TYPE_G_UCHAR_ARRAY))
          {
            g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                "variant is %s not DBUS_TYPE_G_UCHAR_ARRAY",
                G_VALUE_TYPE_NAME (variant));

            return NULL;
          }
        else
          {
            GArray *address = g_value_get_boxed (variant);

            addr = g_unix_socket_address_new_abstract (
                address->data, address->len);
          }
        break;
#endif /* HAVE_GIO_UNIX */

      case TP_SOCKET_ADDRESS_TYPE_IPV4:
      case TP_SOCKET_ADDRESS_TYPE_IPV6:
        if (type == TP_SOCKET_ADDRESS_TYPE_IPV4 &&
            !G_VALUE_HOLDS (variant, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4))
          {
            g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                "variant is %s not TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4",
                G_VALUE_TYPE_NAME (variant));

            return NULL;
          }
        else if (type == TP_SOCKET_ADDRESS_TYPE_IPV6 &&
            !G_VALUE_HOLDS (variant, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV6))
          {
            g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                "variant is %s not TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV6",
                G_VALUE_TYPE_NAME (variant));

            return NULL;
          }
        else
          {
            GValueArray *array = g_value_get_boxed (variant);
            GValue *hostv = g_value_array_get_nth (array, 0);
            GValue *portv = g_value_array_get_nth (array, 1);
            GInetAddress *address;
            const char *host;
            guint16 port;

            g_return_val_if_fail (G_VALUE_HOLDS_STRING (hostv), NULL);
            g_return_val_if_fail (G_VALUE_HOLDS_UINT (portv), NULL);

            host = g_value_get_string (hostv);
            port = g_value_get_uint (portv);

            address = g_inet_address_new_from_string (host);
            addr = g_inet_socket_address_new (address, port);

            g_object_unref (address);
          }
        break;

      default:
        g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "Unknown TpSocketAddressType (%i)",
            type);

        return NULL;
    }

  return addr;
}

/**
 * tp_address_variant_from_g_socket_address:
 * @address: a #GSocketAddress to convert
 * @type: optional return of the Telepathy socket type (or NULL)
 * @error: return location for a #GError (or NULL)
 *
 * Converts a #GSocketAddress to a #GValue address variant that can be used
 * with Telepathy.
 *
 * Returns: a newly allocated #GValue, free with tp_g_value_slice_free()
 */
GValue *
tp_address_variant_from_g_socket_address (GSocketAddress *address,
    TpSocketAddressType *ret_type,
    GError **error)
{
  GValue *variant;
  TpSocketAddressType type;

  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), NULL);

  switch (g_socket_address_get_family (address))
    {
#ifdef HAVE_GIO_UNIX
      case G_SOCKET_FAMILY_UNIX:
          {
            GUnixSocketAddress *unixaddr = G_UNIX_SOCKET_ADDRESS (address);
            GArray *array;
            const char *path = g_unix_socket_address_get_path (unixaddr);
            gsize len = g_unix_socket_address_get_path_len (unixaddr);

            if (g_unix_socket_address_get_is_abstract (unixaddr))
                type = TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX;
            else
                type = TP_SOCKET_ADDRESS_TYPE_UNIX;

            array = g_array_sized_new (TRUE, FALSE, sizeof (char), len);
            array = g_array_append_vals (array, path, len);

            variant = tp_g_value_slice_new (DBUS_TYPE_G_UCHAR_ARRAY);
            g_value_take_boxed (variant, array);
          }
        break;
#endif /* HAVE_GIO_UNIX */

      case G_SOCKET_FAMILY_IPV4:
      case G_SOCKET_FAMILY_IPV6:
          {
            GInetAddress *addr = g_inet_socket_address_get_address (
                G_INET_SOCKET_ADDRESS (address));
            GValueArray *array;
            char *address_str;
            guint port;

            switch (g_inet_address_get_family (addr))
              {
                case G_SOCKET_FAMILY_IPV4:
                  type = TP_SOCKET_ADDRESS_TYPE_IPV4;
                  variant = tp_g_value_slice_new (TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4);
                  break;

                case G_SOCKET_FAMILY_IPV6:
                  type = TP_SOCKET_ADDRESS_TYPE_IPV6;
                  variant = tp_g_value_slice_new (TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV6);
                  break;

                default:
                  g_assert_not_reached ();
              }

            address_str = g_inet_address_to_string (addr);
            port = g_inet_socket_address_get_port (
                G_INET_SOCKET_ADDRESS (address));

            array = tp_value_array_build (2,
                G_TYPE_STRING, address_str,
                G_TYPE_UINT, port,
                G_TYPE_INVALID);

            g_free (address_str);

            g_value_take_boxed (variant, array);
          }
        break;

      default:
        g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "Unknown GSocketAddressFamily %i",
            g_socket_address_get_family (address));

        return NULL;
    }

  if (ret_type != NULL)
    *ret_type = type;

  return variant;
}
