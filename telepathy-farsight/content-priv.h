
#ifndef __TF_CONTENT_PRIV_H__
#define __TF_CONTENT_PRIV_H__

#include <glib-object.h>

G_BEGIN_DECLS

struct _TfContent {
  GObject parent;
};

struct _TfContentClass{
  GObjectClass parent_class;
};


G_END_DECLS

#endif /* __TF_CONTENT_PRIV_H__ */
