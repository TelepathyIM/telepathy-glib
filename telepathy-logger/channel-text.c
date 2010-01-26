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

#include "channel-text.h"

#include <telepathy-glib/contact.h>
#include <telepathy-glib/enums.h>

#include <telepathy-logger/contact.h>
#include <telepathy-logger/channel.h>
#include <telepathy-logger/observer.h>
#include <telepathy-logger/log-entry.h>
#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager-priv.h>

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
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);
  TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel (tpl_text);
  gchar *chan_path;

  chan_path = g_strdup (tpl_channel_get_channel_path (tpl_chan));

  if (!tpl_channel_unregister_from_observer (tpl_chan))
    g_warning ("Channel %s couldn't be unregistered correctly (BUG?)",
	       chan_path);

  g_free (chan_path);
}

static void
_channel_on_lost_message_cb (TpChannel *proxy,
			     gpointer user_data,
           GObject *weak_object)
{
  g_debug ("lost message signal catched. nothing logged");
  // TODO log that the system lost a message
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
  g_error ("unlogged event: "
	   "TP was unable to send the message: %s", arg_Text);
  // TODO log that the system was unable to send the message
}


static void
_channel_on_sent_signal_cb (TpChannel *proxy,
			    guint arg_Timestamp,
			    guint arg_Type,
			    const gchar *arg_Text,
			    gpointer user_data,
          GObject *weak_object)
{
  GError *error = NULL;
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);
  TpContact *remote, *me;
  TplContact *tpl_contact_sender;
  TplContact *tpl_contact_receiver;
  TplLogEntryText *log;
  TplLogManager *logmanager;
  gchar *chat_id;

  g_return_if_fail (TPL_IS_TEXT_CHANNEL (tpl_text));

  /* Initialize data for TplContact */
  me = tpl_text_channel_get_my_contact (tpl_text);
  remote = tpl_text_channel_get_remote_contact (tpl_text);

  if (!tpl_text_channel_is_chatroom (tpl_text) && remote == NULL)
    {
      g_error ("Sending message: Remote TplContact NULL on 1-1 Chat");
    }

  tpl_contact_sender = tpl_contact_from_tp_contact (me);
  tpl_contact_set_contact_type (tpl_contact_sender, TPL_CONTACT_USER);
  tpl_contact_receiver = tpl_contact_from_tp_contact (remote);
  tpl_contact_set_contact_type (tpl_contact_receiver, TPL_CONTACT_USER);

  g_message ("sent: %s (%s): %s",
	     tpl_contact_get_identifier (tpl_contact_sender),
	     tpl_contact_get_alias (tpl_contact_sender),
       arg_Text);

  /* Initialise TplLogEntryText */

  if (!tpl_text_channel_is_chatroom (tpl_text))
    chat_id = g_strdup (tpl_contact_get_identifier (tpl_contact_receiver));
  else
    chat_id = g_strdup (tpl_text_channel_get_chatroom_id (tpl_text));

  log = tpl_log_entry_text_new (arg_Timestamp, chat_id,
      TPL_LOG_ENTRY_DIRECTION_OUT);
  g_free (chat_id);

  tpl_log_entry_text_set_timestamp (log, (time_t) arg_Timestamp);
  tpl_log_entry_text_set_signal_type (log, TPL_LOG_ENTRY_TEXT_SIGNAL_SENT);
  tpl_log_entry_text_set_sender (log, tpl_contact_sender);
  tpl_log_entry_text_set_receiver (log, tpl_contact_receiver);
  tpl_log_entry_text_set_message (log, arg_Text);
  tpl_log_entry_text_set_message_type (log, arg_Type);
  tpl_log_entry_text_set_tpl_text_channel (log, tpl_text);

  /* Initialized LogStore and send the log entry */
  tpl_log_entry_text_set_chatroom (log,
		  tpl_text_channel_is_chatroom (tpl_text));

  logmanager = tpl_log_manager_dup_singleton ();
  tpl_log_manager_add_message (logmanager, TPL_LOG_ENTRY (log), &error);

  if (error != NULL)
    {
      g_error ("LogStore: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
    }

  g_object_unref (tpl_contact_receiver);
  g_object_unref (tpl_contact_sender);
  g_object_unref (logmanager);
  g_object_unref (log);
}

