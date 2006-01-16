/*
 * tp-media-stream-handler.h - Header for TpMediaStreamHandler
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

#ifndef __TP_MEDIA_STREAM_HANDLER_H__
#define __TP_MEDIA_STREAM_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpMediaStreamHandler TpMediaStreamHandler;
typedef struct _TpMediaStreamHandlerClass TpMediaStreamHandlerClass;

struct _TpMediaStreamHandlerClass {
    GObjectClass parent_class;
};

struct _TpMediaStreamHandler {
    GObject parent;
};

GType tp_media_stream_handler_get_type(void);

/* TYPE MACROS */
#define TP_TYPE_MEDIA_STREAM_HANDLER \
  (tp_media_stream_handler_get_type())
#define TP_MEDIA_STREAM_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_MEDIA_STREAM_HANDLER, TpMediaStreamHandler))
#define TP_MEDIA_STREAM_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_MEDIA_STREAM_HANDLER, TpMediaStreamHandlerClass))
#define TP_IS_MEDIA_STREAM_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_MEDIA_STREAM_HANDLER))
#define TP_IS_MEDIA_STREAM_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_MEDIA_STREAM_HANDLER))
#define TP_MEDIA_STREAM_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_MEDIA_STREAM_HANDLER, TpMediaStreamHandlerClass))


gboolean tp_media_stream_handler_codec_choice (TpMediaStreamHandler *obj, gint codec_id, GError **error);
gboolean tp_media_stream_handler_error (TpMediaStreamHandler *obj, gint errno, const gchar * message, GError **error);
gboolean tp_media_stream_handler_introspect (TpMediaStreamHandler *obj, gchar ** ret, GError **error);
gboolean tp_media_stream_handler_native_candidates_prepared (TpMediaStreamHandler *obj, GError **error);
gboolean tp_media_stream_handler_new_active_candidate_pair (TpMediaStreamHandler *obj, const gchar * native_candidate_id, const gchar * remote_candidate_id, GError **error);
gboolean tp_media_stream_handler_new_native_candidate (TpMediaStreamHandler *obj, const gchar * candidate_id, const GArray * transports, GError **error);
gboolean tp_media_stream_handler_ready (TpMediaStreamHandler *obj, GError **error);
gboolean tp_media_stream_handler_supported_codecs (TpMediaStreamHandler *obj, const GArray * codecs, GError **error);


G_END_DECLS

#endif /* #ifndef __TP_MEDIA_STREAM_HANDLER_H__*/
