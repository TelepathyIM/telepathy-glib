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
#include "log-store-sqlite-internal.h"
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
  GArray *arr;

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
      TpHandle handle;

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
pendingproc_get_room_info (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *tpl_text = _tpl_action_chain_get_object (ctx);
  TpHandleType handle_type;
  TpChannel *chan = TP_CHANNEL (tpl_text);

  tp_channel_get_handle (chan, &handle_type);
  if (handle_type != TP_HANDLE_TYPE_ROOM)
    goto out;

  tpl_text->priv->is_chatroom = TRUE;
  tpl_text->priv->chatroom =
    tpl_entity_new_from_room_id (tp_channel_get_identifier (chan));

  PATH_DEBUG (tpl_text, "Chatroom id: %s",
      tpl_entity_get_identifier (tpl_text->priv->chatroom));

out:
  _tpl_action_chain_continue (ctx);
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
  TplLogStore *index = _tpl_log_store_sqlite_dup ();
  const gchar *channel_path = tp_proxy_get_object_path (TP_PROXY (tpl_text));
  gchar *log_id;
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

  /* Check if log_id has already been logged
   *
   * FIXME: There is a race condition for which, right after a 'NewChannel'
   * signal is raised and a message is received, the 'received' signal handler
   * may be cateched before or being slower and arriving after the TplChannel
   * preparation (in which pending message list is examined)
   *
   * Workaround:
   * In the first case the analisys of P.M.L will detect that actually the
   * handler has already received and logged the message.
   * In the latter (here), the handler will detect that the P.M.L analisys
   * has found and logged it, returning immediatly */
  log_id = _tpl_create_message_token (channel_path, timestamp, msg_id);
  if (_tpl_log_store_sqlite_log_id_is_present (index, log_id))
    {
      PATH_DEBUG (tpl_text, "%s found, not logging", log_id);
      g_free (log_id);
      goto out;
    }

  data = g_slice_new0 (ReceivedData);
  data->msg_id = msg_id;
  data->log_id = log_id;
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

out:
  g_object_unref (index);
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


/* Signal's Callbacks */
static void
on_pending_messages_removed_cb (TpChannel *proxy,
    const GArray *message_ids,
    gpointer user_data,
    GObject *weak_object)
{
  TplLogStore *cache = _tpl_log_store_sqlite_dup ();
  guint i;
  GError *error = NULL;

  for (i = 0; i < message_ids->len; ++i)
    {
      guint msg_id = g_array_index (message_ids, guint, i);
      _tpl_log_store_sqlite_set_acknowledgment_by_msg_id (cache, proxy, msg_id,
          &error);
      if (error != NULL)
        {
          PATH_DEBUG (proxy, "cannot set the ACK flag for msg_id %u: %s",
              msg_id, error->message);
          g_clear_error (&error);
        }
      else
        {
          PATH_DEBUG (proxy, "msg_id %d acknowledged", msg_id);
        }
    }

  if (cache != NULL)
    g_object_unref (cache);
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

  if (tp_proxy_has_interface_by_id (tpl_text,
          TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES) &&
      tp_cli_channel_interface_messages_connect_to_pending_messages_removed (
          channel, on_pending_messages_removed_cb, NULL, NULL,
          G_OBJECT (tpl_text), &error) == NULL)
   goto disaster;

  _tpl_action_chain_continue (ctx);
  return;

disaster:
  DEBUG ("couldn't connect to signals: %s", error->message);
  g_clear_error (&error);
  _tpl_action_chain_terminate (ctx);
}


/* Clean up passed messages (GList of tokens), which are known to be stale,
 * setting them acknowledged in SQLite */
static void
tpl_text_channel_clean_up_stale_tokens (TplTextChannel *self,
    GList *stale_tokens)
{
  TplLogStore *cache = _tpl_log_store_sqlite_dup ();
  GError *loc_error = NULL;

  for (; stale_tokens != NULL; stale_tokens = g_list_next (stale_tokens))
    {
      gchar *log_id = stale_tokens->data;

      _tpl_log_store_sqlite_set_acknowledgment (cache, log_id, &loc_error);

      if (loc_error != NULL)
        {
          PATH_CRITICAL (self, "Unable to set %s as acknoledged in "
              "TPL DB: %s", log_id, loc_error->message);
          g_clear_error (&loc_error);
        }
    }

  if (cache != NULL)
    g_object_unref (cache);
}


/* PendingMessages CB for Message interface */
static void
got_message_pending_messages_cb (TpProxy *proxy,
    const GValue *out_Value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  const gchar *channel_path = tp_proxy_get_object_path (proxy);
  TplLogStore *cache = _tpl_log_store_sqlite_dup ();
  TplActionChain *ctx = user_data;
  GPtrArray *result = NULL;
  GList *cached_pending_msgs = NULL;
  GError *loc_error = NULL;
  guint i;

  if (!TPL_IS_TEXT_CHANNEL (proxy))
    {
      CRITICAL ("Passed proxy is not a proper TplTextChannel");
      goto out;
    }

  if (!TPL_IS_TEXT_CHANNEL (weak_object))
    {
      CRITICAL ("Passed weak_object is not a proper TplTextChannel");
      goto out;
    }

  if (error != NULL)
    {
      PATH_CRITICAL (weak_object, "retrieving messages for Message iface: %s",
          error->message);
      goto out;
    }

  /* It's aaa{vs}, a list of message each containing a list of message's parts
   * each containing a dictioanry k:v */
  result = g_value_get_boxed (out_Value);

  /* getting messages ids known to be pending at last TPL exit */
  cached_pending_msgs = _tpl_log_store_sqlite_get_pending_messages (cache,
      TP_CHANNEL (proxy), &loc_error);
  if (loc_error != NULL)
    {
      CRITICAL ("Unable to obtain pending messages stored in TPL DB: %s",
          loc_error->message);
      goto out;
    }

  /* cycle the list of messages */
  if (result->len > 0)
    PATH_DEBUG (proxy, "Checking if there are any un-logged messages among "
        "%d pending messages", result->len);
  for (i = 0; i < result->len; ++i)
    {
      GPtrArray *message_parts;
      GHashTable *message_headers; /* string:gvalue */
      GHashTable *message_part; /* string:gvalue */
      GList *l;
      const gchar *message_token;
      gchar *tpl_message_token;
      gint64 message_timestamp;
      guint message_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
      guint message_flags = 0;
      guint message_id;
      TpHandle message_sender_handle;
      const gchar *message_body;
      gboolean valid;

      /* list of message's parts */
      message_parts = g_ptr_array_index (result, i);
      /* message part 0 is the message's headers */
      message_headers = g_ptr_array_index (message_parts, 0);
      /* message part 1 is is the first part, the most 'faithful' among
       * alternatives.
       * TODO fully support alternatives and attachments/images
       * related to them */
      message_part = g_ptr_array_index (message_parts, 1);
      message_token = tp_asv_get_string (message_headers, "message-token");
      message_id = tp_asv_get_uint32 (message_headers, "pending-message-id",
          &valid);
      if (!valid)
        {
          DEBUG ("pending-message-id not in a valid range, setting to "
              "UNKNOWN");
            message_id = TPL_TEXT_EVENT_MSG_ID_UNKNOWN;
        }
      message_timestamp = tp_asv_get_int64 (message_headers,
          "message-received", NULL);

      tpl_message_token = _tpl_create_message_token (channel_path,
          message_timestamp, message_id);

      message_sender_handle = tp_asv_get_uint32 (message_headers,
          "message-sender", NULL);

      message_type = tp_asv_get_uint32 (message_headers, "message-type",
          &valid);
      if (!valid)
        {
          PATH_DEBUG (proxy, "message-type not in a valid range, falling "
              "back to type=NORMAL");
          message_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
        }

      if (tp_asv_get_boolean (message_headers, "rescued", &valid) && valid)
        message_flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_RESCUED;

      if (tp_asv_get_boolean (message_headers, "scrollback", NULL) && valid)
        message_flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_SCROLLBACK;

      message_body = tp_asv_get_string (message_part, "content");

      /* log only log-ids not in cached_pending_msgs -> not already logged */
      l = g_list_find_custom (cached_pending_msgs, tpl_message_token,
          (GCompareFunc) g_strcmp0);

      if (l == NULL)
        {
          /* call the received signal callback to trigger the message storing */
          /* FIXME Avoid converting gint64 timestamp into guint timestamp */
          on_received_signal_cb (TP_CHANNEL (proxy),
              message_id, (guint) message_timestamp, message_sender_handle,
              message_type, message_flags, message_body,
              NULL, NULL);
        }
      else
        {
          /* the message has been already logged, remove it from the list so
           * that, in the end of the loop, the items still in
           * cached_pending_msgs can be considered stale */
          g_free (l->data);
          cached_pending_msgs = g_list_delete_link (cached_pending_msgs, l);
        }

      g_free (tpl_message_token);
    }

  /* At this point all remaining elements of cached_pending_msgs are those
   * that the TplLogStoreSqlite knew as pending but currently not
   * listed as such in the current pending message list -> stale */
  tpl_text_channel_clean_up_stale_tokens (TPL_TEXT_CHANNEL (proxy),
      cached_pending_msgs);
  while (cached_pending_msgs != NULL)
    {
      PATH_DEBUG (proxy, "%s is stale, removed from DB",
          (gchar *) cached_pending_msgs->data);

      g_free (cached_pending_msgs->data);
      cached_pending_msgs = g_list_delete_link (cached_pending_msgs,
          cached_pending_msgs);
    }

out:
  if (cache != NULL)
    g_object_unref (cache);

  if (loc_error != NULL)
      g_error_free (loc_error);

/* If an error occured, do not terminate(), just have it logged.
 * terminate() would be fatal for TplChannel preparation,
 * but in this case it would just mean that it couldn't retrieve pending
 * messages, but it might still log the rest. If the next operation in chain
 * fails, it's fatal. Partial data loss is better than total data loss */
  _tpl_action_chain_continue (ctx);
}


/* PendingMessages CB for Text interface */
static void
got_text_pending_messages_cb (TpChannel *proxy,
    const GPtrArray *result,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TplLogStore *cache = _tpl_log_store_sqlite_dup ();
  TplActionChain *ctx = user_data;
  GList *cached_pending_msgs, *l;
  const gchar *channel_path;
  GError *loc_error = NULL;
  guint i;

  if (error != NULL)
    {
      PATH_CRITICAL (proxy, "retrieving pending messages for Text iface: %s",
          error->message);
      _tpl_action_chain_terminate (ctx);
      return;
    }

  channel_path = tp_proxy_get_object_path (proxy);

  /* getting messages ids known to be pending at last TPL exit */
  cached_pending_msgs = _tpl_log_store_sqlite_get_pending_messages (cache,
      TP_CHANNEL (proxy), &loc_error);

  if (loc_error != NULL)
    {
      PATH_CRITICAL (proxy,
          "Unable to obtain pending messages stored in TPL DB: %s",
          loc_error->message);
      _tpl_action_chain_terminate (ctx);

      return;
    }

  PATH_DEBUG (proxy, "%d pending message(s) for Text iface", result->len);
  for (i = 0; i < result->len; ++i)
    {
      GValueArray *message_struct;
      const gchar *message_body;
      gchar *tpl_message_token;
      guint message_id;
      guint message_timestamp;
      guint from_handle;
      guint message_type;
      guint message_flags;

      message_struct = g_ptr_array_index (result, i);

      tp_value_array_unpack (message_struct, 6,
          &message_id,
          &message_timestamp,
          &from_handle,
          &message_type,
          &message_flags,
          &message_body);

      tpl_message_token = _tpl_create_message_token (channel_path,
          message_timestamp, message_id);

      /* log only log-ids not in cached_pending_msgs -> not already logged */
      l = g_list_find_custom (cached_pending_msgs, tpl_message_token,
          (GCompareFunc) g_strcmp0);

      if (l == NULL)
        {
          /* call the received signal callback to trigger the message storing */
          on_received_signal_cb (proxy, message_id, message_timestamp,
              from_handle, message_type, message_flags, message_body,
              NULL, NULL);
        }
      else
        {
          /* the message has been already logged, remove it from the list so
           * that, in the end of the loop, the items still in
           * cached_pending_msgs can be considered stale */
          g_free (l->data);
          cached_pending_msgs = g_list_delete_link (cached_pending_msgs, l);
        }

      g_free (tpl_message_token);
    }

  /* At this point all remaining elements of cached_pending_msgs are those
   * that the TplLogStoreSqlite knew as pending but currently not
   * listed as such in the current pending message list -> stale */
  tpl_text_channel_clean_up_stale_tokens (TPL_TEXT_CHANNEL (proxy),
      cached_pending_msgs);
  while (cached_pending_msgs != NULL)
    {
      PATH_DEBUG (proxy, "%s is stale, removed from DB",
          (gchar *) cached_pending_msgs->data);

      g_free (cached_pending_msgs->data);
      cached_pending_msgs = g_list_delete_link (cached_pending_msgs,
          cached_pending_msgs);
    }

  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_get_pending_messages (TplActionChain *ctx,
    gpointer user_data)
{
  TplTextChannel *chan_text = _tpl_action_chain_get_object (ctx);

  if (tp_proxy_has_interface_by_id (chan_text,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES))
    tp_cli_dbus_properties_call_get (chan_text, -1,
        TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "PendingMessages",
        got_message_pending_messages_cb, ctx, NULL,
        G_OBJECT (chan_text));
  else
    tp_cli_channel_type_text_call_list_pending_messages (TP_CHANNEL (chan_text),
        -1, FALSE, got_text_pending_messages_cb, ctx, NULL, NULL);
}


/* Cleans up stale log-ids in the index logstore.
 *
 * It 'brutally' considers as stale all log-ids which timestamp is older than
 * <time_limit> days AND are still not set as acknowledged.
 *
 * NOTE: While retrieving open channels, a partial clean-up for the channel's
 * stale pending messages is done. It's not enough, since it doesn't consider
 * all the channels being closed at retrieval time. This function tries to
 * catch stale ids in the rest of the DB, heuristically.
 *
 * It is wrong to consider all the log-ids not having an channel currently
 * open as stale, since a channel might be temporarely disconnected and
 * reconnected and some protocols might repropose not acknowledged messages on
 * reconnection. We need to consider only reasonably old log-ids.
 *
 * This function is meant only to reduce the size of the DB used for indexing.
 *
 * No _tpl_action_chain_terminate() is called if some fatal error occurs since
 * it's not considered a crucial point for TplChannel preparation.
 */
#if 0
static void
pendingproc_cleanup_pending_messages_db (TplActionChain *ctx,
    gpointer user_data)
{
  /* FIXME: https://bugs.freedesktop.org/show_bug.cgi?id=28791 */
  /* five days ago in seconds */
  TplTextChannel *self = _tpl_action_chain_get_object (ctx);
  const time_t time_limit = _tpl_time_get_current () -
    TPL_LOG_STORE_SQLITE_CLEANUP_DELTA_LIMIT;
  TplLogStore *cache = _tpl_log_store_sqlite_dup ();
  GList *l;
  GError *error = NULL;

  if (cache == NULL)
    {
      DEBUG ("Unable to obtain the TplLogStoreIndex singleton");
      goto out;
    }

  l = _tpl_log_store_sqlite_get_log_ids (cache, NULL, time_limit,
      &error);
  if (error != NULL)
    {
      DEBUG ("unable to obtain log-id in Index DB: %s", error->message);
      g_error_free (error);
      /* do not call _tpl_action_chain_terminate, if it's temporary next startup
       * TPL will re-do the clean-up. If it's fatal, the flow will stop later
       * anyway */
      goto out;
    }

  if (l != NULL)
    PATH_DEBUG (self, "Cleaning up stale messages");
  tpl_text_channel_clean_up_stale_tokens (self, l);
  while (l != NULL)
    {
      PATH_DEBUG (self, "%s is stale, removed from DB", (gchar *) l->data);
      g_free (l->data);
      l = g_list_delete_link (l, l);
    }

out:
  if (cache != NULL)
    g_object_unref (cache);

  _tpl_action_chain_continue (ctx);
}
#endif


static void
tpl_text_channel_call_when_ready (TplChannel *chan,
    GAsyncReadyCallback cb, gpointer user_data)
{
  TplActionChain *actions;

  /* first: connect signals, so none are lost
   * second: prepare all TplChannel
   * third: cache my contact and the remote one.
   * last: check for pending messages
   *
   * If for any reason, the order is changed, it's needed to check what objects
   * are unreferenced by g_object_unref but used by a next action AND what
   * object are actually not prepared but used anyway */
  actions = _tpl_action_chain_new_async (G_OBJECT (chan), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_prepare_tpl_channel, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_my_contact, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_remote_contacts, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_room_info, NULL);
  _tpl_action_chain_append (actions, pendingproc_connect_message_signals, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_pending_messages, NULL);
#if 0
  _tpl_action_chain_append (actions, pendingproc_cleanup_pending_messages_db,
      NULL);
#endif
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
