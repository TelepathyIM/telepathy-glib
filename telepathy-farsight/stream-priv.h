#ifndef __TF_STREAM_PRIV_H__
#define __TF_STREAM_PRIV_H__

#include "stream.h"

G_BEGIN_DECLS

typedef struct _TfStreamPrivate TfStreamPrivate;

/**
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

/**
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

#endif /* __TF_STREAM_PRIV_H__ */
