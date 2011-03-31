/*
 * util.c - Source for telepathy-glib utility functions
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

/**
 * SECTION:util
 * @title: Utilities
 * @short_description: Non-Telepathy utility functions
 *
 * Some utility functions used in telepathy-glib which could have been in
 * GLib, but aren't.
 */

#include <config.h>

#include <gobject/gvaluecollector.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#include <gio/gunixconnection.h>
#endif /* HAVE_GIO_UNIX */

#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>

#include <stdio.h>
#include <string.h>

#define DEBUG_FLAG TP_DEBUG_MISC
#include "debug-internal.h"

/**
 * tp_verify:
 * @R: a requirement (constant expression) to be checked at compile-time
 *
 * Make an assertion at compile time, like C++0x's proposed static_assert
 * keyword. If @R is determined to be true, there is no overhead at runtime;
 * if @R is determined to be false, compilation will fail.
 *
 * This macro can be used at file scope (it expands to a dummy extern
 * declaration).
 *
 * (This is gnulib's verify macro, written by Paul Eggert, Bruno Haible and
 * Jim Meyering.)
 *
 * This macro will be deprecated in a future telepathy-glib release. Please
 * use GLib 2.20's G_STATIC_ASSERT() macro in new code.
 *
 * Since: 0.7.34
 */

/**
 * tp_verify_true:
 * @R: a requirement (constant expression) to be checked at compile-time
 *
 * Make an assertion at compile time, like C++0x's proposed static_assert
 * keyword. If @R is determined to be true, there is no overhead at runtime,
 * and the macro evaluates to 1 as an integer constant expression;
 * if @R is determined to be false, compilation will fail.
 *
 * This macro can be used anywhere that an integer constant expression would
 * be allowed.
 *
 * (This is gnulib's verify_true macro, written by Paul Eggert, Bruno Haible
 * and Jim Meyering.)
 *
 * This macro will be deprecated in a future telepathy-glib release. Please
 * use GLib 2.20's G_STATIC_ASSERT() macro in new code.
 *
 * Returns: 1
 *
 * Since: 0.7.34
 */

/**
 * tp_verify_statement:
 * @R: a requirement (constant expression) to be checked at compile-time
 *
 * Make an assertion at compile time, like C++0x's proposed static_assert
 * keyword. If @R is determined to be true, there is no overhead at runtime;
 * if @R is determined to be false, compilation will fail.
 *
 * This macro can be used anywhere that a statement would be allowed; it
 * is equivalent to ((void) tp_verify_true (R)).
 *
 * This macro will be deprecated in a future telepathy-glib release. Please
 * use GLib 2.20's G_STATIC_ASSERT() macro in new code.
 *
 * Since: 0.7.34
 */

/**
 * tp_g_ptr_array_contains:
 * @haystack: The pointer array to be searched
 * @needle: The pointer to look for
 *
 * <!--no further documentation needed-->
 *
 * Returns: %TRUE if @needle is one of the elements of @haystack
 */

gboolean
tp_g_ptr_array_contains (GPtrArray *haystack, gpointer needle)
{
  guint i;

  for (i = 0; i < haystack->len; i++)
    {
      if (g_ptr_array_index (haystack, i) == needle)
        return TRUE;
    }

  return FALSE;
}


/**
 * tp_g_value_slice_new:
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
 * tp_g_value_slice_new_boolean:
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
 * tp_g_value_slice_new_int:
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
 * tp_g_value_slice_new_int64:
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
 * tp_g_value_slice_new_byte:
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
 * tp_g_value_slice_new_uint:
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
 * tp_g_value_slice_new_uint64:
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
 * tp_g_value_slice_new_double:
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
 * tp_g_value_slice_new_string:
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
 * tp_g_value_slice_new_static_string:
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
 * tp_g_value_slice_new_take_string:
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
 * tp_g_value_slice_new_boxed:
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
 * tp_g_value_slice_new_static_boxed:
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
 * tp_g_value_slice_new_take_boxed:
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
 * tp_g_value_slice_free:
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
 * tp_g_value_slice_dup:
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


struct _tp_g_hash_table_update
{
  GHashTable *target;
  GBoxedCopyFunc key_dup, value_dup;
};

static void
_tp_g_hash_table_update_helper (gpointer key,
                                gpointer value,
                                gpointer user_data)
{
  struct _tp_g_hash_table_update *data = user_data;
  gpointer new_key = (data->key_dup != NULL) ? (data->key_dup) (key) : key;
  gpointer new_value = (data->value_dup != NULL) ? (data->value_dup) (value)
                                                 : value;

  g_hash_table_replace (data->target, new_key, new_value);
}

/**
 * tp_g_hash_table_update:
 * @target: The hash table to be updated
 * @source: The hash table to update it with (read-only)
 * @key_dup: function to duplicate a key from @source so it can be be stored
 *           in @target. If NULL, the key is not copied, but is used as-is
 * @value_dup: function to duplicate a value from @source so it can be stored
 *             in @target. If NULL, the value is not copied, but is used as-is
 *
 * Add each item in @source to @target, replacing any existing item with the
 * same key. @key_dup and @value_dup are used to duplicate the items; in
 * principle they could also be used to convert between types.
 *
 * Since: 0.7.0
 */
