/*
 * Copyright © 2014 Collabora Ltd.
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

#include <telepathy-glib/core-dbus-properties-mixin-internal.h>

#include <gio/gio.h>

#include <telepathy-glib/errors.h>

static TpDBusPropertiesMixinImpl impl = { NULL };

GVariant *
_tp_dbus_properties_mixin_dup_in_dbus_lib (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error)
{
  if (impl.version == NULL)
    {
      /* deliberately not using TP_ERROR to avoid a cross-library reference
       * in the wrong direction */
      g_dbus_error_set_dbus_error (error, TP_ERROR_STR_NOT_IMPLEMENTED,
          "No properties registered with TpDBusPropertiesMixin", NULL);
      return NULL;
    }
  else
    {
      return impl.dup_variant (object, interface_name, property_name, error);
    }
}

gboolean
_tp_dbus_properties_mixin_set_in_dbus_lib (GObject *object,
    const gchar *interface_name,
    const gchar *property_name,
    GVariant *value,
    GError **error)
{
  if (impl.version == NULL)
    {
      g_dbus_error_set_dbus_error (error, TP_ERROR_STR_NOT_IMPLEMENTED,
          "No properties registered with TpDBusPropertiesMixin", NULL);
      return FALSE;
    }
  else
    {
      return impl.set_variant (object, interface_name, property_name,
          value, error);
    }
}

GVariant *
_tp_dbus_properties_mixin_dup_all_in_dbus_lib (GObject *object,
    const gchar *interface_name)
{
  if (impl.version == NULL)
    {
      /* GetAll() always succeeds */
      return g_variant_new ("a{sv}", NULL);
    }
  else
    {
      return impl.dup_all_vardict (object, interface_name);
    }
}

void
tp_private_dbus_properties_mixin_set_implementation (
    const TpDBusPropertiesMixinImpl *real_impl)
{
  g_assert (g_str_equal (real_impl->version, VERSION));
  g_assert (real_impl->size == sizeof (impl));
  g_assert (real_impl->dup_variant != NULL);
  g_assert (real_impl->set_variant != NULL);
  g_assert (real_impl->dup_all_vardict != NULL);

  impl = *real_impl;
}
