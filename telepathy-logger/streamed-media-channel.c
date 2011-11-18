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
 * Authors: Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *          Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "streamed-media-channel-internal.h"

#include <glib.h>
#include <telepathy-glib/telepathy-glib.h>

#include "action-chain-internal.h"
#include "call-event.h"
#include "call-event-internal.h"
#include "channel-internal.h"
#include "entity-internal.h"
#include "event-internal.h"
#include "log-manager-internal.h"
#include "observer-internal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include "debug-internal.h"


typedef enum
{
  PENDING_INITIATOR_STATE,
  PENDING_RECEIVER_STATE,
  ACCEPTED_STATE,
  ENDED_STATE,
} CallState;

struct _TplStreamedMediaChannelPriv
{
  TpAccount *account;
  TplEntity *sender;
  TplEntity *receiver;
  GDateTime *timestamp;
  GTimer *timer;
  gboolean timer_started;
  CallState state;
  TplEntity *end_actor;
  TplCallEndReason end_reason;
  const gchar *detailed_end_reason;
};

static TpContactFeature features[3] = {
  TP_CONTACT_FEATURE_ALIAS,
  TP_CONTACT_FEATURE_PRESENCE,
  TP_CONTACT_FEATURE_AVATAR_TOKEN
};

static void tpl_streamed_media_channel_iface_init (TplChannelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TplStreamedMediaChannel, _tpl_streamed_media_channel,
    TP_TYPE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_CHANNEL, tpl_streamed_media_channel_iface_init))


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
  TplStreamedMediaChannel *chan = _tpl_action_chain_get_object (ctx);
  TpConnection *conn = tp_channel_borrow_connection (TP_CHANNEL (chan));
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (conn, conn_features, proxy_prepared_cb, ctx);
}


static void
pendingproc_prepare_tp_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplStreamedMediaChannel *chan = _tpl_action_chain_get_object (ctx);
  GQuark chan_features[] = {
      TP_CHANNEL_FEATURE_CORE,
      TP_CHANNEL_FEATURE_GROUP,
      0
  };

  tp_proxy_prepare_async (chan, chan_features, proxy_prepared_cb, ctx);
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
  TplStreamedMediaChannel *self = TPL_STREAMED_MEDIA_CHANNEL (weak_object);
  TplActionChain *ctx = user_data;
  TplEntity *remote;

  if (error != NULL)
    {
      _tpl_action_chain_terminate (ctx, error);
      return;
    }

  remote = tpl_entity_new_from_tp_contact (contacts[0], TPL_ENTITY_CONTACT);

  if (tp_channel_get_requested (TP_CHANNEL (self)))
    self->priv->receiver = remote;
  else
    self->priv->sender = remote;

  _tpl_action_chain_continue (ctx);
}


static void
pendingproc_get_remote_contacts (TplActionChain *ctx,
    gpointer user_data)
{
  TplStreamedMediaChannel *self = _tpl_action_chain_get_object (ctx);
  TpChannel *chan = TP_CHANNEL (self);
  TpConnection *tp_conn = tp_channel_borrow_connection (chan);
  GArray *arr;
  TpHandle handle;

  arr = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
  handle = tp_channel_get_handle (chan, NULL);
  g_array_append_val (arr, handle);

  tp_connection_get_contacts_by_handle (tp_conn,
      arr->len, (TpHandle *) arr->data,
      G_N_ELEMENTS (features), features, get_remote_contact_cb, ctx, NULL,
      G_OBJECT (self));

  g_array_unref (arr);
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
  TplStreamedMediaChannel *tpl_media = _tpl_action_chain_get_object (ctx);
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_media);
  TpChannel *tp_chan = TP_CHANNEL (tpl_chan);
  TplEntity *self;

  g_return_if_fail (TPL_IS_CHANNEL_STREAMED_MEDIA (tpl_media));

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

  self = tpl_entity_new_from_tp_contact (contacts[0], TPL_ENTITY_SELF);

  if (tp_channel_get_requested (tp_chan))
    tpl_media->priv->sender = self;
  else
    tpl_media->priv->receiver = self;

  _tpl_action_chain_continue (ctx);
}