void
tp_g_hash_table_update (GHashTable *target,
                        GHashTable *source,
                        GBoxedCopyFunc key_dup,
                        GBoxedCopyFunc value_dup)
{
  struct _tp_g_hash_table_update data = { target, key_dup,
      value_dup };

  g_return_if_fail (target != NULL);
  g_return_if_fail (source != NULL);
  g_return_if_fail (target != source);

  g_hash_table_foreach (source, _tp_g_hash_table_update_helper, &data);
}

/**
 * tp_str_empty:
 * @s: (type utf8) (transfer none): a string
 *
 * Return %TRUE if @s is empty, counting %NULL as empty.
 *
 * Returns: (type boolean): %TRUE if @s is either %NULL or ""
 *
 * Since: 0.11.1
 */
/* no definition here - it's inlined */

/**
 * tp_strdiff:
 * @left: The first string to compare (may be NULL)
 * @right: The second string to compare (may be NULL)
 *
 * Return %TRUE if the given strings are different. Unlike #strcmp this
 * function will handle null pointers, treating them as distinct from any
 * string.
 *
 * Returns: %FALSE if @left and @right are both %NULL, or if
 *          neither is %NULL and both have the same contents; %TRUE otherwise
 */

gboolean
tp_strdiff (const gchar *left, const gchar *right)
{
  if ((NULL == left) != (NULL == right))
    return TRUE;

  else if (left == right)
    return FALSE;

  else
    return (0 != strcmp (left, right));
}



/**
 * tp_mixin_offset_cast:
 * @instance: A pointer to a structure
 * @offset: The offset of a structure member in bytes, which must not be 0
 *
 * Extend a pointer by an offset, provided the offset is not 0.
 * This is used to cast from an object instance to one of the telepathy-glib
 * mixin classes.
 *
 * Returns: a pointer @offset bytes beyond @instance
 */
gpointer
tp_mixin_offset_cast (gpointer instance, guint offset)
{
  g_return_val_if_fail (offset != 0, NULL);

  return ((guchar *) instance + offset);
}


/**
 * tp_mixin_instance_get_offset:
 * @instance: A pointer to a GObject-derived instance structure
 * @quark: A quark that was used to store the offset with g_type_set_qdata()
 *
 * If the type of @instance, or any of its ancestor types, has had an offset
 * attached using qdata with the given @quark, return that offset. If not,
 * return 0.
 *
 * In older telepathy-glib versions, calling this function on an instance that
 * did not have the mixin was considered to be a programming error. Since
 * version 0.13.9, 0 is returned, without error.
 *
 * This is used to implement the telepathy-glib mixin classes.
 *
 * Returns: the offset of the mixin
 */
guint
tp_mixin_instance_get_offset (gpointer instance,
                              GQuark quark)
{
  GType t;

  for (t = G_OBJECT_TYPE (instance);
       t != 0;
       t = g_type_parent (t))
    {
      gpointer qdata = g_type_get_qdata (t, quark);

      if (qdata != NULL)
        return GPOINTER_TO_UINT (qdata);
    }

  return 0;
}


/**
 * tp_mixin_class_get_offset:
 * @klass: A pointer to a GObjectClass-derived class structure
 * @quark: A quark that was used to store the offset with g_type_set_qdata()
 *
 * If the type of @klass, or any of its ancestor types, has had an offset
 * attached using qdata with the given @quark, return that offset; if not,
 * return 0.
 *
 * In older telepathy-glib versions, calling this function on an instance that
 * did not have the mixin was considered to be a programming error. Since
 * version 0.13.9, 0 is returned, without error.
 *
 * This is used to implement the telepathy-glib mixin classes.
 *
 * Returns: the offset of the mixin class
 */