static void
_channel_on_received_signal_with_contact_cb (TpConnection *connection,
					     guint n_contacts,
					     TpContact *const *contacts,
					     guint n_failed,
					     const TpHandle *failed,
					     const GError *error,
					     gpointer user_data,
					     GObject *weak_object)
{
  TplLogEntryText *log = TPL_LOG_ENTRY_TEXT (user_data);
  TplTextChannel *tpl_text = tpl_log_entry_text_get_tpl_text_channel (log);
  GError *e = NULL;
  TplLogManager *logmanager;
  TplContact *tpl_contact_sender;
  TpContact *remote;
  gchar *chat_id;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (log));

  if (error != NULL)
    {
      g_error ("Unrecoverable error retrieving remote contact "
	       "information: %s", error->message);
      g_error ("Not able to log the received message: %s",
	       tpl_log_entry_text_get_message (log));
      return;
    }

  if (n_failed > 0)
    {
      g_error ("%d invalid handle(s) passed to "
	       "tp_connection_get_contacts_by_handle()", n_failed);
      g_error ("Not able to log the received message: %s",
	       tpl_log_entry_text_get_message (log));
      return;
    }

  remote = contacts[0];
  tpl_text_channel_set_remote_contact (tpl_text, remote);
  tpl_contact_sender = tpl_contact_from_tp_contact (remote);

  tpl_contact_set_contact_type (tpl_contact_sender, TPL_CONTACT_USER);
  tpl_log_entry_text_set_sender (log, tpl_contact_sender);

  g_message ("recvd: %s (%s): %s",
	     tpl_contact_get_identifier (tpl_contact_sender),
	     tpl_contact_get_alias (tpl_contact_sender),
	     tpl_log_entry_text_get_message (log));

  /* Initialise LogStore and store the message */

  if (!tpl_text_channel_is_chatroom (tpl_text))
    chat_id = g_strdup (tpl_contact_get_identifier (tpl_contact_sender));
  else
    chat_id = g_strdup (tpl_text_channel_get_chatroom_id (tpl_text));

  tpl_log_entry_text_set_chat_id (log, chat_id);
  tpl_log_entry_text_set_chatroom (log,
      tpl_text_channel_is_chatroom (tpl_text));

  logmanager = tpl_log_manager_dup_singleton ();
  tpl_log_manager_add_message (logmanager, TPL_LOG_ENTRY (log), &e);
  if (e != NULL)
    {
      g_error ("LogStore: %s", e->message);
      g_clear_error (&e);
      g_error_free (e);
    }

  g_object_unref (tpl_contact_sender);
  g_object_unref (logmanager);
  g_free (chat_id);
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
  TpHandle remote_handle = (TpHandle) arg_Sender;
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);
  TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel (tpl_text);
  TpContact *me;
  TplContact *tpl_contact_receiver;
  TplLogEntryText *log;

  // TODO use the Message iface to check the delivery
  // notification and handle it correctly
  if (arg_Flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT)
    {
      g_debug ("Non text content flag set."
	       "Probably a delivery notification for a sent message."
	       "Ignoring");
      return;
    }

  /* Initialize TplLogEntryText (part 1) */
  log = tpl_log_entry_text_new (arg_Timestamp, NULL,
      TPL_LOG_ENTRY_DIRECTION_IN);

  tpl_log_entry_text_set_tpl_text_channel (log, tpl_text);
  tpl_log_entry_text_set_message (log, arg_Text);
  tpl_log_entry_text_set_message_type (log, arg_Type);
  tpl_log_entry_text_set_signal_type (log, TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED);

  me = tpl_text_channel_get_my_contact (tpl_text);
  tpl_contact_receiver = tpl_contact_from_tp_contact (me);
  tpl_contact_set_contact_type (tpl_contact_receiver, TPL_CONTACT_USER);
  tpl_log_entry_text_set_receiver (log, tpl_contact_receiver);

  tpl_log_entry_text_set_timestamp (log, (time_t) arg_Timestamp);

  tp_connection_get_contacts_by_handle (tpl_channel_get_connection (tpl_chan),
					1, &remote_handle,
					TP_CONTACT_FEATURES_LEN, features,
					_channel_on_received_signal_with_contact_cb,
					log, g_object_unref, NULL);

  g_object_unref (tpl_contact_receiver);
}

/* End of Signal's Callbacks */


/* Context related operations  */

static void
context_continue (TplTextChannel *ctx)
{
  if (g_queue_is_empty (ctx->chain))
    {
      // TODO do some sanity checks
    }
  else
    {
      TplPendingProc next = g_queue_pop_head (ctx->chain);
      next (ctx);
    }
}

/* Context TplPendingProc and related CB */

