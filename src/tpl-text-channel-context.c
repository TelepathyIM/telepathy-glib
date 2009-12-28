/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

/*
 * This object acts as a Text Channel context, handling a automaton to
 * set up all the needed information before connect to Text iface
 * signals.
 */

#include <telepathy-glib/contact.h>
#include <telepathy-glib/enums.h>

#include <tpl-observer.h>
#include <tpl-text-channel-context.h>
#include <tpl-channel.h>
#include <tpl-log-entry.h>
#include <tpl-log-entry-text.h>
#include <tpl-log-manager.h>
#include <tpl-contact.h>

#define TP_CONTACT_FEATURES_LEN	2
#define	TP_CONTACT_MYSELF	0
#define	TP_CONTACT_REMOTE	1

typedef void (*TplPendingProc) (TplTextChannel *self);

static TpContactFeature features[TP_CONTACT_FEATURES_LEN] = {
	TP_CONTACT_FEATURE_ALIAS, 
	TP_CONTACT_FEATURE_PRESENCE
};

/* Signal's Callbacks */

static void
_channel_on_closed_cb (TpChannel *proxy,
		gpointer user_data,
		GObject *weak_object)
{
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel(tpl_text);
	gchar* chan_path;
	
	chan_path = g_strdup (tpl_channel_get_channel_path(tpl_chan));
	
	if (!tpl_channel_unregister_from_observer(tpl_chan))
		g_warning("Channel %s couldn't be unregistered correctly (BUG?)\n", chan_path);

	g_free(chan_path);
}

static void
_channel_on_lost_message_cb (TpChannel *proxy,
		gpointer user_data,
		GObject *weak_object)
{
	g_debug("lost message signal catched");
	// log that the system lost a message
}

static void
_channel_on_send_error_cb (TpChannel *proxy,
		guint arg_Error,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object)
{
	g_error("unable to send the message: %s", arg_Text);
	// log that the system was unable to send the message
}


