#ifndef __TF_SESSION_H__
#define __TF_SESSION_H__

#include <glib-object.h>
#include <telepathy-glib/media-interfaces.h>

G_BEGIN_DECLS

#define TF_TYPE_SESSION tf_session_get_type()

#define TF_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_SESSION, TfSession))

#define TF_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_SESSION, TfSessionClass))

#define TP_STREAM_ENGINE_IS_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TF_TYPE_SESSION))

#define TP_STREAM_ENGINE_IS_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TF_TYPE_SESSION))

#define TF_SESSION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_SESSION, TfSessionClass))

typedef struct _TfSessionPrivate TfSessionPrivate;


/**
 * TfSession:
 *
 * All members of the object are private
 */

typedef struct {
  GObject parent;

  TfSessionPrivate *priv;
} TfSession;

/**
 * TfSessionClass:
 *
 * There are no overridable functions
 */

typedef struct {
  GObjectClass parent_class;
} TfSessionClass;

GType tf_session_get_type (void);

TfSession *
tf_session_new (TpMediaSessionHandler *proxy,
                              const gchar *conference_type,
                              GError **error);

gboolean tf_session_bus_message (TfSession *session,
    GstMessage *message);



G_END_DECLS

#endif /* __TF_SESSION_H__ */