static void
pendingproc_get_local_contact (TplActionChain *ctx,
    gpointer user_data)
{
  TplStreamedMediaChannel *tpl_media = _tpl_action_chain_get_object (ctx);
  TpChannel *chan = TP_CHANNEL (tpl_media);
  TpConnection *tp_conn = tp_channel_borrow_connection (chan);
  TpHandle my_handle;

  my_handle = tp_channel_group_get_self_handle (chan);
  if (my_handle == 0)
    my_handle = tp_connection_get_self_handle (tp_conn);

  tp_connection_get_contacts_by_handle (tp_conn, 1, &my_handle,
      G_N_ELEMENTS (features), features, get_self_contact_cb, ctx, NULL,
      G_OBJECT (tpl_media));
}


static void
on_group_members_changed_cb (TpChannel *chan,
    gchar *message,
    GArray *added,
    GArray *removed,
    GArray *local_pending,
    GArray *remote_pending,
    TpHandle actor,
    guint reason,
    gpointer user_data)
{
  TplStreamedMediaChannelPriv *priv = TPL_STREAMED_MEDIA_CHANNEL (user_data)->priv;
  TpHandle *added_handles = (TpHandle *) added->data;
  TpHandle initiator, self, receiver;
  guint i;

  const gchar *reasons[] = {
      "Unknown",
      "User Requested",
      "No Answer"
  };

  initiator = tp_channel_get_initiator_handle (chan);
  self = tp_channel_group_get_self_handle (chan);

  if (tp_channel_get_requested (chan))
    receiver = tp_channel_get_handle (chan, NULL);
  else
    receiver = self;

  g_return_if_fail (receiver != 0);

  switch (priv->state)
    {
    case PENDING_INITIATOR_STATE:
      /* Check if initiator was added */
      for (i = 0; i < added->len; i++)
        {
          if (added_handles[i] == initiator)
            {
              priv->state = PENDING_RECEIVER_STATE;
              i = added->len;
              DEBUG ("StreamedMediaChannel Moving to PENDING_RECEIVER_STATE");
            }
        }
      if (priv->state != PENDING_RECEIVER_STATE)
        break;

    case PENDING_RECEIVER_STATE:
      for (i = 0; i < added->len; i++)
        {
          if (added_handles[i] == receiver)
            {
              priv->state = ACCEPTED_STATE;
              i = added->len;
              g_timer_start (priv->timer);
              priv->timer_started = TRUE;
              DEBUG ("StreamedMediaChannel Moving to ACCEPTED_STATE, start_time=%li",
                  time (NULL));
            }
        }
      break;

    default:
      /* nothing to do */
      break;
    }

  /* If call is not ending we are done */
  if (priv->state == PENDING_INITIATOR_STATE
       || tp_intset_size (tp_channel_group_get_members (chan)) != 0)
    return;

  if (actor == receiver)
    priv->end_actor = g_object_ref (priv->receiver);
  else
    priv->end_actor = g_object_ref (priv->sender);

  switch (reason)
    {
    case TP_CHANNEL_GROUP_CHANGE_REASON_NONE:
      /* This detailed reason may be changed based on the current
       * call state */
      priv->detailed_end_reason = "";
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_OFFLINE:
      priv->detailed_end_reason = TP_ERROR_STR_OFFLINE;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_KICKED:
      priv->detailed_end_reason = TP_ERROR_STR_CHANNEL_KICKED;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_BUSY:
      priv->detailed_end_reason = TP_ERROR_STR_BUSY;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_BANNED:
      priv->detailed_end_reason = TP_ERROR_STR_CHANNEL_BANNED;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_ERROR:
      priv->detailed_end_reason = TP_ERROR_STR_NETWORK_ERROR;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_INVALID_CONTACT:
      priv->detailed_end_reason = TP_ERROR_STR_DOES_NOT_EXIST;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_NO_ANSWER:
      priv->detailed_end_reason = TP_ERROR_STR_NO_ANSWER;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_PERMISSION_DENIED:
      priv->detailed_end_reason = TP_ERROR_STR_PERMISSION_DENIED;
      break;

    default:
      g_warning ("Invalid change reason for StreamMedia call ending: %i",
          reason);
      priv->detailed_end_reason = TP_ERROR_STR_INVALID_ARGUMENT;
      break;
    }

  switch (priv->state)
    {
    case PENDING_RECEIVER_STATE:
      /* Workaround missing rejected reason. A call is rejected when the
       * receiver terminates that call before accepting it, and no other
       * reason was provided. Also, even if the call was not answered, the
       * spec enforces that the end_reason must be user_requested */
      if (reason == TP_CHANNEL_GROUP_CHANGE_REASON_NONE
          && actor == receiver)
        {
          priv->end_reason = TPL_CALL_END_REASON_USER_REQUESTED;
          priv->detailed_end_reason = TP_ERROR_STR_REJECTED;
        }
      else
        priv->end_reason = TPL_CALL_END_REASON_NO_ANSWER;
      break;

    case ACCEPTED_STATE:
      priv->end_reason = TPL_CALL_END_REASON_USER_REQUESTED;

      if (reason == TP_CHANNEL_GROUP_CHANGE_REASON_NONE)
        {
          /*  If the SelfHandle is removed from a group for this reason and the
           *  actor is not the SelfHandle, the equivalent D-Bus error is
           *  org.freedesktop.Telepathy.Error.Terminated. If the SelfHandle is
           *  removed from a group for this reason and the actor is also the
           *  SelfHandle, the equivalent D-Bus error is
           *  org.freedesktop.Telepathy.Error.Cancelled. */
          if (actor != self)
            priv->detailed_end_reason = TP_ERROR_STR_TERMINATED;
          else
            priv->detailed_end_reason = TP_ERROR_STR_CANCELLED;
        }
      break;

    default:
      /* somethings wrong */
      priv->end_reason = TPL_CALL_END_REASON_UNKNOWN;
      break;
    }

  priv->state = ENDED_STATE;

  g_timer_stop (priv->timer);

  DEBUG (
      "Moving to ENDED_STATE, duration=%" G_GINT64_FORMAT " reason=%s details=%s",
      (gint64) (priv->timer_started ? g_timer_elapsed (priv->timer, NULL) : -1),
      reasons[priv->end_reason],
      priv->detailed_end_reason);
}


