#ifndef __TP_STREAM_ENGINE_STREAM_H__
#define __TP_STREAM_ENGINE_STREAM_H__

#include <glib-object.h>
#include <farsight/farsight-session.h>

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_STREAM tp_stream_engine_stream_get_type()

#define TP_STREAM_ENGINE_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_STREAM, TpStreamEngineStream))

#define TP_STREAM_ENGINE_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_STREAM, TpStreamEngineStreamClass))

#define TP_STREAM_ENGINE_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_STREAM))

#define TP_STREAM_ENGINE_IS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_STREAM))

#define TP_STREAM_ENGINE_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_STREAM, TpStreamEngineStreamClass))

typedef struct {
  GObject parent;

  guint stream_id;
} TpStreamEngineStream;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineStreamClass;

GType tp_stream_engine_stream_get_type (void);

TpStreamEngineStream* tp_stream_engine_stream_new (void);
gboolean tp_stream_engine_stream_go (
  TpStreamEngineStream *self,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *stream_handler_path,
  const gchar *channel_path,
  FarsightSession *fs_session,
  guint id,
  guint media_type,
  guint direction);
gboolean tp_stream_engine_stream_mute_input (
  TpStreamEngineStream *chan,
  gboolean mute_state,
  GError **error);
gboolean tp_stream_engine_stream_mute_output (
  TpStreamEngineStream *chan,
  gboolean mute_state,
  GError **error);
gboolean tp_stream_engine_stream_set_output_volume (
  TpStreamEngineStream *chan,
  guint volume,
  GError **error);
gboolean tp_stream_engine_stream_set_output_window (
  TpStreamEngineStream *chan,
  guint window_id,
  GError **error);
gboolean tp_stream_engine_stream_set_preview_window (
  TpStreamEngineStream *chan,
  guint window_id,
  GError **error);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_STREAM_H__ */
