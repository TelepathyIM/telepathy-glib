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
#include <telepathy-glib/telepathy-glib.h>

#include "action-chain-internal.h"
#include "channel-internal.h"
#include "entity-internal.h"
#include "event-internal.h"
#include "log-manager-internal.h"
#include "log-store-sqlite-internal.h"
#include "observer-internal.h"
#include "text-event.h"
#include "text-event-internal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include "debug-internal.h"

struct _TplTextChannelPriv
{
  TpAccount *account;
  TplEntity *self;
  gboolean is_chatroom;
  TplEntity *remote;
};

static TpContactFeature features[3] = {
  TP_CONTACT_FEATURE_ALIAS,
  TP_CONTACT_FEATURE_PRESENCE,
  TP_CONTACT_FEATURE_AVATAR_TOKEN
};

static void tpl_text_channel_iface_init (TplChannelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TplTextChannel, _tpl_text_channel,
    TP_TYPE_TEXT_CHANNEL,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_CHANNEL, tpl_text_channel_iface_init))


static void
channel_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer ctx)
{
  TplChannel *chan = _tpl_action_chain_get_object (ctx);
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      _tpl_action_chain_terminate (ctx, error);
      g_error_free (error);
      return;
    }
  else if (!tp_proxy_has_interface_by_id (TP_PROXY (chan),
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES))
    {
      error = g_error_new (TPL_TEXT_CHANNEL_ERROR,
          TPL_TEXT_CHANNEL_ERROR_NEED_MESSAGE_INTERFACE,
          "The text channel does not implement Message interface.");
      _tpl_action_chain_terminate (ctx, error);
      g_error_free (error);
      return;
    }

  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_prepare_tp_text_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *chan = _tpl_action_chain_get_object (ctx);
  GQuark chan_features[] = {
      TP_CHANNEL_FEATURE_CORE,
      TP_CHANNEL_FEATURE_GROUP,
      TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES,
      0
  };

  tp_proxy_prepare_async (chan, chan_features, channel_prepared_cb, ctx);
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
      GError *new_error = NULL;

      conn_path = tp_proxy_get_object_path (TP_PROXY (tp_conn));

      new_error = g_error_new (error->domain, error->code,
          "Error resolving self handle for connection %s: %s)",
          conn_path, error->message);

      _tpl_action_chain_terminate (ctx, new_error);
      g_error_free (new_error);
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
get_remote_contact_cb (TpConnection *connection,
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

  if (error != NULL)
    {
      GError *new_error = NULL;
      new_error = g_error_new (error->domain, error->code,
          "Failed to get remote contact: %s", error->message);
      _tpl_action_chain_terminate (ctx, new_error);
      g_error_free (new_error);
    }
  else if (n_failed > 0)
    {
      GError *new_error = g_error_new (TPL_TEXT_CHANNEL_ERROR,
          TPL_TEXT_CHANNEL_ERROR_FAILED,
          "Failed to prepare remote contact.");
      _tpl_action_chain_terminate (ctx, new_error);
      g_error_free (new_error);
    }
  else
    {
      self->priv->remote =
        tpl_entity_new_from_tp_contact (contacts[0], TPL_ENTITY_CONTACT);
      _tpl_action_chain_continue (ctx);
    }
}


static void
pendingproc_get_remote_contact (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);
  TpChannel *chan = TP_CHANNEL (self);
  TpHandle handle;
  TpHandleType handle_type;

  handle = tp_channel_get_handle (chan, &handle_type);

  if (handle_type == TP_HANDLE_TYPE_ROOM)
    {
      self->priv->is_chatroom = TRUE;
      self->priv->remote =
        tpl_entity_new_from_room_id (tp_channel_get_identifier (chan));

      PATH_DEBUG (self, "Chatroom id: %s",
          tpl_entity_get_identifier (self->priv->remote));

      _tpl_action_chain_continue (ctx);
    }
  else
    {
      TpConnection *tp_conn = tp_channel_borrow_connection (chan);
      GArray *arr;

      /* Get the contact of the TargetHandle */
      arr = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
      handle = tp_channel_get_handle (chan, NULL);

      g_array_append_val (arr, handle);

      tp_connection_get_contacts_by_handle (tp_conn,
          arr->len, (TpHandle *) arr->data,
          G_N_ELEMENTS (features), features, get_remote_contact_cb, ctx, NULL,
          G_OBJECT (self));

      g_array_unref (arr);
    }
}


static void
on_channel_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TplChannel *tpl_chan = TPL_CHANNEL (user_data);
  TplObserver *observer = _tpl_observer_dup (NULL);

  g_return_if_fail (observer);

  PATH_DEBUG (tpl_chan, "%s #%d %s",
      g_quark_to_string (domain), code, message);

  if (!_tpl_observer_unregister_channel (observer, tpl_chan))
    PATH_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static guint
