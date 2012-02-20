/*
 * call-content.h - Source for TfCallContent
 * Copyright (C) 2010 Collabora Ltd.
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

#ifndef __TF_CALL_CONTENT_H__
#define __TF_CALL_CONTENT_H__

#include <glib-object.h>

#include <gst/gst.h>
#include <telepathy-glib/telepathy-glib.h>

#include "call-channel.h"
#include "content.h"
#include "content-priv.h"

G_BEGIN_DECLS

#define TF_TYPE_CALL_CONTENT tf_call_content_get_type()

#define TF_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_CALL_CONTENT, TfCallContent))

#define TF_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_CALL_CONTENT, TfCallContentClass))

#define TF_IS_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_CALL_CONTENT))

#define TF_IS_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_CALL_CONTENT))

#define TF_CALL_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_CALL_CONTENT, TfCallContentClass))

typedef struct _TfCallContentPrivate TfCallContentPrivate;

/**
 * TfCallContent:
 *
 * All members of the object are private
 */

typedef struct _TfCallContent TfCallContent;

/**
 * TfCallContentClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfCallContentClass TfCallContentClass;

GType tf_call_content_get_type (void);

TfCallContent *tf_call_content_new_async (
    TfCallChannel *call_channel,
    TpCallContent *content_proxy,
    GError **error,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tf_call_content_bus_message (TfCallContent *content,
    GstMessage *message);


/* Private */
FsStream *_tf_call_content_get_fsstream_by_handle (TfCallContent *content,
    guint contact_handle,
    FsStreamDirection dir,
    const gchar *transmitter,
    guint stream_transmitter_n_parameters,
    GParameter *stream_transmitter_parameters,
    GError **error);
void _tf_call_content_put_fsstream (TfCallContent *content, FsStream *fsstream);

FsMediaType
tf_call_content_get_fs_media_type (TfCallContent *content);


gboolean
tf_call_content_bus_message (TfCallContent *content, GstMessage *message);

TpCallContent *
tf_call_content_get_proxy (TfCallContent *content);

G_END_DECLS

#endif /* __TF_CALL_CONTENT_H__ */

