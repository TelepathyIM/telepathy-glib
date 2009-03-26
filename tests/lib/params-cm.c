/*
 * params-cm.h - source for ParamConnectionManager
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
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

#include "params-cm.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>

G_DEFINE_TYPE (ParamConnectionManager,
    param_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

struct _ParamConnectionManagerPrivate
{
  int dummy;
};

static void
param_connection_manager_init (
    ParamConnectionManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TYPE_PARAM_CONNECTION_MANAGER,
      ParamConnectionManagerPrivate);
}

enum {
    PARAM_STRING,
    PARAM_INT16,
    PARAM_INT32,
    PARAM_UINT16,
    PARAM_UINT32,
    PARAM_INT64,
    PARAM_UINT64,
    PARAM_BOOLEAN,
    PARAM_DOUBLE,
    PARAM_ARRAY_STRINGS,
    PARAM_ARRAY_BYTES,
    PARAM_OBJECT_PATH,
    NUM_PARAM
};

static TpCMParamSpec param_example_params[] = {
  { "a-string", "s", G_TYPE_STRING, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_string), NULL, NULL, NULL },
  { "a-int16", "n", G_TYPE_INT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_int16), NULL, NULL, NULL },
  { "a-int32", "i", G_TYPE_INT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_int32), NULL, NULL, NULL },
  { "a-uint16", "q", G_TYPE_UINT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_uint16), NULL, NULL, NULL },
  { "a-uint32", "u", G_TYPE_UINT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_uint32), NULL, NULL, NULL },
  { "a-int64", "x", G_TYPE_INT64, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_int64), NULL, NULL, NULL },
  { "a-uint64", "t", G_TYPE_UINT64, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_uint64), NULL, NULL, NULL },
  { "a-boolean", "b", G_TYPE_BOOLEAN, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_boolean), NULL, NULL, NULL },
  { "a-double", "d", G_TYPE_DOUBLE, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_double), NULL, NULL, NULL },
  { "a-array-of-strings", "as", 0, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_array_of_strings), NULL, NULL, NULL },
  { "a-array-of-bytes", "ay", 0, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_array_of_bytes), NULL, NULL, NULL },
  { "a-object-path", "o", 0, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_object_path), NULL, NULL, NULL },
  { NULL }
};

static CMParams *params = NULL;

static gpointer
alloc_params (void)
{
  params = g_slice_new0 (CMParams);

  return params;
}

static void
free_params (gpointer p)
{
  /* CM user is responsible to free params so he can check their values */
}

static const TpCMProtocolSpec example_protocols[] = {
  { "example", param_example_params,
    alloc_params, free_params },
  { NULL, NULL }
};

static TpBaseConnection *
new_connection (TpBaseConnectionManager *self,
                const gchar *proto,
                TpIntSet *params_present,
                gpointer parsed_params,
                GError **error)
{
  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "No connection for you");
  return NULL;
}

static void
param_connection_manager_class_init (
    ParamConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  g_type_class_add_private (klass,
      sizeof (ParamConnectionManagerPrivate));

  param_example_params[PARAM_ARRAY_STRINGS].gtype = G_TYPE_STRV;
  param_example_params[PARAM_ARRAY_BYTES].gtype = DBUS_TYPE_G_UCHAR_ARRAY;
  param_example_params[PARAM_OBJECT_PATH].gtype = DBUS_TYPE_G_OBJECT_PATH;

  base_class->new_connection = new_connection;
  base_class->cm_dbus_name = "params_cm";
  base_class->protocol_params = example_protocols;
}

CMParams *
param_connection_manager_get_params_last_conn (void)
{
  return params;
}

void
param_connection_manager_free_params (CMParams *p)
{
  g_free (p->a_string);
  g_strfreev (p->a_array_of_strings);
  g_array_free (p->a_array_of_bytes, TRUE);
  g_free (p->a_object_path);

  g_slice_free (CMParams, p);
}
