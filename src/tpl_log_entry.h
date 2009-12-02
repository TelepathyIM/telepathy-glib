#ifndef __TPL_LOG_ENTRY_H__
#define __TPL_LOG_ENTRY_H__

#include <glib-object.h>
#include <telepathy-glib/log_entry.h>

#include <tpl_channel_data.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ENTRY                  (tpl_log_entry_get_type ())
#define TPL_LOG_ENTRY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntry))
#define TPL_LOG_ENTRY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))
#define TPL_IS_LOG_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY))
#define TPL_IS_LOG_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY))
#define TPL_LOG_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))

typedef enum { 
	TPL_LOG_ENTRY_TEXT_CHANNEL_MESSAGE,
	TPL_LOG_ENTRY_TEXT_CHANNEL_ERROR,
	TPL_LOG_ENTRY_LAST
} TplLogEntryType;

typedef struct {
	GObject parent;

	/* Private */
	TplLogEntryType entry_type; 
	const gchar *from, *to;
	const gchar *message;
} TplLogEntry;

const gchar *tpl_log_entry_get_from(TplLogEntry *self);
const gchar *tpl_log_entry_get_to(TplLogEntry *self);
const gchar *tpl_log_entry_get_message(TplLogEntry *self);


typedef struct {
	GObjectClass	parent_class;
} TplLogEntryClass;


GType  tpl_log_entry_get_type (void);

TplLogEntry *tpl_log_entry_new(void);

G_END_DECLS

#endif // __TPL_LOG_ENTRY_H__
