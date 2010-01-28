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
#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager-priv.h>
#include <telepathy-logger/util.h>

#define TP_CONTACT_FEATURES_LEN	2
#define	TP_CONTACT_MYSELF	0
#define	TP_CONTACT_REMOTE	1

#define GET_PRIV(obj)    TPL_GET_PRIV (obj, TplChannelText)
struct _TplChannelTextPriv
{
  gboolean chatroom;
  TpContact *my_contact;
  TpContact *remote_contact;  /* only set if chatroom==FALSE */
  gchar *chatroom_id;	      	/* only set if chatroom==TRUE */

  /* only used as metadata in CB data passing */
  guint selector;
};

static TpContactFeature features[TP_CONTACT_FEATURES_LEN] = {
  TP_CONTACT_FEATURE_ALIAS,
  TP_CONTACT_FEATURE_PRESENCE
};

static void got_tpl_chan_ready_cb (GObject *obj, GAsyncResult *result,
    gpointer user_data);
static void _channel_on_closed_cb (TpChannel *proxy, gpointer user_data,
    GObject *weak_object);
static void _channel_on_lost_message_cb (TpChannel *proxy, gpointer user_data,
    GObject *weak_object);
static void _channel_on_received_signal_cb (TpChannel *proxy, guint arg_ID,
    guint arg_Timestamp, guint arg_Sender, guint arg_Type, guint arg_Flags,
    const gchar *arg_Text, gpointer user_data, GObject *weak_object);
static void _channel_on_sent_signal_cb (TpChannel *proxy, guint arg_Timestamp,
    guint arg_Type, const gchar *arg_Text,  gpointer user_data,
    GObject *weak_object);
static void _channel_on_send_error_cb (TpChannel *proxy, guint arg_Error,
    guint arg_Timestamp, guint arg_Type, const gchar *arg_Text,
    gpointer user_data, GObject *weak_object);
static void pendingproc_prepare_tpl_channel (TplActionChain *ctx);
static void pendingproc_connect_signals (TplActionChain *ctx);
static void pendingproc_get_chatroom_id (TplActionChain *ctx);
static void _tpl_channel_text_get_chatroom_cb (TpConnection *proxy,
    const gchar **out_Identifiers, const GError *error, gpointer user_data,
    GObject *weak_object);


/* retrieve contacts (me and remote buddy/chatroom) and set TplChannelText
 * members  */


// used by _get_my_contact and _get_remote_contact
static void
_tpl_channel_text_get_contact_cb (TpConnection *connection,
    guint n_contacts,
    TpContact *const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplObserver *observer = tpl_observer_new (); /* singleton */
  TplActionChain *ctx = user_data;
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TplChannelTextPriv *priv = GET_PRIV (tpl_text);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TpChannel *tp_chan = TP_CHANNEL (tpl_chan);
  gchar *conn_path, *chan_path;

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (tpl_text));

  g_assert_cmpuint (n_failed, ==, 0);
  g_assert_cmpuint (n_contacts, ==, 1);
  g_assert_cmpuint (priv->selector, <=, TP_CONTACT_REMOTE);

  if (n_failed > 0)
    {
      g_object_get (G_OBJECT (tp_channel_borrow_connection
          (tp_chan)), "object-path", &conn_path, NULL);
      g_object_get (G_OBJECT (tp_chan), "object-path", &chan_path, NULL);

      g_debug ("Error resolving self handle for connection %s."
	       " Aborting channel %s observation", conn_path, chan_path);
      tpl_observer_unregister_channel (observer, TPL_CHANNEL (tpl_text));

      g_free (conn_path);
      g_free (chan_path);
      g_object_unref (observer);
      return;
    }

  switch (priv->selector)
    {
    case TP_CONTACT_MYSELF:
      tpl_channel_text_set_my_contact (tpl_text, *contacts);
      break;
    case TP_CONTACT_REMOTE:
      tpl_channel_text_set_remote_contact (tpl_text, *contacts);
      break;
    default:
      g_object_get (G_OBJECT (tp_chan), "object-path", &chan_path, NULL);
      g_error ("retrieving TpContacts: passing invalid value for selector: %d"
         "Aborting channel %s observation", priv->selector, chan_path);
      g_free (chan_path);
      tpl_observer_unregister_channel (observer, TPL_CHANNEL (tpl_text));
      g_object_unref (observer);
      return;
    }

  g_object_unref (observer);
  tpl_actionchain_continue (ctx);
}