guint
tp_mixin_class_get_offset (gpointer klass,
                           GQuark quark)
{
  GType t;

  for (t = G_OBJECT_CLASS_TYPE (klass);
       t != 0;
       t = g_type_parent (t))
    {
      gpointer qdata = g_type_get_qdata (t, quark);

      if (qdata != NULL)
        return GPOINTER_TO_UINT (qdata);
    }

  return 0;
}


static inline gboolean
_esc_ident_bad (gchar c, gboolean is_first)
{
  return ((c < 'a' || c > 'z') &&
          (c < 'A' || c > 'Z') &&
          (c < '0' || c > '9' || is_first));
}


/**
 * tp_escape_as_identifier:
 * @name: The string to be escaped
 *
 * Escape an arbitrary string so it follows the rules for a C identifier,
 * and hence an object path component, interface element component,
 * bus name component or member name in D-Bus.
 *
 * Unlike g_strcanon this is a reversible encoding, so it preserves
 * distinctness.
 *
 * The escaping consists of replacing all non-alphanumerics, and the first
 * character if it's a digit, with an underscore and two lower-case hex
 * digits:
 *
 *    "0123abc_xyz\x01\xff" -> _30123abc_5fxyz_01_ff
 *
 * i.e. similar to URI encoding, but with _ taking the role of %, and a
 * smaller allowed set. As a special case, "" is escaped to "_" (just for
 * completeness, really).
 *
 * Returns: the escaped string, which must be freed by the caller with #g_free
 */
gchar *
tp_escape_as_identifier (const gchar *name)
{
  gboolean bad = FALSE;
  size_t len = 0;
  GString *op;
  const gchar *ptr, *first_ok;

  g_return_val_if_fail (name != NULL, NULL);

  /* fast path for empty name */
  if (name[0] == '\0')
    return g_strdup ("_");

  for (ptr = name; *ptr; ptr++)
    {
      if (_esc_ident_bad (*ptr, ptr == name))
        {
          bad = TRUE;
          len += 3;
        }
      else
        len++;
    }

  /* fast path if it's clean */
  if (!bad)
    return g_strdup (name);

  /* If strictly less than ptr, first_ok is the first uncopied safe character.
   */
  first_ok = name;
  op = g_string_sized_new (len);
  for (ptr = name; *ptr; ptr++)
    {
      if (_esc_ident_bad (*ptr, ptr == name))
        {
          /* copy preceding safe characters if any */
          if (first_ok < ptr)
            {
              g_string_append_len (op, first_ok, ptr - first_ok);
            }
          /* escape the unsafe character */
          g_string_append_printf (op, "_%02x", (unsigned char)(*ptr));
          /* restart after it */
          first_ok = ptr + 1;
        }
    }
  /* copy trailing safe characters if any */
  if (first_ok < ptr)
    {
      g_string_append_len (op, first_ok, ptr - first_ok);
    }
  return g_string_free (op, FALSE);
}


/**
 * tp_strv_contains:
 * @strv: a NULL-terminated array of strings, or %NULL (which is treated as an
 *        empty strv)
 * @str: a non-NULL string
 *
 * <!-- -->
 * Returns: TRUE if @str is an element of @strv, according to strcmp().
 *
 * Since: 0.7.15
 */
gboolean
tp_strv_contains (const gchar * const *strv,
                  const gchar *str)
{
  g_return_val_if_fail (str != NULL, FALSE);

  if (strv == NULL)
    return FALSE;

  while (*strv != NULL)
    {
      if (!tp_strdiff (str, *strv))
        return TRUE;
      strv++;
    }

  return FALSE;
}

/**
 * tp_g_key_file_get_int64:
 * @key_file: a non-%NULL #GKeyFile
 * @group_name: a non-%NULL group name
 * @key: a non-%NULL key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as a signed
 * 64-bit integer. This is similar to g_key_file_get_integer() but can return
 * 64-bit results without truncation.
 *
 * Returns: the value associated with the key as a signed 64-bit integer, or
 * 0 if the key was not found or could not be parsed.
 *
 * Since: 0.7.31
 */
