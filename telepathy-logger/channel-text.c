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

#include "config.h"
#include "channel-text.h"

#include <glib.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/enums.h>

#include <telepathy-logger/action-chain.h>
#include <telepathy-logger/contact.h>
#include <telepathy-logger/channel.h>
#include <telepathy-logger/observer.h>
#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager-priv.h>
#include <telepathy-logger/util.h>

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include <telepathy-logger/debug.h>

#define TP_CONTACT_FEATURES_LEN 3
#define TP_CONTACT_MYSELF 0
#define TP_CONTACT_REMOTE 1

#define GET_PRIV(obj)    TPL_GET_PRIV (obj, TplChannelText)
struct _TplChannelTextPriv
{
  gboolean chatroom;
  TpContact *my_contact;
  TpContact *remote_contact;  /* only set if chatroom==FALSE */
  gchar *chatroom_id;          /* only set if chatroom==TRUE */

  /* only used as metadata in CB data passing */
  guint selector;
};

static TpContactFeature features[TP_CONTACT_FEATURES_LEN] = {
  TP_CONTACT_FEATURE_ALIAS,
  TP_CONTACT_FEATURE_PRESENCE,
  TP_CONTACT_FEATURE_AVATAR_TOKEN
};

static void call_when_ready_wrapper (TplChannel *tpl_chan,
    GAsyncReadyCallback cb, gpointer user_data);
void got_pending_messages_cb (TpChannel *proxy, const GPtrArray *result,
    const GError *error, gpointer user_data, GObject *weak_object);

static void got_tpl_chan_ready_cb (GObject *obj, GAsyncResult *result,
    gpointer user_data);
static void on_closed_cb (TpChannel *proxy, gpointer user_data,
    GObject *weak_object);
static void on_lost_message_cb (TpChannel *proxy, gpointer user_data,
    GObject *weak_object);
static void on_received_signal_cb (TpChannel *proxy, guint arg_ID,
    guint arg_Timestamp, guint arg_Sender, guint arg_Type, guint arg_Flags,
    const gchar *arg_Text, gpointer user_data, GObject *weak_object);
static void on_sent_signal_cb (TpChannel *proxy, guint arg_Timestamp,
    guint arg_Type, const gchar *arg_Text,  gpointer user_data,
    GObject *weak_object);
static void on_send_error_cb (TpChannel *proxy, guint arg_Error,
    guint arg_Timestamp, guint arg_Type, const gchar *arg_Text,
    gpointer user_data, GObject *weak_object);
static void pendingproc_connect_signals (TplActionChain *ctx);
static void pendingproc_get_pending_messages (TplActionChain *ctx);
static void pendingproc_prepare_tpl_channel (TplActionChain *ctx);
static void pendingproc_get_chatroom_id (TplActionChain *ctx);
static void get_chatroom_id_cb (TpConnection *proxy,
    const gchar **out_Identifiers, const GError *error, gpointer user_data,
    GObject *weak_object);
static void pendingproc_get_my_contact (TplActionChain *ctx);
static void pendingproc_get_remote_contact (TplActionChain *ctx);
static void pendingproc_get_remote_handle_type (TplActionChain *ctx);
static void keepon (TplLogEntryText *log);


/* retrieve contacts (me and remote buddy/chatroom) and set TplChannelText
 * members  */


/* used by _get_my_contact and _get_remote_contact */
static void
got_contact_cb (TpConnection *connection,
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
  gchar *conn_path;

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (tpl_text));

  g_assert_cmpuint (n_failed, ==, 0);
  g_assert_cmpuint (n_contacts, ==, 1);
  g_assert_cmpuint (priv->selector, <=, TP_CONTACT_REMOTE);

  if (n_failed > 0)
    {
      g_object_get (G_OBJECT (tp_channel_borrow_connection
          (tp_chan)), "object-path", &conn_path, NULL);

      CHAN_DEBUG (tpl_text, "Error resolving self handle for connection %s."
         " Aborting channel observation", conn_path);
      tpl_observer_unregister_channel (observer, TPL_CHANNEL (tpl_text));

      g_free (conn_path);
      g_object_unref (observer);
      tpl_actionchain_terminate (ctx);
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
      CHAN_DEBUG (tpl_text, "retrieving TpContacts: passing invalid value for selector: %d"
         "Aborting channel observation", priv->selector);
      tpl_observer_unregister_channel (observer, TPL_CHANNEL (tpl_text));
      g_object_unref (observer);
      tpl_actionchain_terminate (ctx);
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
      TP_CONTACT_FEATURES_LEN, features, got_contact_cb, ctx, NULL, NULL);
}