static void
pendingproc_get_remote_contact (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TpHandleType remote_handle_type;
  TpHandle remote_handle;
  TpConnection *tp_conn = tp_channel_borrow_connection (TP_CHANNEL (
        tpl_chan));

  remote_handle = tp_channel_get_handle (TP_CHANNEL (tpl_chan),
      &remote_handle_type);

  GET_PRIV (tpl_text)->selector = TP_CONTACT_REMOTE;
  tp_connection_get_contacts_by_handle (tp_conn, 1, &remote_handle,
      TP_CONTACT_FEATURES_LEN, features, _tpl_channel_text_get_contact_cb,
      ctx, NULL, NULL);
}


static void
pendingproc_get_my_contact (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TpHandle my_handle = tp_connection_get_self_handle (
      tp_channel_borrow_connection (TP_CHANNEL (tpl_chan)));

  g_debug ("get my contact");
  GET_PRIV (tpl_text)->selector = TP_CONTACT_MYSELF;
  tp_connection_get_contacts_by_handle (tp_channel_borrow_connection (
      TP_CHANNEL (tpl_chan)), 1, &my_handle, TP_CONTACT_FEATURES_LEN,
      features, _tpl_channel_text_get_contact_cb, ctx, NULL, NULL);
}

static void
pendingproc_get_remote_handle_type (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TpHandleType remote_handle_type;

  tp_channel_get_handle (TP_CHANNEL (tpl_text), &remote_handle_type);

  switch (remote_handle_type)
    {
      case TP_HANDLE_TYPE_CONTACT:
        tpl_actionchain_append (ctx, pendingproc_get_remote_contact);
        break;
      case TP_HANDLE_TYPE_ROOM:
        tpl_actionchain_append (ctx, pendingproc_get_chatroom_id);
        break;

      /* follows unhandled TpHandleType */
      case TP_HANDLE_TYPE_NONE:
        g_warning ("remote handle: TP_HANDLE_TYPE_NONE: "
            "un-handled. Check the TelepathyLogger.client file.");
        break;
      case TP_HANDLE_TYPE_LIST:
        g_warning ("remote handle: TP_HANDLE_TYPE_LIST: "
            "un-handled. Check the TelepathyLogger.client file.");
        break;
      case TP_HANDLE_TYPE_GROUP:
        g_warning ("remote handle: TP_HANDLE_TYPE_GROUP: "
            "un-handled. Check the TelepathyLogger.client file.");
        break;
      default:
        g_error ("remote handle type unknown %d.", remote_handle_type);
        break;
    }

  tpl_actionchain_continue (ctx);
}

/* end of async Callbacks */

G_DEFINE_TYPE (TplChannelText, tpl_channel_text, TPL_TYPE_CHANNEL)

static void tpl_channel_text_dispose (GObject *obj)
{
  TplChannelTextPriv *priv = GET_PRIV (obj);

  tpl_object_unref_if_not_null (priv->my_contact);
  priv->my_contact = NULL;
  tpl_object_unref_if_not_null (priv->remote_contact);
  priv->remote_contact = NULL;

  G_OBJECT_CLASS (tpl_channel_text_parent_class)->dispose (obj);
}

static void
tpl_channel_text_finalize (GObject *obj)
{
  TplChannelTextPriv *priv = GET_PRIV(obj);

  g_free (priv->chatroom_id);

  G_OBJECT_CLASS (tpl_channel_text_parent_class)->finalize (obj);
}


static void
tpl_channel_text_class_init (TplChannelTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_channel_text_dispose;
  object_class->finalize = tpl_channel_text_finalize;
  //object_class->constructor = tpl_channel_text_constructor;

  g_type_class_add_private (object_class, sizeof (TplChannelTextPriv));
}


