/*
 * dbus-properties-mixin.h - D-Bus core Properties
 * Copyright (C) 2008 Collabora Ltd.
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

#ifndef __TP_DBUS_PROPERTIES_MIXIN_H__
#define __TP_DBUS_PROPERTIES_MIXIN_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* ---- Semi-abstract property definition (used in TpSvc*) ---------- */

typedef enum {
    TP_DBUS_PROPERTIES_MIXIN_FLAG_READ = 1,
    TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE = 2
} TpDBusPropertiesMixinFlags;

typedef struct {
    GQuark name;
    TpDBusPropertiesMixinFlags flags;
    gchar *dbus_signature;
    GType type;
    /*<private>*/
    GCallback _1;
    GCallback _2;
} TpDBusPropertiesMixinPropInfo;

typedef struct {
    GQuark dbus_interface;
    TpDBusPropertiesMixinPropInfo *props;
    /*<private>*/
    GCallback _1;
    GCallback _2;
} TpDBusPropertiesMixinIfaceInfo;

void tp_svc_interface_set_dbus_properties_info (GType g_interface,
    TpDBusPropertiesMixinIfaceInfo *info);

/* ---- Concrete implementation (in GObject subclasses) ------------- */

typedef void (*TpDBusPropertiesMixinGetter) (GObject *object,
    GQuark interface, GQuark name, GValue *value, gpointer getter_data);

void tp_dbus_properties_mixin_getter_gobject_properties (GObject *object,
    GQuark interface, GQuark name, GValue *value, gpointer getter_data);

typedef gboolean (*TpDBusPropertiesMixinSetter) (GObject *object,
    GQuark interface, GQuark name, const GValue *value, gpointer setter_data,
    GError **error);

gboolean tp_dbus_properties_mixin_setter_gobject_properties (GObject *object,
    GQuark interface, GQuark name, const GValue *value, gpointer setter_data,
    GError **error);

typedef struct {
    const gchar *name;
    gpointer getter_data;
    gpointer setter_data;
    /*<private>*/
    GCallback _1;
    GCallback _2;
    gpointer mixin_priv;
} TpDBusPropertiesMixinPropImpl;

/* this union is to keep ABI if sizeof (GCallback) > sizeof (void *) */
typedef union {
    GCallback _padding;
    gpointer priv;
} _TpDBusPropertiesMixinPaddedPointer;

typedef struct {
    const gchar *name;
    TpDBusPropertiesMixinGetter getter;
    TpDBusPropertiesMixinSetter setter;
    TpDBusPropertiesMixinPropImpl *props;
    /*<private>*/
    GCallback _1;
    GCallback _2;
    _TpDBusPropertiesMixinPaddedPointer mixin_next;
    gpointer mixin_priv;
} TpDBusPropertiesMixinIfaceImpl;

struct _TpDBusPropertiesMixinClass {
    TpDBusPropertiesMixinIfaceImpl *interfaces;
    /*<private>*/
    gpointer _1;
    gpointer _2;
    gpointer _3;
    gpointer _4;
    gpointer _5;
    gpointer _6;
    gpointer _7;
};

typedef struct _TpDBusPropertiesMixinClass TpDBusPropertiesMixinClass;

void tp_dbus_properties_mixin_class_init (GObjectClass *cls,
    gsize offset);

void tp_dbus_properties_mixin_implement_interface (GObjectClass *cls,
    GQuark iface, TpDBusPropertiesMixinGetter getter,
    TpDBusPropertiesMixinSetter setter, TpDBusPropertiesMixinPropImpl *props);

void tp_dbus_properties_mixin_iface_init (gpointer g_iface,
    gpointer iface_data);

gboolean tp_dbus_properties_mixin_get (GObject *self,
    const gchar *interface_name, const gchar *property_name,
    GValue *value, GError **error);

G_END_DECLS

#endif /* #ifndef __TP_DBUS_PROPERTIES_MIXIN_H__ */