static void
_channel_on_sent_signal_cb (TpChannel *proxy,
		guint arg_Timestamp,
		guint arg_Type,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object)
{
	GError *error=NULL;
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TpContact *remote,*me;
	TplContact *tpl_contact_sender;
	TplContact *tpl_contact_receiver;
	TplLogEntryText *tlog;
	TplLogEntry *log;
	TplLogManager *logmanager;
	gchar *chat_id;

	g_assert(TPL_IS_TEXT_CHANNEL(tpl_text));
	g_return_if_fail(TPL_IS_TEXT_CHANNEL(tpl_text));

	/* Initialize data for TplContact */
	me = tpl_text_channel_get_my_contact(tpl_text);
	remote = tpl_text_channel_get_remote_contact(tpl_text);

	if(!tpl_text_channel_is_chatroom(tpl_text) && remote==NULL) {
		g_error("Sending message: Remote TplContact NULL on 1-1 Chat\n");
	}

	tpl_contact_sender = tpl_contact_from_tp_contact(me);
	tpl_contact_set_contact_type(tpl_contact_sender,
		TPL_CONTACT_USER);
	tpl_contact_receiver = tpl_contact_from_tp_contact(remote);
	tpl_contact_set_contact_type(tpl_contact_receiver,
		TPL_CONTACT_USER);

	g_message("%s (%s): %s\n", 
		tpl_contact_get_identifier(tpl_contact_sender), 
		tpl_contact_get_alias(tpl_contact_sender),
		arg_Text);

	/* Initialize TplLogEntryText */
	log = tpl_log_entry_new ();
	tlog = tpl_log_entry_text_new ();

	tpl_log_entry_text_set_tpl_text_channel(tlog, tpl_text);
	tpl_log_entry_text_set_sender(tlog, tpl_contact_sender);
	tpl_log_entry_text_set_receiver(tlog, tpl_contact_receiver);
	tpl_log_entry_text_set_message(tlog, arg_Text);
	tpl_log_entry_text_set_message_type(tlog, arg_Type);
	tpl_log_entry_text_set_signal_type(tlog,
			TPL_LOG_ENTRY_TEXT_SIGNAL_SENT);
	tpl_log_entry_text_set_message_id(tlog, 123);

	tpl_log_entry_set_entry (log, tlog);
	tpl_log_entry_set_timestamp(log, (time_t) arg_Timestamp);

	/* Initialized LogStore and send the log entry */

	if (!tpl_text_channel_is_chatroom(tpl_text))
		chat_id = g_strdup (tpl_contact_get_identifier(
					tpl_contact_receiver));
	else
		chat_id = g_strdup (tpl_text_channel_get_chatroom_id(
			tpl_text));

	tpl_log_entry_text_set_chat_id(tlog, chat_id);

	logmanager = tpl_log_manager_dup_singleton();

	tpl_log_manager_add_message(logmanager, chat_id,
			tpl_text_channel_is_chatroom(tpl_text),
			log, &error);

	if(error!=NULL)
	{
		g_error("LogStore: %s", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}

	g_object_unref(tpl_contact_receiver);
	g_object_unref(tpl_contact_sender);
	g_object_unref(logmanager);
	g_object_unref(log);
	g_free(chat_id);
}

static void
_channel_on_received_signal_with_contact_cb(TpConnection *connection,
		guint n_contacts,
		TpContact * const *contacts,
		guint n_failed,
		const TpHandle *failed,
		const GError *error,
		gpointer user_data,
		GObject *weak_object)
{
	TplLogEntry *log = TPL_LOG_ENTRY(user_data);
	TplLogEntryText *tlog =
		TPL_LOG_ENTRY_TEXT(tpl_log_entry_get_entry(log));
	TplTextChannel *tpl_text = tpl_log_entry_text_get_tpl_text_channel(tlog);
	GError *e = NULL;
	TplLogManager *logmanager;
	TplContact *tpl_contact_sender;
	TpContact *remote;
	gchar *chat_id;

	g_return_if_fail(TPL_IS_LOG_ENTRY(log));
	g_return_if_fail(TPL_IS_LOG_ENTRY_TEXT(tlog));

	if(error!=NULL)
	{
		g_error("Unrecoverable error retrieving remote contact "
				"information: %s\n", error->message);
		g_error("Not able to log the received message: %s\n",
			tpl_log_entry_text_get_message(tlog));
		return;
	}

	if(n_failed>0) {
			g_error("%d invalid handle(s) passed to "
					"tp_connection_get_contacts_by_handle()\n",
					n_failed);
			g_error("Not able to log the received message: %s\n",
					tpl_log_entry_text_get_message(tlog));
			return;
	}
	
	remote = contacts[0];
	tpl_text_channel_set_remote_contact(tpl_text, remote);
	tpl_contact_sender = tpl_contact_from_tp_contact(remote);

	tpl_contact_set_contact_type(tpl_contact_sender,
			TPL_CONTACT_USER);
	tpl_log_entry_text_set_sender(tlog, tpl_contact_sender);

	g_message("%s (%s): %s\n", 
			tpl_contact_get_identifier(tpl_contact_sender), 
			tpl_contact_get_alias(tpl_contact_sender),
			tpl_log_entry_text_get_message(tlog));

	/* Initialize LogStore and store the message */
	
	if (!tpl_text_channel_is_chatroom(tpl_text))
		chat_id = g_strdup (tpl_contact_get_identifier(
					tpl_contact_sender));
	else
		chat_id = g_strdup (tpl_text_channel_get_chatroom_id(
			tpl_text));

	tpl_log_entry_text_set_chat_id(tlog, chat_id);

	logmanager = tpl_log_manager_dup_singleton();
	tpl_log_manager_add_message(logmanager,
			tpl_log_entry_text_get_chat_id(tlog),
			tpl_text_channel_is_chatroom (tpl_text),
			log, &e);
	if(e!=NULL)
	{
		g_error("LogStore: %s", e->message);
		g_clear_error(&e);
		g_error_free(e);
	}

	g_object_unref(tpl_contact_sender);
	g_object_unref(logmanager);
	g_free(chat_id);
}

static void
_channel_on_received_signal_cb (TpChannel *proxy,
		guint arg_ID,
		guint arg_Timestamp,
		guint arg_Sender,
		guint arg_Type,
		guint arg_Flags,
		const gchar *arg_Text,
		gpointer user_data,
		GObject *weak_object)
{
	TpHandle  remote_handle = (TpHandle) arg_Sender;
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);
	TplChannel *tpl_chan =
		tpl_text_channel_get_tpl_channel(tpl_text);
	TpContact *me;
	TplContact *tpl_contact_receiver;
	TplLogEntry *log;
	TplLogEntryText *tlog;

	// TODO use the Message iface to check the delivery
	// notification and handle it correctly
	if(arg_Flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT) {
		g_debug("Non text contenct flag set."
				"Probably a delivery notification for a sent message."
				"Ignoring\n");
		return;
	}

	/* Initialize TplLogEntryText (part 1) */
	log = tpl_log_entry_new();
	tlog = tpl_log_entry_text_new();
	tpl_log_entry_set_entry(log, tlog);

	tpl_log_entry_text_set_tpl_text_channel(tlog, tpl_text);
	tpl_log_entry_text_set_message(tlog, arg_Text);
	tpl_log_entry_text_set_message_type(tlog, arg_Type);
	tpl_log_entry_text_set_signal_type(tlog,
			TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED);
	tpl_log_entry_text_set_message_id(tlog, 123); //TODO set a real Id

	me = tpl_text_channel_get_my_contact(tpl_text);
	tpl_contact_receiver = tpl_contact_from_tp_contact(me);
	tpl_contact_set_contact_type(tpl_contact_receiver,
		TPL_CONTACT_USER);
	tpl_log_entry_text_set_receiver(tlog, tpl_contact_receiver);

	tpl_log_entry_set_timestamp(log, (time_t) arg_Timestamp);

	g_debug("remote handler: %d\n", arg_Sender);

	tp_connection_get_contacts_by_handle(
			tpl_channel_get_connection(tpl_chan),
			1, &remote_handle,
			TP_CONTACT_FEATURES_LEN, features,
			_channel_on_received_signal_with_contact_cb,
			log, g_object_unref, NULL);

	g_object_unref(tpl_contact_receiver);
}

