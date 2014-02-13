/*
 * handle-repo.c - mechanism to store and retrieve handles on a connection
 * (abstract interface)
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

/**
 * SECTION:handle-repo
 * @title: TpHandleRepoIface
 * @short_description: abstract interface for handle allocation
 * @see_also: TpDynamicHandleRepo, TpStaticHandleRepo
 *
 * Abstract interface of a repository for handles, supporting operations
 * which include checking for validity, lookup by
 * string value and lookup by numeric value. See #TpDynamicHandleRepo
 * and #TpStaticHandleRepo for concrete implementations.
 */

#include "config.h"

#include <telepathy-glib/handle-repo.h>

#include <telepathy-glib/handle-repo-internal.h>
#include <telepathy-glib/util-internal.h>

G_DEFINE_INTERFACE (TpHandleRepoIface, tp_handle_repo_iface, G_TYPE_OBJECT);

/**
 * tp_handle_is_valid: (skip)
 * @self: A handle repository implementation
 * @handle: A handle of the type stored in the repository @self
 * @error: Set to InvalidHandle if %FALSE is returned
 *
 * <!--Returns: says it all-->
 *
 * Returns: %TRUE if the handle is nonzero and is present in the repository,
 * else %FALSE
 */

gboolean
tp_handle_is_valid (TpHandleRepoIface *self,
    TpHandle handle,
    GError **error)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->handle_is_valid (self,
      handle, error);
}


/**
 * tp_handles_are_valid: (skip)
 * @self: A handle repository implementation
 * @handles: Array of TpHandle representing handles of the type stored in
 *           the repository @self
 * @allow_zero: If %TRUE, zero is treated like a valid handle
 * @error: Set to InvalidHandle if %FALSE is returned
 *
 * <!--Returns: says it all-->
 *
 * Returns: %TRUE if the handle is present in the repository, else %FALSE
 */

gboolean
tp_handles_are_valid (TpHandleRepoIface *self,
    const GArray *handles,
    gboolean allow_zero,
    GError **error)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->handles_are_valid (self,
      handles, allow_zero, error);
}


/**
 * tp_handle_inspect: (skip)
 * @self: A handle repository implementation
 * @handle: A handle of the type stored in the repository
 *
 * <!--Returns: says it all-->
 *
 * Returns: the string represented by the given handle, or NULL if the
 * handle is absent from the repository. The string is owned by the
 * handle repository and will remain valid as long as a reference to
 * the handle exists.
 */

const char *
tp_handle_inspect (TpHandleRepoIface *self,
    TpHandle handle)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->inspect_handle (self,
      handle);
}


/**
 * tp_handle_ensure:
 * @self: A handle repository implementation
 * @id: A string whose handle is required
 * @context: User data to be passed to the normalization callback
 * @error: Used to return an error if 0 is returned
 *
 * Return a handle for the given string, creating one if necessary. The string
 * is normalized, if possible.
 *
 * Returns: the handle corresponding to the given string, or 0 if it
 * is invalid.
 */

TpHandle
tp_handle_ensure (TpHandleRepoIface *self,
                  const gchar *id,
                  gpointer context,
                  GError **error)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->ensure_handle (self,
      id, context, error);
}

/**
 * tp_handle_ensure_async: (skip)
 * @self: A handle repository implementation
 * @connection: the #TpBaseConnection using this handle repo
 * @id: A string whose handle is required
 * @context: User data to be passed to the normalization callback
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Asyncronously normalize an identifier and create an handle for it. This could
 * involve a server round-trip. This should be used instead of
 * tp_handle_ensure() for user provided contact identifiers, but it is not
 * necessary for identifiers from the server.
 *
 * Since: 0.19.2
 */
void
tp_handle_ensure_async (TpHandleRepoIface *self,
    TpBaseConnection *connection,
    const gchar *id,
    gpointer context,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->ensure_handle_async (self,
      connection, id, context, callback, user_data);
}

/**
 * tp_handle_ensure_finish: (skip)
 * @self: A handle repository implementation
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_handle_ensure_async()
 *
 * Returns: non-0 #TpHandle if the operation was successful, otherwise 0.
 *
 * Since: 0.19.2
 */
TpHandle
tp_handle_ensure_finish (TpHandleRepoIface *self,
    GAsyncResult *result,
    GError **error)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->ensure_handle_finish (self,
      result, error);
}

/**
 * tp_handle_lookup: (skip)
 * @self: A handle repository implementation
 * @id: A string whose handle is required
 * @context: User data to be passed to the normalization callback
 * @error: Used to raise an error if the handle does not exist or is
 *  invalid
 *
 * Return the handle for the given string. The string is normalized if
 * possible. If no handle already exists for the string, none is created.
 *
 * Returns: the handle corresponding to the given string, or 0 if it
 * does not exist or is invalid
 */

TpHandle
tp_handle_lookup (TpHandleRepoIface *self,
                  const gchar *id,
                  gpointer context,
                  GError **error)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (self)->lookup_handle (self,
      id, context, error);
}


static void
default_ensure_handle_async (TpHandleRepoIface *self,
    TpBaseConnection *connection,
    const gchar *id,
    gpointer context,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpHandle handle;
  GError *error = NULL;

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      default_ensure_handle_async);

  handle = tp_handle_ensure (self, id, context, &error);
  if (handle == 0)
    {
      g_simple_async_result_take_error (result, error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (result,
          GUINT_TO_POINTER (handle), NULL);
    }

  g_simple_async_result_complete_in_idle (result);

  g_object_unref (result);
}

static TpHandle
default_ensure_handle_finish (TpHandleRepoIface *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple = (GSimpleAsyncResult *) result;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
      G_OBJECT (self), NULL), 0);

  if (g_simple_async_result_propagate_error (simple, error))
    return 0;

  return GPOINTER_TO_UINT (g_simple_async_result_get_op_res_gpointer (simple));
}

static void
tp_handle_repo_iface_default_init (TpHandleRepoIfaceInterface *iface)
{
  GParamSpec *param_spec;

  iface->ensure_handle_async = default_ensure_handle_async;
  iface->ensure_handle_finish = default_ensure_handle_finish;

  param_spec = g_param_spec_uint ("handle-type", "Handle type",
      "The TpEntityType held in this handle repository.",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_interface_install_property (iface, param_spec);
}
