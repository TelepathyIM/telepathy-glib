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

#include <config.h>

#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/observer-internal.h>
#include <telepathy-logger/dbus-service-internal.h>

#define DEBUG_FLAG TPL_DEBUG_MAIN
#include <telepathy-logger/debug-internal.h>

static GMainLoop *loop = NULL;

#ifdef ENABLE_DEBUG
static TpDebugSender *debug_sender = NULL;
static gboolean stamp_logs = FALSE;


static void
log_to_debug_sender (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *string)
{
  GTimeVal now;

  g_return_if_fail (TP_IS_DEBUG_SENDER (debug_sender));

  g_get_current_time (&now);

  tp_debug_sender_add_message (debug_sender, &now, log_domain, log_level,
      string);
}


static void
log_handler (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  if (stamp_logs)
    {
      GTimeVal now;
      gchar now_str[32];
      gchar *tmp;
      struct tm tm;

      g_get_current_time (&now);
      localtime_r (&(now.tv_sec), &tm);
      strftime (now_str, 32, "%Y-%m-%d %H:%M:%S", &tm);
      tmp = g_strdup_printf ("%s.%06ld: %s",
        now_str, now.tv_usec, message);

      g_log_default_handler (log_domain, log_level, tmp, NULL);

      g_free (tmp);
    }
  else
    {
      g_log_default_handler (log_domain, log_level, message, NULL);
    }

  log_to_debug_sender (log_domain, log_level, message);
}
#endif /* ENABLE_DEBUG */


static TplDBusService *
telepathy_logger_dbus_init (void)
{
  TplDBusService *dbus_srv = NULL;
  TpDBusDaemon *tp_bus = NULL;
  GError *error = NULL;


  DEBUG ("Initializing TPL DBus service");
  tp_bus = tp_dbus_daemon_dup (&error);
  if (tp_bus == NULL)
    {
      g_critical ("Failed to acquire bus daemon: %s", error->message);
      goto out;
    }

  if (!tp_dbus_daemon_request_name (tp_bus, TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME,
        FALSE, &error))
    {
      g_critical ("Failed to acquire bus name %s: %s",
          TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME, error->message);
      goto out;
    }

  dbus_srv = _tpl_dbus_service_new ();
  tp_dbus_daemon_register_object (tp_bus, TPL_DBUS_SRV_OBJECT_PATH,
      G_OBJECT (dbus_srv));

  DEBUG ("TPL DBus service registered to: %s",
      TPL_DBUS_SRV_WELL_KNOWN_BUS_NAME);

out:
  if (error != NULL)
      g_clear_error (&error);
  if (tp_bus != NULL)
    g_object_unref (tp_bus);

  return dbus_srv;
}


int
main (int argc,
    char *argv[])
{
  TplDBusService *dbus_srv = NULL;
  TplObserver *observer = NULL;
  GError *error = NULL;

  g_type_init ();

  g_set_prgname (PACKAGE_NAME);

  tp_debug_divert_messages (g_getenv ("TPL_LOGFILE"));

#ifdef ENABLE_DEBUG
  _tpl_debug_set_flags_from_env ();

  stamp_logs = (g_getenv ("TPL_TIMING") != NULL);
  debug_sender = tp_debug_sender_dup ();

  g_log_set_default_handler (log_handler, NULL);
#endif /* ENABLE_DEBUG */

  observer = _tpl_observer_dup (&error);

  if (observer == NULL) {
    g_critical ("Failed to create observer: %s", error->message);
    g_error_free (error);
    goto out;
  }

  if (!tp_base_client_register (TP_BASE_CLIENT (observer), &error))
    {
      g_critical ("Error during D-Bus registration: %s", error->message);
      goto out;
    }
  DEBUG ("TPL Observer registered to: %s", TPL_OBSERVER_WELL_KNOWN_BUS_NAME);

  dbus_srv = telepathy_logger_dbus_init ();

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

out:
  if (observer != NULL)
    g_object_unref (observer);
  if (dbus_srv != NULL)
    g_object_unref (dbus_srv);

#ifdef ENABLE_DEBUG
  g_log_set_default_handler (g_log_default_handler, NULL);
  g_object_unref (debug_sender);
#endif /* ENABLE_DEBUG */

  return 0;
}
