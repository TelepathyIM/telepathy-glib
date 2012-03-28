/*
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "call-channel-internal.h"

#include <glib.h>
#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>

#include "action-chain-internal.h"
#include "call-event.h"
#include "call-event-internal.h"
#include "channel-internal.h"
#include "entity-internal.h"
#include "event-internal.h"
#include "log-manager-internal.h"
#include "observer-internal.h"
#include "tpl-marshal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include "debug-internal.h"

struct _TplCallChannelPriv
{
  TpAccount *account;
  GHashTable *entities;
  TplEntity *sender;
  TplEntity *receiver;
  GDateTime *timestamp;
  GTimer *timer;
  gboolean timer_started;
  TplEntity *end_actor;
  TpCallStateChangeReason end_reason;
  gchar *detailed_end_reason;
};

static void tpl_call_channel_iface_init (TplChannelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TplCallChannel, _tpl_call_channel,
    TP_TYPE_CALL_CHANNEL,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_CHANNEL, tpl_call_channel_iface_init))


static void
proxy_prepared_cb (GObject *source,
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
  TplCallChannel *chan = _tpl_action_chain_get_object (ctx);
  TpConnection *conn = tp_channel_borrow_connection (TP_CHANNEL (chan));
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (conn, conn_features, proxy_prepared_cb, ctx);
}


static void
pendingproc_prepare_tp_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplCallChannel *chan = _tpl_action_chain_get_object (ctx);
  GQuark chan_features[] = {
      TP_CHANNEL_FEATURE_CORE,
      TP_CHANNEL_FEATURE_GROUP,
      0
  };

  tp_proxy_prepare_async (chan, chan_features, proxy_prepared_cb, ctx);
}


static void
pendingproc_get_contacts (TplActionChain *ctx,
    gpointer user_data)
{
  TplCallChannel *self = _tpl_action_chain_get_object (ctx);
  TplCallChannelPriv *priv = self->priv;
  TpChannel *chan = TP_CHANNEL (self);
  TpConnection *con = tp_channel_borrow_connection (chan);
  GHashTable *members;
  GHashTableIter iter;
  TpHandle handle;
  TpHandleType handle_type;
  gboolean is_room;
  TpContact *contact;
  TplEntity *entity;

  /* Get and store entities */
  members = tp_call_channel_get_members (TP_CALL_CHANNEL (self));

  g_hash_table_iter_init (&iter, members);
  while (g_hash_table_iter_next (&iter, (gpointer *) &contact, NULL))
    {
      handle = tp_contact_get_handle (contact);
      g_hash_table_insert (priv->entities, GUINT_TO_POINTER (handle),
          tpl_entity_new_from_tp_contact (contact, TPL_ENTITY_CONTACT));
    }

  /* Identify target */
  handle = tp_channel_get_handle (chan, &handle_type);
  is_room = (handle_type == TP_HANDLE_TYPE_ROOM);

  if (is_room)
    {
      priv->receiver =
        tpl_entity_new_from_room_id (tp_channel_get_identifier (chan));
    }
  else
    {
      entity = g_hash_table_lookup (priv->entities,  GUINT_TO_POINTER (handle));

      if (entity == NULL)
        {
          GError *new_error = NULL;
          new_error = g_error_new (TPL_CALL_CHANNEL_ERROR,
              TPL_CALL_CHANNEL_ERROR_MISSING_TARGET_CONTACT,
              "Failed to resolve target contact");
          _tpl_action_chain_terminate (ctx, new_error);
          g_error_free (new_error);
          return;
        }

      if (tp_channel_get_requested (chan))
        priv->receiver = g_object_ref (entity);
      else
        priv->sender = g_object_ref (entity);
    }

  /* Get and store self entity */
  contact = tp_channel_group_get_self_contact (chan);
  if (contact == NULL)
      contact = tp_connection_get_self_contact (con);

  handle = tp_contact_get_handle (contact);
  entity = tpl_entity_new_from_tp_contact (contact, TPL_ENTITY_SELF);
  g_hash_table_insert (priv->entities, GUINT_TO_POINTER (handle), entity);

  if (tp_channel_get_requested (chan) || is_room)
    priv->sender = g_object_ref (entity);
  else
    priv->receiver = g_object_ref (entity);

  _tpl_action_chain_continue (ctx);
}


