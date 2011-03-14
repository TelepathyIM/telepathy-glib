/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009-2011 Collabora Ltd.
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
 *          Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "text-channel-internal.h"

#include <glib.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/proxy.h>

#include "action-chain-internal.h"
#include "channel-internal.h"
#include "entity-internal.h"
#include "event-internal.h"
#include "log-manager-internal.h"
#include "observer-internal.h"
#include "text-event.h"
#include "text-event-internal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include "debug-internal.h"

struct _TplTextChannelPriv
{
  TplEntity *self;
  gboolean is_chatroom;
  TplEntity *chatroom;   /* only set if is_chatroom==TRUE */

  /* Entities participating in this channel.
   * This is used as a cache so we don't have to recreate
   * TpContact objects each time we receive something.
   *
   * TpHandle => reffed TplEntity
   */
  GHashTable *entities;
};

typedef struct
{
  gint msg_id;
  gchar *log_id;
  guint type;
  gchar *text;
  gint64 timestamp;

} ReceivedData;

static TpContactFeature features[3] = {
  TP_CONTACT_FEATURE_ALIAS,
  TP_CONTACT_FEATURE_PRESENCE,
  TP_CONTACT_FEATURE_AVATAR_TOKEN
};

G_DEFINE_TYPE (TplTextChannel, _tpl_text_channel, TPL_TYPE_CHANNEL)


static void
got_tpl_chan_ready_cb (GObject *obj,
    GAsyncResult *tpl_chan_result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;

  /* if TplChannel preparation is OK, keep on with the TplTextChannel */
  if (_tpl_action_chain_new_finish (tpl_chan_result))
    _tpl_action_chain_continue (ctx);
  else
     _tpl_action_chain_terminate (ctx);
  return;
}


static void
pendingproc_prepare_tpl_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = TPL_CHANNEL (_tpl_action_chain_get_object (ctx));

  TPL_CHANNEL_GET_CLASS (tpl_chan)->call_when_ready_protected (tpl_chan,
      got_tpl_chan_ready_cb, ctx);
}


static void
get_self_contact_cb (TpConnection *connection,
    guint n_contacts,
    TpContact *const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplActionChain *ctx = user_data;
  TplTextChannel *tpl_text = _tpl_action_chain_get_object (ctx);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_text);
  TpChannel *tp_chan = TP_CHANNEL (tpl_chan);

  g_return_if_fail (TPL_IS_TEXT_CHANNEL (tpl_text));

  if (n_failed > 0)
    {
      TpConnection *tp_conn = tp_channel_borrow_connection (tp_chan);
      const gchar *conn_path;

      conn_path = tp_proxy_get_object_path (TP_PROXY (tp_conn));

      PATH_DEBUG (tpl_text, "Error resolving self handle for connection %s."
         " Aborting channel observation", conn_path);

      _tpl_action_chain_terminate (ctx);
      return;
    }

  tpl_text->priv->self = tpl_entity_new_from_tp_contact (contacts[0],
      TPL_ENTITY_SELF);

  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_get_my_contact (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *tpl_text = _tpl_action_chain_get_object (ctx);
  TpChannel *chan = TP_CHANNEL (tpl_text);
  TpConnection *tp_conn = tp_channel_borrow_connection (chan);
  TpHandle my_handle;

  my_handle = tp_channel_group_get_self_handle (chan);
  if (my_handle == 0)
    my_handle = tp_connection_get_self_handle (tp_conn);

  tp_connection_get_contacts_by_handle (tp_conn, 1, &my_handle,
      G_N_ELEMENTS (features), features, get_self_contact_cb, ctx, NULL,
      G_OBJECT (tpl_text));
}