get_message_pending_id (TpMessage *m)
{
  return tp_asv_get_uint32 (tp_message_peek (TP_MESSAGE (m), 0),
      "pending-message-id", NULL);
}


static gint64
get_original_message_timestamp (TpMessage *message)
{
  gint64 timestamp;

  timestamp = tp_asv_get_int64 (tp_message_peek (message, 0),
      "original-message-sent", NULL);

  if (timestamp == 0)
    timestamp = tp_asv_get_int64 (tp_message_peek (message, 0),
        "original-message-received", NULL);

  return timestamp;
}


static gint64
get_network_timestamp (TpMessage *message)
{
  GDateTime *datetime = g_date_time_new_now_utc ();
  gint64 now = g_date_time_to_unix (datetime);
  gint64 timestamp;

  timestamp = tp_message_get_sent_timestamp (message);

  if (timestamp == 0)
    timestamp = tp_message_get_received_timestamp (message);

  if (timestamp == 0)
    {
      DEBUG ("TpMessage is not timestamped. Using current time instead.");
      timestamp = now;
    }

  if (timestamp - now > 60 * 60)
    DEBUG ("timestamp is more than an hour in the future.");
  else  if (now - timestamp > 60 * 60)
    DEBUG ("timestamp is more than an hour in the past.");

  g_date_time_unref (datetime);

  return timestamp;
}


static gint64
get_message_edit_timestamp (TpMessage *message)
{
  if (tp_message_get_supersedes (message) != NULL)
    return get_network_timestamp (message);
  else
    return 0;
}


static gint64
get_message_timestamp (TpMessage *message)
{
  gint64 timestamp;

  timestamp = get_original_message_timestamp (message);

  if (timestamp == 0)
    timestamp = get_network_timestamp (message);

  return timestamp;
}


static void
tpl_text_channel_store_message (TplTextChannel *self,
    TpMessage *message,
    TplEntity *sender,
    TplEntity *receiver)
{
  TplTextChannelPriv *priv = self->priv;
  const gchar *direction;
  TpChannelTextMessageType type;
  gint64 timestamp;
  gchar *text;
  TplTextEvent *event;
  TplLogManager *logmanager;
  GError *error = NULL;

  if (tpl_entity_get_entity_type (sender) == TPL_ENTITY_SELF)
    direction = "sent";
  else
    direction = "received";

  if (tp_message_is_scrollback (message))
    {
      DEBUG ("Ignoring %s scrollback message.", direction);
      return;
    }

  if (tp_message_is_rescued (message))
    {
      DEBUG ("Ignoring %s rescued message.", direction);
      return;
    }

  type = tp_message_get_message_type (message);

  if (type == TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT)
    {
      DEBUG ("Ignoring %s delivery report message.", direction);
      return;
    }

  /* Ensure timestamp */
  timestamp = get_message_timestamp (message);

  text = tp_message_to_text (message, NULL);

  if (text == NULL)
    {
      DEBUG ("Ignoring %s message with no supported content", direction);
      return;
    }

  if (tpl_entity_get_entity_type (sender) == TPL_ENTITY_SELF)
    DEBUG ("Logging message sent to %s (%s)",
        tpl_entity_get_alias (receiver),
        tpl_entity_get_identifier (receiver));
  else
    DEBUG ("Logging message received from %s (%s)",
        tpl_entity_get_alias (sender),
        tpl_entity_get_identifier (sender));


  /* Initialise TplTextEvent */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", priv->account,
      "channel-path", tp_proxy_get_object_path (TP_PROXY (self)),
      "receiver", receiver,
      "sender", sender,
      "timestamp", timestamp,
      "message-token", tp_message_get_token (message),
      "supersedes-token", tp_message_get_supersedes (message),
      "edit-timestamp", get_message_edit_timestamp (message),
      /* TplTextEvent */
      "message-type", type,
      "message", text,
      NULL);

  /* Store sent event */
  logmanager = tpl_log_manager_dup_singleton ();
  _tpl_log_manager_add_event (logmanager, TPL_EVENT (event), &error);

  if (error != NULL)
    {
      PATH_DEBUG (self, "LogStore: %s", error->message);
      g_error_free (error);
    }
  else if (tpl_entity_get_entity_type (sender) != TPL_ENTITY_SELF)
    {
      TplLogStore *cache = _tpl_log_store_sqlite_dup ();
      _tpl_log_store_sqlite_add_pending_message (cache,
          TP_CHANNEL (self),
          get_message_pending_id (message),
          timestamp,
          &error);

      if (error != NULL)
        {
          PATH_DEBUG (self, "Failed to cache pending message: %s",
              error->message);
          g_error_free (error);
        }
    }

  g_object_unref (logmanager);
  g_object_unref (event);
  g_free (text);
}


