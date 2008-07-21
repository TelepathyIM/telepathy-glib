#ifndef __TPMEDIA_STREAM_H__
#define __TPMEDIA_STREAM_H__

#include <glib-object.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/media-interfaces.h>

#include <gst/farsight/fs-conference-iface.h>

G_BEGIN_DECLS

#define TPMEDIA_TYPE_STREAM tpmedia_stream_get_type()

#define TPMEDIA_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TPMEDIA_TYPE_STREAM, TpmediaStream))

#define TPMEDIA_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TPMEDIA_TYPE_STREAM, TpmediaStreamClass))

#define TP_STREAM_ENGINE_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TPMEDIA_TYPE_STREAM))

#define TP_STREAM_ENGINE_IS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TPMEDIA_TYPE_STREAM))

#define TPMEDIA_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TPMEDIA_TYPE_STREAM, TpmediaStreamClass))

typedef struct _TpmediaStreamPrivate TpmediaStreamPrivate;

typedef struct {
  GObject parent;

  /* Read-only */
  guint stream_id;

  TpmediaStreamPrivate *priv;
} TpmediaStream;

typedef struct {
  GObjectClass parent_class;

} TpmediaStreamClass;

typedef struct {
  gchar *nat_traversal;
  gchar *stun_server;
  guint16 stun_port;
  gchar *relay_token;
} TpStreamEngineNatProperties;

GType tpmedia_stream_get_type (void);

void tpmedia_stream_error (
  TpmediaStream *self,
  guint error,
  const gchar *message);

G_END_DECLS

#endif /* __TPMEDIA_STREAM_H__ */
