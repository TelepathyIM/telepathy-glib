/*
 * gnio-unix.c - Source for telepathy-glib GNIO utility functions
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

#include "config.h"

#include <gio/gio.h>

#include <telepathy-glib/gnio-unix.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/util-internal.h>

#include <string.h>

#ifdef __linux__
/* for getsockopt() and setsockopt() */
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#endif

#ifdef HAVE_GIO_UNIX
#include <gio/gunixconnection.h>
#include <gio/gunixsocketaddress.h>
#include <gio/gunixcredentialsmessage.h>
#endif /* HAVE_GIO_UNIX */

#ifdef HAVE_GIO_UNIX
static gboolean
_tp_unix_connection_send_credentials_with_byte (GUnixConnection *connection,
    guchar byte,
    GCancellable *cancellable,
    GError **error)
{
  /* There is not variant of g_unix_connection_send_credentials allowing us to
   * choose the byte sent :( See bgo #629267
   *
   * This code has been copied from glib/gunixconnection.c
   *
   * Copyright © 2009 Codethink Limited
   */
  GCredentials *credentials;
  GSocketControlMessage *scm;
  GSocket *_socket;
  gboolean ret;
  GOutputVector vector;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = FALSE;

  credentials = g_credentials_new ();

  vector.buffer = &byte;
  vector.size = 1;
  scm = g_unix_credentials_message_new_with_credentials (credentials);
  g_object_get (connection, "socket", &_socket, NULL);
  if (g_socket_send_message (_socket,
                             NULL, /* address */
                             &vector,
                             1,
                             &scm,
                             1,
                             G_SOCKET_MSG_NONE,
                             cancellable,
                             error) != 1)
    {
      g_prefix_error (error, "Error sending credentials: ");
      goto out;
    }

  ret = TRUE;

 out:
  g_object_unref (_socket);
  g_object_unref (scm);
  g_object_unref (credentials);
  return ret;
}
#endif

/**
 * tp_unix_connection_send_credentials_with_byte:
 * @connection: a #GUnixConnection
 * @byte: the byte to send with the credentials
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: a #GError to fill
 *
 * A variant of g_unix_connection_send_credentials() allowing you to choose
 * the byte which is send with the credentials
 *
 * Returns: %TRUE on success, %FALSE if error is set.
 *
 * Since: 0.13.2
 */
gboolean
tp_unix_connection_send_credentials_with_byte (GSocketConnection *connection,
    guchar byte,
    GCancellable *cancellable,
    GError **error)
{
#ifdef HAVE_GIO_UNIX
  return _tp_unix_connection_send_credentials_with_byte (
      G_UNIX_CONNECTION (connection), byte, cancellable, error);
#else
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
      "Unix sockets not supported");
  return FALSE;
#endif
}

static void
send_credentials_with_byte_async_thread (GSimpleAsyncResult *res,
    GObject *object,
    GCancellable *cancellable)
{
  guchar byte;
  GError *error = NULL;

  byte = GPOINTER_TO_UINT (g_simple_async_result_get_op_res_gpointer (res));

  if (!tp_unix_connection_send_credentials_with_byte (
          (GSocketConnection *) object, byte, cancellable, &error))
    {
      g_simple_async_result_take_error (res, error);
    }
}

/**
 * tp_unix_connection_send_credentials_with_byte_async:
 * @connection: A #GUnixConnection.
 * @byte: the byte to send with the credentials
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously send credentials.
 *
 * For more details, see tp_unix_connection_send_credentials_with_byte() which
 * is the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * tp_unix_connection_send_credentials_with_byte_finish() to get the result of
 * the operation.
 *
 * Since: 0.17.5
 **/
void
tp_unix_connection_send_credentials_with_byte_async (
    GSocketConnection *connection,
    guchar byte,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (connection), callback, user_data,
      tp_unix_connection_send_credentials_with_byte_async);

  /* Extra casting to guint to work around GNOME#661546 for GLib < 2.32 */
  g_simple_async_result_set_op_res_gpointer (res,
      GUINT_TO_POINTER ((guint) byte), NULL);

  g_simple_async_result_run_in_thread (res,
      send_credentials_with_byte_async_thread, G_PRIORITY_DEFAULT, cancellable);

  g_object_unref (res);
}

