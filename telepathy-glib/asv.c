/*
 * asv.c - GHashTable<gchar *,GValue *> utilities
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

#include "config.h"
#include <telepathy-glib/asv.h>

#include <string.h>

#include <glib.h>
#include <gobject/gvaluecollector.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/sliced-gvalue.h>

/* this is the core library, we don't have debug infrastructure yet */
#define CRITICAL(format, ...) \
  g_log (G_LOG_DOMAIN "/misc", G_LOG_LEVEL_CRITICAL, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)
#define WARNING(format, ...) \
  g_log (G_LOG_DOMAIN "/misc", G_LOG_LEVEL_WARNING, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)

/**
 * SECTION:asv
 * @title: Manipulating a{sv} mappings
 * @short_description: Functions to manipulate mappings from string to
 *  variant, as represented in dbus-glib by a #GHashTable from string
 *  to #GValue
 *
 * Mappings from string to variant (D-Bus signature a{sv}) are commonly used
 * to provide extensibility, but in dbus-glib they're somewhat awkward to deal
 * with.
 *
 * These functions provide convenient access to the values in such
 * a mapping.
 *
 * They also work around the fact that none of the #GHashTable public API
 * takes a const pointer to a #GHashTable, even the read-only methods that
 * logically ought to.
 *
 * Parts of telepathy-glib return const pointers to #GHashTable, to encourage
 * the use of this API.
 *
 * Since: 0.7.9
 */

/**
 * tp_asv_size: (skip)
 * @asv: a GHashTable
 *
 * Return the size of @asv as if via g_hash_table_size().
 *
 * The only difference is that this version takes a const #GHashTable and
 * casts it.
 *
 * Since: 0.7.12
 */
/* (#define + static inline in dbus.h) */

/**
 * tp_asv_new: (skip)
 * @first_key: the name of the first key (or NULL)
 * @...: type and value for the first key, followed by a NULL-terminated list
 *  of (key, type, value) tuples
 *
 * Creates a new #GHashTable for use with a{sv} maps, containing the values
 * passed in as parameters.
 *
 * The #GHashTable is synonymous with:
 * <informalexample><programlisting>
 * GHashTable *asv = g_hash_table_new_full (g_str_hash, g_str_equal,
 *    NULL, (GDestroyNotify) tp_g_value_slice_free);
 * </programlisting></informalexample>
 * Followed by manual insertion of each of the parameters.
 *
 * Parameters are stored in slice-allocated GValues and should be set using
 * tp_asv_set_*() and retrieved using tp_asv_get_*().
 *
 * tp_g_value_slice_new() and tp_g_value_slice_dup() may also be used to insert
 * into the map if required.
 * <informalexample><programlisting>
 * g_hash_table_insert (parameters, "account",
 *    tp_g_value_slice_new_string ("bob@mcbadgers.com"));
 * </programlisting></informalexample>
 *
 * <example>
 *  <title>Using tp_asv_new()</title>
 *  <programlisting>
 * GHashTable *parameters = tp_asv_new (
 *    "answer", G_TYPE_INT, 42,
 *    "question", G_TYPE_STRING, "We just don't know",
 *    NULL);</programlisting>
 * </example>
 *
 * Allocated values will be automatically free'd when overwritten, removed or
 * the hash table destroyed with g_hash_table_unref().
 *
 * Returns: a newly created #GHashTable for storing a{sv} maps, free with
 * g_hash_table_unref().
 * Since: 0.7.29
 */
GHashTable *
tp_asv_new (const gchar *first_key, ...)
{
  va_list var_args;
  char *key;
  GType type;
  GValue *value;
  char *error = NULL; /* NB: not a GError! */

  /* create a GHashTable */
  GHashTable *asv = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);

  va_start (var_args, first_key);

  for (key = (char *) first_key; key != NULL; key = va_arg (var_args, char *))
  {
    type = va_arg (var_args, GType);

    value = tp_g_value_slice_new (type);
    G_VALUE_COLLECT (value, var_args, 0, &error);

    if (error != NULL)
    {
      CRITICAL ("key %s: %s", key, error);
      g_free (error);
      error = NULL;
      tp_g_value_slice_free (value);
      continue;
    }

    g_hash_table_insert (asv, key, value);
  }

  va_end (var_args);

  return asv;
}

