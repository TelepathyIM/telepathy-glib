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