/**
 * tp_unix_connection_send_credentials_with_byte_finish:
 * @connection: A #GUnixConnection.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous send credentials operation started with
 * tp_unix_connection_send_credentials_with_byte_async().
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.17.5
 **/
gboolean
tp_unix_connection_send_credentials_with_byte_finish (
    GSocketConnection *connection,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (connection,
      tp_unix_connection_send_credentials_with_byte_async);
}

#ifdef HAVE_GIO_UNIX
static GCredentials *
_tp_unix_connection_receive_credentials_with_byte (GUnixConnection *connection,
    guchar *byte,
    GCancellable *cancellable,
    GError **error)
{
  /* There is not variant of g_unix_connection_receive_credentials allowing us
   * to choose the byte sent :( See bgo #629267
   *
   * This code has been copied from glib/gunixconnection.c
   *
   * Copyright © 2009 Codethink Limited
   */
  GCredentials *ret;
  GSocketControlMessage **scms;
  gint nscm;
  GSocket *_socket;
  gint n;
  gssize num_bytes_read;
#ifdef __linux__
  gboolean turn_off_so_passcreds;
#endif
  GInputVector vector;
  guchar buffer[1];

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;
  scms = NULL;

  g_object_get (connection, "socket", &_socket, NULL);

  /* On Linux, we need to turn on SO_PASSCRED if it isn't enabled
   * already. We also need to turn it off when we're done.  See
   * #617483 for more discussion.
   */
#ifdef __linux__
  {
    gint opt_val;
    socklen_t opt_len;

    turn_off_so_passcreds = FALSE;
    opt_val = 0;
    opt_len = sizeof (gint);
    if (getsockopt (g_socket_get_fd (_socket),
                    SOL_SOCKET,
                    SO_PASSCRED,
                    &opt_val,
                    &opt_len) != 0)
      {
        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_errno (errno),
                     "Error checking if SO_PASSCRED is enabled for socket: %s",
                     strerror (errno));
        goto out;
      }
    if (opt_len != sizeof (gint))
      {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Unexpected option length while checking if SO_PASSCRED is enabled for socket. "
                       "Expected %d bytes, got %d",
                     (gint) sizeof (gint), (gint) opt_len);
        goto out;
      }
    if (opt_val == 0)
      {
        opt_val = 1;
        if (setsockopt (g_socket_get_fd (_socket),
                        SOL_SOCKET,
                        SO_PASSCRED,
                        &opt_val,
                        sizeof opt_val) != 0)
          {
            g_set_error (error,
                         G_IO_ERROR,
                         g_io_error_from_errno (errno),
                         "Error enabling SO_PASSCRED: %s",
                         strerror (errno));
            goto out;
          }
        turn_off_so_passcreds = TRUE;
      }
  }
#endif

  vector.buffer = buffer;
  vector.size = 1;

  /* ensure the type of GUnixCredentialsMessage has been registered with the type system */
  (void) (G_TYPE_UNIX_CREDENTIALS_MESSAGE);
  num_bytes_read = g_socket_receive_message (_socket,
                                             NULL, /* GSocketAddress **address */
                                             &vector,
                                             1,
                                             &scms,
                                             &nscm,
                                             NULL,
                                             cancellable,
                                             error);
  if (num_bytes_read != 1)
    {
      /* Handle situation where g_socket_receive_message() returns
       * 0 bytes and not setting @error
       */
      if (num_bytes_read == 0 && error != NULL && *error == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Expecting to read a single byte for receiving credentials but read zero bytes");
        }
      goto out;
    }

  if (nscm != 1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Expecting 1 control message, got %d",
                   nscm);
      goto out;
    }

  if (!G_IS_UNIX_CREDENTIALS_MESSAGE (scms[0]))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Unexpected type of ancillary data");
      goto out;
    }

  if (byte != NULL)
    {
      *byte = buffer[0];
    }

  ret = g_unix_credentials_message_get_credentials (G_UNIX_CREDENTIALS_MESSAGE (scms[0]));
  g_object_ref (ret);

 out:

#ifdef __linux__
  if (turn_off_so_passcreds)
    {
      gint opt_val;
      opt_val = 0;
      if (setsockopt (g_socket_get_fd (_socket),
                      SOL_SOCKET,
                      SO_PASSCRED,
                      &opt_val,
                      sizeof opt_val) != 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       "Error while disabling SO_PASSCRED: %s",
                       strerror (errno));
          goto out;
        }
    }