/**
 * tp_asv_get_boolean:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location to store %TRUE if the key actually
 *  exists and has a boolean value
 *
 * If a value for @key in @asv is present and boolean, return it,
 * and set *@valid to %TRUE if @valid is not %NULL.
 *
 * Otherwise return %FALSE, and set *@valid to %FALSE if @valid is not %NULL.
 *
 * Returns: a boolean value for @key
 * Since: 0.7.9
 */
gboolean
tp_asv_get_boolean (const GHashTable *asv,
                    const gchar *key,
                    gboolean *valid)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
    {
      if (valid != NULL)
        *valid = FALSE;

      return FALSE;
    }

  if (valid != NULL)
    *valid = TRUE;

  return g_value_get_boolean (value);
}

/**
 * tp_asv_set_boolean: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_boolean(), tp_g_value_slice_new_boolean()
 * Since: 0.7.29
 */
void
tp_asv_set_boolean (GHashTable *asv,
                    const gchar *key,
                    gboolean value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_boolean (value));
}

/**
 * tp_asv_get_bytes:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 *
 * If a value for @key in @asv is present and is an array of bytes
 * (its GType is %DBUS_TYPE_G_UCHAR_ARRAY), return it.
 *
 * Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it with
 * g_boxed_copy (DBUS_TYPE_G_UCHAR_ARRAY, ...) if you need to keep
 * it for longer.
 *
 * Returns: (transfer none) (allow-none) (element-type guint8): the string value
 * of @key, or %NULL
 * Since: 0.7.9
 */
const GArray *
tp_asv_get_bytes (const GHashTable *asv,
                   const gchar *key)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS (value, DBUS_TYPE_G_UCHAR_ARRAY))
    return NULL;

  return g_value_get_boxed (value);
}

/**
 * tp_asv_set_bytes: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @length: the number of bytes to copy
 * @bytes: location of an array of bytes to be copied (this may be %NULL
 * if and only if length is 0)
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_bytes(), tp_g_value_slice_new_bytes()
 * Since: 0.7.29
 */
void
tp_asv_set_bytes (GHashTable *asv,
                  const gchar *key,
                  guint length,
                  gconstpointer bytes)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (!(length > 0 && bytes == NULL));

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_bytes (length, bytes));
}

/**
 * tp_asv_take_bytes: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: a non-NULL #GArray of %guchar, ownership of which will be taken by
 * the #GValue
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_bytes(), tp_g_value_slice_new_take_bytes()
 * Since: 0.7.29
 */
void
tp_asv_take_bytes (GHashTable *asv,
                   const gchar *key,
                   GArray *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_take_bytes (value));
}

/**
 * tp_asv_get_string:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 *
 * If a value for @key in @asv is present and is a string, return it.
 *
 * Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it with g_strdup() if you
 * need to keep it for longer.
 *
 * Returns: (transfer none) (allow-none): the string value of @key, or %NULL
 * Since: 0.7.9
 */
const gchar *
tp_asv_get_string (const GHashTable *asv,
                   const gchar *key)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS_STRING (value))
    return NULL;

  return g_value_get_string (value);
}

/**
 * tp_asv_set_string: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_string(), tp_g_value_slice_new_string()
 * Since: 0.7.29
 */
void
tp_asv_set_string (GHashTable *asv,
                   const gchar *key,
                   const gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_string (value));
}

/**
 * tp_asv_take_string: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_string(),
 * tp_g_value_slice_new_take_string()
 * Since: 0.7.29
 */
void
tp_asv_take_string (GHashTable *asv,
                    const gchar *key,
                    gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_take_string (value));
}

/**
 * tp_asv_set_static_string: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_string(),
 * tp_g_value_slice_new_static_string()
 * Since: 0.7.29
 */
