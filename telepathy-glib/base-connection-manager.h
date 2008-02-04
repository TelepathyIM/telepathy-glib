/*
 * base-connection-manager.h - Header for TpBaseConnectionManager
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#ifndef __TP_BASE_CONNECTION_MANAGER_H__
#define __TP_BASE_CONNECTION_MANAGER_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/svc-connection-manager.h>

G_BEGIN_DECLS

typedef struct _TpCMParamSpec TpCMParamSpec;

typedef gboolean (*TpCMParamFilter) (const TpCMParamSpec *paramspec,
    GValue *value, GError **error);

gboolean tp_cm_param_filter_string_nonempty (const TpCMParamSpec *paramspec,
    GValue *value, GError **error);

gboolean tp_cm_param_filter_uint_nonzero (const TpCMParamSpec *paramspec,
    GValue *value, GError **error);

/* XXX: This should be driven by GTypes, but the GType is insufficiently
 * descriptive: if it's UINT we can't tell whether the D-Bus type is
 * UInt32, UInt16 or possibly even Byte. So we have the D-Bus type too.
 *
 * As it stands at the moment it could be driven by the *D-Bus* type, but
 * in future we may want to have more than one possible GType for a D-Bus
 * type, e.g. converting arrays of string into either a strv or a GPtrArray.
 * So, we keep the redundancy for future expansion.
 */

struct _TpCMParamSpec {
    const gchar *name;
    const gchar *dtype;
    GType gtype;
    guint flags;
    gconstpointer def;
    gsize offset;

    TpCMParamFilter filter;
    gconstpointer filter_data;

    gconstpointer setter_data;

    /*<private>*/
    gpointer _future1;
};

typedef void (*TpCMParamSetter) (const TpCMParamSpec *paramspec,
    const GValue *value, gpointer params);

void tp_cm_param_setter_offset (const TpCMParamSpec *paramspec,
    const GValue *value, gpointer params);

typedef struct {
    const gchar *name;
    const TpCMParamSpec *parameters;
    gpointer (*params_new) (void);
    void (*params_free) (gpointer);
    TpCMParamSetter set_param;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
    gpointer _future3;
} TpCMProtocolSpec;

typedef struct _TpBaseConnectionManager TpBaseConnectionManager;

typedef struct _TpBaseConnectionManagerClass TpBaseConnectionManagerClass;

typedef TpBaseConnection *(*TpBaseConnectionManagerNewConnFunc)(
    TpBaseConnectionManager *self, const gchar *proto,
    TpIntSet *params_present, void *parsed_params, GError **error);

struct _TpBaseConnectionManagerClass {
    GObjectClass parent_class;

    const char *cm_dbus_name;
    const TpCMProtocolSpec *protocol_params;
    TpBaseConnectionManagerNewConnFunc new_connection;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
    gpointer _future3;
    gpointer _future4;

    gpointer priv;
};

struct _TpBaseConnectionManager {
    /*<private>*/
    GObject parent;

    gpointer priv;
};

GType tp_base_connection_manager_get_type (void);

gboolean tp_base_connection_manager_register (TpBaseConnectionManager *self);

/* TYPE MACROS */
#define TP_TYPE_BASE_CONNECTION_MANAGER \
  (tp_base_connection_manager_get_type ())
#define TP_BASE_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_BASE_CONNECTION_MANAGER, \
                              TpBaseConnectionManager))
#define TP_BASE_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_BASE_CONNECTION_MANAGER, \
                           TpBaseConnectionManagerClass))
#define TP_IS_BASE_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_BASE_CONNECTION_MANAGER))
#define TP_IS_BASE_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_BASE_CONNECTION_MANAGER))
#define TP_BASE_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_CONNECTION_MANAGER, \
                              TpBaseConnectionManagerClass))

G_END_DECLS

#endif /* #ifndef __TP_BASE_CONNECTION_MANAGER_H__*/