#endif

  if (scms != NULL)
    {
      for (n = 0; n < nscm; n++)
        g_object_unref (scms[n]);
      g_free (scms);
    }
  g_object_unref (_socket);
  return ret;
}
#endif

/**
 * tp_unix_connection_receive_credentials_with_byte
 * @connection: a #GUnixConnection
 * @byte: (out): if not %NULL, used to return the byte
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: a #GError to fill
 *
 * A variant of g_unix_connection_receive_credentials() allowing you to get
 * the byte which has been received with the credentials.
 *
 * Returns: (transfer full): Received credentials on success (free with
 * g_object_unref()), %NULL if error is set.
 *
 * Since: 0.13.2
 */
GCredentials *
tp_unix_connection_receive_credentials_with_byte (GSocketConnection *connection,
    guchar *byte,
    GCancellable *cancellable,
    GError **error)
{
#ifdef HAVE_GIO_UNIX
  return _tp_unix_connection_receive_credentials_with_byte (
      G_UNIX_CONNECTION (connection), byte, cancellable, error);
#else
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
      "Unix sockets not supported");
  return FALSE;
#endif
}

typedef struct
{
  GCredentials *creds;
  guchar byte;
} ReceiveCredentialsWithByteData;

static ReceiveCredentialsWithByteData *
receive_credentials_with_byte_data_new (GCredentials *creds,
    guchar byte)
{
  ReceiveCredentialsWithByteData *data;

  data = g_slice_new0 (ReceiveCredentialsWithByteData);
  data->creds = g_object_ref (creds);
  data->byte = byte;

  return data;
}

static void
receive_credentials_with_byte_data_free (ReceiveCredentialsWithByteData *data)
{
  g_object_unref (data->creds);
  g_slice_free (ReceiveCredentialsWithByteData, data);
}

static void
receive_credentials_with_byte_async_thread (GSimpleAsyncResult *res,
    GObject *object,
    GCancellable *cancellable)
{
  guchar byte;
  GCredentials *creds;
  GError *error = NULL;

  creds = tp_unix_connection_receive_credentials_with_byte (
      (GSocketConnection *) object, &byte, cancellable, &error);
  if (creds == NULL)
    {
      g_simple_async_result_take_error (res, error);
      return;
    }

  g_simple_async_result_set_op_res_gpointer (res,
      receive_credentials_with_byte_data_new (creds, byte),
      (GDestroyNotify) receive_credentials_with_byte_data_free);

  g_object_unref (creds);
}

/**
 * tp_unix_connection_receive_credentials_with_byte_async:
 * @connection: A #GUnixConnection.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously receive credentials.
 *
 * For more details, see tp_unix_connection_receive_credentials_with_byte()
 * which is the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * tp_unix_connection_receive_credentials_with_byte_finish() to get the result
 * of the operation.
 *
 * Since: 0.17.5
 **/
void
tp_unix_connection_receive_credentials_with_byte_async (
    GSocketConnection *connection,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (connection), callback, user_data,
      tp_unix_connection_receive_credentials_with_byte_async);

  g_simple_async_result_run_in_thread (res,
      receive_credentials_with_byte_async_thread, G_PRIORITY_DEFAULT,
      cancellable);

  g_object_unref (res);
}

/**
 * tp_unix_connection_receive_credentials_with_byte_finish:
 * @connection: A #GUnixConnection.
 * @result: a #GAsyncResult.
 * @byte: (out): if not %NULL, used to return the byte
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous receive credentials operation started with
 * tp_unix_connection_receive_credentials_with_byte_async().
 *
 * Returns: (transfer full): a #GCredentials, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 0.17.5
 **/
GCredentials *
tp_unix_connection_receive_credentials_with_byte_finish (
    GSocketConnection *connection,
    GAsyncResult *result,
    guchar *byte,
    GError **error)
{
  GSimpleAsyncResult *simple = (GSimpleAsyncResult *) result;
  ReceiveCredentialsWithByteData *data;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (connection),
      tp_unix_connection_receive_credentials_with_byte_async),
      NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (byte != NULL)
    *byte = data->byte;

  return g_object_ref (data->creds);
}