static void
tpl_channel_text_init (TplChannelText *self)
{
  TplChannelTextPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CHANNEL_TEXT, TplChannelTextPriv);
  self->priv = priv;
}


/**
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Channel Text proxy. The returned
 * #TplChannelText is not guaranteed to be ready at the point of return. Use #TpChannel
 * methods casting the #TplChannelText instance to a TpChannel
 *
 * TplChannelText instances are subclasses or the abstract TplChannel which is
 * subclass of TpChannel.
 *
 * Returns: the TplChannelText instance or %NULL in @object_path is not valid
 */

TplChannelText *
tpl_channel_text_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);

  /* Do what tpl_channel_new does + set TplChannelText specific properties */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TPL_TYPE_CHANNEL_TEXT,
      /* TplChannel properties */
      "account", account,
      /* TpChannel properties */
      "connection", conn,
      "dbus-daemon", conn_proxy->dbus_daemon,
      "bus-name", conn_proxy->bus_name,
      "object-path", object_path,
      "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
      "channel-properties", tp_chan_props,
      NULL);
}


TpContact *
tpl_channel_text_get_remote_contact (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CHANNEL_TEXT (self), NULL);

  return priv->remote_contact;
}

TpContact *
tpl_channel_text_get_my_contact (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_val_if_fail (TPL_IS_CHANNEL_TEXT (self), NULL);

  return priv->my_contact;
}

gboolean
tpl_channel_text_is_chatroom (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_val_if_fail(TPL_IS_CHANNEL_TEXT (self), FALSE);

  return priv->chatroom;
}

const gchar *
tpl_channel_text_get_chatroom_id (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_val_if_fail(TPL_IS_CHANNEL_TEXT (self), NULL);

  return priv->chatroom_id;
}

void
tpl_channel_text_set_remote_contact (TplChannelText *self,
    TpContact *data)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (TP_IS_CONTACT (data));
  g_return_if_fail (priv->remote_contact == NULL);

  priv->remote_contact = data;
  g_object_ref (data);
}

void
tpl_channel_text_set_my_contact (TplChannelText *self,
    TpContact *data)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (TP_IS_CONTACT (data));
  g_return_if_fail (priv->my_contact == NULL);

  priv->my_contact = data;
  g_object_ref (data);
}

void
tpl_channel_text_set_chatroom (TplChannelText *self,
    gboolean data)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));

  priv->chatroom = data;
}


void
tpl_channel_text_set_chatroom_id (TplChannelText *self,
    const gchar *data)
{
  TplChannelTextPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (priv->chatroom_id == NULL);
  priv->chatroom_id = g_strdup (data);
}


void
tpl_channel_text_call_when_ready (TplChannelText *self,
    GAsyncReadyCallback cb, gpointer user_data)
{
  TplActionChain *actions;

  /* first: connect signals, so none are lost
   * second: prepare all TplChannel
   * then: us TpContact to cache my contact and the remote one.
   * If for any reason, the order is changed, it's need to check what objects
   * are unreferenced by g_object_unref: after the order change, it might
   * happend that an object still has to be created after the change */
  actions = tpl_actionchain_new (G_OBJECT (self), cb, user_data);
  tpl_actionchain_append (actions, pendingproc_connect_signals);
  tpl_actionchain_append (actions, pendingproc_prepare_tpl_channel);
  tpl_actionchain_append (actions, pendingproc_get_my_contact);
  tpl_actionchain_append (actions, pendingproc_get_remote_handle_type);
  /* start the queue consuming */
  tpl_actionchain_continue (actions);
}


static void
pendingproc_prepare_tpl_channel (TplActionChain *ctx)
{
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_actionchain_get_object (ctx));

  g_debug ("prepare tpl");
  TPL_CHANNEL_GET_CLASS (tpl_chan)->call_when_ready (tpl_chan,
      got_tpl_chan_ready_cb, ctx);
}


static void
got_tpl_chan_ready_cb (GObject *obj,
    GAsyncResult *result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  g_debug ("PREPARE");

  if (tpl_actionchain_finish (result) == TRUE)
    tpl_actionchain_continue (ctx);
  return;
}


