/*
 * tp-channel-handler.h - Header for TpChannelHandler
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

#ifndef __TP_CHANNEL_HANDLER_H__
#define __TP_CHANNEL_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpChannelHandler TpChannelHandler;
typedef struct _TpChannelHandlerClass TpChannelHandlerClass;

struct _TpChannelHandlerClass {
    GObjectClass parent_class;
};

struct _TpChannelHandler {
    GObject parent;
};

GType tp_channel_handler_get_type(void);

/* TYPE MACROS */
#define TP_TYPE_CHANNEL_HANDLER \
  (tp_channel_handler_get_type())
#define TP_CHANNEL_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_CHANNEL_HANDLER, TpChannelHandler))
#define TP_CHANNEL_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_CHANNEL_HANDLER, TpChannelHandlerClass))
#define TP_IS_CHANNEL_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CHANNEL_HANDLER))
#define TP_IS_CHANNEL_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CHANNEL_HANDLER))
#define TP_CHANNEL_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CHANNEL_HANDLER, TpChannelHandlerClass))


gboolean tp_channel_handler_handle_channel (TpChannelHandler *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error);


G_END_DECLS

#endif /* #ifndef __TP_CHANNEL_HANDLER_H__*/
