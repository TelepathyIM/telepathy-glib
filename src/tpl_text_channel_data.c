#include <telepathy-glib/contact.h>

#include <tpl-log-store.h>
#include <tpl-log-store-empathy.h>
#include <tpl_observer.h>
#include <tpl_channel_data.h>
#include <tpl_text_channel_data.h>
#include <tpl_log_entry_text.h>
#include <tpl_contact.h>

#define TP_CONTACT_FEATURES_LEN	2
#define	TP_CONTACT_CONTACTS_LEN	2
#define	TP_CONTACT_MYSELF	0
#define	TP_CONTACT_REMOTE	1

static TpContactFeature features[TP_CONTACT_FEATURES_LEN] = {
	TP_CONTACT_FEATURE_ALIAS, 
	TP_CONTACT_FEATURE_PRESENCE
};


/* definitions */

void _channel_on_sent_signal_cb (TpChannel *proxy,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object);
void _channel_on_sent_signal_cb (TpChannel *proxy,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object);
void _channel_on_received_signal_cb (TpChannel *proxy,
		guint arg_ID,
		guint arg_Timestamp,
		guint arg_Sender,
		guint arg_Type,
		guint arg_Flags,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object);
void _tpl_text_channel_connect_signals(TplTextChannel* self);
void _tpl_text_channel_set_ready_cb(TpConnection *connection,
		guint n_contacts,
		TpContact * const *contacts,
		guint n_failed,
		const TpHandle *failed,
		const GError *error,
		gpointer user_data,
		GObject *weak_object);
/* end of definitions */




/* Callbacks */