static void
pendingproc_get_my_contact (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  TpConnection *tp_conn = tp_channel_borrow_connection (TP_CHANNEL (
        tpl_text));
  TpHandle my_handle = tp_connection_get_self_handle (tp_conn);

  GET_PRIV (tpl_text)->selector = TP_CONTACT_MYSELF;
  tp_connection_get_contacts_by_handle (tp_conn, 1, &my_handle,
      TP_CONTACT_FEATURES_LEN, features, got_contact_cb, ctx, NULL, NULL);
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
        tpl_actionchain_prepend (ctx, pendingproc_get_remote_contact);
        break;
      case TP_HANDLE_TYPE_ROOM:
        tpl_actionchain_prepend (ctx, pendingproc_get_chatroom_id);
        break;
      case TP_HANDLE_TYPE_NONE:
        CHAN_DEBUG (tpl_text, "HANDLE_TYPE_NONE received, probably an anonymous "
            "chat, like MSN ones. TODO: implement this possibility");
        tpl_actionchain_terminate (ctx);
        return;
        break;
      /* follows unhandled TpHandleType */
      case TP_HANDLE_TYPE_LIST:
        CHAN_DEBUG (tpl_text, "remote handle: TP_HANDLE_TYPE_LIST: "
            "un-handled. Check the TelepathyLogger.client file.");
        tpl_actionchain_terminate (ctx);
        return;
        break;
      case TP_HANDLE_TYPE_GROUP:
        CHAN_DEBUG (tpl_text, "remote handle: TP_HANDLE_TYPE_GROUP: "
            "un-handled. Check the TelepathyLogger.client file.");
        tpl_actionchain_terminate (ctx);
        return;
        break;
      default:
        CHAN_DEBUG (tpl_text, "remote handle type unknown %d.",
            remote_handle_type);
        tpl_actionchain_terminate (ctx);
        return;
        break;
    }

  tpl_actionchain_continue (ctx);
}
/* end of async Callbacks */


G_DEFINE_TYPE (TplChannelText, tpl_channel_text, TPL_TYPE_CHANNEL)

static void
tpl_channel_text_dispose (GObject *obj)
{
  TplChannelTextPriv *priv = GET_PRIV (obj);

  if (priv->my_contact != NULL)
    {
      g_object_unref (priv->my_contact);
      priv->my_contact = NULL;
    }
  if (priv->remote_contact != NULL)
    {
      g_object_unref (priv->remote_contact);
      priv->remote_contact = NULL;
    }

  G_OBJECT_CLASS (tpl_channel_text_parent_class)->dispose (obj);
}


static void
tpl_channel_text_finalize (GObject *obj)
{
  TplChannelTextPriv *priv = GET_PRIV (obj);

  g_free (priv->chatroom_id);
  priv->chatroom_id = NULL;

  G_OBJECT_CLASS (tpl_channel_text_parent_class)->finalize (obj);
}


static void
tpl_channel_text_class_init (TplChannelTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplChannelClass *tpl_chan_class = TPL_CHANNEL_CLASS (klass);

  object_class->dispose = tpl_channel_text_dispose;
  object_class->finalize = tpl_channel_text_finalize;

  tpl_chan_class->call_when_ready = call_when_ready_wrapper;

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
 * tpl_channel_text_new
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @account: TpAccount instance, related to the new #TplChannelText
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Channel Text proxy.
 * The returned #TplChannelText is not guaranteed to be ready at the point of
 * return.
 *
 * TplChannelText is actually a subclass of the abstract TplChannel which is a
 * subclass of TpChannel.
 * Use #TpChannel methods, casting the #TplChannelText instance to a
 * TpChannel, to access TpChannel data/methods from it.
 *
 * TplChannelText is usually created using #tpl_channel_factory_build, from
 * within a #TplObserver singleton, when its Observer_Channel method is called
 * by the Channel Dispatcher.
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
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CHANNEL_TEXT (self), NULL);

  return priv->my_contact;
}


gboolean
tpl_channel_text_is_chatroom (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CHANNEL_TEXT (self), FALSE);

  return priv->chatroom;
}