gint64
tp_g_key_file_get_int64 (GKeyFile *key_file,
                         const gchar *group_name,
                         const gchar *key,
                         GError **error)
{
  gchar *s, *end;
  gint64 v;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  s = g_key_file_get_value (key_file, group_name, key, error);

  if (s == NULL)
    return 0;

  v = g_ascii_strtoll (s, &end, 10);

  if (*s == '\0' || *end != '\0')
    {
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
          "Key '%s' in group '%s' has value '%s' where int64 was expected",
          key, group_name, s);
      return 0;
    }

  g_free (s);
  return v;
}

/**
 * tp_g_key_file_get_uint64:
 * @key_file: a non-%NULL #GKeyFile
 * @group_name: a non-%NULL group name
 * @key: a non-%NULL key
 * @error: return location for a #GError
 *
 * Returns the value associated with @key under @group_name as an unsigned
 * 64-bit integer. This is similar to g_key_file_get_integer() but can return
 * large positive results without truncation.
 *
 * Returns: the value associated with the key as an unsigned 64-bit integer,
 * or 0 if the key was not found or could not be parsed.
 *
 * Since: 0.7.31
 */
guint64
tp_g_key_file_get_uint64 (GKeyFile *key_file,
                          const gchar *group_name,
                          const gchar *key,
                          GError **error)
{
  gchar *s, *end;
  guint64 v;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  s = g_key_file_get_value (key_file, group_name, key, error);

  if (s == NULL)
    return 0;

  v = g_ascii_strtoull (s, &end, 10);

  if (*s == '\0' || *end != '\0')
    {
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
          "Key '%s' in group '%s' has value '%s' where uint64 was expected",
          key, group_name, s);
      return 0;
    }

  g_free (s);
  return v;
}

typedef struct {
    GObject *instance;
    GObject *observer;
    GClosure *closure;
    gulong handler_id;
} WeakHandlerCtx;

static WeakHandlerCtx *
whc_new (GObject *instance,
         GObject *observer)
{
  WeakHandlerCtx *ctx = g_slice_new0 (WeakHandlerCtx);

  ctx->instance = instance;
  ctx->observer = observer;

  return ctx;
}

static void
whc_free (WeakHandlerCtx *ctx)
{
  g_slice_free (WeakHandlerCtx, ctx);
}

static void observer_destroyed_cb (gpointer, GObject *);
static void closure_invalidated_cb (gpointer, GClosure *);

/*
 * If signal handlers are removed before the object is destroyed, this
 * callback will never get triggered.
 */
static void
instance_destroyed_cb (gpointer ctx_,
    GObject *where_the_instance_was)
{
  WeakHandlerCtx *ctx = ctx_;

  /* No need to disconnect the signal here, the instance has gone away. */
  g_object_weak_unref (ctx->observer, observer_destroyed_cb, ctx);
  g_closure_remove_invalidate_notifier (ctx->closure, ctx,
      closure_invalidated_cb);
  whc_free (ctx);
}

/* Triggered when the observer is destroyed. */
static void
observer_destroyed_cb (gpointer ctx_,
    GObject *where_the_observer_was)
{
  WeakHandlerCtx *ctx = ctx_;

  g_closure_remove_invalidate_notifier (ctx->closure, ctx,
      closure_invalidated_cb);
  g_signal_handler_disconnect (ctx->instance, ctx->handler_id);
  g_object_weak_unref (ctx->instance, instance_destroyed_cb, ctx);
  whc_free (ctx);
}

/* Triggered when either object is destroyed or the handler is disconnected. */
static void
closure_invalidated_cb (gpointer ctx_,
    GClosure *where_the_closure_was)
{
  WeakHandlerCtx *ctx = ctx_;

  g_object_weak_unref (ctx->instance, instance_destroyed_cb, ctx);
  g_object_weak_unref (ctx->observer, observer_destroyed_cb, ctx);
  whc_free (ctx);
}

