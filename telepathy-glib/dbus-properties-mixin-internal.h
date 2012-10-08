/*<private_header>*/
/*
 * dbus-properties-mixin-internal.h - D-Bus core Properties - internal API
 * Copyright (C) 2008-2012 Collabora Ltd.
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

#ifndef __TP_DBUS_PROPERTIES_MIXIN_INTERNAL_H__
#define __TP_DBUS_PROPERTIES_MIXIN_INTERNAL_H__

#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

GHashTable *_tp_dbus_properties_mixin_get_all (GObject *self,
    const gchar *interface_name);

G_END_DECLS

#endif /* #ifndef __TP_DBUS_PROPERTIES_MIXIN_H__ */