static void
get_remote_contacts_cb (TpConnection *connection,
    guint n_contacts,
    TpContact *const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplActionChain *ctx = user_data;
  TplTextChannel *self = TPL_TEXT_CHANNEL (weak_object);
  guint i;

  if (error != NULL)
    {
      DEBUG ("Failed to get remote contacts: %s", error->message);

      if (ctx != NULL)
        _tpl_action_chain_terminate (ctx);
      return;
    }

  for (i = 0; i < n_contacts; i++)
    {
      TpContact *contact = contacts[i];
      TpHandle handle = tp_contact_get_handle (contact);

      g_hash_table_insert (self->priv->entities, GUINT_TO_POINTER (handle),
          tpl_entity_new_from_tp_contact (contact, TPL_ENTITY_CONTACT));
    }

  if (ctx != NULL)
    _tpl_action_chain_continue (ctx);
}


static void
chan_members_changed_cb (TpChannel *chan,
    gchar *message,
    GArray *added,
    GArray *removed,
    GArray *local_pending,
    GArray *remote_pending,
    TpHandle actor,
    guint reason,
    gpointer user_data)
{
  TplTextChannel *self = user_data;
  guint i;

  if (added->len > 0)
    {
      tp_connection_get_contacts_by_handle (tp_channel_borrow_connection (chan),
          added->len, (TpHandle *) added->data,
          G_N_ELEMENTS (features), features, get_remote_contacts_cb, NULL, NULL,
          G_OBJECT (self));
    }

  for (i = 0; i < removed->len; i++)
    {
      TpHandle handle = g_array_index (removed, TpHandle, i);

      g_hash_table_remove (self->priv->entities, GUINT_TO_POINTER (handle));
    }
}


static void
pendingproc_get_remote_contacts (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);
  TpChannel *chan = TP_CHANNEL (self);
  TpConnection *tp_conn = tp_channel_borrow_connection (chan);
  TpHandle handle;
  TpHandleType handle_type;
  GArray *arr;

  handle = tp_channel_get_handle (chan, &handle_type);

  if (handle_type == TP_HANDLE_TYPE_ROOM)
    {
      self->priv->is_chatroom = TRUE;
      self->priv->chatroom =
        tpl_entity_new_from_room_id (tp_channel_get_identifier (chan));

      PATH_DEBUG (self, "Chatroom id: %s",
          tpl_entity_get_identifier (self->priv->chatroom));
    }

  if (tp_proxy_has_interface_by_id (chan,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP))
    {
      /* Get the contacts of all the members */
      const TpIntSet *members;

      members = tp_channel_group_get_members (chan);
      arr = tp_intset_to_array (members);

      tp_g_signal_connect_object (chan, "group-members-changed",
          G_CALLBACK (chan_members_changed_cb), self, 0);
    }
  else
    {
      /* Get the contact of the TargetHandle */
      arr = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
      handle = tp_channel_get_handle (chan, NULL);

      g_array_append_val (arr, handle);
    }

  tp_connection_get_contacts_by_handle (tp_conn,
      arr->len, (TpHandle *) arr->data,
      G_N_ELEMENTS (features), features, get_remote_contacts_cb, ctx, NULL,
      G_OBJECT (self));

  g_array_free (arr, TRUE);
}


static void
on_channel_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TplChannel *tpl_chan = TPL_CHANNEL (user_data);
  TplObserver *observer = _tpl_observer_new ();

  PATH_DEBUG (tpl_chan, "%s #%d %s",
      g_quark_to_string (domain), code, message);

  if (!_tpl_observer_unregister_channel (observer, tpl_chan))
    PATH_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static void
