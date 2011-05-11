/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */


#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>

#include <telepathy-logger/dbus-service-internal.h>
#include <extensions/extensions.h>

static GMainLoop *mainloop = NULL;

int
main (int argc, char *argv[])
{
  TpDBusDaemon *bus;
  TpProxy *proxy;
  GError *error = NULL;
  char *account;

  g_type_init ();
  mainloop = g_main_loop_new (NULL, FALSE);

  if (argc != 3)
    {
      g_printerr ("Usage: ./test-api <account> <identifier>\n");
      return -1;
    }

  account = g_strdup_printf ("%s%s", TP_ACCOUNT_OBJECT_PATH_BASE, argv[1]);

  bus = tp_dbus_daemon_dup (&error);
  g_assert_no_error (error);

  proxy = g_object_new (TP_TYPE_PROXY,
      "bus-name", TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME,
      "object-path", TPL_DBUS_SRV_OBJECT_PATH,
      "dbus-daemon", bus,
      NULL);

  g_object_unref (bus);

  tp_proxy_add_interface_by_id (proxy, TPL_IFACE_QUARK_LOGGER);

  // FIXME Test favorites

  g_free (account);

  g_main_loop_run (mainloop);

  g_object_unref (proxy);

  return 0;
}
