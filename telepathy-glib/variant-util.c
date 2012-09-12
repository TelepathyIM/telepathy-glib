/*
 * variant-util.c - Source for GVariant utilities
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:variant-util
 * @title: GVariant utilities
 * @short_description: some GVariant utility functions
 *
 * GVariant utility functions used in telepathy-glib.
 */

/**
 * SECTION:vardic
 * @title: Manipulating a{sv} mappings
 * @short_description: Functions to manipulate mappings from string to
 *  variant, as represented in GDBus by a %G_VARIANT_TYPE_VARDICT
 *
 * These functions provide convenient access to the values in such
 * a mapping.
 *
 * Since: 0.UNRELEASED
 */

#include "config.h"

#include <telepathy-glib/variant-util.h>
#include <telepathy-glib/variant-util-internal.h>

#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_MISC
#include "debug-internal.h"

/*
 * _tp_asv_to_vardict:
 *
 * Returns: (transfer full): a #GVariant of type %G_VARIANT_TYPE_VARDICT
 */
GVariant *
_tp_asv_to_vardict (const GHashTable *asv)
{
  return _tp_boxed_to_variant (TP_HASH_TYPE_STRING_VARIANT_MAP, "a{sv}", (gpointer) asv);
}

GVariant *
_tp_boxed_to_variant (GType gtype,
    const gchar *variant_type,
    gpointer boxed)
{
  GValue v = G_VALUE_INIT;
  GVariant *ret;

  g_return_val_if_fail (boxed != NULL, NULL);

  g_value_init (&v, gtype);
  g_value_set_boxed (&v, boxed);

  ret = dbus_g_value_build_g_variant (&v);
  g_return_val_if_fail (!tp_strdiff (g_variant_get_type_string (ret), variant_type), NULL);

  g_value_unset (&v);

  return g_variant_ref_sink (ret);
}

/*
 * _tp_asv_from_vardict:
 * @variant: a #GVariant of type %G_VARIANT_TYPE_VARDICT
 *
 * Returns: (transfer full): a newly created #GHashTable of
 * type #TP_HASH_TYPE_STRING_VARIANT_MAP
 */
GHashTable *
_tp_asv_from_vardict (GVariant *variant)
{
  GValue v = G_VALUE_INIT;
  GHashTable *result;

  g_return_val_if_fail (variant != NULL, NULL);
  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT),
      NULL);

  dbus_g_value_parse_g_variant (variant, &v);
  g_assert (G_VALUE_HOLDS (&v, TP_HASH_TYPE_STRING_VARIANT_MAP));

  result = g_value_dup_boxed (&v);

  g_value_unset (&v);
  return result;
}