void _channel_on_sent_signal_cb (TpChannel *proxy,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object)
{
	GError *error=NULL;
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TpContact *remote,*me;
	const gchar *my_id, *my_alias, *remote_id, *remote_alias;
	const gchar *my_pres_msg, *my_pres_status;
	const gchar *remote_pres_msg, *remote_pres_status;
	TplContact *tpl_contact_sender;
	TplContact *tpl_contact_receiver;
	TplLogEntryText *log;
	TplLogStoreEmpathy *logstore;

	/* Initialize data for TplContact */
	me = tpl_text_channel_get_my_contact(tpl_text);
	remote = tpl_text_channel_get_remote_contact(tpl_text);

	my_id = tp_contact_get_identifier(me);
	remote_id = tp_contact_get_identifier(remote);

	my_alias = tp_contact_get_alias(me);
	remote_alias = tp_contact_get_alias(remote);

	my_pres_status = tp_contact_get_presence_status(me);
	remote_pres_status = tp_contact_get_presence_status(remote);

	my_pres_msg = tp_contact_get_presence_message (me);
	remote_pres_msg = tp_contact_get_presence_message (remote);
	
	tpl_contact_sender = tpl_contact_new();
	tpl_contact_receiver = tpl_contact_new();
#define CONTACT_ENTRY_SET(x,y) tpl_contact_set_##x(tpl_contact_sender,y)
	CONTACT_ENTRY_SET(contact, me);
	CONTACT_ENTRY_SET(alias, my_alias);
	CONTACT_ENTRY_SET(identifier, my_id);
	CONTACT_ENTRY_SET(presence_status, my_pres_status);
	CONTACT_ENTRY_SET(presence_message, my_pres_msg );
	CONTACT_ENTRY_SET(contact_type, TPL_CONTACT_USER); 
#undef CONTACT_ENTRY_SET
#define CONTACT_ENTRY_SET(x,y) tpl_contact_set_##x(tpl_contact_receiver,y)
	CONTACT_ENTRY_SET(contact, remote);
	CONTACT_ENTRY_SET(alias, remote_alias);
	CONTACT_ENTRY_SET(identifier, remote_id);
	CONTACT_ENTRY_SET(presence_status, remote_pres_status );
	CONTACT_ENTRY_SET(presence_message, remote_pres_msg);
	CONTACT_ENTRY_SET(contact_type, TPL_CONTACT_USER); 
#undef CONTACT_ENTRY_SET

	g_message("%s (%s): %s\n", 
		tpl_contact_get_identifier(tpl_contact_sender), 
		tpl_contact_get_alias(tpl_contact_sender),
		arg_Text);


	/* Initialize TplLogEntryText */

	log = tpl_log_entry_text_new();
	tpl_log_entry_text_set_tpl_channel(log, 
		tpl_text_channel_get_tpl_channel(tpl_text));
	tpl_log_entry_text_set_sender(log, tpl_contact_sender);
	tpl_log_entry_text_set_receiver(log, tpl_contact_receiver);
	tpl_log_entry_text_set_message(log, arg_Text);
	tpl_log_entry_text_set_message_type(log, arg_Type);
	tpl_log_entry_text_set_signal_type(log,
			TPL_LOG_ENTRY_TEXT_CHANNEL_MESSAGE);
	tpl_log_entry_text_set_timestamp(log, (time_t) arg_Timestamp);
	tpl_log_entry_text_set_id(log, 123);

	/* Initialized LogStore and send the message */

	logstore = g_object_new(TPL_TYPE_LOG_STORE_EMPATHY, NULL);
	if (!TPL_LOG_STORE_GET_INTERFACE(logstore)->add_message) {
		g_warning("LOGSTORE IFACE: add message not implemented\n");
		return;
	}

	tpl_log_store_add_message( TPL_LOG_STORE(logstore),
			tpl_contact_get_identifier(tpl_contact_sender),
			FALSE,
			log,
			&error);
	if(error!=NULL) {
		g_error("LOGSTORE: %s", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}

}

void _channel_on_received_signal_cb (TpChannel *proxy,
		guint arg_ID,
		guint arg_Timestamp,
		guint arg_Sender,
		guint arg_Type,
		guint arg_Flags,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object)
{
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TpContact *remote,*me;
	const gchar *my_id, *my_alias, *remote_id, *remote_alias;
	const gchar *my_pres_msg, *my_pres_status;
	const gchar *remote_pres_msg, *remote_pres_status;
	TplContact *tpl_contact_sender;
	TplContact *tpl_contact_receiver;
	TplLogEntryText *log;

	me = tpl_text_channel_get_my_contact(tpl_text);
	remote = tpl_text_channel_get_remote_contact(tpl_text);

	my_id = tp_contact_get_identifier(me);
	remote_id = tp_contact_get_identifier(remote);

	my_alias = tp_contact_get_alias(me);
	remote_alias = tp_contact_get_alias(remote);

	my_pres_status = tp_contact_get_presence_status(me);
	remote_pres_status = tp_contact_get_presence_status(remote);

	my_pres_msg = tp_contact_get_presence_message (me);
	remote_pres_msg = tp_contact_get_presence_message (remote);
	
	tpl_contact_sender = tpl_contact_new();
	tpl_contact_receiver = tpl_contact_new();
#define CONTACT_ENTRY_SET(x,y) tpl_contact_set_##x(tpl_contact_sender,y)
	CONTACT_ENTRY_SET(contact, remote);
	CONTACT_ENTRY_SET(alias, remote_alias);
	CONTACT_ENTRY_SET(identifier, remote_id);
	CONTACT_ENTRY_SET(presence_status, remote_pres_status);
	CONTACT_ENTRY_SET(presence_message, remote_pres_msg );
#undef CONTACT_ENTRY_SET
#define CONTACT_ENTRY_SET(x,y) tpl_contact_set_##x(tpl_contact_receiver,y)
	CONTACT_ENTRY_SET(contact, me);
	CONTACT_ENTRY_SET(alias, my_alias);
	CONTACT_ENTRY_SET(identifier, my_id);
	CONTACT_ENTRY_SET(presence_status, my_pres_status );
	CONTACT_ENTRY_SET(presence_message, my_pres_msg);
#undef CONTACT_ENTRY_SET

	g_message("%s (%s): %s\n", 
		tpl_contact_get_identifier(tpl_contact_sender), 
		tpl_contact_get_alias(tpl_contact_sender),
		arg_Text);


	log = tpl_log_entry_text_new();
	tpl_log_entry_text_set_tpl_channel(log,
		tpl_text_channel_get_tpl_channel(tpl_text));
	tpl_log_entry_text_set_sender(log, tpl_contact_sender);
	tpl_log_entry_text_set_receiver(log, tpl_contact_receiver);
	tpl_log_entry_text_set_message(log, arg_Text);
	tpl_log_entry_text_set_message_type(log, arg_Type);
	tpl_log_entry_text_set_signal_type(log, TPL_LOG_ENTRY_TEXT_CHANNEL_MESSAGE);
}

/* connect signals to TplTextChannel instance */

void _tpl_text_channel_connect_signals(TplTextChannel* self)
{
	GError *error=NULL;
//	 Signals for Text channels
//	   "lost-message"                                   : Run Last / Has Details
//	   "received"                                       : Run Last / Has Details
//	   "send-error"                                     : Run Last / Has Details
//	   "sent"                                           : Run Last / Has Details
//	   "chat-state-changed"                             : Run Last / Has Details
//	   "password-flags-changed"                         : Run Last / Has Details
	//TODO handle data destruction
	tp_cli_channel_type_text_connect_to_received(self->tpl_channel->channel,
		_channel_on_received_signal_cb, self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("received signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}

	//TODO handle data destruction
	tp_cli_channel_type_text_connect_to_sent(self->tpl_channel->channel,
		_channel_on_sent_signal_cb, self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("sent signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}
}


/* retrieve contacts (me and remove buddy) and set TplTextChannel
 * members  */

void _tpl_text_channel_set_ready_cb(TpConnection *connection,
		guint n_contacts,
		TpContact * const *contacts,
		guint n_failed,
		const TpHandle *failed,
		const GError *error,
		gpointer user_data,
		GObject *weak_object)
{
	TplTextChannel *tpl_text = (TplTextChannel*) user_data;
	
	tpl_text_channel_set_my_contact(tpl_text, contacts[TP_CONTACT_MYSELF]);
	tpl_text_channel_set_remote_contact(tpl_text, contacts[TP_CONTACT_REMOTE]);
	
	//g_debug("MY ALIAS: %s\n", tp_contact_get_alias(
	//			tpl_text_channel_get_my_contact(tpl_text)));
	//g_debug("REMOTE ID: %s\n", tp_contact_get_identifier(
	//			tpl_text_channel_get_remote_contact(tpl_text)));


	_tpl_text_channel_connect_signals(tpl_text);
}





/* end of async Callbacks */



G_DEFINE_TYPE (TplTextChannel, tpl_text_channel, G_TYPE_OBJECT)


static void tpl_text_channel_class_init(TplTextChannelClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}


static void tpl_text_channel_init(TplTextChannel* self) {
	/* Init TplTextChannel's members to zero/NULL */
#define TPL_SET_NULL(x) tpl_text_channel_set_##x(self, NULL)
	TPL_SET_NULL(tpl_channel);
	TPL_SET_NULL(my_contact);
	TPL_SET_NULL(remote_contact);
#undef TPL_SET_NULL
}

TplTextChannel* tpl_text_channel_new(TplChannel* tpl_channel)
{
	TplTextChannel *ret = g_object_new(TPL_TYPE_TEXT_CHANNEL,NULL);
	ret->tpl_channel = tpl_channel;
	tpl_text_channel_set_tpl_channel(ret, tpl_channel);

	// here some post instance-initialization, the object needs
	// to set some type's members and probably access (futurely) some
	// props
	TpHandle contacts[TP_CONTACT_CONTACTS_LEN] = {0,0};
	TpHandleType remote_handle_type;

	contacts[TP_CONTACT_REMOTE] = tp_channel_get_handle(
			ret->tpl_channel->channel, &remote_handle_type);
	contacts[TP_CONTACT_MYSELF] = tp_connection_get_self_handle(
			ret->tpl_channel->connection);
	tp_connection_get_contacts_by_handle(
			ret->tpl_channel->connection,
			TP_CONTACT_CONTACTS_LEN, contacts,
			TP_CONTACT_FEATURES_LEN, features,
			_tpl_text_channel_set_ready_cb,
			ret, NULL, NULL);
	return ret;
}

void tpl_text_channel_free(TplTextChannel* tpl_text) {
	/* TODO free and unref other members */
	g_free(tpl_text);
}


TplChannel *tpl_text_channel_get_tpl_channel(TplTextChannel *self) 
{
	return self->tpl_channel;
}

TpContact *tpl_text_channel_get_remote_contact(TplTextChannel *self)
{
	return self->remote_contact;
}
TpContact *tpl_text_channel_get_my_contact(TplTextChannel *self)
{
	return self->my_contact;
}

void tpl_text_channel_set_tpl_channel(TplTextChannel *self, TplChannel *data) {
	//g_debug("SET TPL CHANNEL\n");
	_unref_object_if_not_null(&(self->tpl_channel));
	self->tpl_channel = data;	
	_ref_object_if_not_null(data);
}

void tpl_text_channel_set_remote_contact(TplTextChannel *self, TpContact *data)
{
	//g_debug("SET remote contact\n");
	_unref_object_if_not_null(&(self->remote_contact));
	self->remote_contact = data;
	_ref_object_if_not_null(data);
}
void tpl_text_channel_set_my_contact(TplTextChannel *self, TpContact *data)
{
	//g_debug("SET my contact\n");
	_unref_object_if_not_null(&(self->my_contact));
	self->my_contact = data;
	_ref_object_if_not_null(data);
}
