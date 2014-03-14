/*
 * TpLogger - a TpProxy for the logger
 *
 * Copyright (C) 2014 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"

#include "logger.h"

#include <telepathy-glib/telepathy-glib-dbus.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_PROXY
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/dbus-internal.h"

/**
 * SECTION: logger
 * @title: TpLogger
 * @short_description: proxy object on the logger
 *
 * #TpLogger is a #TpProxy subclass on the telepathy logger.
 */

/**
 * TpLogger:
 *
 * Data structure representing a #TpLogger.
 *
 * Since: 0.99.8
 */

/**
 * TpLoggerClass:
 *
 * The class of a #TpLogger.
 *
 * Since: 0.99.8
 */

G_DEFINE_TYPE (TpLogger, tp_logger, TP_TYPE_PROXY)

struct _TpLoggerPriv
{
  gpointer unused;
};

static void
tp_logger_class_init (
    TpLoggerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  proxy_class->interface = TP_IFACE_QUARK_LOGGER;

  g_type_class_add_private (klass, sizeof (TpLoggerPriv));
}

static void
tp_logger_init (TpLogger *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_LOGGER, TpLoggerPriv);
}

static gpointer logger_singleton = NULL;

/**
 * tp_logger_dup:
 *
 * Returns an logger proxy on the D-Bus daemon on which this
 * process was activated (if it was launched by D-Bus service activation), or
 * the session bus (otherwise). This logger proxy will always have
 * the result of tp_dbus_daemon_dup() as its #TpProxy:dbus-daemon.
 *
 * The returned #TpLogger is cached; the same #TpLogger object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Returns: (transfer full): an logger proxy on the starter or session
 *          bus, or %NULL if it wasn't possible to get a dbus daemon proxy for
 *          the appropriate bus

 * Since: 0.99.8
 */
TpLogger *
tp_logger_dup (void)
{
  TpDBusDaemon *dbus;
  GError *error = NULL;

  if (logger_singleton != NULL)
    g_object_ref (logger_singleton);

  dbus = tp_dbus_daemon_dup (&error);
  if (dbus == NULL)
    {
      WARNING ("Error getting default TpDBusDaemon: %s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  logger_singleton = g_object_new (TP_TYPE_LOGGER,
      "dbus-daemon", dbus,
      "bus-name", TP_LOGGER_BUS_NAME,
      "object-path", TP_LOGGER_OBJECT_PATH,
      NULL);

  g_assert (logger_singleton != NULL);
  g_object_add_weak_pointer (logger_singleton, &logger_singleton);

  g_object_unref (dbus);
  return logger_singleton;
}
