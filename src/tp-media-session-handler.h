/*
 * tp-media-session-handler.h - Header for TpMediaSessionHandler
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

#ifndef __TP_MEDIA_SESSION_HANDLER_H__
#define __TP_MEDIA_SESSION_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpMediaSessionHandler TpMediaSessionHandler;
typedef struct _TpMediaSessionHandlerClass TpMediaSessionHandlerClass;

struct _TpMediaSessionHandlerClass {
    GObjectClass parent_class;
};

struct _TpMediaSessionHandler {
    GObject parent;
};

GType tp_media_session_handler_get_type(void);

/* TYPE MACROS */
#define TP_TYPE_MEDIA_SESSION_HANDLER \
  (tp_media_session_handler_get_type())
#define TP_MEDIA_SESSION_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_MEDIA_SESSION_HANDLER, TpMediaSessionHandler))
#define TP_MEDIA_SESSION_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_MEDIA_SESSION_HANDLER, TpMediaSessionHandlerClass))
#define TP_IS_MEDIA_SESSION_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_MEDIA_SESSION_HANDLER))
#define TP_IS_MEDIA_SESSION_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_MEDIA_SESSION_HANDLER))
#define TP_MEDIA_SESSION_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_MEDIA_SESSION_HANDLER, TpMediaSessionHandlerClass))


gboolean tp_media_session_handler_error (TpMediaSessionHandler *obj, gint errno, const gchar * message, GError **error);
gboolean tp_media_session_handler_introspect (TpMediaSessionHandler *obj, gchar ** ret, GError **error);


G_END_DECLS

#endif /* #ifndef __TP_MEDIA_SESSION_HANDLER_H__*/