void
tp_asv_set_static_string (GHashTable *asv,
                          const gchar *key,
                          const gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_static_string (value));
}

/**
 * tp_asv_get_int32:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location in which to store %TRUE on success
 * or %FALSE on failure
 *
 * If a value for @key in @asv is present, has an integer type used by
 * dbus-glib (guchar, gint, guint, gint64 or guint64) and fits in the
 * range of a gint32, return it, and if @valid is not %NULL, set *@valid to
 * %TRUE.
 *
 * Otherwise, return 0, and if @valid is not %NULL, set *@valid to %FALSE.
 *
 * Returns: the 32-bit signed integer value of @key, or 0
 * Since: 0.7.9
 */
gint32
tp_asv_get_int32 (const GHashTable *asv,
                  const gchar *key,
                  gboolean *valid)
{
  gint64 i;
  guint64 u;
  gint32 ret;
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL)
    goto return_invalid;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_UCHAR:
      ret = g_value_get_uchar (value);
      break;

    case G_TYPE_UINT:
      u = g_value_get_uint (value);

      if (G_UNLIKELY (u > G_MAXINT32))
        goto return_invalid;

      ret = u;
      break;

    case G_TYPE_INT:
      ret = g_value_get_int (value);
      break;

    case G_TYPE_INT64:
      i = g_value_get_int64 (value);

      if (G_UNLIKELY (i < G_MININT32 || i > G_MAXINT32))
        goto return_invalid;

      ret = i;
      break;

    case G_TYPE_UINT64:
      u = g_value_get_uint64 (value);

      if (G_UNLIKELY (u > G_MAXINT32))
        goto return_invalid;

      ret = u;
      break;

    default:
      goto return_invalid;
    }

  if (valid != NULL)
    *valid = TRUE;

  return ret;

return_invalid:
  if (valid != NULL)
    *valid = FALSE;

  return 0;
}

/**
 * tp_asv_set_int32: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_int32(), tp_g_value_slice_new_int()
 * Since: 0.7.29
 */
void
tp_asv_set_int32 (GHashTable *asv,
                  const gchar *key,
                  gint32 value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_int (value));
}

/**
 * tp_asv_get_uint32:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location in which to store %TRUE on success
 * or %FALSE on failure
 *
 * If a value for @key in @asv is present, has an integer type used by
 * dbus-glib (guchar, gint, guint, gint64 or guint64) and fits in the
 * range of a guint32, return it, and if @valid is not %NULL, set *@valid to
 * %TRUE.
 *
 * Otherwise, return 0, and if @valid is not %NULL, set *@valid to %FALSE.
 *
 * Returns: the 32-bit unsigned integer value of @key, or 0
 * Since: 0.7.9
 */
guint32
tp_asv_get_uint32 (const GHashTable *asv,
                   const gchar *key,
                   gboolean *valid)
{
  gint64 i;
  guint64 u;
  guint32 ret;
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL)
    goto return_invalid;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_UCHAR:
      ret = g_value_get_uchar (value);
      break;

    case G_TYPE_UINT:
      ret = g_value_get_uint (value);
      break;

    case G_TYPE_INT:
      i = g_value_get_int (value);

      if (G_UNLIKELY (i < 0))
        goto return_invalid;

      ret = i;
      break;

    case G_TYPE_INT64:
      i = g_value_get_int64 (value);

      if (G_UNLIKELY (i < 0 || i > G_MAXUINT32))
        goto return_invalid;

      ret = i;
      break;

    case G_TYPE_UINT64:
      u = g_value_get_uint64 (value);

      if (G_UNLIKELY (u > G_MAXUINT32))
        goto return_invalid;

      ret = u;
      break;

    default:
      goto return_invalid;
    }

  if (valid != NULL)
    *valid = TRUE;

  return ret;

return_invalid:
  if (valid != NULL)
    *valid = FALSE;

  return 0;
}

