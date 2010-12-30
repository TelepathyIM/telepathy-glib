
#ifndef __TF_CONTENT_PRIV_H__
#define __TF_CONTENT_PRIV_H__

#include <glib-object.h>

G_BEGIN_DECLS

struct _TfContent {
  GObject parent;

  gboolean sending;
};

struct _TfContentClass{
  GObjectClass parent_class;
};

gboolean _tf_content_start_sending (TfContent *self);
void _tf_content_stop_sending (TfContent *self);


G_END_DECLS

#endif /* __TF_CONTENT_PRIV_H__ */
