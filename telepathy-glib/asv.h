/*
 * asv.h - GHashTable<gchar *,GValue *> utilities
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

#ifndef __TP_ASV_H__
#define __TP_ASV_H__

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

GVariant * tp_asv_to_vardict (const GHashTable *asv);

GHashTable * tp_asv_from_vardict (GVariant *variant);

#define tp_asv_size(asv) _tp_asv_size_inline (asv)

static inline guint
_tp_asv_size_inline (const GHashTable *asv)
{
  /* The empty comment here is to stop gtkdoc thinking g_hash_table_size is
   * a declaration. */
  return g_hash_table_size /* */ ((GHashTable *) asv);
}

GHashTable *tp_asv_new (const gchar *first_key, ...)
  G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;
gboolean tp_asv_get_boolean (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_boolean (GHashTable *asv, const gchar *key, gboolean value);
gpointer tp_asv_get_boxed (const GHashTable *asv, const gchar *key,
    GType type);
void tp_asv_set_boxed (GHashTable *asv, const gchar *key, GType type,
    gconstpointer value);
void tp_asv_take_boxed (GHashTable *asv, const gchar *key, GType type,
    gpointer value);
void tp_asv_set_static_boxed (GHashTable *asv, const gchar *key, GType type,
    gconstpointer value);
const GArray *tp_asv_get_bytes (const GHashTable *asv, const gchar *key);
void tp_asv_set_bytes (GHashTable *asv, const gchar *key, guint length,
    gconstpointer bytes);
void tp_asv_take_bytes (GHashTable *asv, const gchar *key, GArray *value);
gdouble tp_asv_get_double (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_double (GHashTable *asv, const gchar *key, gdouble value);
gint32 tp_asv_get_int32 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_int32 (GHashTable *asv, const gchar *key, gint32 value);
gint64 tp_asv_get_int64 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_int64 (GHashTable *asv, const gchar *key, gint64 value);
const gchar *tp_asv_get_object_path (const GHashTable *asv, const gchar *key);
void tp_asv_set_object_path (GHashTable *asv, const gchar *key,
    const gchar *value);
void tp_asv_take_object_path (GHashTable *asv, const gchar *key,
    gchar *value);
void tp_asv_set_static_object_path (GHashTable *asv, const gchar *key,
    const gchar *value);
const gchar *tp_asv_get_string (const GHashTable *asv, const gchar *key);
void tp_asv_set_string (GHashTable *asv, const gchar *key, const gchar *value);
void tp_asv_take_string (GHashTable *asv, const gchar *key, gchar *value);
void tp_asv_set_static_string (GHashTable *asv, const gchar *key,
    const gchar *value);
guint32 tp_asv_get_uint32 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_uint32 (GHashTable *asv, const gchar *key, guint32 value);
guint64 tp_asv_get_uint64 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_uint64 (GHashTable *asv, const gchar *key, guint64 value);
const GValue *tp_asv_lookup (const GHashTable *asv, const gchar *key);

const gchar * const *
/* this comment stops gtkdoc denying that this function exists */
tp_asv_get_strv (const GHashTable *asv, const gchar *key);
void tp_asv_set_strv (GHashTable *asv, const gchar *key, gchar **value);
void tp_asv_dump (GHashTable *asv);

G_END_DECLS

#endif /* single-inclusion guard */