/**
 * tp_asv_set_uint32: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_uint32(), tp_g_value_slice_new_uint()
 * Since: 0.7.29
 */
void
tp_asv_set_uint32 (GHashTable *asv,
                   const gchar *key,
                   guint32 value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_uint (value));
}

/**
 * tp_asv_get_int64:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location in which to store %TRUE on success
 * or %FALSE on failure
 *
 * If a value for @key in @asv is present, has an integer type used by
 * dbus-glib (guchar, gint, guint, gint64 or guint64) and fits in the
 * range of a gint64, return it, and if @valid is not %NULL, set *@valid to
 * %TRUE.
 *
 * Otherwise, return 0, and if @valid is not %NULL, set *@valid to %FALSE.
 *
 * Returns: the 64-bit signed integer value of @key, or 0
 * Since: 0.7.9
 */
gint64
tp_asv_get_int64 (const GHashTable *asv,
                  const gchar *key,
                  gboolean *valid)
{
  gint64 ret;
  guint64 u;
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL)
    goto return_invalid;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_UCHAR:
      ret = g_value_get_uchar (value);
      break;

    case G_TYPE_UINT:
      ret = g_value_get_uint (value);
      break;

    case G_TYPE_INT:
      ret = g_value_get_int (value);
      break;

    case G_TYPE_INT64:
      ret = g_value_get_int64 (value);
      break;

    case G_TYPE_UINT64:
      u = g_value_get_uint64 (value);

      if (G_UNLIKELY (u > G_MAXINT64))
        goto return_invalid;

      ret = u;
      break;

    default:
      goto return_invalid;
    }

  if (valid != NULL)
    *valid = TRUE;

  return ret;

return_invalid:
  if (valid != NULL)
    *valid = FALSE;

  return 0;
}

/**
 * tp_asv_set_int64: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_int64(), tp_g_value_slice_new_int64()
 * Since: 0.7.29
 */
void
tp_asv_set_int64 (GHashTable *asv,
                  const gchar *key,
                  gint64 value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_int64 (value));
}

/**
 * tp_asv_get_uint64:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location in which to store %TRUE on success
 * or %FALSE on failure
 *
 * If a value for @key in @asv is present, has an integer type used by
 * dbus-glib (guchar, gint, guint, gint64 or guint64) and is non-negative,
 * return it, and if @valid is not %NULL, set *@valid to %TRUE.
 *
 * Otherwise, return 0, and if @valid is not %NULL, set *@valid to %FALSE.
 *
 * Returns: the 64-bit unsigned integer value of @key, or 0
 * Since: 0.7.9
 */
guint64
tp_asv_get_uint64 (const GHashTable *asv,
                   const gchar *key,
                   gboolean *valid)
{
  gint64 tmp;
  guint64 ret;
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL)
    goto return_invalid;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_UCHAR:
      ret = g_value_get_uchar (value);
      break;

    case G_TYPE_UINT:
      ret = g_value_get_uint (value);
      break;

    case G_TYPE_INT:
      tmp = g_value_get_int (value);

      if (G_UNLIKELY (tmp < 0))
        goto return_invalid;

      ret = tmp;
      break;

    case G_TYPE_INT64:
      tmp = g_value_get_int64 (value);

      if (G_UNLIKELY (tmp < 0))
        goto return_invalid;

      ret = tmp;
      break;

    case G_TYPE_UINT64:
      ret = g_value_get_uint64 (value);
      break;

    default:
      goto return_invalid;
    }

  if (valid != NULL)
    *valid = TRUE;

  return ret;

return_invalid:
  if (valid != NULL)
    *valid = FALSE;

  return 0;
}

/**
 * tp_asv_set_uint64: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_uint64(), tp_g_value_slice_new_uint64()
 * Since: 0.7.29
 */
void
tp_asv_set_uint64 (GHashTable *asv,
                   const gchar *key,
                   guint64 value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_uint64 (value));
}

