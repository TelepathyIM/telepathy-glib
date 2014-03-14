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

#include <config.h>
#include <telepathy-glib/core-svc-interface.h>

/**
 * SECTION:core-svc-interface
 * @title: TpSvcInterface
 * @short_description: glue to export `TpSvc` interfaces on D-Bus
 *
 * #TpSvcInterfaceInfo describes a dbus-glib-style #GInterface in sufficient
 * detail to export it on a #GDBusConnection.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpSvcInterfaceInfo: (skip)
 * @ref_count: currently -1 since these structures can only be statically
 *  allocated; will be used for the reference count in the same way
 *  as #GDBusInterfaceInfo if necessary
 * @interface_info: the GDBus interface information
 * @vtable: the GDBus vtable, which must expect the object that
 *  implements the `TpSvc` interface (*not* the #GDBusInterfaceSkeleton!)
 *  as its `user_data`
 * @signals: (array zero-terminated=1): a %NULL-terminated array of
 *  GLib signal names in the same order as `interface_info->signals`
 *
 * The necessary glue between a dbus-glib-style `TpSvc` #GInterface
 * and telepathy-glib.
 *
 * These structs are intended to be programmatically-generated.
 */

static GQuark
quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string (
          "tp_svc_interface_set_dbus_interface_info");
    }

  return q;
}

/**
 * tp_svc_interface_peek_dbus_interface_info: (skip)
 * @g_interface: The #GType of a service interface
 *
 * See whether the given interface has Telepathy code generation data
 * attached.
 *
 * Returns: (transfer null): a #TpSvcInterfaceInfo struct, or %NULL
 */
const TpSvcInterfaceInfo *
tp_svc_interface_peek_dbus_interface_info (GType g_interface)
{
  return g_type_get_qdata (g_interface, quark ());
}

/**
 * tp_svc_interface_set_dbus_interface_info: (skip)
 * @g_interface: The #GType of a service interface
 * @info: struct encapsulating the #GDBusInterfaceInfo, the #GDBusVTable
 *  and the GLib signal names corresponding to D-Bus signals
 * @vtable: the vtable, whose methods must expect the object
 *  implementing @g_interface as their user-data
 *
 * Declare that @g_interface implements the given D-Bus interface, with the
 * given vtable. This may only be called once per GInterface, usually from
 * a section of its base_init function that only runs once.
 *
 * This is typically only used within generated code; there is normally no
 * reason to call it manually.
 */
void
tp_svc_interface_set_dbus_interface_info (GType g_interface,
    const TpSvcInterfaceInfo *info)
{
  g_return_if_fail (G_TYPE_IS_INTERFACE (g_interface));
  g_return_if_fail (info->ref_count == -1);
  g_return_if_fail (tp_svc_interface_peek_dbus_interface_info (g_interface)
      == NULL);

  /* g_type_set_qdata wants a non-const pointer */
  g_type_set_qdata (g_interface, quark (), (gpointer) info);
}
