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
connection_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      _tpl_action_chain_terminate (ctx, error);
      g_error_free (error);
      return;
    }

  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_prepare_tp_connection (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *chan = _tpl_action_chain_get_object (ctx);
  TpConnection *conn = tp_channel_borrow_connection (TP_CHANNEL (chan));
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (conn, conn_features, connection_prepared_cb, ctx);
}


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

  /* user_data is a TplChannel instance */
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
      _tpl_action_chain_terminate (ctx, error);
    }
  else if (n_failed > 0)
    {
      error = g_error_new (TPL_TEXT_CHANNEL_ERROR,
          TPL_TEXT_CHANNEL_ERROR_FAILED,
          "Failed to prepare remote contact.");
      _tpl_action_chain_terminate (ctx, error);
    }
  else
    {
      self->priv->remote =
        tpl_entity_new_from_tp_contact (contacts[1], TPL_ENTITY_CONTACT);
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

      g_array_free (arr, TRUE);
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
  TplObserver *observer = _tpl_observer_new ();

  PATH_DEBUG (tpl_chan, "%s #%d %s",
      g_quark_to_string (domain), code, message);

  if (!_tpl_observer_unregister_channel (observer, tpl_chan))
    PATH_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
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
  timestamp = tp_message_get_sent_timestamp (message);

  if (timestamp == 0)
    timestamp = tp_message_get_received_timestamp (message);

  if (timestamp == 0)
    {
      GDateTime *datetime = g_date_time_new_now_utc ();
      timestamp = g_date_time_to_unix (datetime);
      g_date_time_unref (datetime);
    }

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
pendingproc_store_pending_messages (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);
  GList *pending_messages;
  GList *it;

  pending_messages =
    tp_text_channel_get_pending_messages (TP_TEXT_CHANNEL (self));

  for (it = pending_messages; it != NULL; it = g_list_next (it))
      on_message_received_cb (TP_TEXT_CHANNEL (self),
          TP_SIGNALLED_MESSAGE (it->data), self);

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
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_connection, NULL);
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
  TplTextChannel *self;

  /* Do what tpl_channel_new does + set TplTextChannel specific */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
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

  self->priv->account = g_object_ref (account);

  return self;
}
