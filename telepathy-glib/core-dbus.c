/*
 * core-dbus.c - minimal D-Bus utilities for generated code
 *
 * Copyright © 2005-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2005-2008 Nokia Corporation
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

#include "telepathy-glib/dbus.h"
#include "telepathy-glib/dbus-properties-mixin-internal.h"
#include "telepathy-glib/errors.h"

/**
 * tp_dbus_g_method_return_not_implemented: (skip)
 * @context: The D-Bus method invocation context
 *
 * Return the Telepathy error NotImplemented from the method invocation
 * given by @context.
 */
void
tp_dbus_g_method_return_not_implemented (GDBusMethodInvocation *context)
{
  g_dbus_method_invocation_return_dbus_error (context,
      TP_ERROR_STR_NOT_IMPLEMENTED, "Not implemented");
}

/* this is the core library, we don't have debug infrastructure yet */
#define CRITICAL(format, ...) \
  g_log (G_LOG_DOMAIN "/properties", G_LOG_LEVEL_CRITICAL, "%s: " format, \
      G_STRFUNC, ##__VA_ARGS__)

/**
 * tp_svc_interface_set_dbus_properties_info:
 * @g_interface: The #GType of a service interface
 * @info: an interface description
 *
 * Declare that @g_interface implements the given D-Bus interface, with the
 * given properties. This may only be called once per GInterface, usually from
 * a section of its base_init function that only runs once.
 *
 * This is typically only used within generated code; there is normally no
 * reason to call it manually.
 *
 * Since: 0.7.3
 */
void
tp_svc_interface_set_dbus_properties_info (GType g_interface,
    TpDBusPropertiesMixinIfaceInfo *info)
{
  GQuark q = g_quark_from_static_string (
        TP_SVC_INTERFACE_DBUS_PROPERTIES_MIXIN_QUARK_NAME);
  TpDBusPropertiesMixinPropInfo *prop;

  g_return_if_fail (G_TYPE_IS_INTERFACE (g_interface));
  g_return_if_fail (g_type_get_qdata (g_interface, q) == NULL);
  g_return_if_fail (info->dbus_interface != 0);
  g_return_if_fail (info->props != NULL);

  for (prop = info->props; prop->name != 0; prop++)
    {
      g_return_if_fail (prop->flags != 0);
      g_return_if_fail (
        (prop->flags & ~( TP_DBUS_PROPERTIES_MIXIN_FLAG_READ
                        | TP_DBUS_PROPERTIES_MIXIN_FLAG_WRITE
                        | TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_CHANGED
                        | TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_INVALIDATED
                        )) == 0);

      /* Check that at most one change-related flag is set. */
      if ((prop->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_CHANGED) &&
          (prop->flags & TP_DBUS_PROPERTIES_MIXIN_FLAG_EMITS_INVALIDATED))
        {
          CRITICAL ("at most one of EMITS_CHANGED and EMITS_INVALIDATED may be "
              "specified for a property, but %s.%s has both",
              g_quark_to_string (info->dbus_interface),
              g_quark_to_string (prop->name));
          g_return_if_reached ();
        }

      g_return_if_fail (prop->dbus_signature != NULL);
      g_return_if_fail (prop->dbus_signature[0] != '\0');
      g_return_if_fail (prop->type != 0);
    }

  g_type_set_qdata (g_interface, q, info);
}