/**
 * tp_g_signal_connect_object:
 * @instance: the instance to connect to.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @c_handler: the #GCallback to connect.
 * @gobject: the object to pass as data to @c_handler.
 * @connect_flags: a combination of #GConnectFlags. Only
 *  %G_CONNECT_AFTER and %G_CONNECT_SWAPPED are supported by this function.
 *
 * Connects a #GCallback function to a signal for a particular object, as if
 * with g_signal_connect(). Additionally, arranges for the signal handler to be
 * disconnected if @gobject is destroyed.
 *
 * This is similar to g_signal_connect_data(), but uses a closure which
 * ensures that the @gobject stays alive during the call to @c_handler
 * by temporarily adding a reference count to @gobject.
 *
 * This is similar to g_signal_connect_object(), but doesn't have the
 * documented bug that everyone is too scared to fix. Also, it does not allow
 * you to pass in NULL as @gobject
 *
 * This is intended to be a convenient way for objects to use themselves as
 * user_data for callbacks without having to explicitly disconnect all the
 * handlers in their finalizers.
 *
 * Changed in 0.10.4 and 0.11.3: %G_CONNECT_AFTER is now respected.
 *
 * Returns: the handler id
 *
 * Since: 0.9.2
 */
gulong
tp_g_signal_connect_object (gpointer instance,
    const gchar *detailed_signal,
    GCallback c_handler,
    gpointer gobject,
    GConnectFlags connect_flags)
{
  GObject *instance_obj = G_OBJECT (instance);
  WeakHandlerCtx *ctx = whc_new (instance_obj, gobject);

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (c_handler != NULL, 0);
  g_return_val_if_fail (G_IS_OBJECT (gobject), 0);
  g_return_val_if_fail (
      (connect_flags & ~(G_CONNECT_AFTER|G_CONNECT_SWAPPED)) == 0, 0);

  if (connect_flags & G_CONNECT_SWAPPED)
    ctx->closure = g_cclosure_new_object_swap (c_handler, gobject);
  else
    ctx->closure = g_cclosure_new_object (c_handler, gobject);

  ctx->handler_id = g_signal_connect_closure (instance, detailed_signal,
      ctx->closure, (connect_flags & G_CONNECT_AFTER) ? TRUE : FALSE);

  g_object_weak_ref (instance_obj, instance_destroyed_cb, ctx);
  g_object_weak_ref (gobject, observer_destroyed_cb, ctx);
  g_closure_add_invalidate_notifier (ctx->closure, ctx,
      closure_invalidated_cb);

  return ctx->handler_id;
}

/*
 * _tp_quark_array_copy:
 * @quarks: A 0-terminated list of quarks to copy
 *
 * Copy a zero-terminated array into a GArray. The trailing
 * 0 is not counted in the @len member of the returned
 * array, but the @data member is guaranteed to be
 * zero-terminated.
 *
 * Returns: A new GArray containing a copy of @quarks.
 */
GArray *
_tp_quark_array_copy (const GQuark *quarks)
{
  GArray *array;
  const GQuark *q;

  array = g_array_new (TRUE, TRUE, sizeof (GQuark));

  for (q = quarks; q != NULL && *q != 0; q++)
    {
      g_array_append_val (array, *q);
    }

  return array;
}

/**
 * tp_value_array_build:
 * @length: The number of elements that should be in the array
 * @type: The type of the first argument.
 * @...: The value of the first item in the struct followed by a list of type,
 * value pairs terminated by G_TYPE_INVALID.
 *
 * Creates a new #GValueArray for use with structs, containing the values
 * passed in as parameters. The values are copied or reffed as appropriate for
 * their type.
 *
 * <example>
 *   <title> using tp_value_array_build</title>
 *    <programlisting>
 * GValueArray *array = tp_value_array_build (2,
 *    G_TYPE_STRING, host,
 *    G_TYPE_UINT, port,
 *    G_TYPE_INVALID);
 *    </programlisting>
 * </example>
 *
 * Returns: a newly created #GValueArray, free with g_value_array_free.
 *
 * Since: 0.9.2
 */
GValueArray *
tp_value_array_build (gsize length,
  GType type,
  ...)
{
  GValueArray *arr;
  GType t;
  va_list var_args;
  char *error = NULL;

  arr = g_value_array_new (length);

  va_start (var_args, type);

  for (t = type; t != G_TYPE_INVALID; t = va_arg (var_args, GType))
    {
      GValue *v = arr->values + arr->n_values;

      g_value_array_append (arr, NULL);

      g_value_init (v, t);

      G_VALUE_COLLECT (v, var_args, 0, &error);

      if (error != NULL)
        {
          CRITICAL ("%s", error);
          g_free (error);

          g_value_array_free (arr);
          va_end (var_args);
          return NULL;
        }
    }

  g_warn_if_fail (arr->n_values == length);

  va_end (var_args);
  return arr;
}