const gchar *
tpl_channel_text_get_chatroom_id (TplChannelText *self)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CHANNEL_TEXT (self), NULL);

  return priv->chatroom_id;
}


void
tpl_channel_text_set_remote_contact (TplChannelText *self,
    TpContact *data)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (TP_IS_CONTACT (data));
  g_return_if_fail (priv->remote_contact == NULL);

  priv->remote_contact = g_object_ref (data);
}


void
tpl_channel_text_set_my_contact (TplChannelText *self,
    TpContact *data)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (TP_IS_CONTACT (data));
  g_return_if_fail (priv->my_contact == NULL);

  priv->my_contact = g_object_ref (data);
}


void
tpl_channel_text_set_chatroom (TplChannelText *self,
    gboolean data)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));

  priv->chatroom = data;
}


void
tpl_channel_text_set_chatroom_id (TplChannelText *self,
    const gchar *data)
{
  TplChannelTextPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (priv->chatroom_id == NULL);
  priv->chatroom_id = g_strdup (data);
}


static void
call_when_ready_wrapper (TplChannel *tpl_chan,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  tpl_channel_text_call_when_ready (TPL_CHANNEL_TEXT (tpl_chan), cb,
      user_data);
}


void
tpl_channel_text_call_when_ready (TplChannelText *self,
    GAsyncReadyCallback cb, gpointer user_data)
{
  TplActionChain *actions;

  /* first: connect signals, so none are lost
   * second: prepare all TplChannel
   * third: cache my contact and the remote one.
   * last: check for pending messages
   *
   * If for any reason, the order is changed, it's needed to check what objects
   * are unreferenced by g_object_unref but used by a next action AND what object are actually not
   * prepared but used anyway */
  actions = tpl_actionchain_new (G_OBJECT (self), cb, user_data);
  tpl_actionchain_append (actions, pendingproc_connect_signals);
  tpl_actionchain_append (actions, pendingproc_prepare_tpl_channel);
  tpl_actionchain_append (actions, pendingproc_get_my_contact);
  tpl_actionchain_append (actions, pendingproc_get_remote_handle_type);
  tpl_actionchain_append (actions, pendingproc_get_pending_messages);
  /* start the chain consuming */
  tpl_actionchain_continue (actions);
}


static void
pendingproc_prepare_tpl_channel (TplActionChain *ctx)
{
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_actionchain_get_object (ctx));

  TPL_CHANNEL_GET_CLASS (tpl_chan)->call_when_ready_protected (tpl_chan,
      got_tpl_chan_ready_cb, ctx);
}


static void
got_tpl_chan_ready_cb (GObject *obj,
    GAsyncResult *tpl_chan_result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;

  /* if TplChannel preparation is OK, keep on with the TplChannelText */
  if (tpl_actionchain_finish (tpl_chan_result))
    tpl_actionchain_continue (ctx);
  else
     tpl_actionchain_terminate (ctx);
  return;
}


static void
pendingproc_get_pending_messages (TplActionChain *ctx)
{
  TplChannelText *chan_text = tpl_actionchain_get_object (ctx);

  tp_cli_channel_type_text_call_list_pending_messages (TP_CHANNEL (chan_text),
      -1, FALSE, got_pending_messages_cb, ctx, NULL, NULL);
}

void
got_pending_messages_cb (TpChannel *proxy,
    const GPtrArray *result,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplActionChain *ctx = user_data;
  guint i;

  if (error != NULL)
    {
      CHAN_DEBUG (proxy, "retrieving pending messages: %s", error->message);
      tpl_actionchain_terminate (ctx);
      return;
    }

  CHAN_DEBUG (proxy, "%d pending message(s)", result->len);
  for (i = 0; i < result->len; ++i)
    {
      GValueArray    *message_struct;
      const gchar    *message_body;
      guint           message_id;
      guint           message_timestamp;
      guint           from_handle;
      guint           message_type;
      guint           message_flags;

      message_struct = g_ptr_array_index (result, i);

      message_id = g_value_get_uint (g_value_array_get_nth (message_struct, 0));
      message_timestamp = g_value_get_uint (g_value_array_get_nth
          (message_struct, 1));
      from_handle = g_value_get_uint (g_value_array_get_nth (message_struct, 2));
      message_type = g_value_get_uint (g_value_array_get_nth (message_struct, 3));
      message_flags = g_value_get_uint (g_value_array_get_nth (message_struct, 4));
      message_body = g_value_get_string (g_value_array_get_nth (message_struct, 5));

      /* call the received signal callback to trigger the message storing */
      on_received_signal_cb (proxy, message_id, message_timestamp, from_handle,
          message_type, message_flags, message_body, TPL_CHANNEL_TEXT (proxy),
          NULL);
    }

  tpl_actionchain_continue (ctx);
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
      -1, TP_HANDLE_TYPE_ROOM, handles, get_chatroom_id_cb,
      ctx, NULL, NULL);

  g_array_unref (handles);
}