static void
on_message_received_cb (TpTextChannel *text_chan,
    TpSignalledMessage *message,
    gpointer user_data)
{
  TplTextChannel *self = TPL_TEXT_CHANNEL (text_chan);
  TplTextChannelPriv *priv = self->priv;
  TplEntity *receiver;
  TplEntity *sender;

  if (priv->is_chatroom)
    receiver = priv->remote;
  else
    receiver = priv->self;

  sender = tpl_entity_new_from_tp_contact (
      tp_signalled_message_get_sender (TP_MESSAGE (message)),
      TPL_ENTITY_CONTACT);

  tpl_text_channel_store_message (self, TP_MESSAGE (message),
      sender, receiver);

  g_object_unref (sender);
}


static void
on_message_sent_cb (TpChannel *proxy,
    TpSignalledMessage *message,
    guint flags,
    const gchar *token,
    gpointer user_data,
    GObject *weak_object)
{
  TplTextChannel *self = TPL_TEXT_CHANNEL (proxy);
  TplTextChannelPriv *priv = self->priv;
  TplEntity *sender;
  TplEntity *receiver = priv->remote;

  if (tp_signalled_message_get_sender (TP_MESSAGE (message)) != NULL)
    sender = tpl_entity_new_from_tp_contact (
        tp_signalled_message_get_sender (TP_MESSAGE (message)),
        TPL_ENTITY_SELF);
  else
    sender = g_object_ref (priv->self);

  tpl_text_channel_store_message (self, TP_MESSAGE (message),
      sender, receiver);

  g_object_unref (sender);
}


static void
on_pending_message_removed_cb (TpTextChannel *self,
    TpSignalledMessage *message,
    gpointer user_data)
{
  TplLogStore *cache;
  GList *ids = NULL;
  GError *error = NULL;

  ids = g_list_prepend (ids,
      GUINT_TO_POINTER (get_message_pending_id (TP_MESSAGE (message))));

  cache = _tpl_log_store_sqlite_dup ();
  _tpl_log_store_sqlite_remove_pending_messages (cache, TP_CHANNEL (self),
      ids, &error);

  if (error != NULL)
    {
      PATH_DEBUG (self, "Failed to remove pending message from cache: %s",
          error->message);
      g_error_free (error);
    }

  g_object_unref (cache);
}


static gint
pending_message_compare_id (TpSignalledMessage *m1,
    TpSignalledMessage *m2)
{
  guint id1, id2;

  id1 = get_message_pending_id (TP_MESSAGE (m1));
  id2 = get_message_pending_id (TP_MESSAGE (m2));

  if (id1 > id2)
    return 1;
  else if (id1 < id2)
    return -1;
  else
    return 0;
}


static gint
pending_message_compare_timestamp (TpSignalledMessage *m1,
    TpSignalledMessage *m2)
{
  gint64 ts1, ts2;

  ts1 = get_message_timestamp (TP_MESSAGE (m1));
  ts2 = get_message_timestamp (TP_MESSAGE (m2));

  if (ts1 > ts2)
    return 1;
  else if (ts1 < ts2)
    return -1;
  else
    return 0;
}