/**
 * tp_value_array_unpack:
 * @array: the array to unpack
 * @len: The number of elements that should be in the array
 * @...: a list of correctly typed pointers to store the values in
 *
 * Unpacks a #GValueArray into separate variables.
 *
 * The contents of the values aren't copied into the variables, and so become
 * invalid when @array is freed.
 *
 * <example>
 *   <title>using tp_value_array_unpack</title>
 *    <programlisting>
 * const gchar *host;
 * guint port;
 *
 * tp_value_array_unpack (array, 2,
 *    &host,
 *    &port);
 *    </programlisting>
 * </example>
 *
 * Since: 0.11.0
 */
void
tp_value_array_unpack (GValueArray *array,
    gsize len,
    ...)
{
  va_list var_args;
  guint i;

  va_start (var_args, len);

  for (i = 0; i < len; i++)
    {
      GValue *value;
      char *error = NULL;

      if (G_UNLIKELY (i > array->n_values))
        {
          WARNING ("More parameters than entries in the struct!");
          break;
        }

      value = g_value_array_get_nth (array, i);

      G_VALUE_LCOPY (value, var_args, G_VALUE_NOCOPY_CONTENTS, &error);
      if (error != NULL)
        {
          WARNING ("%s", error);
          g_free (error);
          break;
        }
    }

  va_end (var_args);
}

/**
 * TpWeakRef:
 *
 * A simple wrapper for a weak reference to a #GObject, suitable for use in
 * asynchronous calls which should only affect the object if it hasn't already
 * been freed.
 *
 * As well as wrapping a weak reference to an object, this structure can
 * contain an extra pointer to arbitrary data. This is useful for asynchronous
 * calls which act on an object and some second piece of data, which are quite
 * common in practice.
 *
 * If more than one piece of auxiliary data is required, the @user_data
 * argument to the constructor can be a struct or a #GValueArray.
 *
 * Since: 0.11.3
 */
struct _TpWeakRef {
    /*<private>*/
    gpointer object;
    gpointer user_data;
    GDestroyNotify destroy;
};

/**
 * tp_weak_ref_new:
 * @object: (type GObject.Object): an object to which to take a weak reference
 * @user_data: optional additional data to store alongside the weak ref
 * @destroy: destructor for @user_data, called when the weak ref
 *  is freed
 *
 * Return a new weak reference wrapper for @object.
 *
 * Returns: a new weak-reference wrapper
 *
 * Free-function: tp_weak_ref_destroy()
 *
 * Since: 0.11.3
 */
TpWeakRef *
tp_weak_ref_new (gpointer object,
    gpointer user_data,
    GDestroyNotify destroy)
{
  TpWeakRef *self;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  self = g_slice_new (TpWeakRef);
  self->object = object;
  g_object_add_weak_pointer (self->object, &self->object);
  self->user_data = user_data;
  self->destroy = destroy;
  return self;
}

/**
 * tp_weak_ref_get_user_data:
 * @self: a weak reference
 *
 * Return the additional data that was passed to tp_weak_ref_new().
 *
 * Returns: the additional data supplied in tp_weak_ref_new(), which may be
 *  %NULL
 *
 * Since: 0.11.3
 */
gpointer
tp_weak_ref_get_user_data (TpWeakRef *self)
{
  return self->user_data;
}

/**
 * tp_weak_ref_dup_object:
 * @self: a weak reference
 *
 * If the weakly referenced object still exists, return a new reference to
 * it. Otherwise, return %NULL.
 *
 * Returns: (type GObject.Object) (transfer full): a new reference, or %NULL
 *
 * Since: 0.11.3
 */
gpointer
tp_weak_ref_dup_object (TpWeakRef *self)
{
  if (self->object != NULL)
    return g_object_ref (self->object);

  return NULL;
}

/**
 * tp_weak_ref_destroy:
 * @self: a weak reference
 *
 * Free a weak reference wrapper. This drops the weak reference to the
 * object (if it still exists), and frees the user data with the user-supplied
 * destructor function if one was provided.
 *
 * Since: 0.11.3
 */
void
tp_weak_ref_destroy (TpWeakRef *self)
{
  if (self->object != NULL)
    g_object_remove_weak_pointer (self->object, &self->object);

  if (self->destroy != NULL)
    (self->destroy) (self->user_data);

  g_slice_free (TpWeakRef, self);
}

