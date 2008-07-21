#ifndef __TP_STREAM_ENGINE_VIDEO_STREAM_H__
#define __TP_STREAM_ENGINE_VIDEO_STREAM_H__

#include <glib-object.h>

#include <telepathy-farsight/stream.h>

#include "videosink.h"

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_VIDEO_STREAM tp_stream_engine_video_stream_get_type()

#define TP_STREAM_ENGINE_VIDEO_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStream))

#define TP_STREAM_ENGINE_VIDEO_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStreamClass))

#define TP_STREAM_ENGINE_IS_VIDEO_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_STREAM))

#define TP_STREAM_ENGINE_IS_VIDEO_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_STREAM))

#define TP_STREAM_ENGINE_VIDEO_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_STREAM, TpStreamEngineVideoStreamClass))

typedef struct _TpStreamEngineVideoStreamPrivate
          TpStreamEngineVideoStreamPrivate;

typedef struct {
  TpStreamEngineVideoSink parent;

  TpStreamEngineVideoStreamPrivate *priv;
} TpStreamEngineVideoStream;

typedef struct {
  TpStreamEngineVideoSinkClass parent_class;
} TpStreamEngineVideoStreamClass;

GType tp_stream_engine_video_stream_get_type (void);


TpStreamEngineVideoStream *
tp_stream_engine_video_stream_new (
  TfStream *stream,
  GstBin *bin,
  GstPad *pad,
  GError **error);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_VIDEO_STREAM_H__ */
