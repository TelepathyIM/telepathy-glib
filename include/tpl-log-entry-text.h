#ifndef __TPL_LOG_ENTRY_TEXT_H__
#define __TPL_LOG_ENTRY_TEXT_H__

#include <telepathy-glib/enums.h>
#include <glib-object.h>
#include <tpl-contact.h>
#include <tpl-text-channel-context.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ENTRY_TEXT                  (tpl_log_entry_text_get_type ())
#define TPL_LOG_ENTRY_TEXT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryText))
#define TPL_LOG_ENTRY_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextClass))
#define TPL_IS_LOG_ENTRY_TEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY_TEXT))
#define TPL_IS_LOG_ENTRY_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY_TEXT))
#define TPL_LOG_ENTRY_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextClass))


/* Valid for org.freedesktop.Telepathy.Channel.Type.Text */

typedef enum { 
	TPL_LOG_ENTRY_TEXT_SIGNAL_SENT,
	TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED,
	TPL_LOG_ENTRY_TEXT_SIGNAL_SEND_ERROR,
	TPL_LOG_ENTRY_TEXT_SIGNAL_LOST_MESSAGE,
	TPL_LOG_ENTRY_TEXT_SIGNAL_CHAT_STATUS_CHANGED
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
	TplTextChannel *tpl_text; 
	// what kind of signal caused this log entry
	TplLogEntryTextSignalType signal_type; 
	TpChannelTextMessageType message_type;
	// is the this entry cause by something incoming or outgoing
	TplLogEntryTextDirection direction;

	// message and receiver may be NULL depending on the signal. ie.
	// status changed signals set only the sender
	TplContact *sender;
	TplContact *receiver;
	const gchar *message;
	guint	message_id;
        const gchar *chat_id;
	time_t timestamp;
	gboolean chatroom;
	
	// extra data passed, currently unuse
	gpointer data;
} TplLogEntryText;

typedef struct {
	GObjectClass	parent_class;
} TplLogEntryTextClass;

GType tpl_log_entry_text_get_type (void);

TplLogEntryText *tpl_log_entry_text_new (void);

TpChannelTextMessageType tpl_log_entry_text_message_type_from_str (const gchar *type_str);
const gchar *tpl_log_entry_text_message_type_to_str (TpChannelTextMessageType msg_type);


TplChannel *tpl_log_entry_text_get_tpl_channel (TplLogEntryText *self);
TplTextChannel *tpl_log_entry_text_get_tpl_text_channel (
		TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_sender (TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_receiver (TplLogEntryText *self);
const gchar *tpl_log_entry_text_get_message (TplLogEntryText *self);
TpChannelTextMessageType tpl_log_entry_text_get_message_type (TplLogEntryText *self);
TplLogEntryTextSignalType tpl_log_entry_text_get_signal_type (TplLogEntryText *self);
TplLogEntryTextDirection tpl_log_entry_text_get_direction (TplLogEntryText *self);
time_t tpl_log_entry_text_get_timestamp (TplLogEntryText *self);
guint tpl_log_entry_text_get_message_id (TplLogEntryText *self);
const gchar *tpl_log_entry_text_get_chat_id (TplLogEntryText *self);
gboolean tpl_log_entry_text_is_chatroom (TplLogEntryText *self);

void tpl_log_entry_text_set_tpl_text_channel (TplLogEntryText *self, TplTextChannel *data);
void tpl_log_entry_text_set_sender (TplLogEntryText *self, TplContact *data);
void tpl_log_entry_text_set_receiver (TplLogEntryText *self, TplContact *data);
void tpl_log_entry_text_set_message (TplLogEntryText *self, const gchar *data);
void tpl_log_entry_text_set_message_type (TplLogEntryText *self, TpChannelTextMessageType data);
void tpl_log_entry_text_set_signal_type (TplLogEntryText *self, TplLogEntryTextSignalType data);
void tpl_log_entry_text_set_direction (TplLogEntryText *self, TplLogEntryTextDirection data);
void tpl_log_entry_text_set_timestamp (TplLogEntryText *self, time_t data);
void tpl_log_entry_text_set_message_id (TplLogEntryText *self, guint data);
void tpl_log_entry_text_set_chat_id (TplLogEntryText *self, const gchar *data);
void tpl_log_entry_text_set_chatroom (TplLogEntryText *self, gboolean data);

G_END_DECLS

#endif // __TPL_LOG_ENTRY_TEXT_H__