/**
 * tp_asv_get_double:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @valid: (out): Either %NULL, or a location in which to store %TRUE on success
 * or %FALSE on failure
 *
 * If a value for @key in @asv is present and has any numeric type used by
 * dbus-glib (guchar, gint, guint, gint64, guint64 or gdouble),
 * return it as a double, and if @valid is not %NULL, set *@valid to %TRUE.
 *
 * Otherwise, return 0.0, and if @valid is not %NULL, set *@valid to %FALSE.
 *
 * Returns: the double precision floating-point value of @key, or 0.0
 * Since: 0.7.9
 */
gdouble
tp_asv_get_double (const GHashTable *asv,
                   const gchar *key,
                   gboolean *valid)
{
  gdouble ret;
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0.0);
  g_return_val_if_fail (key != NULL, 0.0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL)
    goto return_invalid;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_DOUBLE:
      ret = g_value_get_double (value);
      break;

    case G_TYPE_UCHAR:
      ret = g_value_get_uchar (value);
      break;

    case G_TYPE_UINT:
      ret = g_value_get_uint (value);
      break;

    case G_TYPE_INT:
      ret = g_value_get_int (value);
      break;

    case G_TYPE_INT64:
      ret = g_value_get_int64 (value);
      break;

    case G_TYPE_UINT64:
      ret = g_value_get_uint64 (value);
      break;

    default:
      goto return_invalid;
    }

  if (valid != NULL)
    *valid = TRUE;

  return ret;

return_invalid:
  if (valid != NULL)
    *valid = FALSE;

  return 0;
}

/**
 * tp_asv_set_double: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_double(), tp_g_value_slice_new_double()
 * Since: 0.7.29
 */
void
tp_asv_set_double (GHashTable *asv,
                   const gchar *key,
                   gdouble value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key, tp_g_value_slice_new_double (value));
}

/**
 * tp_asv_get_object_path:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 *
 * If a value for @key in @asv is present and is an object path, return it.
 *
 * Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it with g_strdup() if you
 * need to keep it for longer.
 *
 * Returns: (transfer none) (allow-none): the object-path value of @key, or
 * %NULL
 * Since: 0.7.9
 */
const gchar *
tp_asv_get_object_path (const GHashTable *asv,
                        const gchar *key)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, 0);
  g_return_val_if_fail (key != NULL, 0);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS (value, DBUS_TYPE_G_OBJECT_PATH))
    return NULL;

  return g_value_get_boxed (value);
}

/**
 * tp_asv_set_object_path: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_object_path(),
 * tp_g_value_slice_new_object_path()
 * Since: 0.7.29
 */
void
tp_asv_set_object_path (GHashTable *asv,
                        const gchar *key,
                        const gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_object_path (value));
}

/**
 * tp_asv_take_object_path: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_object_path(),
 * tp_g_value_slice_new_take_object_path()
 * Since: 0.7.29
 */
void
tp_asv_take_object_path (GHashTable *asv,
                         const gchar *key,
                         gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_take_object_path (value));
}

/**
 * tp_asv_set_static_object_path: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_object_path(),
 * tp_g_value_slice_new_static_object_path()
 * Since: 0.7.29
 */
void
tp_asv_set_static_object_path (GHashTable *asv,
                               const gchar *key,
                               const gchar *value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_static_object_path (value));
}

/**
 * tp_asv_get_boxed:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 * @type: The type that the key's value should have, which must be derived
 *  from %G_TYPE_BOXED
 *
 * If a value for @key in @asv is present and is of the desired type,
 * return it.
 *
 * Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it, for instance with
 * g_boxed_copy(), if you need to keep it for longer.
 *
 * Returns: (transfer none) (allow-none): the value of @key, or %NULL
 * Since: 0.7.9
 */
gpointer
tp_asv_get_boxed (const GHashTable *asv,
                  const gchar *key,
                  GType type)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);
  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS (value, type))
    return NULL;

  return g_value_get_boxed (value);
}

/**
 * tp_asv_set_boxed: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @type: the type of the key's value, which must be derived from %G_TYPE_BOXED
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_boxed(), tp_g_value_slice_new_boxed()
 * Since: 0.7.29
 */
