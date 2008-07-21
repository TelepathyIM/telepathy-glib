#ifndef __TPMEDIA_CHANNEL_H__
#define __TPMEDIA_CHANNEL_H__

#include <glib-object.h>

#include "stream.h"

G_BEGIN_DECLS

#define TPMEDIA_TYPE_CHANNEL tpmedia_channel_get_type()

#define TPMEDIA_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TPMEDIA_TYPE_CHANNEL, TpmediaChannel))

#define TPMEDIA_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TPMEDIA_TYPE_CHANNEL, TpmediaChannelClass))

#define TP_STREAM_ENGINE_IS_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPMEDIA_TYPE_CHANNEL))

#define TP_STREAM_ENGINE_IS_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TPMEDIA_TYPE_CHANNEL))

#define TPMEDIA_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TPMEDIA_TYPE_CHANNEL, TpmediaChannelClass))

typedef struct _TpmediaChannelPrivate TpmediaChannelPrivate;

typedef struct {
  GObject parent;

  TpmediaChannelPrivate *priv;
} TpmediaChannel;

typedef struct {
  GObjectClass parent_class;
} TpmediaChannelClass;

GType tpmedia_channel_get_type (void);

TpmediaChannel *tpmedia_channel_new (TpDBusDaemon *dbus_daemon,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *channel_path,
  guint handle_type,
  guint handle,
  GError **error);

void tpmedia_channel_error (
  TpmediaChannel *self,
  guint error,
  const gchar *message);

TpmediaStream *tpmedia_channel_lookup_stream (
  TpmediaChannel *chan,
  guint stream_id);

typedef void (* TpmediaChannelStreamFunc) (TpmediaChannel *chan,
  guint stream_id, TpmediaStream *stream, gpointer user_data);

void tpmedia_channel_foreach_stream (
  TpmediaChannel *chan,
  TpmediaChannelStreamFunc func,
  gpointer user_data);

gboolean tpmedia_channel_bus_message (TpmediaChannel *channel,
    GstMessage *message);

G_END_DECLS

#endif /* __TPMEDIA_CHANNEL_H__ */

