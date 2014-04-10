/*<private_header>*/
/*
 * Copyright © 2014 Collabora Ltd.
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

#ifndef __TP_CORE_DBUS_PROPERTIES_MIXIN_H__
#define __TP_CORE_DBUS_PROPERTIES_MIXIN_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct {
    const gchar *version;

    GVariant *(*dup_variant) (GObject *object,
        const gchar *interface_name,
        const gchar *property_name,
        GError **error);

    gboolean (*set_variant) (GObject *object,
        const gchar *interface_name,
        const gchar *property_name,
        GVariant *value,
        GError **error);

    GVariant *(*dup_all_vardict) (GObject *object,
        const gchar *interface_name);

    gsize size;
} TpDBusPropertiesMixinImpl;

GVariant *_tp_dbus_properties_mixin_dup_in_dbus_lib (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error);

gboolean _tp_dbus_properties_mixin_set_in_dbus_lib (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GVariant *value,
    GError **error);

GVariant *_tp_dbus_properties_mixin_dup_all_in_dbus_lib (GObject *object,
    const gchar *interface_name);

void tp_private_dbus_properties_mixin_set_implementation (
    const TpDBusPropertiesMixinImpl *real_impl);

G_END_DECLS

#endif
