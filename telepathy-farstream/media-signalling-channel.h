/*
 * media-signalling-channel.h - Source for TfMediaSignallingChannel
 * Copyright (C) 2006-2011 Collabora Ltd.
 * Copyright (C) 2006-2011 Nokia Corporation
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


#ifndef __TF_MEDIA_SIGNALLING_CHANNEL_H__
#define __TF_MEDIA_SIGNALLING_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>
#include "stream.h"
#include "session-priv.h"

G_BEGIN_DECLS

#define TF_TYPE_MEDIA_SIGNALLING_CHANNEL tf_media_signalling_channel_get_type()

#define TF_MEDIA_SIGNALLING_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_MEDIA_SIGNALLING_CHANNEL, TfMediaSignallingChannel))

#define TF_MEDIA_SIGNALLING_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_MEDIA_SIGNALLING_CHANNEL, TfMediaSignallingChannelClass))

#define TF_IS_MEDIA_SIGNALLING_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_MEDIA_SIGNALLING_CHANNEL))

#define TF_IS_MEDIA_SIGNALLING_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_MEDIA_SIGNALLING_CHANNEL))

#define TF_MEDIA_SIGNALLING_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_MEDIA_SIGNALLING_CHANNEL, TfMediaSignallingChannelClass))

typedef struct _TfMediaSignallingChannelPrivate TfMediaSignallingChannelPrivate;

/**
 * TfMediaSignallingChannel:
 *
 * All members of the object are private
 */

typedef struct _TfMediaSignallingChannel TfMediaSignallingChannel;

/**
 * TfMediaSignallingChannelClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */


struct _TfMediaSignallingChannel {
  GObject parent;

  TpChannel *channel_proxy;

  TfNatProperties nat_props;
  guint prop_id_nat_traversal;
  guint prop_id_stun_server;
  guint prop_id_stun_port;
  guint prop_id_gtalk_p2p_relay_token;

  /* sessions is NULL until we've had a reply from GetSessionHandlers */
  TfSession *session;
  gboolean got_sessions;
  GPtrArray *streams;
};

struct _TfMediaSignallingChannelClass{
  GObjectClass parent_class;
};


typedef struct _TfMediaSignallingChannelClass TfMediaSignallingChannelClass;

GType tf_media_signalling_channel_get_type (void);

TfMediaSignallingChannel *tf_media_signalling_channel_new (
    TpChannel *channel_proxy);

TfStream *tf_media_signalling_channel_lookup_stream (
    TfMediaSignallingChannel *chan,
    guint stream_id);

gboolean tf_media_signalling_channel_bus_message (
    TfMediaSignallingChannel *channel,
    GstMessage *message);

G_END_DECLS

#endif /* __TF_MEDIA_SIGNALLING_CHANNEL_H__ */

