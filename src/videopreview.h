#ifndef __TP_STREAM_ENGINE_VIDEO_PREVIEW_H__
#define __TP_STREAM_ENGINE_VIDEO_PREVIEW_H__

#include <glib-object.h>

#include "videosink.h"

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW tp_stream_engine_video_preview_get_type()

#define TP_STREAM_ENGINE_VIDEO_PREVIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW, TpStreamEngineVideoPreview))

#define TP_STREAM_ENGINE_VIDEO_PREVIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW, TpStreamEngineVideoPreviewClass))

#define TP_STREAM_ENGINE_IS_VIDEO_PREVIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW))

#define TP_STREAM_ENGINE_IS_VIDEO_PREVIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW))

#define TP_STREAM_ENGINE_VIDEO_PREVIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_VIDEO_PREVIEW, TpStreamEngineVideoPreviewClass))

typedef struct _TpStreamEngineVideoPreviewPrivate
          TpStreamEngineVideoPreviewPrivate;

typedef struct {
  TpStreamEngineVideoSink parent;

  TpStreamEngineVideoPreviewPrivate *priv;
} TpStreamEngineVideoPreview;

typedef struct {
  TpStreamEngineVideoSinkClass parent_class;
} TpStreamEngineVideoPreviewClass;

GType tp_stream_engine_video_preview_get_type (void);

TpStreamEngineVideoPreview *
tp_stream_engine_video_preview_new (GstBin *bin,
    GError **error);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_VIDEO_PREVIEW_H__ */