/**
 * tp_clear_object: (skip)
 * @op: (allow-none): a pointer to a variable, struct member etc. holding a
 *  #GObject
 *
 * Set a variable holding a #GObject to %NULL. If it was not already %NULL,
 * unref the object it previously pointed to.
 *
 * This is exactly equivalent to calling tp_clear_pointer() on @op,
 * with @destroy = g_object_unref(). See tp_clear_pointer() for example usage.
 *
 * Since: 0.11.7
 */

/**
 * tp_clear_pointer: (skip)
 * @pp: (allow-none): a pointer to a variable, struct member etc. holding a
 *  pointer
 * @destroy: a function to which a gpointer can be passed, to destroy *@pp
 *  (if calling this macro from C++, explicitly casting the function to
 *  #GDestroyNotify may be necessary)
 *
 * Set a variable holding a pointer to %NULL. If it was not already %NULL,
 * unref or destroy the object it previously pointed to with @destroy.
 *
 * More precisely, if both @pp and *@pp are non-%NULL, set *@pp to %NULL, then
 * call @destroy on the object that *@pp previously pointed to.
 *
 * This is analogous to g_clear_error() for non-error objects, but also
 * ensures that @pp is already %NULL before the destructor is run.
 *
 * Typical usage is something like this:
 *
 * |[
 * typedef struct {
 *   TpConnection *conn;
 *   GError *error;
 *   GHashTable *table;
 *   MyStruct *misc;
 * } Foo;
 * Foo *foo;
 *
 * ...
 *
 * tp_clear_object (&amp;foo->conn);
 * g_clear_error (&amp;foo->error);
 * tp_clear_boxed (G_TYPE_HASH_TABLE, &amp;foo->table);
 * tp_clear_pointer (&amp;foo->misc, my_struct_destroy);
 * ]|
 *
 * Since: 0.11.7
 */

/**
 * tp_clear_boxed: (skip)
 * @gtype: (type GObject.Type): the #GType of *@pp, e.g. %G_TYPE_HASH_TABLE
 * @pp: (allow-none): a pointer to a variable, struct member etc. holding a
 *  boxed object
 *
 * Set a variable holding a boxed object to %NULL. If it was not already %NULL,
 * destroy the boxed object it previously pointed to, as appropriate for
 * @gtype.
 *
 * More precisely, if both @pp and *@pp are non-%NULL, set *@pp to %NULL, then
 * call g_boxed_free() on the object that *@pp previously pointed to.
 *
 * This is similar to tp_clear_pointer(); see that function's documentation
 * for typical usage.
 *
 * Since: 0.11.7
 */

/**
 * tp_simple_async_report_success_in_idle:
 * @source: the source object
 * @callback: the callback
 * @user_data: user data for @callback
 * @source_tag: the source tag for the #GSimpleAsyncResult
 *
 * Create a new #GSimpleAsyncResult with no operation result, and call
 * g_simple_async_result_complete_in_idle() on it.
 *
 * This is like a successful version of g_simple_async_report_error_in_idle(),
 * suitable for asynchronous functions that (conceptually) either succeed and
 * return nothing, or raise an error, such as tp_proxy_prepare_async().
 *
 * The corresponding finish function should not call a function that attempts
 * to get a result, such as g_simple_async_result_get_op_res_gpointer().
 *
 * Since: 0.11.9
 */
