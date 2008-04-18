#ifndef __TP_STREAM_ENGINE_VIDEO_SINK_H__
#define __TP_STREAM_ENGINE_VIDEO_SINK_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_VIDEO_SINK tp_stream_engine_video_sink_get_type()

#define TP_STREAM_ENGINE_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_SINK, TpStreamEngineVideoSink))

#define TP_STREAM_ENGINE_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_SINK, TpStreamEngineVideoSinkClass))

#define TP_STREAM_ENGINE_IS_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_SINK))

#define TP_STREAM_ENGINE_IS_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_SINK))

#define TP_STREAM_ENGINE_VIDEO_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_SINK, TpStreamEngineVideoSinkClass))

typedef struct _TpStreamEngineVideoSinkPrivate
          TpStreamEngineVideoSinkPrivate;

typedef struct {
  GObject parent;

  TpStreamEngineVideoSinkPrivate *priv;
} TpStreamEngineVideoSink;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineVideoSinkClass;

GType tp_stream_engine_video_sink_get_type (void);

gboolean
tp_stream_engine_video_sink_bus_sync_message (
    TpStreamEngineVideoSink *self,
    GstMessage *message);


G_END_DECLS

#endif /* __TP_STREAM_ENGINE_VIDEO_SINK_H__ */
