/*
 * util.c - Source for utility functions
 * Copyright (C) 2006 Collabora Ltd.
 * Copyright (C) 2006 Nokia Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <libtelepathy/tp-helpers.h>

#include "util.h"

#ifdef MAEMO_OSSO_SUPPORT
#include "media-engine-gen.h"

#define MEDIA_SERVER_SERVICE_NAME "com.nokia.osso_media_server"
#define MEDIA_SERVER_INTERFACE_NAME "com.nokia.osso_media_server"
#define MEDIA_SERVER_SERVICE_OBJECT "/com/nokia/osso_media_server"

static void
media_server_proxy_cleanup (DBusGProxy **media_server_proxy)
{
  DBusGProxy *proxy;

  if (*media_server_proxy == NULL)
    return;

  proxy = *media_server_proxy;
  *media_server_proxy = NULL;
  g_object_unref (proxy);
}

static void
media_server_proxy_destroyed (DBusGProxy *proxy, gpointer user_data)
{
  DBusGProxy **media_server_proxy = (DBusGProxy **) user_data;

  g_message ("media server proxy destroyed");

  media_server_proxy_cleanup (media_server_proxy);
}

void
media_server_disable (DBusGProxy **media_server_proxy)
{
  GError *error;

  *media_server_proxy =
    dbus_g_proxy_new_for_name (tp_get_bus(),
                               MEDIA_SERVER_SERVICE_NAME,
                               MEDIA_SERVER_SERVICE_OBJECT,
                               MEDIA_SERVER_INTERFACE_NAME);

  g_signal_connect (*media_server_proxy, "destroy",
      G_CALLBACK (media_server_proxy_destroyed), media_server_proxy);

  if (!com_nokia_osso_media_server_disable (*media_server_proxy, &error))
    {
      if (error)
        {
          g_message ("unable to disable media server: %s", error->message);
          g_error_free (error);
        }
      else
        {
          g_message ("unable to disable media server: unknown error");
        }

      media_server_proxy_cleanup (media_server_proxy);
    }
}

void
media_server_enable (DBusGProxy **media_server_proxy)
{
  if (*media_server_proxy != NULL)
    {
      GError *error = NULL;

      if (!com_nokia_osso_media_server_enable (*media_server_proxy, &error))
        {
          if (error != NULL)
            {
              g_message ("unable to re-enable media server: %s",
                  error->message);
              g_error_free (error);
             }
          else
            {
              g_message ("unable to re-enable media server: unknown error");
            }
        }

      media_server_proxy_cleanup (media_server_proxy);
    }
}

#else

void
media_server_enable (DBusGProxy **media_server_proxy)
{
  return;
}

void
media_server_disable (DBusGProxy **media_server_proxy)
{
  return;
}

#endif

gboolean
g_object_has_property (GObject *object, const gchar *property)
{
  GObjectClass *klass;

  klass = G_OBJECT_GET_CLASS (object);
  return NULL != g_object_class_find_property (klass, property);
}

