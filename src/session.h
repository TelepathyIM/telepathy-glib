#ifndef __TP_STREAM_ENGINE_SESSION_H__
#define __TP_STREAM_ENGINE_SESSION_H__

#include <glib-object.h>

#include "stream.h"

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_SESSION tp_stream_engine_session_get_type()

#define TP_STREAM_ENGINE_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSession))

#define TP_STREAM_ENGINE_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionClass))

#define TP_STREAM_ENGINE_IS_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_SESSION))

#define TP_STREAM_ENGINE_IS_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_SESSION))

#define TP_STREAM_ENGINE_SESSION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionClass))

typedef struct {
  GObject parent;

  GPtrArray *streams;
} TpStreamEngineSession;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineSessionClass;

GType tp_stream_engine_session_get_type (void);

TpStreamEngineSession* tp_stream_engine_session_new (void);
gboolean tp_stream_engine_session_go (
  TpStreamEngineSession *self, const gchar *bus_name,
  const gchar *connection_path, const gchar *session_handler_path,
  const gchar *channel_path, const gchar *type,
  const TpStreamEngineNatProperties *nat_props);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_SESSION_H__ */

