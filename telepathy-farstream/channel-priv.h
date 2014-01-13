#ifndef __TF_CHANNEL_PRIV_H__
#define __TF_CHANNEL_PRIV_H__

#include "channel.h"

G_BEGIN_DECLS


struct _TfChannel{
  GObject parent;

  /*< private >*/

  TfChannelPrivate *priv;
};

struct _TfChannelClass{
  GObjectClass parent_class;

  /*< private >*/

  gpointer unused[4];
};


G_END_DECLS

#endif /* __TF_CHANNEL_PRIV_H__ */