void
tp_simple_async_report_success_in_idle (GObject *source,
    GAsyncReadyCallback callback,
    gpointer user_data,
    gpointer source_tag)
{
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (source, callback, user_data, source_tag);
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

/**
 * tp_user_action_time_from_x11:
 * @x11_time: an X11 timestamp, or 0 to indicate the current time
 *
 * Convert an X11 timestamp into a user action time as used in Telepathy.
 *
 * This also works for the timestamps used by Gdk 2.x and Clutter 1.0;
 * it may or may not work with other toolkits or versions.
 *
 * Returns: a nonzero Telepathy user action time, or
 *  %TP_USER_ACTION_TIME_CURRENT_TIME
 *
 * Since: 0.11.13
 */
gint64
tp_user_action_time_from_x11 (guint32 x11_time)
{
  if (x11_time == 0)
    {
      return TP_USER_ACTION_TIME_CURRENT_TIME;
    }

  return x11_time;
}

/**
 * tp_user_action_time_should_present:
 * @user_action_time: (type gint64): the Telepathy user action time
 * @x11_time: (out) (allow-none): a pointer to guint32 used to
 *  return an X11 timestamp, or 0 to indicate the current time; if
 *  %FALSE is returned, the value placed here is not meaningful
 *
 * Interpret a Telepathy user action time to decide whether a Handler should
 * attempt to gain focus. If %TRUE is returned, it would be appropriate to
 * call gtk_window_present_with_time() using @x11_time as input, for instance.
 *
 * @x11_time is used to return a timestamp in the right format for X11,
 * Gdk 2.x and Clutter 1.0; it may or may not work with other toolkits or
 * versions.
 *
 * Returns: %TRUE if it would be appropriate to present a window
 *
 * Since: 0.11.13
 */

gboolean
tp_user_action_time_should_present (gint64 user_action_time,
    guint32 *x11_time)
{
  guint32 when = 0;
  gboolean ret;

  if (user_action_time > 0 && user_action_time <= G_MAXUINT32)
    {
      when = (guint32) user_action_time;
      ret = TRUE;
    }
  else if (user_action_time == TP_USER_ACTION_TIME_CURRENT_TIME)
    {
      ret = TRUE;
    }
  else
    {
      ret = FALSE;
    }

  if (ret && x11_time != NULL)
    *x11_time = when;

  return ret;
}

/* Add each of @quarks to @array if it isn't already present.
 *
 * There are @n quarks, or if @n == -1, the array is 0-terminated. */
void
_tp_quark_array_merge (GArray *array,
    const GQuark *quarks,
    gssize n)
{
  gssize i;
  guint j;

  g_return_if_fail (array != NULL);
  g_return_if_fail (g_array_get_element_size (array) == sizeof (GQuark));
  g_return_if_fail (n >= -1);
  g_return_if_fail (n <= 0 || quarks != NULL);

  if (quarks == NULL || n == 0)
    return;

  if (n < 0)
    {
      n = 0;

      for (i = 0; quarks[i] != 0; i++)
        n++;
    }
  else
    {
      for (i = 0; i < n; i++)
        g_return_if_fail (quarks[i] != 0);
    }

  if (array->len == 0)
    {
      /* fast-path for the common case: there's nothing to merge with */
      g_array_append_vals (array, quarks, n);
      return;
    }

  for (i = 0; i < n; i++)
    {
      for (j = 0; j < array->len; j++)
        {
          if (g_array_index (array, GQuark, j) == quarks[i])
            goto next_i;
        }

      g_array_append_val (array, quarks[i]);
next_i:
      continue;
    }
}

#ifdef HAVE_GIO_UNIX
GSocketAddress *
_tp_create_temp_unix_socket (GSocketService *service,
    GError **error)
{
  guint i;
  GSocketAddress *address;

  /* why doesn't GIO provide a method to create a socket we don't
   * care about? Iterate until we find a valid temporary name.
   *
   * Try a maximum of 10 times to get a socket */
  for (i = 0; i < 10; i++)
    {
      address = g_unix_socket_address_new (tmpnam (NULL));

      g_clear_error (error);

      if (g_socket_listener_add_address (
            G_SOCKET_LISTENER (service),
            address, G_SOCKET_TYPE_STREAM,
            G_SOCKET_PROTOCOL_DEFAULT,
            NULL, NULL, error))
        return address;
      else
        g_object_unref (address);
    }

  return NULL;
}
#endif /* HAVE_GIO_UNIX */

static void
add_to_array (gpointer data,
    gpointer user_data)
{
  g_ptr_array_add (user_data, data);
}

/**
 * tp_g_ptr_array_extend:
 * @target: a #GPtrArray to copy items to
 * @source: a #GPtrArray to copy items from
 *
 * Appends all elements of @source to @target. Note that this only copies the
 * pointers from @source; any duplication or reference-incrementing must be
 * performed by the caller.
 *
 * After this function has been called, it is safe to call
 * g_ptr_array_free() on @source and also free the actual pointer array,
 * as long as doing so does not free the data pointed to by the new
 * items in @target.
 *
 * Since: 0.13.UNRELEASED
 */
void
tp_g_ptr_array_extend (GPtrArray *target,
    GPtrArray *source)
{
  g_ptr_array_foreach (source, add_to_array, target);
}