static void
pendingproc_get_chatroom_id (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TpConnection *connection = tp_channel_borrow_connection (TP_CHANNEL (
        tpl_chan));
  TpHandle room_handle;
  GArray *handles;

  handles = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  room_handle = tp_channel_get_handle (TP_CHANNEL (tpl_chan), NULL);
  g_array_append_val (handles, room_handle);

  tpl_channel_text_set_chatroom (tpl_text, TRUE);
  tp_cli_connection_call_inspect_handles (connection,
      -1, TP_HANDLE_TYPE_ROOM, handles, _tpl_channel_text_get_chatroom_cb,
      ctx, NULL, NULL);

  g_array_unref (handles);
}


static void
_tpl_channel_text_get_chatroom_cb (TpConnection *proxy,
    const gchar **out_Identifiers,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplActionChain *ctx = user_data;
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (tpl_text));

  if (error != NULL)
    g_error ("retrieving chatroom identifier: %s", error->message);

  g_debug ("Chatroom id: %s", *out_Identifiers);
  tpl_channel_text_set_chatroom_id (tpl_text, *out_Identifiers);

  tpl_actionchain_continue (ctx);
}


static void
pendingproc_connect_signals (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  GError *error = NULL;
  TpChannel *channel = NULL;

  g_debug ("CONNECT");

  channel = TP_CHANNEL (TPL_CHANNEL (tpl_text));

  tp_cli_channel_type_text_connect_to_received (channel,
      _channel_on_received_signal_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("received signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_sent (channel,
      _channel_on_sent_signal_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("sent signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_send_error (channel,
      _channel_on_send_error_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("send error signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_type_text_connect_to_lost_message (channel,
      _channel_on_lost_message_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      g_error ("lost message signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  tp_cli_channel_connect_to_closed (channel, _channel_on_closed_cb,
      tpl_text, NULL, NULL, &error);

  if (error != NULL)
    {
      g_error ("channel closed signal connect: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      error = NULL;
    }

  /* TODO connect to TpContacts' notify::presence-type */

  tpl_actionchain_continue (ctx);
}



/* Signal's Callbacks */
static void
_channel_on_closed_cb (TpChannel *proxy,
    gpointer user_data,
    GObject *weak_object)
{
  TplChannelText *tpl_text = TPL_CHANNEL_TEXT (user_data);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  gchar *chan_path;
  TplObserver *observer = tpl_observer_new ();

  /* set chan_path from the TpConnection's property */
  g_object_get (G_OBJECT (tp_channel_borrow_connection (TP_CHANNEL (tpl_chan))),
      "object-path", &chan_path, NULL);

  if (!tpl_observer_unregister_channel (observer, tpl_chan))
    g_warning ("Channel %s couldn't be unregistered correctly (BUG?)",
	       chan_path);

  g_object_unref (observer);
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
  TplChannelText *tpl_text = TPL_CHANNEL_TEXT (user_data);
  TpContact *remote = NULL;
  TpContact *me;
  TplContact *tpl_contact_sender;
  TplContact *tpl_contact_receiver = NULL;
  TplLogEntryText *log;
  TplLogManager *logmanager;
  gchar *chat_id;

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (tpl_text));

  /* Initialize data for TplContact */
  me = tpl_channel_text_get_my_contact (tpl_text);
  tpl_contact_sender = tpl_contact_from_tp_contact (me);
  tpl_contact_set_contact_type (tpl_contact_sender, TPL_CONTACT_USER);

  if (tpl_channel_text_is_chatroom (tpl_text) == FALSE)
    {
      remote = tpl_channel_text_get_remote_contact (tpl_text);
      if (remote == NULL)
        g_error ("sending message: Remote TplContact=NULL on 1-1 Chat");
      tpl_contact_receiver = tpl_contact_from_tp_contact (remote);
      tpl_contact_set_contact_type (tpl_contact_receiver, TPL_CONTACT_USER);
    }

  g_message ("sent: %s (%s): %s",
      tpl_contact_get_identifier (tpl_contact_sender),
      tpl_contact_get_alias (tpl_contact_sender), arg_Text);

  /* Initialise TplLogEntryText */
  if (!tpl_channel_text_is_chatroom (tpl_text))
    chat_id = g_strdup (tpl_contact_get_identifier (tpl_contact_receiver));
  else
    chat_id = g_strdup (tpl_channel_text_get_chatroom_id (tpl_text));

  log = tpl_log_entry_text_new (arg_Timestamp, chat_id,
      TPL_LOG_ENTRY_DIRECTION_OUT);
  g_free (chat_id);

  tpl_log_entry_text_set_timestamp (log, (time_t) arg_Timestamp);
  tpl_log_entry_text_set_signal_type (log, TPL_LOG_ENTRY_TEXT_SIGNAL_SENT);
  tpl_log_entry_text_set_sender (log, tpl_contact_sender);
  tpl_log_entry_text_set_receiver (log, tpl_contact_receiver);
  tpl_log_entry_text_set_message (log, arg_Text);
  tpl_log_entry_text_set_message_type (log, arg_Type);
  tpl_log_entry_text_set_tpl_channel_text (log, tpl_text);

  /* Initialized LogStore and send the log entry */
  tpl_log_entry_text_set_chatroom (log,
		  tpl_channel_text_is_chatroom (tpl_text));

  logmanager = tpl_log_manager_dup_singleton ();
  tpl_log_manager_add_message (logmanager, TPL_LOG_ENTRY (log), &error);

  if (error != NULL)
    {
      g_error ("LogStore: %s", error->message);
      g_clear_error (&error);
      g_error_free (error);
    }

  if (tpl_contact_receiver)
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
  TplChannelText *tpl_text = tpl_log_entry_text_get_tpl_channel_text (log);
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
  tpl_channel_text_set_remote_contact (tpl_text, remote);
  tpl_contact_sender = tpl_contact_from_tp_contact (remote);

  tpl_contact_set_contact_type (tpl_contact_sender, TPL_CONTACT_USER);
  tpl_log_entry_text_set_sender (log, tpl_contact_sender);

  g_message ("recvd: %s (%s): %s",
	     tpl_contact_get_identifier (tpl_contact_sender),
	     tpl_contact_get_alias (tpl_contact_sender),
	     tpl_log_entry_text_get_message (log));

  /* Initialise LogStore and store the message */

  if (!tpl_channel_text_is_chatroom (tpl_text))
    chat_id = g_strdup (tpl_contact_get_identifier (tpl_contact_sender));
  else
    chat_id = g_strdup (tpl_channel_text_get_chatroom_id (tpl_text));

  tpl_log_entry_text_set_chat_id (log, chat_id);
  tpl_log_entry_text_set_chatroom (log,
      tpl_channel_text_is_chatroom (tpl_text));

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
  TplChannelText *tpl_text = TPL_CHANNEL_TEXT (user_data);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
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

  tpl_log_entry_text_set_tpl_channel_text (log, tpl_text);
  tpl_log_entry_text_set_message (log, arg_Text);
  tpl_log_entry_text_set_message_type (log, arg_Type);
  tpl_log_entry_text_set_signal_type (log, TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED);

  me = tpl_channel_text_get_my_contact (tpl_text);
  tpl_contact_receiver = tpl_contact_from_tp_contact (me);
  tpl_contact_set_contact_type (tpl_contact_receiver, TPL_CONTACT_USER);
  tpl_log_entry_text_set_receiver (log, tpl_contact_receiver);

  tpl_log_entry_text_set_timestamp (log, (time_t) arg_Timestamp);

  tp_connection_get_contacts_by_handle (tp_channel_borrow_connection (
      TP_CHANNEL (tpl_chan)), 1, &remote_handle, TP_CONTACT_FEATURES_LEN,
      features, _channel_on_received_signal_with_contact_cb, log,
      g_object_unref, NULL);

  g_object_unref (tpl_contact_receiver);
}

/* End of Signal's Callbacks */

