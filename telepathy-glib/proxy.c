/*
 * proxy.c - Base class for Telepathy client proxies
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#include "telepathy-glib/proxy.h"

#include <telepathy-glib/errors.h>

G_DEFINE_TYPE (TpProxy,
    tp_proxy,
    DBUS_TYPE_G_PROXY);

DBusGProxy *
tp_proxy_get_interface (TpProxy *self,
                        GQuark interface,
                        GError **error)
{
  /* stub implementation - for now we claim no object has any interfaces */
  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Object %s does not have interface %s",
      dbus_g_proxy_get_path ((DBusGProxy *) self),
      g_quark_to_string (interface));

  return NULL;
}

static void
tp_proxy_init (TpProxy *self)
{
}

static void
tp_proxy_class_init (TpProxyClass *klass)
{
}
