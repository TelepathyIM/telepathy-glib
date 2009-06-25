/*
 * params-cm.h - header for ParamConnectionManager
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

#ifndef __PARAM_CONNECTION_MANAGER_H__
#define __PARAM_CONNECTION_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

typedef struct _ParamConnectionManager
    ParamConnectionManager;
typedef struct _ParamConnectionManagerPrivate
    ParamConnectionManagerPrivate;

typedef struct _ParamConnectionManagerClass
    ParamConnectionManagerClass;
typedef struct _ParamConnectionManagerClassPrivate
    ParamConnectionManagerClassPrivate;

struct _ParamConnectionManagerClass {
    TpBaseConnectionManagerClass parent_class;

    ParamConnectionManagerClassPrivate *priv;
};

struct _ParamConnectionManager {
    TpBaseConnectionManager parent;

    ParamConnectionManagerPrivate *priv;
};

GType param_connection_manager_get_type (void);

/* TYPE MACROS */
#define TYPE_PARAM_CONNECTION_MANAGER \
  (param_connection_manager_get_type ())
#define PARAM_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_PARAM_CONNECTION_MANAGER, \
                              ParamConnectionManager))
#define PARAM_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_PARAM_CONNECTION_MANAGER, \
                           ParamConnectionManagerClass))
#define IS_PARAM_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_PARAM_CONNECTION_MANAGER))
#define IS_PARAM_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_PARAM_CONNECTION_MANAGER))
#define PARAM_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PARAM_CONNECTION_MANAGER, \
                              ParamConnectionManagerClass))

typedef struct {
    gchar *a_string;
    gint a_int16;
    gint a_int32;
    guint a_uint16;
    guint a_uint32;
    gint64 a_int64;
    guint64 a_uint64;
    gboolean a_boolean;
    gdouble a_double;
    GStrv a_array_of_strings;
    GArray *a_array_of_bytes;
    gchar *a_object_path;
} CMParams;

CMParams * param_connection_manager_get_params_last_conn (void);
void param_connection_manager_free_params (CMParams *params);

G_END_DECLS

#endif