static void
get_chatroom_id_cb (TpConnection *proxy,
    const gchar **out_Identifiers,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplActionChain *ctx = user_data;
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);

  g_return_if_fail (TPL_IS_CHANNEL_TEXT (tpl_text));

  if (error != NULL) {
    CHAN_DEBUG (proxy, "retrieving chatroom identifier: %s", error->message);
    tpl_actionchain_terminate (ctx);
    return;
  }

  CHAN_DEBUG (proxy, "Chatroom id: %s", *out_Identifiers);
  tpl_channel_text_set_chatroom_id (tpl_text, *out_Identifiers);

  tpl_actionchain_continue (ctx);
}


static void
pendingproc_connect_signals (TplActionChain *ctx)
{
  TplChannelText *tpl_text = tpl_actionchain_get_object (ctx);
  GError *error = NULL;
  gboolean is_error = FALSE;
  TpChannel *channel = NULL;

  channel = TP_CHANNEL (TPL_CHANNEL (tpl_text));

  tp_cli_channel_type_text_connect_to_received (channel,
      on_received_signal_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "received signal connect: %s", error->message);
      g_error_free (error);
      error = NULL;
      is_error = TRUE;
    }

  tp_cli_channel_type_text_connect_to_sent (channel,
      on_sent_signal_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "sent signal connect: %s", error->message);
      g_error_free (error);
      error = NULL;
      is_error = TRUE;
    }

  tp_cli_channel_type_text_connect_to_send_error (channel,
      on_send_error_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "send error signal connect: %s", error->message);
      g_error_free (error);
      error = NULL;
      is_error = TRUE;
    }

  tp_cli_channel_type_text_connect_to_lost_message (channel,
      on_lost_message_cb, tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "lost message signal connect: %s", error->message);
      g_error_free (error);
      error = NULL;
      is_error = TRUE;
    }

  tp_cli_channel_connect_to_closed (channel, on_closed_cb,
      tpl_text, NULL, NULL, &error);
  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "channel closed signal connect: %s", error->message);
      g_error_free (error);
      error = NULL;
      is_error = TRUE;
    }

  /* TODO connect to TpContacts' notify::presence-type */

  if (is_error)
    tpl_actionchain_terminate (ctx);
  else
    tpl_actionchain_continue (ctx);
}



/* Signal's Callbacks */
static void
on_closed_cb (TpChannel *proxy,
    gpointer user_data,
    GObject *weak_object)
{
  TplChannelText *tpl_text = TPL_CHANNEL_TEXT (user_data);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TplObserver *observer = tpl_observer_new ();

  if (!tpl_observer_unregister_channel (observer, tpl_chan))
    CHAN_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static void
on_lost_message_cb (TpChannel *proxy,
           gpointer user_data,
           GObject *weak_object)
{
  CHAN_DEBUG (proxy, "lost message signal catched. nothing logged");
  /* TODO log that the system lost a message */
}


static void
on_send_error_cb (TpChannel *proxy,
         guint arg_Error,
         guint arg_Timestamp,
         guint arg_Type,
         const gchar *arg_Text,
         gpointer user_data,
         GObject *weak_object)
{
  CHAN_DEBUG (proxy, "unlogged event: TP was unable to send the message: %s", arg_Text);
  /* TODO log that the system was unable to send the message */
}


static void
on_sent_signal_cb (TpChannel *proxy,
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

  if (!tpl_channel_text_is_chatroom (tpl_text))
    {
      remote = tpl_channel_text_get_remote_contact (tpl_text);
      if (remote == NULL)
        CHAN_DEBUG (tpl_text, "sending message: Remote TplContact=NULL on 1-1"
            "Chat");
      tpl_contact_receiver = tpl_contact_from_tp_contact (remote);
      tpl_contact_set_contact_type (tpl_contact_receiver, TPL_CONTACT_USER);
    }

  DEBUG ("sent:\n\tto=\"%s (%s)\"\n\tfrom=\"%s (%s)\"\n\tmsg=\"%s\"",
      tpl_contact_get_identifier (tpl_contact_receiver),
      tpl_contact_get_alias (tpl_contact_receiver),
      tpl_contact_get_identifier (tpl_contact_sender),
      tpl_contact_get_alias (tpl_contact_sender),
      arg_Text);


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
      CHAN_DEBUG (tpl_text, "LogStore: %s", error->message);
      g_error_free (error);
    }

  if (tpl_contact_receiver)
    g_object_unref (tpl_contact_receiver);
  g_object_unref (tpl_contact_sender);
  g_object_unref (logmanager);
  g_object_unref (log);
}