keepon_on_receiving_signal (TplTextChannel *tpl_text,
    TplEntity *sender,
    ReceivedData *data)
{
  TplTextEvent *text_log;
  GError *error = NULL;
  TplLogManager *logmanager;
  TplEntity *receiver;

  if (tpl_text->priv->is_chatroom)
    receiver = tpl_text->priv->chatroom;
  else
    receiver = tpl_text->priv->self;

  /* Initialize TplTextEvent */
  text_log = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", _tpl_channel_get_account (TPL_CHANNEL (tpl_text)),
      "channel-path", tp_proxy_get_object_path (TP_PROXY (tpl_text)),
      "log-id", data->log_id,
      "receiver", receiver,
      "sender", sender,
      "timestamp", data->timestamp,
      /* TplTextEvent */
      "message-type", data->type,
      "message", data->text,
      "pending-msg-id", data->msg_id,
      NULL);

  DEBUG ("recvd:\n\tlog_id=\"%s\"\n\tto=\"%s "
      "(%s)\"\n\tfrom=\"%s (%s)\"\n\tmsg=\"%s\"",
      _tpl_event_get_log_id (TPL_EVENT (text_log)),
      tpl_entity_get_identifier (receiver),
      tpl_entity_get_alias (receiver),
      tpl_entity_get_identifier (sender),
      tpl_entity_get_alias (sender),
      tpl_text_event_get_message (text_log));

  logmanager = tpl_log_manager_dup_singleton ();
  _tpl_log_manager_add_event (logmanager, TPL_EVENT (text_log), &error);

  if (error != NULL)
    {
      DEBUG ("%s", error->message);
      g_error_free (error);
    }

  g_object_unref (text_log);
  g_object_unref (logmanager);
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
  ReceivedData *data = user_data;
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (weak_object);
  TplEntity *remote;
  TpHandle handle;

  if (error != NULL)
    {
      PATH_DEBUG (tpl_text, "An Unrecoverable error retrieving remote contact "
         "information occured: %s", error->message);
      PATH_DEBUG (tpl_text, "Unable to log the received message: %s",
          data->text);
      return;
    }

  if (n_failed > 0)
    {
      PATH_DEBUG (tpl_text, "%d invalid handle(s) passed to "
         "tp_connection_get_contacts_by_handle()", n_failed);
      PATH_DEBUG (tpl_text, "Not able to log the received message: %s",
         data->text);
      return;
    }

  remote = tpl_entity_new_from_tp_contact (contacts[0], TPL_ENTITY_CONTACT);
  handle = tp_contact_get_handle (contacts[0]);

  g_hash_table_insert (tpl_text->priv->entities, GUINT_TO_POINTER (handle),
    remote);

  keepon_on_receiving_signal (tpl_text, remote, data);
}


static void
received_data_free (gpointer arg)
{
  ReceivedData *data = arg;
  g_free (data->log_id);
  g_free (data->text);
  g_slice_free (ReceivedData,data);
}


static void
on_received_signal_cb (TpChannel *proxy,
    guint msg_id,
    guint timestamp,
    TpHandle sender,
    guint type,
    guint flags,
    const gchar *text,
    gpointer user_data,
    GObject *weak_object)
{
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (proxy);
  TpConnection *tp_conn;
  TplEntity *remote;
  const gchar *channel_path = tp_proxy_get_object_path (TP_PROXY (tpl_text));
  ReceivedData *data;

  /* TODO use the Message iface to check the delivery
     notification and handle it correctly */
  if (flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT)
    {
      PATH_DEBUG (tpl_text, "Non text content flag set. "
          "Probably a delivery notification for a sent message. "
          "Ignoring");
      return;
    }

  if (flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_RESCUED)
    {
      PATH_DEBUG (tpl_text, "Ignore 'rescued' message");
      return;
    }

  data = g_slice_new0 (ReceivedData);
  data->msg_id = msg_id;
  data->log_id = _tpl_create_message_token (channel_path, timestamp, msg_id);
  data->type = type;
  data->text = g_strdup (text);
  data->timestamp = timestamp;

  tp_conn = tp_channel_borrow_connection (TP_CHANNEL (tpl_text));
  remote = g_hash_table_lookup (tpl_text->priv->entities,
      GUINT_TO_POINTER (sender));

  if (remote == NULL)
    {
      /* Contact is not in the cache */
      tp_connection_get_contacts_by_handle (tp_conn, 1, &sender,
          G_N_ELEMENTS (features), features, on_received_signal_with_contact_cb,
          data, received_data_free, G_OBJECT (tpl_text));
    }
  else
    {
      keepon_on_receiving_signal (tpl_text, remote, data);
      received_data_free (data);
    }
}


