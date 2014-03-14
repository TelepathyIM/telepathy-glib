/*
 * util.c - Source for telepathy-glib utility functions
 * Copyright © 2006-2014 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include <config.h>
#include <telepathy-glib/util.h>

#include <string.h>
#include <gobject/gvaluecollector.h>

#include <telepathy-glib/defs.h>

/* this is the core library, we don't have debug infrastructure yet */
#define CRITICAL(format, ...) \
  g_log (G_LOG_DOMAIN "/misc", G_LOG_LEVEL_CRITICAL, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)
#define WARNING(format, ...) \
  g_log (G_LOG_DOMAIN "/misc", G_LOG_LEVEL_WARNING, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)

/**
 * tp_value_array_build: (skip)
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
 * Returns: a newly created #GValueArray, free with tp_value_array_free()
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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  arr = g_value_array_new (length);
  G_GNUC_END_IGNORE_DEPRECATIONS

  va_start (var_args, type);

  for (t = type; t != G_TYPE_INVALID; t = va_arg (var_args, GType))
    {
      GValue *v = arr->values + arr->n_values;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_array_append (arr, NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_value_init (v, t);

      G_VALUE_COLLECT (v, var_args, 0, &error);

      if (error != NULL)
        {
          CRITICAL ("%s", error);
          g_free (error);

          tp_value_array_free (arr);
          va_end (var_args);
          return NULL;
        }
    }

  g_warn_if_fail (arr->n_values == length);

  va_end (var_args);
  return arr;
}

/**
 * tp_value_array_unpack: (skip)
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

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      value = g_value_array_get_nth (array, i);
      G_GNUC_END_IGNORE_DEPRECATIONS

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
 * tp_value_array_free:
 * @va: a #GValueArray
 *
 * Free @va. This is exactly the same as g_value_array_free(), but does not
 * provoke deprecation warnings from GLib when used in conjunction with
 * tp_value_array_build() and tp_value_array_unpack().
 *
 * Since: 0.23.0
 */
void
(tp_value_array_free) (GValueArray *va)
{
  _tp_value_array_free_inline (va);
}