static void
pendingproc_store_pending_messages (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);
  TplLogStore *cache;
  GError *error = NULL;
  GList *cached_messages;
  GList *pending_messages;
  GList *cached_it, *pending_it;
  GList *to_remove = NULL;
  GList *to_log = NULL;

  cache = _tpl_log_store_sqlite_dup ();
  cached_messages = _tpl_log_store_sqlite_get_pending_messages (cache,
      TP_CHANNEL (self), &error);

  if (error != NULL)
    {
      DEBUG ("Failed to read pending_message cache: %s.", error->message);
      g_error_free (error);
      /* We simply ignore this error, as if the list was empty */
    }

  pending_messages =
    tp_text_channel_get_pending_messages (TP_TEXT_CHANNEL (self));

  pending_messages = g_list_sort (pending_messages,
      (GCompareFunc) pending_message_compare_id);

  cached_it = cached_messages;
  pending_it = pending_messages;

  while (cached_it != NULL || pending_it != NULL)
    {
      TplPendingMessage *cached;
      TpSignalledMessage *pending;
      guint pending_id;
      gint64 pending_ts;

      if (cached_it == NULL)
        {
          /* No more cached pending, just log the pending messages */
          to_log = g_list_prepend (to_log, pending_it->data);
          pending_it = g_list_next (pending_it);
          continue;
        }

      cached = cached_it->data;

      if (pending_it == NULL)
        {
          /* No more pending, just remove the cached messages */
          to_remove = g_list_prepend (to_remove, GUINT_TO_POINTER (cached->id));
          cached_it = g_list_next (cached_it);
          continue;
        }

      pending = pending_it->data;
      pending_id = get_message_pending_id (TP_MESSAGE (pending));
      pending_ts = get_message_timestamp (TP_MESSAGE (pending));

      if (cached->id == pending_id)
        {
          if (cached->timestamp != pending_ts)
            {
              /* The cache messaged is invalid, remove it */
              to_remove = g_list_prepend (to_remove,
                  GUINT_TO_POINTER (cached->id));
              cached_it = g_list_next (cached_it);
            }
          else
            {
              /* The message is already logged */
              cached_it = g_list_next (cached_it);
              pending_it = g_list_next (pending_it);
            }
        }
      else if (cached->id < pending_id)
        {
          /* The cached ID is not valid anymore, remove it */
          to_remove = g_list_prepend (to_remove, GUINT_TO_POINTER (cached->id));
          cached_it = g_list_next (cached_it);
        }
      else
        {
          /* The pending message has not been logged */
          to_log = g_list_prepend (to_log, pending);
          pending_it = g_list_next (pending_it);
        }
    }

  g_list_foreach (cached_messages, (GFunc) g_free, NULL);
  g_list_free (cached_messages);
  g_list_free (pending_messages);


  /* We need to remove before we log to avoid collisions */
  if (to_remove != NULL)
    {
      if (!_tpl_log_store_sqlite_remove_pending_messages (cache,
            TP_CHANNEL (self), to_remove, &error))
        {
          DEBUG ("Failed remove old pending messages from cache: %s", error->message);
          g_error_free (error);
        }
      g_list_free (to_remove);
    }

  if (to_log != NULL)
    {
      GList *it;

      to_log = g_list_sort (to_log,
          (GCompareFunc) pending_message_compare_timestamp);

      for (it = to_log; it != NULL; it = g_list_next (it))
        on_message_received_cb (TP_TEXT_CHANNEL (self),
            TP_SIGNALLED_MESSAGE (it->data), self);

      g_list_free (to_log);
    }

  g_object_unref (cache);
  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_connect_message_signals (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);

  tp_g_signal_connect_object (self, "invalidated",
      G_CALLBACK (on_channel_invalidated_cb), self, 0);

  tp_g_signal_connect_object (self, "message-received",
      G_CALLBACK (on_message_received_cb), self, 0);

  tp_g_signal_connect_object (self, "message-sent",
      G_CALLBACK (on_message_sent_cb), self, 0);

  tp_g_signal_connect_object (self, "pending-message-removed",
      G_CALLBACK (on_pending_message_removed_cb), self, 0);

  _tpl_action_chain_continue (ctx);
}

static gboolean
tpl_text_channel_prepare_finish (TplChannel *chan,
    GAsyncResult *result,
    GError **error)
{
  return _tpl_action_chain_new_finish (G_OBJECT (chan), result, error);
}


static void
tpl_text_channel_prepare_async (TplChannel *chan,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *actions;

  actions = _tpl_action_chain_new_async (G_OBJECT (chan), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_text_channel, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_my_contact, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_remote_contact, NULL);
  _tpl_action_chain_append (actions, pendingproc_store_pending_messages, NULL);
  _tpl_action_chain_append (actions, pendingproc_connect_message_signals, NULL);

  _tpl_action_chain_continue (actions);
}


static void
tpl_text_channel_dispose (GObject *obj)
{
  TplTextChannelPriv *priv = TPL_TEXT_CHANNEL (obj)->priv;

  tp_clear_object (&priv->remote);
  tp_clear_object (&priv->self);

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

  object_class->dispose = tpl_text_channel_dispose;
  object_class->finalize = tpl_text_channel_finalize;

  g_type_class_add_private (object_class, sizeof (TplTextChannelPriv));
}


static void
tpl_text_channel_iface_init (TplChannelInterface *iface)
{
  iface->prepare_async = tpl_text_channel_prepare_async;
  iface->prepare_finish = tpl_text_channel_prepare_finish;
}


static void
_tpl_text_channel_init (TplTextChannel *self)
{
  TplTextChannelPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_TEXT_CHANNEL, TplTextChannelPriv);

  self->priv = priv;
}


/**
 * _tpl_text_channel_new:
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
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
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);
  TplTextChannel *self;

  /* Do what tpl_channel_new does + set TplTextChannel specific */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  self = g_object_new (TPL_TYPE_TEXT_CHANNEL,
      /* TpChannel properties */
      "connection", conn,
      "dbus-daemon", conn_proxy->dbus_daemon,
      "bus-name", conn_proxy->bus_name,
      "object-path", object_path,
      "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
      "channel-properties", tp_chan_props,
      NULL);

  self->priv->account = g_object_ref (tp_connection_get_account (conn));

  return self;
}
