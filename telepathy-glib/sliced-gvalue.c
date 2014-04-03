/*
 * sliced-gvalue.c - slice-allocated GValues
 *
 * Copyright (C) 2005-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2009 Nokia Corporation
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

/**
 * SECTION:sliced-gvalue
 * @title: slice-allocated GValues
 * @short_description: GValue utility functions
 *
 * These functions are a convenient way to slice-allocate GValues
 * with various contents, as used in dbus-glib.
 */

#include "config.h"

#include <telepathy-glib/sliced-gvalue.h>

#include <dbus/dbus-glib.h>

/**
 * tp_g_value_slice_new: (skip)
 * @type: The type desired for the new GValue
 *
 * Slice-allocate an empty #GValue. tp_g_value_slice_new_boolean() and similar
 * functions are likely to be more convenient to use for the types supported.
 *
 * Returns: a newly allocated, newly initialized #GValue, to be freed with
 * tp_g_value_slice_free() or g_slice_free().
 * Since: 0.5.14
 */
GValue *
tp_g_value_slice_new (GType type)
{
  GValue *ret = g_slice_new0 (GValue);

  g_value_init (ret, type);
  return ret;
}

/**
 * tp_g_value_slice_new_boolean: (skip)
 * @b: a boolean value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_BOOLEAN with value @b, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_boolean (gboolean b)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_BOOLEAN);

  g_value_set_boolean (v, b);
  return v;
}

/**
 * tp_g_value_slice_new_int: (skip)
 * @n: an integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_INT with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_int (gint n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_INT);

  g_value_set_int (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_int64: (skip)
 * @n: a 64-bit integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_INT64 with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_int64 (gint64 n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_INT64);

  g_value_set_int64 (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_byte: (skip)
 * @n: an unsigned integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_UCHAR with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.11.0
 */