static void
call_state_changed_cb (TpCallChannel *call,
    TpCallState state,
    TpCallFlags flags,
    TpCallStateReason *reason,
    GHashTable *details,
    gpointer user_data)
{
  TplCallChannel *self = TPL_CALL_CHANNEL (user_data);
  TplCallChannelPriv *priv = self->priv;

  switch (state)
    {
    case TP_CALL_STATE_ACCEPTED:
        {
          if (!priv->timer_started)
            {
              DEBUG ("Moving to ACCEPTED_STATE, start_time=%li",
                  time (NULL));
              g_timer_start (priv->timer);
              priv->timer_started = TRUE;
            }
        }
      break;

    case TP_CALL_STATE_ENDED:
        {
          priv->end_actor = g_hash_table_lookup (priv->entities,
              GUINT_TO_POINTER (reason->actor));

          if (priv->end_actor == NULL)
            priv->end_actor = tpl_entity_new ("unknown", TPL_ENTITY_UNKNOWN,
                NULL, NULL);
          else
            g_object_ref (priv->end_actor);

          priv->end_reason = reason->reason;

          if (reason->dbus_reason == NULL)
            priv->detailed_end_reason = g_strdup ("");
          else
            priv->detailed_end_reason = g_strdup (reason->dbus_reason);

          g_timer_stop (priv->timer);

          DEBUG (
              "Moving to ENDED_STATE, duration=%" G_GINT64_FORMAT " reason=%s details=%s",
              (gint64) (priv->timer_started ? g_timer_elapsed (priv->timer, NULL) : -1),
              _tpl_call_event_end_reason_to_str(priv->end_reason),
              priv->detailed_end_reason);
        }
      break;

    default:
      /* just wait */
      break;
    }
}


static void
call_members_changed_cb (TpCallChannel *call,
    GHashTable *updates,
    GArray *removed,
    TpCallStateReason reason,
    gpointer user_data)
{
  TplCallChannel *self = TPL_CALL_CHANNEL (call);
  TplCallChannelPriv *priv = self->priv;
  GHashTableIter iter;
  TpContact *contact;

  g_hash_table_iter_init (&iter, updates);
  while (g_hash_table_iter_next (&iter, (gpointer *) &contact, NULL))
    {
      TpHandle handle = tp_contact_get_handle (contact);
      TplEntity *entity = g_hash_table_lookup (priv->entities,
          GUINT_TO_POINTER (handle));

      if (!entity)
        {
          entity = tpl_entity_new_from_tp_contact (contact,
              TPL_ENTITY_CONTACT);
          g_hash_table_insert (priv->entities, GUINT_TO_POINTER (handle),
              entity);
        }
    }
}


static void
store_call (TplCallChannel *self)
{
  TplCallChannelPriv *priv = self->priv;
  GError *error = NULL;
  TplCallEvent *call_log;
  TplLogManager *logmanager;
  const gchar *channel_path = tp_proxy_get_object_path (TP_PROXY (self));
  GTimeSpan duration = -1;

  if (priv->timer_started)
    duration = g_timer_elapsed (priv->timer, NULL);

  /* Initialize data for TplEntity */
  call_log = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", priv->account,
      "channel-path", channel_path,
      "receiver", priv->receiver,
      "sender", priv->sender,
      "timestamp", g_date_time_to_unix (priv->timestamp),
      /* TplCallEvent */
      "duration", duration,
      "end-actor", priv->end_actor,
      "end-reason", priv->end_reason,
      "detailed-end-reason", priv->detailed_end_reason,
      NULL);

  logmanager = tpl_log_manager_dup_singleton ();
  _tpl_log_manager_add_event (logmanager, TPL_EVENT (call_log), &error);

  if (error != NULL)
    {
      PATH_DEBUG (self, "TplCallChannel: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (logmanager);
  g_object_unref (call_log);
}


static void
channel_invalidated_cb (TpProxy *proxy,
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

  store_call (TPL_CALL_CHANNEL (user_data));

  if (!_tpl_observer_unregister_channel (observer, tpl_chan))
    PATH_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static void
pendingproc_connect_signals (TplActionChain *ctx,
    gpointer user_data)
{
  TplCallChannel *self = _tpl_action_chain_get_object (ctx);

  tp_g_signal_connect_object (self, "state-changed",
      G_CALLBACK (call_state_changed_cb), self, 0);

  tp_g_signal_connect_object (self, "members-changed",
      G_CALLBACK (call_members_changed_cb), self, 0);

  tp_g_signal_connect_object (TP_CHANNEL (self), "invalidated",
      G_CALLBACK (channel_invalidated_cb), self, 0);

  _tpl_action_chain_continue (ctx);
}


static void
tpl_call_channel_prepare_async (TplChannel *chan,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *actions;

  actions = _tpl_action_chain_new_async (G_OBJECT (chan), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_connect_signals, NULL);
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_connection, NULL);
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_channel, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_contacts, NULL);

  _tpl_action_chain_continue (actions);
}


