#ifndef __TF_MEDIA_SIGNALLING_CHANNEL_H__
#define __TF_MEDIA_SIGNALLING_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/channel.h>
#include "stream.h"

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

