
#ifndef __TF_CONTENT_PRIV_H__
#define __TF_CONTENT_PRIV_H__

#include <glib-object.h>

#include <gst/farsight/fs-conference-iface.h>


G_BEGIN_DECLS

struct _TfContent {
  GObject parent;

  guint sending_count;
};

struct _TfContentClass{
  GObjectClass parent_class;

  void (*content_error) (TfContent *chan,
      guint reason, /* TfFutureContentRemovalReason */
      const gchar *detailed_reason,
      const gchar *message);
};

gboolean _tf_content_start_sending (TfContent *self);
void _tf_content_stop_sending (TfContent *self);

void _tf_content_emit_src_pad_added (TfContent *self, guint handle,
    FsStream *stream, GstPad *pad, FsCodec *codec);


G_END_DECLS

#endif /* __TF_CONTENT_PRIV_H__ */
