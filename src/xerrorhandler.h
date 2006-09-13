/*
 * xerrorhandler.h - Map X errors to GObject signals
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

#include <glib-object.h>

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER tp_stream_engine_x_error_handler_get_type()

#define TP_STREAM_ENGINE_X_ERROR_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandler))

#define TP_STREAM_ENGINE_X_ERROR_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandlerClass))

#define TP_STREAM_ENGINE_IS_X_ERROR_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER))

#define TP_STREAM_ENGINE_IS_X_ERROR_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER))

#define TP_STREAM_ENGINE_X_ERROR_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandlerClass))

typedef struct {
  GObject parent;
} TpStreamEngineXErrorHandler;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineXErrorHandlerClass;

GType tp_stream_engine_x_error_handler_get_type (void);

TpStreamEngineXErrorHandler* tp_stream_engine_x_error_handler_get (void);

G_END_DECLS

