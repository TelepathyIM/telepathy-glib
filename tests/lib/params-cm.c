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

static const TpCMParamSpec param_example_params[] = {
  { "a-string", "s", G_TYPE_STRING, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_string), NULL, NULL, NULL },
  { "a-int16", "n", G_TYPE_INT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_int16), NULL, NULL, NULL },
  { "a-int32", "i", G_TYPE_INT, 0, NULL,
    G_STRUCT_OFFSET (CMParams, a_int32), NULL, NULL, NULL },
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
free_cm_params (CMParams *p)
{
  g_free (p->a_string);

  g_slice_free (CMParams, p);
}
