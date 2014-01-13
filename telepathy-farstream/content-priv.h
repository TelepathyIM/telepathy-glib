
#ifndef __TF_CONTENT_PRIV_H__
#define __TF_CONTENT_PRIV_H__

#include <glib-object.h>

#include <farstream/fs-conference.h>


G_BEGIN_DECLS

struct _TfContent {
  GObject parent;

  guint sending_count;
};

struct _TfContentClass{
  GObjectClass parent_class;

  void (*content_error) (TfContent *content,
      const gchar *message);

  void (*sending_failed) (TfContent *content,
      const gchar *message);
  void (*receiving_failed) (TfContent *content,
      guint *handles, guint handle_count,
      const gchar *message);

  GstIterator * (*iterate_src_pads) (TfContent *content, guint *handle,
      guint handle_count);
};

gboolean _tf_content_start_sending (TfContent *self);
void _tf_content_stop_sending (TfContent *self);
void _tf_content_mute_to_stop_sending (TfContent *self);

void _tf_content_emit_src_pad_added (TfContent *self, guint handle,
    FsStream *stream, GstPad *pad, FsCodec *codec);

gboolean _tf_content_start_receiving (TfContent *self, guint *handles,
    guint handle_count);
void _tf_content_stop_receiving (TfContent *self,
    guint *handles, guint handle_count);

G_END_DECLS

#endif /* __TF_CONTENT_PRIV_H__ */
