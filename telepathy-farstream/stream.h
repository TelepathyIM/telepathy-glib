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

#define TF_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TF_TYPE_STREAM))

#define TF_IS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TF_TYPE_STREAM))

#define TF_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_STREAM, TfStreamClass))

/**
 * TfStream:
 *
 * All members are privated
 */

typedef struct _TfStream TfStream;

/**
 * TfStreamClass:
 *
 * This class is not subclassable
 */

typedef struct _TfStreamClass TfStreamClass;

GType tf_stream_get_type (void);

guint tf_stream_get_id (TfStream *stream);

void tf_stream_error (TfStream *self,
  TpMediaStreamError error,
  const gchar *message);


G_END_DECLS

#endif /* __TF_STREAM_H__ */
