/*
 * gtypes.c - Specialized GTypes representing D-Bus structs etc.
 * Copyright (C) 2007 Collabora Ltd.
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

#include <telepathy-glib/gtypes.h>

#include <glib-object.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/util.h>

/**
 * tp_dbus_specialized_value_slice_new:
 * @type: A D-Bus specialized type (i.e. probably a specialized GValueArray
 * representing a D-Bus struct)
 *
 * <!-- -->
 *
 * Returns: a slice-allocated GValue containing an empty value of the
 * given type.
 */
GValue *
tp_dbus_specialized_value_slice_new (GType type)
{
  GValue *value = tp_g_value_slice_new (type);

  g_value_take_boxed (value, dbus_g_type_specialized_construct (type));
  return value;
}

/* auto-generated implementation stubs */
#include "_gen/gtypes-body.h"
