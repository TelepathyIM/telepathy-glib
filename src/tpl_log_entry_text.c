#include <tpl_log_entry.h>
#include <tpl_utils.h>

G_DEFINE_TYPE (TplLogEntry, tpl_log_entry, G_TYPE_OBJECT)

static void tpl_log_entry_class_init(TplLogEntryClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}

static void tpl_log_entry_init(TplLogEntry* self) {
	/* Init TplTextChannel's members to zero/NULL */
#define TPL_SET_NULL(x) tpl_log_entry_set_##x(self, NULL)
#undef TPL_SET_NULL
}

TplLogEntry *tpl_log_entry_new(void) {
	TplLogEntry *ret = g_object_new(TPL_TYPE_LOG_ENTRY, NULL);
	return ret;
}


TplTextChannel *tpl_log_entry_text_get_tpl_channel(TplLogEntryText *self) {
	return self->tpl_channel;
}
TpContact *tpl_log_entry_text_get_sender (TplLogEntry *self) {
	return self->sender;
}
TpContact *tpl_log_entry_text_get_receiver (TplLogEntry *self) {
	return self->receiver;
}
const gchar *tpl_log_entry_text_get_message (TplLogEntry *self) {
	return self->message;
}
TpChannelTextMessageType tpl_log_entry_text_get_message_type (TplLogEntry *self) {
	return self->message_type;;
}
TplLogEntryTextSignalType tpl_log_entry_text_get_signal_type (TplLogEntry *self) {
	return self->signal_type;;
}
TplLogEntryTextDirection tpl_log_entry_text_get_direction (TplLogEntry *self) {
	return self->direction;
}


void tpl_log_entry_text_set_tpl_channel(TplLogEntryText *self, TplChannel *data) {
	_unref_object_if_not_null(self->tpl_channel);
	self->tpl_channel = data;
	_ref_object_if_not_null(data);
}

void tpl_log_entry_text_set_sender (TplLogEntry *self, TpContact *data) {
	self->sender = data;
}
void tpl_log_entry_text_set_receiver (TplLogEntry *self, TpContact *data) {
	self->receiver = data;
}
void tpl_log_entry_text_set_message (TplLogEntry *self, const gchar *data) {
	self->message = data;
}
void tpl_log_entry_text_set_message_type (TplLogEntry *self, TpChannelTextMessageType data) {
	self->message_type; = data;
}
void tpl_log_entry_text_set_signal_type (TplLogEntry *self, TplLogEntryTextSignalType data) {
	self->signal_type; = data;
}
void tpl_log_entry_text_set_direction (TplLogEntry *self, TplLogEntryTextDirection data) {
	self->direction = data;
}
