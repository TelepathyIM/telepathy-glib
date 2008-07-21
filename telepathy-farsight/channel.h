#ifndef __TF_CHANNEL_H__
#define __TF_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/channel.h>

#include "stream.h"

G_BEGIN_DECLS

#define TF_TYPE_CHANNEL tf_channel_get_type()

#define TF_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_CHANNEL, TfChannel))

#define TF_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_CHANNEL, TfChannelClass))

#define TP_STREAM_ENGINE_IS_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_CHANNEL))

#define TP_STREAM_ENGINE_IS_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_CHANNEL))

#define TF_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_CHANNEL, TfChannelClass))

typedef struct _TfChannelPrivate TfChannelPrivate;

/**
 * TfChannel:
 *
 * All members of the object are private
 */

typedef struct {
  GObject parent;

  /*< private >*/

  TfChannelPrivate *priv;
} TfChannel;

/**
 * TfChannelClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct {
  GObjectClass parent_class;

  /*< private >*/

  gpointer unused[4];
} TfChannelClass;

GType tf_channel_get_type (void);

TfChannel *tf_channel_new (
    TpDBusDaemon *dbus_daemon,
    const gchar *bus_name,
    const gchar *connection_path,
    const gchar *channel_path,
    guint handle_type,
    guint handle,
    GError **error);

TfChannel *tf_channel_new_from_proxy (TpChannel *channel_proxy);


void tf_channel_error (TfChannel *chan,
  guint error,
  const gchar *message);

TfStream *tf_channel_lookup_stream (TfChannel *chan,
  guint stream_id);

/**
 * TfChannelStreamFunc:
 * @chan: The #TpMediaChannel
 * @stream_id: the id of the stream
 * @stream: the #TfStream
 * @user_data: the passed user data
 *
 * Callback function called on every stream by tf_channel_foreach_stream()
 */

typedef void (* TfChannelStreamFunc) (TfChannel *chan,
    guint stream_id,
    TfStream *stream,
    gpointer user_data);

void tf_channel_foreach_stream (TfChannel *chan,
    TfChannelStreamFunc func,
    gpointer user_data);

gboolean tf_channel_bus_message (TfChannel *channel,
    GstMessage *message);

G_END_DECLS

#endif /* __TF_CHANNEL_H__ */

