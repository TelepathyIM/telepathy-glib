#ifndef __TF_CONTENT_H__
#define __TF_CONTENT_H__

#include <glib-object.h>
#include <gst/farsight/fs-conference-iface.h>

G_BEGIN_DECLS

#define TF_TYPE_CONTENT tf_content_get_type()

#define TF_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_CONTENT, TfContent))

#define TF_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_CONTENT, TfContentClass))

#define TF_IS_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_CONTENT))

#define TF_IS_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_CONTENT))

#define TF_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_CONTENT, TfContentClass))

typedef struct _TfContentPrivate TfContentPrivate;

/**
 * TfContent:
 *
 * This structure is private, this class is not subclassable.
 */

typedef struct _TfContent TfContent;

/**
 * TfContentClass:
 *
 * This structure is private, this class is not subclassable.
 */

typedef struct _TfContentClass TfContentClass;

GType tf_content_get_type (void);

void tf_content_error_literal (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message);

void tf_content_error (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message_format, ...) G_GNUC_PRINTF (4, 5);

GstIterator *tf_content_iterate_src_pads (TfContent *content,
    guint *handles, guint handle_count);

G_END_DECLS

#endif /* __TF_CONTENT_H__ */
