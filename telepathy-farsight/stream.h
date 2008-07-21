#ifndef __TF_STREAM_H__
#define __TF_STREAM_H__

#include <glib-object.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/media-interfaces.h>

#include <gst/farsight/fs-conference-iface.h>

G_BEGIN_DECLS

#define TF_TYPE_STREAM tf_stream_get_type()

#define TF_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_STREAM, TfStream))

#define TF_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_STREAM, TfStreamClass))

#define TP_STREAM_ENGINE_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TF_TYPE_STREAM))

#define TP_STREAM_ENGINE_IS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TF_TYPE_STREAM))

#define TF_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_STREAM, TfStreamClass))

typedef struct _TfStreamPrivate TfStreamPrivate;

/**
 * TfStream:
 * @parent: the parent #GObject
 * @stream_id: the ID of the stream (READ-ONLY)
 *
 * All other members are privated
 */

typedef struct _TfStream {
  GObject parent;

  /* Read-only */
  guint stream_id;

  /*< private >*/

  TfStreamPrivate *priv;
} TfStream;

/**
 * TfStreamClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfStreamClass {
  GObjectClass parent_class;

  /*< private >*/

  gpointer unused[4];
} TfStreamClass;


GType tf_stream_get_type (void);

void tf_stream_error (
  TfStream *self,
  guint error,
  const gchar *message);

G_END_DECLS

#endif /* __TF_STREAM_H__ */