GValue *
tp_g_value_slice_new_byte (guchar n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_UCHAR);

  g_value_set_uchar (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_uint: (skip)
 * @n: an unsigned integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_UINT with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_uint (guint n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_UINT);

  g_value_set_uint (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_uint64: (skip)
 * @n: a 64-bit unsigned integer
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_UINT64 with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_uint64 (guint64 n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_UINT64);

  g_value_set_uint64 (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_double: (skip)
 * @d: a number
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_DOUBLE with value @n, to be freed with
 * tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_double (double n)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_DOUBLE);

  g_value_set_double (v, n);
  return v;
}

/**
 * tp_g_value_slice_new_string: (skip)
 * @string: a string to be copied into the value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is a copy of @string,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_string (const gchar *string)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_STRING);

  g_value_set_string (v, string);
  return v;
}

/**
 * tp_g_value_slice_new_static_string: (skip)
 * @string: a static string which must remain valid forever, to be pointed to
 *  by the value
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is @string,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_static_string (const gchar *string)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_STRING);

  g_value_set_static_string (v, string);
  return v;
}

/**
 * tp_g_value_slice_new_take_string: (skip)
 * @string: a string which will be freed with g_free() by the returned #GValue
 *  (the caller must own it before calling this function, but no longer owns
 *  it after this function returns)
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %G_TYPE_STRING whose value is @string,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_take_string (gchar *string)
{
  GValue *v = tp_g_value_slice_new (G_TYPE_STRING);

  g_value_take_string (v, string);
  return v;
}

/**
 * tp_g_value_slice_new_boxed: (skip)
 * @type: a boxed type
 * @p: a pointer of type @type, which will be copied
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is a copy of @p,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_boxed (GType type,
                            gconstpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = tp_g_value_slice_new (type);
  g_value_set_boxed (v, p);
  return v;
}

/**
 * tp_g_value_slice_new_static_boxed: (skip)
 * @type: a boxed type
 * @p: a pointer of type @type, which must remain valid forever
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is @p,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_static_boxed (GType type,
                                   gconstpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = tp_g_value_slice_new (type);
  g_value_set_static_boxed (v, p);
  return v;
}

/**
 * tp_g_value_slice_new_take_boxed: (skip)
 * @type: a boxed type
 * @p: a pointer of type @type which will be freed with g_boxed_free() by the
 *  returned #GValue (the caller must own it before calling this function, but
 *  no longer owns it after this function returns)
 *
 * Slice-allocate and initialize a #GValue. This function is convenient to
 * use when constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type @type whose value is @p,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_take_boxed (GType type,
                                 gpointer p)
{
  GValue *v;

  g_return_val_if_fail (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED, NULL);
  v = tp_g_value_slice_new (type);
  g_value_take_boxed (v, p);
  return v;
}

/**
 * tp_g_value_slice_free: (skip)
 * @value: A GValue which was allocated with the g_slice API
 *
 * Unset and free a slice-allocated GValue.
 *
 * <literal>(GDestroyNotify) tp_g_value_slice_free</literal> can be used
 * as a destructor for values in a #GHashTable, for example.
 */

void
tp_g_value_slice_free (GValue *value)
{
  g_value_unset (value);
  g_slice_free (GValue, value);
}


/**
 * tp_g_value_slice_dup: (skip)
 * @value: A GValue
 *
 * <!-- 'Returns' says it all -->
 *
 * Returns: a newly allocated copy of @value, to be freed with
 * tp_g_value_slice_free() or g_slice_free().
 * Since: 0.5.14
 */
GValue *
tp_g_value_slice_dup (const GValue *value)
{
  GValue *ret = tp_g_value_slice_new (G_VALUE_TYPE (value));

  g_value_copy (value, ret);
  return ret;
}

/**
 * tp_g_value_slice_new_bytes: (skip)
 * @length: number of bytes to copy
 * @bytes: location of an array of bytes to be copied (this may be %NULL
 *  if and only if length is 0)
 *
 * Slice-allocate a #GValue containing a byte-array, using
 * tp_g_value_slice_new_boxed(). This function is convenient to use when
 * constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %DBUS_TYPE_G_UCHAR_ARRAY whose value is a copy
 * of @length bytes from @bytes, to be freed with tp_g_value_slice_free() or
 * g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_bytes (guint length,
                            gconstpointer bytes)
{
  GArray *arr;

  g_return_val_if_fail (length == 0 || bytes != NULL, NULL);
  arr = g_array_sized_new (FALSE, FALSE, 1, length);

  if (length > 0)
    g_array_append_vals (arr, bytes, length);

  return tp_g_value_slice_new_take_boxed (DBUS_TYPE_G_UCHAR_ARRAY, arr);
}

/**
 * tp_g_value_slice_new_take_bytes: (skip)
 * @bytes: a non-NULL #GArray of guchar, ownership of which will be taken by
 *  the #GValue
 *
 * Slice-allocate a #GValue containing @bytes, using
 * tp_g_value_slice_new_boxed(). This function is convenient to use when
 * constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %DBUS_TYPE_G_UCHAR_ARRAY whose value is
 * @bytes, to be freed with tp_g_value_slice_free() or
 * g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_take_bytes (GArray *bytes)
{
  g_return_val_if_fail (bytes != NULL, NULL);
  return tp_g_value_slice_new_take_boxed (DBUS_TYPE_G_UCHAR_ARRAY, bytes);
}

/**
 * tp_g_value_slice_new_object_path: (skip)
 * @path: a valid D-Bus object path which will be copied
 *
 * Slice-allocate a #GValue containing an object path, using
 * tp_g_value_slice_new_boxed(). This function is convenient to use when
 * constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %DBUS_TYPE_G_OBJECT_PATH whose value is a copy
 * of @path, to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_object_path (const gchar *path)
{
  g_return_val_if_fail (g_variant_is_object_path (path), NULL);
  return tp_g_value_slice_new_boxed (DBUS_TYPE_G_OBJECT_PATH, path);
}

/**
 * tp_g_value_slice_new_static_object_path: (skip)
 * @path: a valid D-Bus object path which must remain valid forever
 *
 * Slice-allocate a #GValue containing an object path, using
 * tp_g_value_slice_new_static_boxed(). This function is convenient to use when
 * constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %DBUS_TYPE_G_OBJECT_PATH whose value is @path,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_static_object_path (const gchar *path)
{
  g_return_val_if_fail (g_variant_is_object_path (path), NULL);
  return tp_g_value_slice_new_static_boxed (DBUS_TYPE_G_OBJECT_PATH, path);
}

/**
 * tp_g_value_slice_new_take_object_path: (skip)
 * @path: a valid D-Bus object path which will be freed with g_free() by the
 *  returned #GValue (the caller must own it before calling this function, but
 *  no longer owns it after this function returns)
 *
 * Slice-allocate a #GValue containing an object path, using
 * tp_g_value_slice_new_take_boxed(). This function is convenient to use when
 * constructing hash tables from string to #GValue, for example.
 *
 * Returns: a #GValue of type %DBUS_TYPE_G_OBJECT_PATH whose value is @path,
 * to be freed with tp_g_value_slice_free() or g_slice_free()
 *
 * Since: 0.7.27
 */
GValue *
tp_g_value_slice_new_take_object_path (gchar *path)
{
  g_return_val_if_fail (g_variant_is_object_path (path), NULL);
  return tp_g_value_slice_new_take_boxed (DBUS_TYPE_G_OBJECT_PATH, path);
}