static void
on_received_signal_with_contact_cb (TpConnection *connection,
    guint n_contacts,
    TpContact *const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplLogEntryText *log = TPL_LOG_ENTRY_TEXT (user_data);
  TplChannelText *tpl_text;
  TpContact *remote;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (log));

  tpl_text = tpl_log_entry_text_get_tpl_channel_text (log);

  if (error != NULL)
    {
      CHAN_DEBUG (tpl_text, "Unrecoverable error retrieving remote contact "
         "information: %s", error->message);
      DEBUG ("Not able to log the received message: %s",
         tpl_log_entry_text_get_message (log));
      g_object_unref (log);
      return;
    }

  if (n_failed > 0)
    {
      DEBUG ("%d invalid handle(s) passed to "
         "tp_connection_get_contacts_by_handle()", n_failed);
      DEBUG ("Not able to log the received message: %s",
         tpl_log_entry_text_get_message (log));
      g_object_unref (log);
      return;
    }

  remote = contacts[0];
  tpl_channel_text_set_remote_contact (tpl_text, remote);

  keepon (log);
}

static void
keepon (TplLogEntryText *log)
{
  TplChannelText *tpl_text;
  GError *e = NULL;
  TplLogManager *logmanager;
  TplContact *tpl_contact_sender;
  TplContact *tpl_contact_receiver;
  TpContact *remote;
  TpContact *local;
  gchar *chat_id;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (log));

  tpl_text = tpl_log_entry_text_get_tpl_channel_text (log);
  remote = tpl_channel_text_get_remote_contact (tpl_text);
  local = tpl_channel_text_get_my_contact (tpl_text);

  tpl_contact_sender = tpl_contact_from_tp_contact (remote);
  tpl_contact_set_contact_type (tpl_contact_sender, TPL_CONTACT_USER);
  tpl_log_entry_text_set_sender (log, tpl_contact_sender);

  tpl_contact_receiver = tpl_contact_from_tp_contact (local);

  DEBUG ("recvd:\n\tto=\"%s (%s)\"\n\tfrom=\"%s (%s)\"\n\tmsg=\"%s\"",
      tpl_contact_get_identifier (tpl_contact_receiver),
      tpl_contact_get_alias (tpl_contact_receiver),
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
      DEBUG ("LogStore: %s", e->message);
      g_error_free (e);
    }

  g_object_unref (tpl_contact_sender);
  g_object_unref (logmanager);
  g_free (chat_id);
}

static void
on_received_signal_cb (TpChannel *proxy,
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
  TpConnection *tp_conn;
  TpContact *me;
  TplContact *tpl_contact_receiver;
  TplLogEntryText *log;

  /* TODO use the Message iface to check the delivery
     notification and handle it correctly */
  if (arg_Flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT)
    {
      DEBUG ("Non text content flag set. "
          "Probably a delivery notification for a sent message. "
          "Ignoring");
      return;
    }

  /* Initialize TplLogEntryText (part 1) - chat_id still unknown */
  log = tpl_log_entry_text_new (arg_ID, NULL,
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

  tp_conn = tp_channel_borrow_connection (TP_CHANNEL (tpl_chan));
  /* it's a chatroom and no contact has been pre-cached */
  if (tpl_channel_text_get_remote_contact (tpl_text) == NULL)
    tp_connection_get_contacts_by_handle (tp_conn, 1, &remote_handle,
        TP_CONTACT_FEATURES_LEN, features, on_received_signal_with_contact_cb,
        log, g_object_unref, NULL);
  else
    keepon (log);

  g_object_unref (tpl_contact_receiver);
}

/* End of Signal's Callbacks */

