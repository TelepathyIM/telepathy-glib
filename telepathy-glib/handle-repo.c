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

static void
repo_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      GParamSpec *param_spec;

      initialized = TRUE;

      param_spec = g_param_spec_uint ("handle-type", "Handle type",
          "The TpHandleType held in this handle repository.",
          0, G_MAXUINT32, 0,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
      g_object_interface_install_property (klass, param_spec);
    }
}

GType
tp_handle_repo_iface_get_type (void)
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (TpHandleRepoIfaceClass),
        repo_base_init,
        NULL,   /* base_finalize */
        NULL,   /* class_init */
        NULL,   /* class_finalize */
        NULL,   /* class_data */
        0,
        0,      /* n_preallocs */
        NULL    /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE, "TpHandleRepoIface",
          &info, 0);
    }

  return type;
}


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


/**
 * tp_handle_set_qdata: (skip)
 * @repo: A handle repository implementation
 * @handle: A handle to set data on
 * @key_id: Key id to associate data with
 * @data: data to associate with handle
 * @destroy: A #GDestroyNotify to call to destroy the data,
 *           or NULL if not needed.
 *
 * Associates a blob of data with a given handle and a given key
 *
 * If @destroy is set, then the data is freed when the handle is freed.
 *
 * Since version 0.13.8, handles always last as long as the
 * connection, so @destroy will not be called until the connection
 * disconnects.
 */

void
tp_handle_set_qdata (TpHandleRepoIface *repo,
                     TpHandle handle,
                     GQuark key_id,
                     gpointer data,
                     GDestroyNotify destroy)
{
  TP_HANDLE_REPO_IFACE_GET_CLASS (repo)->set_qdata (repo,
      handle, key_id, data, destroy);
}

/**
 * tp_handle_get_qdata: (skip)
 * @repo: A handle repository implementation
 * @handle: A handle to get data from
 * @key_id: Key id of data to fetch
 *
 * <!--Returns: says it all-->
 *
 * Returns: the data associated with a given key on a given handle; %NULL
 * if there is no associated data.
 */
gpointer
tp_handle_get_qdata (TpHandleRepoIface *repo, TpHandle handle,
                     GQuark key_id)
{
  return TP_HANDLE_REPO_IFACE_GET_CLASS (repo)->get_qdata (repo,
      handle, key_id);
}