static gboolean
tpl_call_channel_prepare_finish (TplChannel *chan,
    GAsyncResult *result,
    GError **error)
{
  return _tpl_action_chain_new_finish (G_OBJECT (chan), result, error);
}


static void
tpl_call_channel_dispose (GObject *obj)
{
  TplCallChannelPriv *priv = TPL_CALL_CHANNEL (obj)->priv;

  tp_clear_object (&priv->account);
  tp_clear_pointer (&priv->entities, g_hash_table_unref);
  tp_clear_object (&priv->sender);
  tp_clear_object (&priv->receiver);
  tp_clear_pointer (&priv->timestamp, g_date_time_unref);
  tp_clear_pointer (&priv->timer, g_timer_destroy);
  tp_clear_object (&priv->end_actor);
  tp_clear_pointer (&priv->detailed_end_reason, g_free);

  G_OBJECT_CLASS (_tpl_call_channel_parent_class)->dispose (obj);
}


static void
tpl_call_channel_finalize (GObject *obj)
{
  PATH_DEBUG (obj, "finalizing channel %p", obj);

  G_OBJECT_CLASS (_tpl_call_channel_parent_class)->finalize (obj);
}


static void
_tpl_call_channel_class_init (TplCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_call_channel_dispose;
  object_class->finalize = tpl_call_channel_finalize;

  g_type_class_add_private (object_class, sizeof (TplCallChannelPriv));

  dbus_g_object_register_marshaller (tpl_marshal_VOID__UINT_UINT_BOXED_BOXED,
      G_TYPE_NONE,
      G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOXED, G_TYPE_BOXED,
      G_TYPE_INVALID);

  dbus_g_object_register_marshaller (tpl_marshal_VOID__BOXED_BOXED,
      G_TYPE_NONE,
      G_TYPE_BOXED, G_TYPE_BOXED,
      G_TYPE_INVALID);
}


static void
tpl_call_channel_iface_init (TplChannelInterface *iface)
{
  iface->prepare_async = tpl_call_channel_prepare_async;
  iface->prepare_finish = tpl_call_channel_prepare_finish;
}


static void
_tpl_call_channel_init (TplCallChannel *self)
{
  gchar *date;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CALL_CHANNEL, TplCallChannelPriv);

  self->priv->timestamp = g_date_time_new_now_utc ();
  self->priv->timer = g_timer_new ();

  date = g_date_time_format (self->priv->timestamp, "%Y-%m-%d %H:%M:%S");
  DEBUG ("New call, timestamp=%s UTC", date);
  g_free (date);

  self->priv->entities = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_object_unref);
}


/**
 * _tpl_call_channel_new
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @account: TpAccount instance, related to the new #TplCallChannel
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Call Channel proxy.
 * The returned #TplCallChannel is not guaranteed to be ready
 * at the point of return.
 *
 * TplCallChannel is actually a subclass of #TpChannel implementing
 * interface #TplChannel. Use #TpChannel methods, casting the #TplCallChannel
 * instance to a TpChannel, to access TpChannel data/methods from it.
 *
 * TplCallChannel is usually created using
 * #tpl_channel_factory_build, from within a #TplObserver singleton,
 * when its Observer_Channel method is called by the Channel Dispatcher.
 *
 * Returns: the TplCallChannel instance or %NULL if
 * @object_path is not valid.
 */
TplCallChannel *
_tpl_call_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);
  TplCallChannel *self;

  /* Do what tpl_channel_new does + set TplCallChannel
   * specific properties */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  self = g_object_new (TPL_TYPE_CALL_CHANNEL,
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
