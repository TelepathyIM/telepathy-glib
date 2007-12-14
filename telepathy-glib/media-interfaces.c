/*
 * media-interfaces.c - proxies for Telepathy media session/stream handlers
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#include "telepathy-glib/media-interfaces.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>

#include "telepathy-glib/_gen/tp-cli-media-session-handler-body.h"
#include "telepathy-glib/_gen/tp-cli-media-stream-handler-body.h"

/**
 * SECTION:media-interfaces
 * @title: TpMediaSessionHandler, TpMediaStreamHandler
 * @short_description: proxy objects for Telepathy media streaming
 * @see_also: #TpChannel, #TpProxy
 *
 * This module provides access to the auxiliary objects used to
 * implement #TpSvcChannelTypeStreamedMedia.
 */

/**
 * TpMediaStreamHandlerClass:
 *
 * The class of a #TpMediaStreamHandler.
 */
struct _TpMediaStreamHandlerClass {
    TpProxyClass parent_class;
    /*<private>*/
    gpointer priv;
};

/**
 * TpMediaStreamHandler:
 *
 * A proxy object for a Telepathy connection manager.
 */
struct _TpMediaStreamHandler {
    TpProxy parent;
    /*<private>*/
    TpMediaStreamHandlerPrivate *priv;
};

G_DEFINE_TYPE (TpMediaStreamHandler,
    tp_media_stream_handler,
    TP_TYPE_PROXY);

static void
tp_media_stream_handler_init (TpMediaStreamHandler *self)
{
}

static void
tp_media_stream_handler_class_init (TpMediaStreamHandlerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  proxy_class->must_have_unique_name = TRUE;
  proxy_class->interface = TP_IFACE_QUARK_MEDIA_STREAM_HANDLER;
  tp_proxy_class_hook_on_interface_add (proxy_class,
      tp_cli_media_stream_handler_add_signals);
}

/**
 * tp_media_stream_handler_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @unique_name: the unique name of the connection process; may not be %NULL
 *  or a well-known name
 * @object_path: the object path of the media stream handler; may not be %NULL
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new media stream handler proxy, or %NULL on invalid arguments
 */
TpMediaStreamHandler *
tp_media_stream_handler_new (TpDBusDaemon *dbus,
                             const gchar *unique_name,
                             const gchar *object_path,
                             GError **error)
{
  TpMediaStreamHandler *ret = NULL;

  if (!tp_dbus_check_valid_bus_name (unique_name,
        TP_DBUS_NAME_TYPE_UNIQUE, error))
    goto finally;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  ret = TP_MEDIA_STREAM_HANDLER (g_object_new (TP_TYPE_MEDIA_STREAM_HANDLER,
        "dbus-daemon", dbus,
        "bus-name", unique_name,
        "object-path", object_path,
        NULL));

finally:
  return ret;
}

/**
 * TpMediaSessionHandlerClass:
 *
 * The class of a #TpMediaSessionHandler.
 */
struct _TpMediaSessionHandlerClass {
    TpProxyClass parent_class;
    /*<private>*/
    gpointer priv;
};

/**
 * TpMediaSessionHandler:
 *
 * A proxy object for a Telepathy connection manager.
 */
struct _TpMediaSessionHandler {
    TpProxy parent;
    /*<private>*/
    TpMediaSessionHandlerPrivate *priv;
};

G_DEFINE_TYPE (TpMediaSessionHandler,
    tp_media_session_handler,
    TP_TYPE_PROXY);

static void
tp_media_session_handler_init (TpMediaSessionHandler *self)
{
}

static void
tp_media_session_handler_class_init (TpMediaSessionHandlerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  proxy_class->must_have_unique_name = TRUE;
  proxy_class->interface = TP_IFACE_QUARK_MEDIA_SESSION_HANDLER;
  tp_proxy_class_hook_on_interface_add (proxy_class,
      tp_cli_media_session_handler_add_signals);
}

/**
 * tp_media_session_handler_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @unique_name: the unique name of the connection process; may not be %NULL
 *  or a well-known name
 * @object_path: the object path of the media session handler; may not be %NULL
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new media session handler proxy, or %NULL on invalid arguments
 */
TpMediaSessionHandler *
tp_media_session_handler_new (TpDBusDaemon *dbus,
                              const gchar *unique_name,
                              const gchar *object_path,
                              GError **error)
{
  TpMediaSessionHandler *ret = NULL;

  if (!tp_dbus_check_valid_bus_name (unique_name,
        TP_DBUS_NAME_TYPE_UNIQUE, error))
    goto finally;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  ret = TP_MEDIA_SESSION_HANDLER (g_object_new (TP_TYPE_MEDIA_SESSION_HANDLER,
        "dbus-daemon", dbus,
        "bus-name", unique_name,
        "object-path", object_path,
        NULL));

finally:
  return ret;
}
