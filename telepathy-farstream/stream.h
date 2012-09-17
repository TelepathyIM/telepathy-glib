#ifndef __TF_STREAM_H__
#define __TF_STREAM_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include <farstream/fs-conference.h>

G_BEGIN_DECLS

#define TF_TYPE_STREAM tf_stream_get_type()

#define TF_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_STREAM, TfStream))

#define TF_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_STREAM, TfStreamClass))

#define TF_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TF_TYPE_STREAM))

#define TF_IS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TF_TYPE_STREAM))

#define TF_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_STREAM, TfStreamClass))

/**
 * TfStream:
 *
 * All members are privated
 */

typedef struct _TfStream TfStream;

/**
 * TfStreamClass:
 *
 * This class is not subclassable
 */

typedef struct _TfStreamClass TfStreamClass;

GType tf_stream_get_type (void);

guint tf_stream_get_id (TfStream *stream);

void tf_stream_error (TfStream *self,
  TpMediaStreamError error,
  const gchar *message);


typedef struct _TfStreamPrivate TfStreamPrivate;

/*
 * TfStream:
 * @parent: the parent #GObject
 * @stream_id: the ID of the stream (READ-ONLY)
 *
 * All other members are privated
 */

struct _TfStream {
  GObject parent;

  /* Read-only */
  guint stream_id;

  /*< private >*/

  TfStreamPrivate *priv;
};

/*
 * TfStreamClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

struct _TfStreamClass {
  GObjectClass parent_class;

  /*< private >*/

  gpointer unused[4];
};


typedef struct {
  gchar *nat_traversal;
  gchar *stun_server;
  guint16 stun_port;
  gchar *relay_token;
} TfNatProperties;

typedef void (NewStreamCreatedCb) (TfStream *stream, gpointer channel);

TfStream *
_tf_stream_new (gpointer channel,
    FsConference *conference,
    FsParticipant *participant,
    TpMediaStreamHandler *proxy,
    guint stream_id,
    TpMediaStreamType media_type,
    TpMediaStreamDirection direction,
    TfNatProperties *nat_props,
    GList *local_codecs_config,
    NewStreamCreatedCb new_stream_created_cb);

gboolean _tf_stream_bus_message (TfStream *stream,
    GstMessage *message);

void _tf_stream_try_sending_codecs (TfStream *stream);

TpMediaStreamError fserror_to_tperror (GError *error);



G_END_DECLS

#endif /* __TF_STREAM_H__ */
