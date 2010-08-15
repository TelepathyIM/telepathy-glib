/*
 * call-stream.h - proxy for a Stream in a Call channel
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#ifndef TF_FUTURE_CALL_STREAM_H
#define TF_FUTURE_CALL_STREAM_H

#include <telepathy-glib/channel.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TfFutureCallStream TfFutureCallStream;
typedef struct _TfFutureCallStreamPrivate TfFutureCallStreamPrivate;
typedef struct _TfFutureCallStreamClass TfFutureCallStreamClass;

GType tf_future_call_stream_get_type (void);

/* TYPE MACROS */
#define TF_FUTURE_TYPE_CALL_STREAM \
  (tf_future_call_stream_get_type ())
#define TF_FUTURE_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TF_FUTURE_TYPE_CALL_STREAM, \
                              TfFutureCallStream))
#define TF_FUTURE_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TF_FUTURE_TYPE_CALL_STREAM, \
                           TfFutureCallStreamClass))
#define TF_FUTURE_IS_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TF_FUTURE_TYPE_CALL_STREAM))
#define TF_FUTURE_IS_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TF_FUTURE_TYPE_CALL_STREAM))
#define TF_FUTURE_CALL_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TF_FUTURE_TYPE_CALL_STREAM, \
                              TfFutureCallStreamClass))

TfFutureCallStream *tf_future_call_stream_new (TpChannel *channel,
    const gchar *object_path, GError **error);

void tf_future_call_stream_init_known_interfaces (void);

G_END_DECLS

#include "extensions/_gen/cli-call-stream.h"

#endif
