/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <telepathy-glib/dbus.h>

#include <telepathy-logger/dbus-service.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define ID "echo@test.collabora.co.uk"

//static GMainLoop *loop = NULL;

static void
last_chats_cb (DBusGProxy *proxy,
    GPtrArray *result,
    GError *error,
    gpointer userdata)
{
  /* Just do demonstrate remote exceptions versus regular GError */
  if (error != NULL) {
      if (error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        g_printerr ("Caught remote method exception %s: %s",
            dbus_g_error_get_name (error),
            error->message);
      else
        g_printerr ("Error: %s\n", error->message);
      g_error_free (error);
      return;
  }

  g_print ("Names on the message bus:\n");

  for (guint i = 0; i < result->len; ++i)
    {
      GValueArray    *message_struct;
      const gchar    *message_body;
      const gchar           *message_sender;
      guint           message_timestamp;

      message_struct = g_ptr_array_index (result, i);

      message_body = g_value_get_string (g_value_array_get_nth
          (message_struct, 0));
      message_sender = g_value_get_string (g_value_array_get_nth
          (message_struct, 1));
      message_timestamp = g_value_get_uint (g_value_array_get_nth
          (message_struct, 2));

      g_debug ("%d: [%d] from=%s - %s", i, message_timestamp, message_sender,
          message_body);
    }
}

int
main (int argc, char *argv[])
{
  DBusGConnection *connection;
  GError *error = NULL;
  DBusGProxy *proxy;

  g_type_init ();

  error = NULL;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (connection == NULL)
    {
      g_printerr ("Failed to open connection to bus: %s\n",
          error->message);
      g_error_free (error);
      return 1;
    }

  /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */

  proxy = dbus_g_proxy_new_for_name (connection,
      TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME,
      TPL_DBUS_SRV_OBJECT_PATH,
      TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME);

  tpl_dbus_service_last_chats_async (proxy, ACCOUNT_PATH, ID, FALSE, 5,
        last_chats_cb, NULL);

  g_object_unref (proxy);

  return 0;
}