/* Connect signals to TplTextChannel instance */
static void
_tpl_text_channel_pendingproc_connect_signals (TplTextChannel *self)
{
  GError *error = NULL;
  TpChannel *channel = NULL;

  channel = tpl_channel_get_channel (tpl_text_channel_get_tpl_channel (self));

  tp_cli_channel_type_text_connect_to_received (channel,
						_channel_on_received_signal_cb,
						self, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("received signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_sent (channel,
					    _channel_on_sent_signal_cb, self,
					    NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("sent signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_send_error (channel,
						  _channel_on_send_error_cb,
						  self, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("send error signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_lost_message (channel,
						    _channel_on_lost_message_cb,
						    self, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("lost message signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_connect_to_closed (channel, _channel_on_closed_cb,
				    self, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("channel closed signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  // TODO connect to TpContacts' notify::presence-type
  context_continue (self);
}

static void
_tpl_text_channel_get_chatroom_cb (TpConnection *proxy,
				   const gchar **out_Identifiers,
				   const GError *error,
				   gpointer user_data, GObject *weak_object)
{
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);

  if (error != NULL)
    {
      g_error ("retrieving chatroom identifier: %s", error->message);
    }

  tpl_text_channel_set_chatroom_id (tpl_text, *out_Identifiers);

  context_continue (tpl_text);
}

static void
_tpl_text_channel_pendingproc_get_chatroom_id (TplTextChannel *ctx)
{
  TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel (ctx);
  TpConnection *connection = tpl_channel_get_connection (tpl_chan);
  TpHandle room_handle;
  GArray *handles;

  handles = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  room_handle = tp_channel_get_handle (tpl_channel_get_channel (tpl_chan),
				       NULL);
  g_array_append_val (handles, room_handle);

  tpl_text_channel_set_chatroom (ctx, TRUE);
  tp_cli_connection_call_inspect_handles (connection,
					  -1, TP_HANDLE_TYPE_ROOM, handles,
					  _tpl_text_channel_get_chatroom_cb,
					  ctx, NULL, NULL);

  g_array_unref (handles);
}


/* retrieve contacts (me and remote buddy/chatroom) and set TplTextChannel
 * members  */


// used by _get_my_contact and _get_remote_contact
static void
_tpl_text_channel_get_contact_cb (TpConnection *connection,
				  guint n_contacts,
				  TpContact *const *contacts,
				  guint n_failed,
				  const TpHandle *failed,
				  const GError *error,
				  gpointer user_data,
          GObject *weak_object)
{
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);

  g_assert_cmpuint (n_failed, ==, 0);
  g_assert_cmpuint (n_contacts, ==, 1);
  g_assert_cmpuint (tpl_text->selector, <=, TP_CONTACT_REMOTE);

  if (n_failed > 0)
    {
      g_error ("Error resolving self handle for connection %s."
	       " Aborting channel %s observation",
	       tpl_channel_get_connection_path
	       (tpl_text_channel_get_tpl_channel (tpl_text)),
	       tpl_channel_get_channel_path (tpl_text_channel_get_tpl_channel
					     (tpl_text)));
      tpl_channel_unregister_from_observer (tpl_text_channel_get_tpl_channel
					    (tpl_text));
      return;
    }

  switch (tpl_text->selector)
    {
    case TP_CONTACT_MYSELF:
      tpl_text_channel_set_my_contact (tpl_text, *contacts);
      break;
    case TP_CONTACT_REMOTE:
      tpl_text_channel_set_remote_contact (tpl_text, *contacts);
      break;
    default:
      g_error ("retrieving TpContacts: passing invalid value "
	       "for selector: %d"
	       "Aborting channel %s observation",
	       tpl_text->selector,
	       tpl_channel_get_channel_path (tpl_text_channel_get_tpl_channel
					     (tpl_text)));
      tpl_channel_unregister_from_observer (tpl_text_channel_get_tpl_channel
					    (tpl_text));
      return;
    }

  context_continue (tpl_text);
}


static void
_tpl_text_channel_pendingproc_get_remote_contact (TplTextChannel *ctx)
{
  TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel (ctx);
  TpHandleType remote_handle_type;
  TpHandle remote_handle;

  remote_handle = tp_channel_get_handle (tpl_channel_get_channel (tpl_chan),
					 &remote_handle_type);

  ctx->selector = TP_CONTACT_REMOTE;
  tp_connection_get_contacts_by_handle (tpl_channel_get_connection (tpl_chan),
					1, &remote_handle,
					TP_CONTACT_FEATURES_LEN, features,
					_tpl_text_channel_get_contact_cb,
					ctx, NULL, NULL);
}

static void
_tpl_text_channel_pendingproc_get_my_contact (TplTextChannel *ctx)
{
  TplChannel *tpl_chan = tpl_text_channel_get_tpl_channel (ctx);
  TpHandle my_handle =
    tp_connection_get_self_handle (tpl_channel_get_connection (tpl_chan));

  ctx->selector = TP_CONTACT_MYSELF;
  tp_connection_get_contacts_by_handle (tpl_channel_get_connection (tpl_chan),
					1, &my_handle,
					TP_CONTACT_FEATURES_LEN, features,
					_tpl_text_channel_get_contact_cb,
					ctx, NULL, NULL);
}

/* end of async Callbacks */


G_DEFINE_TYPE (TplTextChannel, tpl_text_channel, G_TYPE_OBJECT)

static void tpl_text_channel_dispose (GObject *obj)
{
  TplTextChannel *self = TPL_TEXT_CHANNEL (obj);

  tpl_object_unref_if_not_null (self->tpl_channel);
  self->tpl_channel = NULL;
  tpl_object_unref_if_not_null (self->my_contact);
  self->my_contact = NULL;
  tpl_object_unref_if_not_null (self->remote_contact);
  self->remote_contact = NULL;
  g_queue_free (self->chain);
  self->chain = NULL;

  G_OBJECT_CLASS (tpl_text_channel_parent_class)->dispose (obj);
}

static void
tpl_text_channel_finalize (GObject *obj)
{
  TplTextChannel *self = TPL_TEXT_CHANNEL (obj);

  g_free (self->chatroom_id);
  G_OBJECT_CLASS (tpl_text_channel_parent_class)->finalize (obj);
}

static void
tpl_text_channel_class_init (TplTextChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_text_channel_dispose;
  object_class->finalize = tpl_text_channel_finalize;
}


static void
tpl_text_channel_init (TplTextChannel *self)
{
}

TplTextChannel *
tpl_text_channel_new (TplChannel *tpl_channel)
{
  TplTextChannel *ret = g_object_new (TPL_TYPE_TEXT_CHANNEL, NULL);
  tpl_text_channel_set_tpl_channel (ret, tpl_channel);

  /* here some post instance-initialization, the object needs
     to set some type's members and probably access (futurely) some
     props */
  TpHandleType remote_handle_type;
  tp_channel_get_handle (tpl_channel_get_channel (tpl_channel),
			 &remote_handle_type);

  ret->chain = g_queue_new ();

  g_queue_push_tail (ret->chain,
		     _tpl_text_channel_pendingproc_connect_signals);

  g_queue_push_tail (ret->chain,
		     _tpl_text_channel_pendingproc_get_my_contact);

  switch (remote_handle_type)
    {
    case TP_HANDLE_TYPE_CONTACT:
      g_queue_push_tail (ret->chain,
			 _tpl_text_channel_pendingproc_get_remote_contact);
      break;
    case TP_HANDLE_TYPE_ROOM:
      g_queue_push_tail (ret->chain,
			 _tpl_text_channel_pendingproc_get_chatroom_id);
      break;

      /* follows unhandled TpHandleType */
    case TP_HANDLE_TYPE_NONE:
      g_warning ("remote handle: TP_HANDLE_TYPE_NONE: "
		 "un-handled. It's probably OK.");
      break;
    case TP_HANDLE_TYPE_LIST:
      g_warning ("remote handle: TP_HANDLE_TYPE_LIST: "
		 "un-handled. It's probably OK.");
      break;
    case TP_HANDLE_TYPE_GROUP:
      g_warning ("remote handle: TP_HANDLE_TYPE_GROUP: "
		 "un-handled. It's probably OK.");
      break;
    default:
      g_error ("remote handle type unknown %d.", remote_handle_type);
      break;
    }

  // start the queue consuming
  context_continue (ret);
  return ret;
}


TplChannel *
tpl_text_channel_get_tpl_channel (TplTextChannel *self)
{
  return self->tpl_channel;
}

TpContact *
tpl_text_channel_get_remote_contact (TplTextChannel *self)
{
  return self->remote_contact;
}

TpContact *
tpl_text_channel_get_my_contact (TplTextChannel *self)
{
  return self->my_contact;
}

gboolean
tpl_text_channel_is_chatroom (TplTextChannel *self)
{
  g_return_val_if_fail(TPL_IS_TEXT_CHANNEL (self), FALSE);
  return self->chatroom;
}

const gchar *
tpl_text_channel_get_chatroom_id (TplTextChannel *self)
{
  g_return_val_if_fail(TPL_IS_TEXT_CHANNEL (self), NULL);
  return self->chatroom_id;
}

void
tpl_text_channel_set_tpl_channel (TplTextChannel *self, TplChannel *data)
{
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TPL_IS_CHANNEL (data));

  tpl_object_unref_if_not_null (self->tpl_channel);
  self->tpl_channel = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_text_channel_set_remote_contact (TplTextChannel *self, TpContact *data)
{
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_CONTACT (data));

  tpl_object_unref_if_not_null (self->remote_contact);
  self->remote_contact = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_text_channel_set_my_contact (TplTextChannel *self, TpContact *data)
{
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_CONTACT (data));

  tpl_object_unref_if_not_null (self->my_contact);
  self->my_contact = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_text_channel_set_chatroom (TplTextChannel *self, gboolean data)
{
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (self));

  self->chatroom = data;
}

void
tpl_text_channel_set_chatroom_id (TplTextChannel *self, const gchar *data)
{
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (self));

  g_free (self->chatroom_id);
  self->chatroom_id = g_strdup (data);
}