/* End of Signal's Callbacks */


/* Context related operations  */

static void 
context_continue(TplTextChannel *ctx)
{
	if (g_queue_is_empty(ctx->chain)) {
		// TODO do some sanity checks
	} else {
		TplPendingProc next = g_queue_pop_head(ctx->chain);
		next(ctx);
	}
}

/* Context TplPendingProc and related CB */

/* Connect signals to TplTextChannel instance */
static void
_tpl_text_channel_pendingproc_connect_signals(TplTextChannel* self)
{
	GError *error=NULL;
	TpChannel *channel = NULL;

	channel =  tpl_channel_get_channel(
		tpl_text_channel_get_tpl_channel (self) );

	tp_cli_channel_type_text_connect_to_received(
		channel, _channel_on_received_signal_cb,
		self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("received signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
		error = NULL;
	}

	tp_cli_channel_type_text_connect_to_sent(
		channel, _channel_on_sent_signal_cb,
		self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("sent signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
		error = NULL;
	}

	tp_cli_channel_type_text_connect_to_send_error(
		channel, _channel_on_send_error_cb,
		self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("send error signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
		error = NULL;
	}

	tp_cli_channel_type_text_connect_to_lost_message(
			channel, _channel_on_lost_message_cb,
			self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("lost message signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
		error = NULL;
	}
	
	tp_cli_channel_connect_to_closed (
			channel, _channel_on_closed_cb,
			self, NULL, NULL, &error);
	if (error!=NULL) {
		g_error("channel closed signal connect: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
		error = NULL;
	}

	// TODO connect to TpContacts' notify::presence-type
	context_continue(self);
}

static void
_tpl_text_channel_get_chatroom_cb (TpConnection *proxy,
		const gchar **out_Identifiers,
		const GError *error,
		gpointer user_data, GObject *weak_object)
{
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);	
	
	if(error!=NULL) {
		g_error("retrieving chatroom identifier: %s\n",
			error->message);
	}

	g_debug("SETTING CHATROOM ID: %s\n", *out_Identifiers);
	tpl_text_channel_set_chatroom_id(tpl_text, *out_Identifiers);

	context_continue(tpl_text);
}

static void
_tpl_text_channel_pendingproc_get_chatroom_id(TplTextChannel *ctx)
{
	TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel(ctx);
	TpConnection *connection = tpl_channel_get_connection(tpl_chan);
	TpHandle room_handle;
	GArray *handles;

	handles = g_array_new(FALSE, FALSE, sizeof(TpHandle));
	room_handle = tp_channel_get_handle(
			tpl_channel_get_channel (tpl_chan),
			NULL);
	g_array_append_val(handles, room_handle);

	tpl_text_channel_set_chatroom(ctx, TRUE);
	tp_cli_connection_call_inspect_handles(connection,
			-1, TP_HANDLE_TYPE_ROOM, handles,
			_tpl_text_channel_get_chatroom_cb,
			ctx, NULL, NULL);

	g_array_unref(handles);
}


/* retrieve contacts (me and remote buddy/chatroom) and set TplTextChannel
 * members  */


// used by _get_my_contact and _get_remote_contact
static void
_tpl_text_channel_get_contact_cb(TpConnection *connection,
		guint n_contacts,
		TpContact * const *contacts,
		guint n_failed,
		const TpHandle *failed,
		const GError *error,
		gpointer user_data,
		GObject *weak_object)
{
	TplTextChannel *tpl_text = TPL_TEXT_CHANNEL(user_data);

	g_assert_cmpuint(n_failed, ==, 0);
	g_assert_cmpuint(n_contacts, ==, 1);
	g_assert_cmpuint(tpl_text->selector, <=, TP_CONTACT_REMOTE);

	if (n_failed > 0) {
		g_error("error resolving self handle for connection %s.\n"
			"Aborting channel %s observation\n",
			tpl_channel_get_connection_path(
				tpl_text_channel_get_tpl_channel(tpl_text)),
			tpl_channel_get_channel_path(
				tpl_text_channel_get_tpl_channel(tpl_text))
		);
		tpl_channel_unregister_from_observer(
			tpl_text_channel_get_tpl_channel(tpl_text));
		return;
	}

	switch(tpl_text->selector)
	{
	case TP_CONTACT_MYSELF:
		tpl_text_channel_set_my_contact(tpl_text, *contacts);
		break;
	case TP_CONTACT_REMOTE:
		tpl_text_channel_set_remote_contact(tpl_text,
			*contacts);
		break;
	default:
		g_error("retrieving TpContacts: passing invalid value "
				"for selector: %d\n"
				"Aborting channel %s observation\n",
				tpl_text->selector,
				tpl_channel_get_channel_path(
					tpl_text_channel_get_tpl_channel(tpl_text))

		       );
		tpl_channel_unregister_from_observer(
				tpl_text_channel_get_tpl_channel(tpl_text));
		return;
	}

	context_continue(tpl_text);
}


static void
_tpl_text_channel_pendingproc_get_remote_contact(TplTextChannel *ctx)
{
	TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel(ctx);
	TpHandleType remote_handle_type;
	TpHandle remote_handle;
		
	remote_handle = tp_channel_get_handle(
			tpl_channel_get_channel (tpl_chan),
			&remote_handle_type);

	ctx->selector = TP_CONTACT_REMOTE;
	tp_connection_get_contacts_by_handle(
			tpl_channel_get_connection(tpl_chan),
			1, &remote_handle,
			TP_CONTACT_FEATURES_LEN, features,
			_tpl_text_channel_get_contact_cb,
			ctx, NULL, NULL);
}

static void
_tpl_text_channel_pendingproc_get_my_contact(TplTextChannel *ctx)
{
	TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel(ctx);
	TpHandle my_handle = tp_connection_get_self_handle(
		tpl_channel_get_connection(tpl_chan));

	ctx->selector = TP_CONTACT_MYSELF;
	tp_connection_get_contacts_by_handle(
			tpl_channel_get_connection(tpl_chan),
			1, &my_handle,
			TP_CONTACT_FEATURES_LEN, features,
			_tpl_text_channel_get_contact_cb,
			ctx, NULL, NULL);
}
/* end of async Callbacks */


G_DEFINE_TYPE (TplTextChannel, tpl_text_channel, G_TYPE_OBJECT)


static void
tpl_text_channel_dispose(GObject *obj) {
	TplTextChannel *self = TPL_TEXT_CHANNEL(obj);

	g_debug("TplTextChannel: disposing\n");	

	tpl_object_unref_if_not_null(self->tpl_channel);
	self->tpl_channel = NULL;
	tpl_object_unref_if_not_null(self->my_contact);
	self->my_contact = NULL;
	tpl_object_unref_if_not_null(self->remote_contact);
	self->remote_contact = NULL;
	g_queue_free(self->chain);	
	self->chain = NULL;

	G_OBJECT_CLASS (tpl_text_channel_parent_class)->dispose (obj);

	g_debug("TplTextChannel: disposed\n");	
}

static void
tpl_text_channel_finalize(GObject *obj) {
	TplTextChannel *self = TPL_TEXT_CHANNEL(obj);

	g_debug("TplTextChannel: finalizing\n");	

	g_free ((gchar*) self->chatroom_id);
	G_OBJECT_CLASS (tpl_text_channel_parent_class)->finalize (obj);

	g_debug("TplTextChannel: finalized\n");	
}

static void
tpl_text_channel_class_init(TplTextChannelClass* klass) {
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = tpl_text_channel_dispose;
	object_class->finalize = tpl_text_channel_finalize;
}


static void
tpl_text_channel_init(TplTextChannel* self) {
	/* Init TplTextChannel's members to zero/NULL */
#undef TPL_SET_NULL
	tpl_text_channel_set_tpl_channel(self, NULL);
	tpl_text_channel_set_my_contact(self, NULL);
	tpl_text_channel_set_remote_contact(self, NULL);
	tpl_text_channel_set_chatroom_id(self, NULL);
	tpl_text_channel_set_chatroom(self, FALSE);
}

TplTextChannel *
tpl_text_channel_new(TplChannel* tpl_channel)
{
	TplTextChannel *ret = g_object_new(TPL_TYPE_TEXT_CHANNEL,NULL);
	tpl_text_channel_set_tpl_channel(ret, tpl_channel);

	// here some post instance-initialization, the object needs
	// to set some type's members and probably access (futurely) some
	// props
	TpHandleType remote_handle_type;
	tp_channel_get_handle(
			tpl_channel_get_channel(tpl_channel),
			&remote_handle_type);
	
	ret->chain = g_queue_new();
	g_queue_push_tail(ret->chain,
		_tpl_text_channel_pendingproc_get_my_contact);

	switch (remote_handle_type)
	{
    	case TP_HANDLE_TYPE_CONTACT:
		g_queue_push_tail(ret->chain, 
			_tpl_text_channel_pendingproc_get_remote_contact);
		break;
    	case TP_HANDLE_TYPE_ROOM:
		g_queue_push_tail(ret->chain, 
			_tpl_text_channel_pendingproc_get_chatroom_id);
		break;

	/* follows unhandled TpHandleType */
	case TP_HANDLE_TYPE_NONE:
		g_debug("remote handle: TP_HANDLE_TYPE_NONE: un-handled\n");
		break;
    	case TP_HANDLE_TYPE_LIST:
		g_debug("remote handle: TP_HANDLE_TYPE_LIST: un-handled\n");
		break;
    	case TP_HANDLE_TYPE_GROUP:
		g_debug("remote handle: TP_HANDLE_TYPE_GROUP: un-handled\n");
		break;
	default:
		g_error("remote handle unknown\n");
		break;
	}

	g_queue_push_tail(ret->chain,
		_tpl_text_channel_pendingproc_connect_signals);

	// start the chain consuming
	context_continue(ret);
	return ret;
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
gboolean tpl_text_channel_is_chatroom(TplTextChannel *self)
{
	return self->chatroom;
}
const gchar *tpl_text_channel_get_chatroom_id(TplTextChannel *self)
{
	return self->chatroom_id;
}

void tpl_text_channel_set_tpl_channel(TplTextChannel *self, TplChannel *data)
{
	tpl_object_unref_if_not_null(self->tpl_channel);
	self->tpl_channel = data;	
	tpl_object_ref_if_not_null(data);
}

void tpl_text_channel_set_remote_contact(TplTextChannel *self, TpContact *data)
{
	tpl_object_unref_if_not_null(self->remote_contact);
	self->remote_contact = data;
	tpl_object_ref_if_not_null(data);
}
void tpl_text_channel_set_my_contact(TplTextChannel *self, TpContact *data)
{
	tpl_object_unref_if_not_null(self->my_contact);
	self->my_contact = data;
	tpl_object_ref_if_not_null(data);
}
void tpl_text_channel_set_chatroom(TplTextChannel *self, gboolean data)
{
	self->chatroom = data;
}
void tpl_text_channel_set_chatroom_id(TplTextChannel *self, const gchar *data)
{
	g_free ((gchar*)self->chatroom_id);
	self->chatroom_id = g_strdup (data);
}
