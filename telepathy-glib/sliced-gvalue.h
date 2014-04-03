/*
 * sliced-gvalue.h - slice-allocated GValues
 *
 * Copyright © 2005-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2005-2009 Nokia Corporation
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

#ifndef __TP_SLICED_GVALUE_H__
#define __TP_SLICED_GVALUE_H__

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

/* Functions with _new in their names confuse the g-i scanner, but these
 * are all (skip)'d anyway. See GNOME bug#656743 */
#ifndef __GI_SCANNER__
GValue *tp_g_value_slice_new_bytes (guint length, gconstpointer bytes)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_take_bytes (GArray *bytes)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_object_path (const gchar *path)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_static_object_path (const gchar *path)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_take_object_path (gchar *path)
  G_GNUC_WARN_UNUSED_RESULT;

GValue *tp_g_value_slice_new (GType type) G_GNUC_WARN_UNUSED_RESULT;

GValue *tp_g_value_slice_new_boolean (gboolean b) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_int (gint n) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_int64 (gint64 n) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_byte (guchar n) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_uint (guint n) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_uint64 (guint64 n) G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_double (double d) G_GNUC_WARN_UNUSED_RESULT;

GValue *tp_g_value_slice_new_string (const gchar *string)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_static_string (const gchar *string)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_take_string (gchar *string)
  G_GNUC_WARN_UNUSED_RESULT;

GValue *tp_g_value_slice_new_boxed (GType type, gconstpointer p)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_static_boxed (GType type, gconstpointer p)
  G_GNUC_WARN_UNUSED_RESULT;
GValue *tp_g_value_slice_new_take_boxed (GType type, gpointer p)
  G_GNUC_WARN_UNUSED_RESULT;

#endif

void tp_g_value_slice_free (GValue *value);

GValue *tp_g_value_slice_dup (const GValue *value) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* single-inclusion guard */
