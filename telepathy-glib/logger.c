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
#include "telepathy-glib/client-factory-internal.h"
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

TpLogger *
_tp_logger_new (TpClientFactory *factory)
{
  return g_object_new (TP_TYPE_LOGGER,
      "bus-name", TP_LOGGER_BUS_NAME,
      "object-path", TP_LOGGER_OBJECT_PATH,
      "factory", factory,
      NULL);
}

/**
 * tp_logger_dup:
 *
 * Returns the default #TpClientFactory's #TpLogger. It will use
 * tp_client_factory_dup(), print a warning and return %NULL if it fails.
 *
 * Returns: (transfer full): a reference on a #TpLogger singleton.
 *
 * Since: 0.99.8
 */
TpLogger *
tp_logger_dup (void)
{
  TpLogger *self;
  TpClientFactory *factory;
  GError *error = NULL;

  factory = tp_client_factory_dup (&error);
  if (factory == NULL)
    {
      WARNING ("Error getting default TpClientFactory: %s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  self = tp_client_factory_ensure_logger (factory);

  g_object_unref (factory);

  return self;
}