void
tp_asv_set_boxed (GHashTable *asv,
                  const gchar *key,
                  GType type,
                  gconstpointer value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_boxed (type, value));
}

/**
 * tp_asv_take_boxed: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @type: the type of the key's value, which must be derived from %G_TYPE_BOXED
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_boxed(), tp_g_value_slice_new_take_boxed()
 * Since: 0.7.29
 */
void
tp_asv_take_boxed (GHashTable *asv,
                   const gchar *key,
                   GType type,
                   gpointer value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_take_boxed (type, value));
}

/**
 * tp_asv_set_static_boxed: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @type: the type of the key's value, which must be derived from %G_TYPE_BOXED
 * @value: value
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_boxed(),
 * tp_g_value_slice_new_static_boxed()
 * Since: 0.7.29
 */
void
tp_asv_set_static_boxed (GHashTable *asv,
                         const gchar *key,
                         GType type,
                         gconstpointer value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_static_boxed (type, value));
}

/**
 * tp_asv_get_strv:
 * @asv: (element-type utf8 GObject.Value): A GHashTable where the keys are
 * strings and the values are GValues
 * @key: The key to look up
 *
 * If a value for @key in @asv is present and is an array of strings (strv),
 * return it.
 *
 * Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it with g_strdupv() if you
 * need to keep it for longer.
 *
 * Returns: (transfer none) (allow-none): the %NULL-terminated string-array
 * value of @key, or %NULL
 * Since: 0.7.9
 */
const gchar * const *
tp_asv_get_strv (const GHashTable *asv,
                 const gchar *key)
{
  GValue *value;

  g_return_val_if_fail (asv != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  value = g_hash_table_lookup ((GHashTable *) asv, key);

  if (value == NULL || !G_VALUE_HOLDS (value, G_TYPE_STRV))
    return NULL;

  return g_value_get_boxed (value);
}

/**
 * tp_asv_set_strv: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 * @key: string key
 * @value: a %NULL-terminated string array
 *
 * Stores the value in the map.
 *
 * The value is stored as a slice-allocated GValue.
 *
 * See Also: tp_asv_new(), tp_asv_get_strv()
 * Since: 0.7.29
 */
void
tp_asv_set_strv (GHashTable *asv,
                 const gchar *key,
                 gchar **value)
{
  g_return_if_fail (asv != NULL);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (asv, (char *) key,
      tp_g_value_slice_new_boxed (G_TYPE_STRV, value));
}

/**
 * tp_asv_lookup: (skip)
 * @asv: A GHashTable where the keys are strings and the values are GValues
 * @key: The key to look up
 *
 * If a value for @key in @asv is present, return it. Otherwise return %NULL.
 *
 * The returned value is not copied, and is only valid as long as the value
 * for @key in @asv is not removed or altered. Copy it with (for instance)
 * g_value_copy() if you need to keep it for longer.
 *
 * Returns: the value of @key, or %NULL
 * Since: 0.7.9
 */
const GValue *
tp_asv_lookup (const GHashTable *asv,
               const gchar *key)
{
  g_return_val_if_fail (asv != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_hash_table_lookup ((GHashTable *) asv, key);
}

/**
 * tp_asv_dump: (skip)
 * @asv: a #GHashTable created with tp_asv_new()
 *
 * Dumps the a{sv} map to the debugging console.
 *
 * The purpose of this function is give the programmer the ability to easily
 * inspect the contents of an a{sv} map for debugging purposes.
 */
void
tp_asv_dump (GHashTable *asv)
{
  GHashTableIter iter;
  char *key;
  GValue *value;

  g_return_if_fail (asv != NULL);

  g_debug ("{");

  g_hash_table_iter_init (&iter, asv);
  while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value))
  {
    char *str = g_strdup_value_contents (value);
    g_debug ("  '%s' : %s", key, str);
    g_free (str);
  }

  g_debug ("}");
}
