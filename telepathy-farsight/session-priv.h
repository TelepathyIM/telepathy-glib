#ifndef __TF_SESSION_PRIV_H__
#define __TF_SESSION_PRIV_H__

#include "session.h"

G_BEGIN_DECLS

TfSession *
_tf_session_new (TpMediaSessionHandler *proxy,
                              const gchar *conference_type,
                              GError **error);

gboolean _tf_session_bus_message (TfSession *session,
    GstMessage *message);

G_END_DECLS

#endif /* __TF_SESSION_PRIV_H__ */

