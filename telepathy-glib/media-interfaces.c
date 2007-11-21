/*
 * media-interfaces.c - proxies for Telepathy media session/stream handlers
 *
 * Copyright (C) 2007 Collabora Ltd.
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
};

/**
 * TpMediaStreamHandler:
 *
 * A proxy object for a Telepathy connection manager.
 */
struct _TpMediaStreamHandler {
    TpProxy parent;
    /*<private>*/
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
}

/**
 * TpMediaSessionHandlerClass:
 *
 * The class of a #TpMediaSessionHandler.
 */
struct _TpMediaSessionHandlerClass {
    TpProxyClass parent_class;
    /*<private>*/
};

/**
 * TpMediaSessionHandler:
 *
 * A proxy object for a Telepathy connection manager.
 */
struct _TpMediaSessionHandler {
    TpProxy parent;
    /*<private>*/
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
}
