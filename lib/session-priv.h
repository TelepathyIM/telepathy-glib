#ifndef __TPMEDIA_SESSION_PRIV_H__
#define __TPMEDIA_SESSION_PRIV_H__

#include "session.h"

G_BEGIN_DECLS

TpmediaSession *
_tpmedia_session_new (TpMediaSessionHandler *proxy,
                              const gchar *session_type,
                              GError **error);

gboolean _tpmedia_session_bus_message (TpmediaSession *session,
    GstMessage *message);

G_END_DECLS

#endif /* __TPMEDIA_SESSION_PRIV_H__ */

