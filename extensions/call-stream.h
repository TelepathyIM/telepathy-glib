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

#ifndef FUTURE_CALL_STREAM_H
#define FUTURE_CALL_STREAM_H

#include <telepathy-glib/channel.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _FutureCallStream FutureCallStream;
typedef struct _FutureCallStreamPrivate FutureCallStreamPrivate;
typedef struct _FutureCallStreamClass FutureCallStreamClass;

GType future_call_stream_get_type (void);

/* TYPE MACROS */
#define FUTURE_TYPE_CALL_STREAM \
  (future_call_stream_get_type ())
#define FUTURE_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), FUTURE_TYPE_CALL_STREAM, \
                              FutureCallStream))
#define FUTURE_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), FUTURE_TYPE_CALL_STREAM, \
                           FutureCallStreamClass))
#define FUTURE_IS_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), FUTURE_TYPE_CALL_STREAM))
#define FUTURE_IS_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), FUTURE_TYPE_CALL_STREAM))
#define FUTURE_CALL_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FUTURE_TYPE_CALL_STREAM, \
                              FutureCallStreamClass))

FutureCallStream *future_call_stream_new (TpChannel *channel,
    const gchar *object_path, GError **error);

void future_call_stream_init_known_interfaces (void);

G_END_DECLS

#include "extensions/_gen/cli-call-stream.h"

#endif
