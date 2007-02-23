#ifndef __TP_STREAM_ENGINE_CHANNEL_H__
#define __TP_STREAM_ENGINE_CHANNEL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_CHANNEL tp_stream_engine_channel_get_type()

#define TP_STREAM_ENGINE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannel))

#define TP_STREAM_ENGINE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannelClass))

#define TP_STREAM_ENGINE_IS_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_CHANNEL))

#define TP_STREAM_ENGINE_IS_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_CHANNEL))

#define TP_STREAM_ENGINE_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannelClass))

typedef struct {
  GObject parent;

  gchar *channel_path;
  GPtrArray *sessions;
} TpStreamEngineChannel;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineChannelClass;

GType tp_stream_engine_channel_get_type (void);

TpStreamEngineChannel* tp_stream_engine_channel_new (void);
gboolean tp_stream_engine_channel_go (
  TpStreamEngineChannel *chan,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *channel_path,
  guint handle_type,
  guint handle,
  GError **error);
void tp_stream_engine_channel_error (
  TpStreamEngineChannel *self,
  guint error,
  const gchar *message);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_CHANNEL_H__ */

