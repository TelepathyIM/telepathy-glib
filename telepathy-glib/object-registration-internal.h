/*<private_header>*/
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

/* Not bothering with an include-once guard here because this is very much
 * internal.
 *
 * The TpDBusPropertiesMixin and tp_dbus_connection_try_register_object()
 * both use this. Please try not to use it elsewhere. */

typedef struct _TpDBusConnectionRegistration TpDBusConnectionRegistration;

struct _TpDBusConnectionRegistration {
    /* (transfer full) */
    GDBusConnection *conn;
    /* (transfer full) */
    gchar *object_path;
    /* (transfer full) */
    GList *skeletons;
    /* (transfer none), do not dereference */
    gpointer object;
};

static GQuark
_tp_dbus_connection_registration_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string ("tp_dbus_connection_register_object");
    }

  return q;
}
