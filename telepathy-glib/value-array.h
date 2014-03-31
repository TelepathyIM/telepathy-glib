/*
 * value-array.h - GValueArray utility functions
 *
 * Copyright © 2006-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2006-2008 Nokia Corporation
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

#if !defined (_TP_GLIB_DBUS_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib-dbus.h> can be included directly."
#endif

#ifndef __TP_VALUE_ARRAY_H__
#define __TP_VALUE_ARRAY_H__

#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

GValueArray *tp_value_array_build (gsize length,
  GType type,
  ...) G_GNUC_WARN_UNUSED_RESULT;
void tp_value_array_unpack (GValueArray *array,
    gsize len,
    ...);

/* Work around GLib having deprecated something that is part of our API. */
_TP_AVAILABLE_IN_0_24
void tp_value_array_free (GValueArray *va);
#if TP_VERSION_MAX_ALLOWED >= TP_VERSION_0_24
#define tp_value_array_free(va) _tp_value_array_free_inline (va)
#ifndef __GTK_DOC_IGNORE__ /* gtk-doc can't parse this */
static inline void
_tp_value_array_free_inline (GValueArray *va)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_value_array_free (va);
  G_GNUC_END_IGNORE_DEPRECATIONS
}
#endif
#endif

G_END_DECLS

#endif /* single-inclusion guard */
