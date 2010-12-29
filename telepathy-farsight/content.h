#ifndef __TF_CONTENT_H__
#define __TF_CONTENT_H__

#include <glib-object.h>

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
 * All members of the object are private
 */

typedef struct _TfContent TfContent;

/**
 * TfContentClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfContentClass TfContentClass;

GType tf_content_get_type (void);

void tf_content_error (TfContent *chan,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message);

G_END_DECLS

#endif /* __TF_CONTENT_H__ */
