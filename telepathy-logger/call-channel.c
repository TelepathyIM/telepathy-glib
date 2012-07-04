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

#include "call-event.h"
#include "call-event-internal.h"
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

G_DEFINE_TYPE (TplCallChannel, _tpl_call_channel, TP_TYPE_CALL_CHANNEL)


static gboolean
get_contacts (TplCallChannel *self,
    GError **error)
{
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
          g_set_error (error, TPL_CALL_CHANNEL_ERROR,
              TPL_CALL_CHANNEL_ERROR_MISSING_TARGET_CONTACT,
              "Failed to resolve target contact");
          return FALSE;
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

  return TRUE;
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
          if (priv->end_actor != NULL)
            g_object_unref (priv->end_actor);

          priv->end_actor = g_hash_table_lookup (priv->entities,
              GUINT_TO_POINTER (reason->actor));

          if (priv->end_actor == NULL)
            priv->end_actor = tpl_entity_new ("unknown", TPL_ENTITY_UNKNOWN,
                NULL, NULL);
          else
            g_object_ref (priv->end_actor);

          priv->end_reason = reason->reason;

          g_free (priv->detailed_end_reason);

          if (reason->dbus_reason == NULL)
            priv->detailed_end_reason = g_strdup ("");
          else
            priv->detailed_end_reason = g_strdup (reason->dbus_reason);

          g_timer_stop (priv->timer);

          DEBUG (
              "Moving to ENDED_STATE, duration=%" G_GINT64_FORMAT " reason=%s details=%s",
              (gint64) (priv->timer_started ? g_timer_elapsed (priv->timer, NULL) : -1),
              _tpl_call_event_end_reason_to_str (priv->end_reason),
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
  TpChannel *chan = TP_CHANNEL (user_data);
  TplObserver *observer = _tpl_observer_dup (NULL);

  g_return_if_fail (observer);

  PATH_DEBUG (chan, "%s #%d %s",
      g_quark_to_string (domain), code, message);

  store_call (TPL_CALL_CHANNEL (user_data));

  if (!_tpl_observer_unregister_channel (observer, chan))
    PATH_DEBUG (chan, "Channel couldn't be unregistered correctly (BUG?)");

  g_object_unref (observer);
}


static void
connect_signals (TplCallChannel *self)
{
  tp_g_signal_connect_object (self, "state-changed",
      G_CALLBACK (call_state_changed_cb), self, 0);

  tp_g_signal_connect_object (self, "members-changed",
      G_CALLBACK (call_members_changed_cb), self, 0);

  tp_g_signal_connect_object (TP_CHANNEL (self), "invalidated",
      G_CALLBACK (channel_invalidated_cb), self, 0);
}


static void
_tpl_call_channel_prepare_core_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplCallChannel *self = (TplCallChannel *) proxy;
  GError *error = NULL;

  connect_signals (self);

  if (!get_contacts (self, &error))
    {
      g_simple_async_report_take_gerror_in_idle ((GObject *) self, callback,
          user_data, error);
      return;
    }

  tp_simple_async_report_success_in_idle ((GObject *) self, callback, user_data,
      _tpl_call_channel_prepare_core_async);
}

GQuark
_tpl_call_channel_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tpl-call-channel-feature-core");
}

enum {
    FEAT_CORE,
    N_FEAT
};

static const TpProxyFeature *
tpl_call_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name = TPL_CALL_CHANNEL_FEATURE_CORE;
  features[FEAT_CORE].prepare_async = _tpl_call_channel_prepare_core_async;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
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
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  object_class->dispose = tpl_call_channel_dispose;
  object_class->finalize = tpl_call_channel_finalize;

  proxy_class->list_features = tpl_call_channel_list_features;

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
 * _tpl_call_channel_new:
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
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
    GError **error)
{
  return _tpl_call_channel_new_with_factory (NULL, conn, object_path,
      tp_chan_props, error);
}

TplCallChannel *
_tpl_call_channel_new_with_factory (TpSimpleClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *tp_chan_props,
    GError **error)
{
  TpProxy *conn_proxy = TP_PROXY (conn);
  TplCallChannel *self;

  /* Do what tpl_channel_new does + set TplCallChannel
   * specific properties */

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (object_path), NULL);
  g_return_val_if_fail (tp_chan_props != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  self = g_object_new (TPL_TYPE_CALL_CHANNEL,
      "factory", factory,
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