static void
store_call (TplStreamedMediaChannel *self)
{
  TplStreamedMediaChannelPriv *priv = self->priv;
  GError *error = NULL;
  TplCallEvent *call_log;
  TplLogManager *logmanager;
  const gchar *channel_path = tp_proxy_get_object_path (TP_PROXY (self));
  GTimeSpan duration = -1;

  if (priv->timer_started)
    duration = g_timer_elapsed (priv->timer, NULL);

  if (priv->end_actor == NULL)
    priv->end_actor = tpl_entity_new ("unknown", TPL_ENTITY_UNKNOWN, NULL, NULL);

  if (priv->detailed_end_reason == NULL)
    priv->detailed_end_reason = "";

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
      PATH_DEBUG (self, "StreamedMediaChannel: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (logmanager);
  g_object_unref (call_log);
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

  store_call (TPL_STREAMED_MEDIA_CHANNEL (user_data));

  if (!_tpl_observer_unregister_channel (observer, tpl_chan))
    PATH_DEBUG (tpl_chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static void
tpl_streamed_media_channel_prepare_async (TplChannel *chan,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *actions;

  actions = _tpl_action_chain_new_async (G_OBJECT (chan), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_connection, NULL);
  _tpl_action_chain_append (actions, pendingproc_prepare_tp_channel, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_remote_contacts, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_local_contact, NULL);

  _tpl_action_chain_continue (actions);
}


static gboolean
tpl_streamed_media_channel_prepare_finish (TplChannel *chan,
    GAsyncResult *result,
    GError **error)
{
  return _tpl_action_chain_new_finish (G_OBJECT (chan), result, error);
}


static void
tpl_streamed_media_channel_dispose (GObject *obj)
{
  TplStreamedMediaChannelPriv *priv = TPL_STREAMED_MEDIA_CHANNEL (obj)->priv;

  tp_clear_object (&priv->account);
  tp_clear_object (&priv->sender);
  tp_clear_object (&priv->receiver);
  tp_clear_pointer (&priv->timestamp, g_date_time_unref);
  tp_clear_pointer (&priv->timer, g_timer_destroy);
  tp_clear_object (&priv->end_actor);

  G_OBJECT_CLASS (_tpl_streamed_media_channel_parent_class)->dispose (obj);
}


static void
tpl_streamed_media_channel_finalize (GObject *obj)
{
  PATH_DEBUG (obj, "finalizing channel %p", obj);

  G_OBJECT_CLASS (_tpl_streamed_media_channel_parent_class)->finalize (obj);
}


static void
_tpl_streamed_media_channel_class_init (TplStreamedMediaChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_streamed_media_channel_dispose;
  object_class->finalize = tpl_streamed_media_channel_finalize;

  g_type_class_add_private (object_class, sizeof (TplStreamedMediaChannelPriv));
}


static void
tpl_streamed_media_channel_iface_init (TplChannelInterface *iface)
{
  iface->prepare_async = tpl_streamed_media_channel_prepare_async;
  iface->prepare_finish = tpl_streamed_media_channel_prepare_finish;
}


static void
_tpl_streamed_media_channel_init (TplStreamedMediaChannel *self)
{
  gchar *date;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_STREAMED_MEDIA_CHANNEL, TplStreamedMediaChannelPriv);

  self->priv->timestamp = g_date_time_new_now_utc ();
  self->priv->timer = g_timer_new ();
  self->priv->state = PENDING_INITIATOR_STATE;

  date = g_date_time_format (self->priv->timestamp, "%Y-%m-%d %H:%M:%S");
  DEBUG ("New call, timestamp=%s UTC", date);
  g_free (date);

  tp_g_signal_connect_object (TP_CHANNEL (self), "group-members-changed",
      G_CALLBACK (on_group_members_changed_cb), self, 0);

  tp_g_signal_connect_object (TP_CHANNEL (self), "invalidated",
      G_CALLBACK (on_channel_invalidated_cb), self, 0);
}


/**
 * _tpl_streamed_media_channel_new
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @account: TpAccount instance, related to the new #TplStreamedMediaChannel
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Streamed Media Channel proxy.
 * The returned #TplStreamedMediaChannel is not guaranteed to be ready
 * at the point of return.
 *
 * TplStreamedMediaChannel is actually a subclass of #TpChannel implementing
 * TplChannel interface. Use #TpChannel methods, casting the
 * #TplStreamedMediaChannel instance to a TpChannel, to access TpChannel
 * data/methods from it.
 *
 * TplStreamedMediaChannel is usually created using
 * #tpl_channel_factory_build, from within a #TplObserver singleton,
 * when its Observer_Channel method is called by the Channel Dispatcher.
 *
 * Returns: the TplStreamedMediaChannel instance or %NULL if
 * @object_path is not valid.
 */
TplStreamedMediaChannel *
_tpl_streamed_media_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);
  TplStreamedMediaChannel *self;

  /* Do what tpl_channel_new does + set TplStreamedMediaChannel
   * specific properties */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  self = g_object_new (TPL_TYPE_STREAMED_MEDIA_CHANNEL,
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
