/*
 * tp-voip-engine.c - Source for TpVoipEngine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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


#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/telepathy-errors.h"
#include "common/telepathy-errors-enumtypes.h"
#include "common/telepathy-helpers.h"
#include "common/telepathy-interfaces.h"

#include "test-streamed-media-channel.h"
#include "tp-media-session-handler.h"
#include "tp-media-stream-handler.h"
#include "tp-voip-engine-gen.h"

#include "test-voip-engine.h"

void
register_service() {
  DBusGConnection *bus;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint request_name_result;

  bus = tp_get_bus ();
  bus_proxy = tp_get_bus_proxy ();

  if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
                          G_TYPE_STRING, TEST_APP_NAME,
                          G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, voip engine already running?");

}

int main(int argc, char **argv) {
  GMainLoop *mainloop;
  DBusGConnection *bus;
  DBusGProxy *proxy;
  GError *error = NULL;
  guint result;

  g_type_init();

  g_set_prgname("test-voip-engine");

  mainloop = g_main_loop_new (NULL, FALSE);

  bus = tp_get_bus ();
  register_service();

  dbus_g_error_domain_register (TELEPATHY_ERRORS, NULL, TELEPATHY_TYPE_ERRORS);

  dbus_g_connection_register_g_object (bus, TEST_STREAM_PATH,
    g_object_new (TP_TYPE_MEDIA_STREAM_HANDLER, NULL));
  dbus_g_connection_register_g_object (bus, TEST_SESSION_PATH,
    g_object_new (TP_TYPE_MEDIA_SESSION_HANDLER, NULL));
  dbus_g_connection_register_g_object (bus, TEST_CHANNEL_PATH,
    g_object_new (TEST_TYPE_STREAMED_MEDIA_CHANNEL, NULL));

  /* Activate voip engine*/
  g_print ("Activating VoipEngine\n");

  error = NULL;
  if (!dbus_g_proxy_call (tp_get_bus_proxy(), "StartServiceByName", &error,
                          G_TYPE_STRING,
                          "org.freedesktop.Telepathy.VoipEngine",
                          G_TYPE_UINT,
                          0,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &result,
                          G_TYPE_INVALID)) {
    g_warning ("Failed to complete Activate call %s", error->message);
  }
  else
    g_message ("Voip engine activated\n");

  proxy = dbus_g_proxy_new_for_name (bus,
    "org.freedesktop.Telepathy.VoipEngine",
    "/org/freedesktop/Telepathy/VoipEngine",
    "org.freedesktop.Telepathy.ChannelHandler");

  error = NULL;
  if (!org_freedesktop_Telepathy_ChannelHandler_handle_channel 
         (proxy, TEST_APP_NAME, "/dummy",  TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, 
          TEST_CHANNEL_PATH, 0, 0, &error))
    {
      g_error ("Handle Channel failed, %s", error->message);
      g_error_free (error);
      exit (1);
    }
  g_object_unref (proxy);
  
  g_debug("started");

  g_main_loop_run (mainloop);

  return 0;
}
