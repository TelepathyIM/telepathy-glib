#ifndef __TPL_LOG_ENTRY_H__
#define __TPL_LOG_ENTRY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ENTRY                  (tpl_log_entry_text_get_signal_type ())
#define TPL_LOG_ENTRY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntry))
#define TPL_LOG_ENTRY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))
#define TPL_IS_LOG_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY))
#define TPL_IS_LOG_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY))
#define TPL_LOG_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))


/* Valid for org.freedesktop.Telepathy.Channel.Type.Text */

typedef enum { 
	TPL_LOG_ENTRY_TEXT_CHANNEL_MESSAGE,
	TPL_LOG_ENTRY_TEXT_CHANNEL_ERROR,
	TPL_LOG_ENTRY_TEXT_CHANNEL_LOSTMESSAGE
} TplLogEntryTextSignalType;

/* wether the log entry is referring to something outgoing on incoming */
typedef enum {
	TPL_LOG_ENTRY_TEXT_CHANNEL_IN,
	TPL_LOG_ENTRY_TEXT_CHANNEL_OUT
} TplLogEntryTextDirection;

typedef struct {
	GObject parent;

	/* Private */
	// tpl_channel has informations about channel/account/connection
	TplChannel *tpl_channel; 
	// what kind of signal caused this log entry
	TplLogEntryTextSignalType signal_type; 
	TpChannelTextMessageType message_type;
	// is the this entry cause by something incoming or outgoing
	TplLogEntryTextDirection direction;

	TplContact *sender;
	TplContact *receiver;	
	const gchar *message;
} TplLogEntryText;

typedef struct {
	GObjectClass	parent_class;
} TplLogEntryClass;

GType tpl_log_entry_text_get_signal_type (void);

TplLogEntryText *tpl_log_entry_text_new ();

TplTextChannel *tpl_log_entry_text_get_tpl_channel(TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_sender (TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_receiver (TplLogEntryText *self);
const gchar *tpl_log_entry_text_get_message (TplLogEntryText *self);
TpChannelTextMessageType tpl_log_entry_text_get_message_type (TplLogEntry *self);
TplLogEntryTextSignalType tpl_log_entry_text_get_signal_type (TplLogEntryTextSignalType *self);
TplLogEntryTextDirection tpl_log_entry_text_get_direction (TplLogEntryTextDirection *self);

void tpl_log_entry_text_set_tpl_channel(TplLogEntryText *self, TplChannel *data);
void tpl_log_entry_text_set_sender (TplLogEntryText *self, TplContact *data);
void tpl_log_entry_text_set_receiver (TplLogEntryText *self, TplContact *data);
void tpl_log_entry_text_set_message (TplLogEntryText *self, const gchar *data);
void tpl_log_entry_text_set_message_type (TplLogEntry *self, TpChannelTextMessageType data);
void tpl_log_entry_text_set_signal_type (TplLogEntryText *self, TplLogEntryTextSignalType data);
void tpl_log_entry_text_set_direction (TplLogEntryText *self, TplLogEntryTextDirection data);

G_END_DECLS

#endif // __TPL_LOG_ENTRY_H__
