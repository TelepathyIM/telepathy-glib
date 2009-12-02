#include <telepathy-glib/contact.h>

#include <tpl_observer.h>
#include <tpl_channel_data.h>
#include <tpl_text_channel_data.h>

#define TP_CONTACT_FEATURES_LEN	2
#define	TP_CONTACT_CONTACTS_LEN	2
#define	TP_CONTACT_MYSELF	0
#define	TP_CONTACT_REMOTE	1

static TpContactFeature features[TP_CONTACT_FEATURES_LEN] = {
	TP_CONTACT_FEATURE_ALIAS, 
	TP_CONTACT_FEATURE_PRESENCE
};


/* Callbacks */

void _channel_on_sent_signal_cb (TpChannel *proxy,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object) {

	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TpContact *remote,*me;
	const gchar *my_id, *my_alias, *remote_id, *remote_alias;

	me = tpl_text_channel_get_my_contact(tpl_text);
	remote = tpl_text_channel_get_remote_contact(tpl_text);

	my_id = tp_contact_get_identifier(me);
	remote_id = tp_contact_get_identifier(remote);

	my_alias = tp_contact_get_alias(me);
	remote_alias = tp_contact_get_alias(remote);
	
	g_message("%s (%s): %s\n", my_id, my_alias, arg_Text);
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

	me = tpl_text_channel_get_my_contact(tpl_text);
	remote = tpl_text_channel_get_remote_contact(tpl_text);

	my_id = tp_contact_get_identifier(me);
	remote_id = tp_contact_get_identifier(remote);

	my_alias = tp_contact_get_alias(me);
	remote_alias = tp_contact_get_alias(remote);

	g_message("%s (%s): %s\n", remote_id, remote_alias, arg_Text);
}

/* connect signals to TplTextChannel instance */

void _tpl_text_channel_connect_signals(TplTextChannel* self) {
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
		g_error("receaived signal connect: %s\n", error->message);
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
	tpl_text_channel_set_remote_contact(tpl_text, contacts[TP_CONTACT_MYSELF]);
	
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


TplChannel *tpl_text_channel_get_tpl_channel(TplTextChannel *self) {
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
