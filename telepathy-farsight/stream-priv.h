#ifndef __TF_STREAM_PRIV_H__
#define __TF_STREAM_PRIV_H__

#include "stream.h"

G_BEGIN_DECLS

typedef struct {
  gchar *nat_traversal;
  gchar *stun_server;
  guint16 stun_port;
  gchar *relay_token;
} TfNatProperties;

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
    GError **error);

gboolean _tf_stream_bus_message (TfStream *stream,
    GstMessage *message);

G_END_DECLS

#endif /* __TF_STREAM_PRIV_H__ */