static void
on_sent_signal_cb (TpChannel *proxy,
    guint tp_timestamp,
    guint type,
    const gchar *text,
    gpointer user_data,
    GObject *weak_object)
{
  GError *error = NULL;
  TplTextChannel *tpl_text = TPL_TEXT_CHANNEL (user_data);
  TplEntity *sender;
  TplEntity *receiver = NULL;
  TplTextEvent *text_log;
  TplLogManager *logmanager;
  TpAccount *account;
  const gchar *channel_path;
  gchar *log_id;
  gint64 timestamp = (gint64) tp_timestamp;

  g_return_if_fail (TPL_IS_TEXT_CHANNEL (tpl_text));

  channel_path = tp_proxy_get_object_path (TP_PROXY (tpl_text));
  log_id = _tpl_create_message_token (channel_path, timestamp,
      TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED);

  /* Initialize data for TplEntity */
  sender = tpl_text->priv->self;

  if (tpl_text->priv->is_chatroom)
    {
      receiver = tpl_text->priv->chatroom;

      DEBUG ("sent:\n\tlog_id=\"%s\"\n\tto "
          "chatroom=\"%s\"\n\tfrom=\"%s (%s)\"\n\tmsg=\"%s\"",
          log_id,
          tpl_entity_get_identifier (receiver),
          tpl_entity_get_identifier (sender),
          tpl_entity_get_alias (sender),
          text);
    }
  else
    {
      TpHandle handle = tp_channel_get_handle (TP_CHANNEL (tpl_text), NULL);

      receiver = g_hash_table_lookup (tpl_text->priv->entities,
          GUINT_TO_POINTER (handle));

      /* FIXME Create unkown entity when supported, this way we can survive
       * buggy connection managers */
      g_assert (receiver != NULL);

      DEBUG ("sent:\n\tlog_id=\"%s\"\n\tto=\"%s "
          "(%s)\"\n\tfrom=\"%s (%s)\"\n\tmsg=\"%s\"",
          log_id,
          tpl_entity_get_identifier (receiver),
          tpl_entity_get_alias (receiver),
          tpl_entity_get_identifier (sender),
          tpl_entity_get_alias (sender),
          text);
    }

  /* Initialise TplTextEvent */
  account = _tpl_channel_get_account (TPL_CHANNEL (tpl_text));
  text_log = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "channel-path", channel_path,
      "log-id", log_id,
      "receiver", receiver,
      "sender", sender,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", type,
      "message", text,
      "pending-msg-id", TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED,
      NULL);

  logmanager = tpl_log_manager_dup_singleton ();
  _tpl_log_manager_add_event (logmanager, TPL_EVENT (text_log), &error);

  if (error != NULL)
    {
      PATH_DEBUG (tpl_text, "LogStore: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (logmanager);
  g_object_unref (text_log);

  g_free (log_id);
}


static void
on_send_error_cb (TpChannel *proxy,
         guint error,
         guint timestamp,
         guint type,
         const gchar *text,
         gpointer user_data,
         GObject *weak_object)
{
  PATH_DEBUG (proxy, "unlogged event: TP was unable to send the message: %s",
      text);
  /* TODO log that the system was unable to send the message */
}


static void
on_lost_message_cb (TpChannel *proxy,
           gpointer user_data,
           GObject *weak_object)
{
  PATH_DEBUG (proxy, "lost message signal catched. nothing logged");
  /* TODO log that the system lost a message */
}


static void
pendingproc_connect_message_signals (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *tpl_text = _tpl_action_chain_get_object (ctx);
  TpChannel *channel = TP_CHANNEL (tpl_text);
  GError *error = NULL;

  tp_g_signal_connect_object (channel, "invalidated",
      G_CALLBACK (on_channel_invalidated_cb), tpl_text, 0);

  if (tp_cli_channel_type_text_connect_to_received (channel,
          on_received_signal_cb, NULL, NULL, NULL, &error) == NULL)
    goto disaster;

  if (tp_cli_channel_type_text_connect_to_sent (channel,
          on_sent_signal_cb, tpl_text, NULL, NULL, &error) == NULL)
    goto disaster;

  if (tp_cli_channel_type_text_connect_to_send_error (channel,
          on_send_error_cb, tpl_text, NULL, NULL, &error) == NULL)
    goto disaster;

  if (tp_cli_channel_type_text_connect_to_lost_message (channel,
          on_lost_message_cb, tpl_text, NULL, NULL, &error) == NULL)
    goto disaster;

  _tpl_action_chain_continue (ctx);
  return;

disaster:
  DEBUG ("couldn't connect to signals: %s", error->message);
  g_clear_error (&error);
  _tpl_action_chain_terminate (ctx);
}


static void
tpl_text_channel_call_when_ready (TplChannel *chan,
    GAsyncReadyCallback cb, gpointer user_data)
{
  TplActionChain *actions;

  /* first: connect signals, so none are lost
   * second: prepare all TplChannel
   * third: cache my contact and the remote one.
   * last: connect message signals
   *
   * If for any reason, the order is changed, it's needed to check what objects
   * are unreferenced by g_object_unref but used by a next action AND what
   * object are actually not prepared but used anyway */
  actions = _tpl_action_chain_new_async (G_OBJECT (chan), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_prepare_tpl_channel, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_my_contact, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_remote_contacts, NULL);
  _tpl_action_chain_append (actions, pendingproc_connect_message_signals, NULL);
  /* start the chain consuming */
  _tpl_action_chain_continue (actions);
}


static void
tpl_text_channel_dispose (GObject *obj)
{
  TplTextChannelPriv *priv = TPL_TEXT_CHANNEL (obj)->priv;

  tp_clear_object (&priv->chatroom);
  tp_clear_object (&priv->self);
  tp_clear_pointer (&priv->entities, g_hash_table_unref);

  G_OBJECT_CLASS (_tpl_text_channel_parent_class)->dispose (obj);
}


static void
tpl_text_channel_finalize (GObject *obj)
{
  PATH_DEBUG (obj, "finalizing channel %p", obj);

  G_OBJECT_CLASS (_tpl_text_channel_parent_class)->finalize (obj);
}


static void
_tpl_text_channel_class_init (TplTextChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplChannelClass *tpl_chan_class = TPL_CHANNEL_CLASS (klass);

  object_class->dispose = tpl_text_channel_dispose;
  object_class->finalize = tpl_text_channel_finalize;

  tpl_chan_class->call_when_ready = tpl_text_channel_call_when_ready;

  g_type_class_add_private (object_class, sizeof (TplTextChannelPriv));
}


static void
_tpl_text_channel_init (TplTextChannel *self)
{
  TplTextChannelPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_TEXT_CHANNEL, TplTextChannelPriv);

  self->priv = priv;

  self->priv->entities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_object_unref);
}


/**
 * _tpl_text_channel_new
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @account: TpAccount instance, related to the new #TplTextChannel
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Channel Text proxy.
 * The returned #TplTextChannel is not guaranteed to be ready at the point of
 * return.
 *
 * TplTextChannel is actually a subclass of the abstract TplChannel which is a
 * subclass of TpChannel.
 * Use #TpChannel methods, casting the #TplTextChannel instance to a
 * TpChannel, to access TpChannel data/methods from it.
 *
 * TplTextChannel is usually created using #tpl_channel_factory_build, from
 * within a #TplObserver singleton, when its Observer_Channel method is called
 * by the Channel Dispatcher.
 *
 * Returns: the TplTextChannel instance or %NULL if @object_path is not valid
 */
TplTextChannel *
_tpl_text_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);

  /* Do what tpl_channel_new does + set TplTextChannel specific properties */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TPL_TYPE_TEXT_CHANNEL,
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
