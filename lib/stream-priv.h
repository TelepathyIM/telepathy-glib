#ifndef __TPMEDIA_STREAM_PRIV_H__
#define __TPMEDIA_STREAM_PRIV_H__

#include "stream.h"

G_BEGIN_DECLS

typedef struct {
  gchar *nat_traversal;
  gchar *stun_server;
  guint16 stun_port;
  gchar *relay_token;
} TpmediaNatProperties;

TpmediaStream *
_tpmedia_stream_new (gpointer channel,
    FsConference *conference,
    FsParticipant *participant,
    TpMediaStreamHandler *proxy,
    guint stream_id,
    TpMediaStreamType media_type,
    TpMediaStreamDirection direction,
    TpmediaNatProperties *nat_props,
    GList *local_codecs_config,
    GError **error);

gboolean _tpmedia_stream_bus_message (TpmediaStream *stream,
    GstMessage *message);

G_END_DECLS

#endif /* __TPMEDIA_STREAM_PRIV_H__ */
