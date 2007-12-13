#ifndef __TP_STREAM_ENGINE_SESSION_H__
#define __TP_STREAM_ENGINE_SESSION_H__

#include <glib-object.h>

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

typedef struct _TpStreamEngineSessionPrivate TpStreamEngineSessionPrivate;

typedef struct {
  GObject parent;

  TpStreamEngineSessionPrivate *priv;
} TpStreamEngineSession;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineSessionClass;

GType tp_stream_engine_session_get_type (void);

TpStreamEngineSession *
tp_stream_engine_session_new (const gchar *bus_name,
                              const gchar *object_path,
                              const gchar *session_type,
                              GError **error);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_SESSION_H__ */

