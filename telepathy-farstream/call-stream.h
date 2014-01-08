/*
 * call-stream.h - Source for TfCallStream
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

#ifndef __TF_CALL_STREAM_H__
#define __TF_CALL_STREAM_H__

#include <glib-object.h>

#include <gst/gst.h>
#include <telepathy-glib/telepathy-glib.h>

#include "call-channel.h"
#include "call-content.h"

G_BEGIN_DECLS

#define TF_TYPE_CALL_STREAM tf_call_stream_get_type()

#define TF_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_CALL_STREAM, TfCallStream))

#define TF_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_CALL_STREAM, TfCallStreamClass))

#define TF_IS_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_CALL_STREAM))

#define TF_IS_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_CALL_STREAM))

#define TF_CALL_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_CALL_STREAM, TfCallStreamClass))

typedef struct _TfCallStreamPrivate TfCallStreamPrivate;

/**
 * TfCallStream:
 *
 * All members of the object are private
 */

typedef struct _TfCallStream TfCallStream;

/**
 * TfCallStreamClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfCallStreamClass TfCallStreamClass;


struct _TfCallStream {
  GObject parent;

  TfCallContent *call_content;

  TpCallStream *proxy;

  gboolean has_endpoint_properties;
  gchar *endpoint_objpath;
  TpProxy *endpoint;
  gchar *creds_username;
  gchar *creds_password;
  GList *stored_remote_candidates;
  gboolean multiple_usernames;
  gboolean controlling;

  gchar *last_local_username;
  gchar *last_local_password;

  TpStreamFlowState sending_state;
  gboolean has_send_resource;

  TpStreamFlowState receiving_state;
  gboolean has_receive_resource;

  gboolean has_contact;
  guint contact_handle;
  FsStream *fsstream;

  gboolean has_media_properties;
  TpStreamTransportType transport_type;
  gboolean server_info_retrieved;
  GPtrArray *stun_servers;
  GPtrArray *relay_info;
};

struct _TfCallStreamClass{
  GObjectClass parent_class;
};


GType tf_call_stream_get_type (void);

TfCallStream *tf_call_stream_new (
    TfCallContent *content,
    TpCallStream *stream_proxy);

gboolean tf_call_stream_bus_message (TfCallStream *stream, GstMessage *message);

void tf_call_stream_sending_failed (TfCallStream *stream, const gchar *message);

void tf_call_stream_receiving_failed (TfCallStream *stream,
    guint *handles, guint handle_count,
    const gchar *message);

TpCallStream *
tf_call_stream_get_proxy (TfCallStream *stream);


G_END_DECLS

#endif /* __TF_CALL_STREAM_H__ */
