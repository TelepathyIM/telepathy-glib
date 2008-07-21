#ifndef __TPMEDIA_SESSION_H__
#define __TPMEDIA_SESSION_H__

#include <glib-object.h>
#include <telepathy-glib/media-interfaces.h>

G_BEGIN_DECLS

#define TPMEDIA_TYPE_SESSION tpmedia_session_get_type()

#define TPMEDIA_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TPMEDIA_TYPE_SESSION, TpmediaSession))

#define TPMEDIA_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TPMEDIA_TYPE_SESSION, TpmediaSessionClass))

#define TP_STREAM_ENGINE_IS_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TPMEDIA_TYPE_SESSION))

#define TP_STREAM_ENGINE_IS_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TPMEDIA_TYPE_SESSION))

#define TPMEDIA_SESSION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TPMEDIA_TYPE_SESSION, TpmediaSessionClass))

typedef struct _TpmediaSessionPrivate TpmediaSessionPrivate;


/**
 * TpmediaSession:
 *
 * All members of the object are private
 */

typedef struct {
  GObject parent;

  TpmediaSessionPrivate *priv;
} TpmediaSession;

/**
 * TpmediaSessionClass:
 *
 * There are no overridable functions
 */

typedef struct {
  GObjectClass parent_class;
} TpmediaSessionClass;

GType tpmedia_session_get_type (void);

TpmediaSession *
tpmedia_session_new (TpMediaSessionHandler *proxy,
                              const gchar *conference_type,
                              GError **error);

gboolean tpmedia_session_bus_message (TpmediaSession *session,
    GstMessage *message);



G_END_DECLS

#endif /* __TPMEDIA_SESSION_H__ */

