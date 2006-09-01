
#include <glib-object.h>

G_BEGIN_DECLS

#define TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER tp_stream_engine_x_error_handler_get_type()

#define TP_STREAM_ENGINE_X_ERROR_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandler))

#define TP_STREAM_ENGINE_X_ERROR_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandlerClass))

#define TP_STREAM_ENGINE_IS_X_ERROR_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER))

#define TP_STREAM_ENGINE_IS_X_ERROR_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER))

#define TP_STREAM_ENGINE_X_ERROR_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TP_STREAM_ENGINE_TYPE_X_ERROR_HANDLER, TpStreamEngineXErrorHandlerClass))

typedef struct {
  GObject parent;
} TpStreamEngineXErrorHandler;

typedef struct {
  GObjectClass parent_class;
} TpStreamEngineXErrorHandlerClass;

GType tp_stream_engine_x_error_handler_get_type (void);

TpStreamEngineXErrorHandler* tp_stream_engine_x_error_handler_get (void);
void tp_stream_engine_x_error_handler_cleanup (void);

G_END_DECLS

