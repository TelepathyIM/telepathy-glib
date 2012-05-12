/* Helper to do stuff to Telepathy handles.
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

#include "config.h"

#include "telepathy-glib/connection-internal.h"

#include <telepathy-glib/cli-connection.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#define DEBUG_FLAG TP_DEBUG_HANDLES
#include "telepathy-glib/debug-internal.h"

typedef struct {
    TpHandleType handle_type;
    guint n_ids;
    gchar **ids;
    gpointer user_data;
    GDestroyNotify destroy;
    TpConnectionRequestHandlesCb callback;
} RequestHandlesContext;


static void
request_handles_context_free (gpointer p)
{
  RequestHandlesContext *context = p;

  g_strfreev (context->ids);

  if (context->destroy != NULL)
    context->destroy (context->user_data);

  g_slice_free (RequestHandlesContext, context);
}


/**
 * TpConnectionRequestHandlesCb:
 * @connection: the connection
 * @handle_type: the handle type that was passed to
 *  tp_connection_request_handles()
 * @n_handles: the number of IDs that were passed to
 *  tp_connection_request_handles() on success, or 0 on failure
 * @handles: (element-type uint) (array length=n_handles): the @n_handles
 *  handles corresponding to @ids, in the same order, or %NULL on failure
 * @ids: (element-type utf8) (array length=n_handles): a copy of the array of
 *  @n_handles IDs that was passed to tp_connection_request_handles() on
 *  success, or %NULL on failure
 * @error: %NULL on success, or an error on failure
 * @user_data: the same arbitrary pointer that was passed to
 *  tp_connection_request_handles()
 * @weak_object: the same object that was passed to
 *  tp_connection_request_handles()
 *
 * Signature of the callback called when tp_connection_request_handles()
 * succeeds or fails.
 *
 * On success, the caller has a reference to each handle in @handles.
 *
 * For convenience, the handle type and IDs requested by the caller are
 * passed through to this callback, so the caller does not have to include
 * them in @user_data.
 *
 * Deprecated: See tp_connection_request_handles().
 */


static void
connection_requested_handles (TpConnection *self,
                              const GArray *handles,
                              const GError *error,
                              gpointer user_data,
                              GObject *weak_object)
{
  RequestHandlesContext *context = user_data;

  g_object_ref (self);

  if (error == NULL)
    {
      if (G_UNLIKELY (g_strv_length (context->ids) != handles->len))
        {
          const gchar *cm = tp_proxy_get_bus_name ((TpProxy *) self);
          GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
              "Connection manager %s is broken: we asked for %u "
              "handles but RequestHandles returned %u",
              cm, g_strv_length (context->ids), handles->len);

          /* This CM is bad and wrong. We can't trust it to get anything
           * right. */
          WARNING ("%s", e->message);

          context->callback (self, context->handle_type, 0, NULL, NULL,
              e, context->user_data, weak_object);
          g_error_free (e);
          return;
        }

      DEBUG ("%u handles of type %u", handles->len,
          context->handle_type);
      /* On the Telepathy side, we have held these handles (at least once).
       * That's all we need. */

      context->callback (self, context->handle_type, handles->len,
          (const TpHandle *) handles->data,
          (const gchar * const *) context->ids,
          NULL, context->user_data, weak_object);
    }
  else
    {
      DEBUG ("%u handles of type %u failed: %s %u: %s",
          g_strv_length (context->ids), context->handle_type,
          g_quark_to_string (error->domain), error->code, error->message);
      context->callback (self, context->handle_type, 0, NULL, NULL, error,
          context->user_data, weak_object);
    }

  g_object_unref (self);
}


/**
 * tp_connection_request_handles:
 * @self: a connection
 * @timeout_ms: the timeout in milliseconds, or -1 to use the default
 * @handle_type: the handle type
 * @ids: (array zero-terminated=1): an array of string identifiers for which
 *  handles are required, terminated by %NULL (must not be %NULL or empty)
 * @callback: called on success or failure (unless @weak_object has become
 *  unreferenced)
 * @user_data: arbitrary user-supplied data
 * @destroy: called to destroy @user_data after calling @callback, or when
 *  @weak_object becomes unreferenced (whichever occurs sooner)
 * @weak_object: if not %NULL, an object to be weakly referenced: if it is
 *  destroyed, @callback will not be called
 *
 * Request the handles corresponding to the given identifiers if they
 * are valid.
 *
 * If they are valid, the callback will later be called with the given
 * handles; if not all of them are valid, the callback will be called with
 * an error.
 *
 * Deprecated: If @handle_type is TP_HANDLE_TYPE_CONTACT, use
 *  tp_connection_dup_contact_by_id_async() instead. For channel requests,
 *  use tp_account_channel_request_set_target_id() instead.
 */
void
tp_connection_request_handles (TpConnection *self,
                               gint timeout_ms,
                               TpHandleType handle_type,
                               const gchar * const *ids,
                               TpConnectionRequestHandlesCb callback,
                               gpointer user_data,
                               GDestroyNotify destroy,
                               GObject *weak_object)
{
  RequestHandlesContext *context;

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < TP_NUM_HANDLE_TYPES);
  g_return_if_fail (ids != NULL);
  g_return_if_fail (ids[0] != NULL);
  g_return_if_fail (callback != NULL);

  context = g_slice_new0 (RequestHandlesContext);
  context->handle_type = handle_type;
  context->ids = g_strdupv ((GStrv) ids);
  context->user_data = user_data;
  context->destroy = destroy;
  context->callback = callback;

  tp_cli_connection_call_request_handles (self, timeout_ms, handle_type,
      (const gchar **) context->ids, connection_requested_handles,
      context, request_handles_context_free, weak_object);
}
