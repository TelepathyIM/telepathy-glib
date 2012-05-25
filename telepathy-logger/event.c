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
#include "event.h"
#include "event-internal.h"

#include <glib.h>
#include "entity-internal.h"

#define DEBUG_FLAG TPL_DEBUG_EVENT
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:event
 * @title: TplEvent
 * @short_description: Abstract representation of a log event
 * @see_also: #TplTextEvent and other subclasses when they'll exist
 *
 * The TPLogger log event represents a generic log event, which will be
 * specialized by subclasses of #TplEvent.
 */

/**
 * TplEvent:
 *
 * An object representing a generic log event.
 */

G_DEFINE_ABSTRACT_TYPE (TplEvent, tpl_event, G_TYPE_OBJECT)

struct _TplEventPriv
{
  gint64 timestamp;
  TpAccount *account;
  gchar *channel_path;

  /* message and receiver may be NULL depending on the signal. ie. status
   * changed signals set only the sender */
  TplEntity *sender;
  TplEntity *receiver;
};

enum {
    PROP_TIMESTAMP = 1,
    PROP_TARGET_ID,
    PROP_ACCOUNT,
    PROP_ACCOUNT_PATH,
    PROP_CHANNEL_PATH,
    PROP_SENDER,
    PROP_RECEIVER
};


static void
tpl_event_finalize (GObject *obj)
{
  TplEvent *self = TPL_EVENT (obj);
  TplEventPriv *priv = self->priv;

  tp_clear_pointer (&priv->channel_path, g_free);

  G_OBJECT_CLASS (tpl_event_parent_class)->finalize (obj);
}


static void
tpl_event_dispose (GObject *obj)
{
  TplEvent *self = TPL_EVENT (obj);
  TplEventPriv *priv = self->priv;

  tp_clear_object (&priv->account);
  tp_clear_object (&priv->sender);
  tp_clear_object (&priv->receiver);

  G_OBJECT_CLASS (tpl_event_parent_class)->dispose (obj);
}


static void
tpl_event_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplEvent *self = TPL_EVENT (object);
  TplEventPriv *priv = self->priv;

  switch (param_id)
    {
      case PROP_TIMESTAMP:
        g_value_set_int64 (value, priv->timestamp);
        break;
      case PROP_ACCOUNT:
        g_value_set_object (value, priv->account);
        break;
      case PROP_ACCOUNT_PATH:
        g_value_set_string (value, tpl_event_get_account_path (self));
        break;
      case PROP_CHANNEL_PATH:
        g_value_set_string (value, priv->channel_path);
        break;
      case PROP_SENDER:
        g_value_set_object (value, priv->sender);
        break;
      case PROP_RECEIVER:
        g_value_set_object (value, priv->receiver);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_event_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplEvent *self = TPL_EVENT (object);
  TplEventPriv *priv = self->priv;

  switch (param_id) {
      case PROP_TIMESTAMP:
        g_assert (priv->timestamp == 0);
        priv->timestamp = g_value_get_int64 (value);
        break;
      case PROP_ACCOUNT:
        g_assert (priv->account == NULL);
        priv->account = g_value_dup_object (value);
        break;
      case PROP_CHANNEL_PATH:
        g_assert (priv->channel_path == NULL);
        priv->channel_path = g_value_dup_string (value);
        break;
      case PROP_SENDER:
        g_assert (priv->sender == NULL);
        g_return_if_fail (TPL_IS_ENTITY (g_value_get_object (value)));
        priv->sender = g_value_dup_object (value);
        break;
      case PROP_RECEIVER:
        g_assert (priv->receiver == NULL);
        /* can be NULL */
        priv->receiver = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}

static inline gboolean
account_equal (TpAccount *account1, TpAccount *account2)
{
  g_return_val_if_fail (TP_IS_PROXY (account1), FALSE);
  g_return_val_if_fail (TP_IS_PROXY (account2), FALSE);

  return !tp_strdiff (tp_proxy_get_object_path (TP_PROXY (account1)),
      tp_proxy_get_object_path (TP_PROXY (account2)));
}


static gboolean
tpl_event_equal_default (TplEvent *message1,
    TplEvent *message2)
{
  g_return_val_if_fail (TPL_IS_EVENT (message1), FALSE);
  g_return_val_if_fail (TPL_IS_EVENT (message2), FALSE);

  return message1->priv->timestamp == message2->priv->timestamp
    && account_equal (message1->priv->account, message2->priv->account)
    && _tpl_entity_compare (message1->priv->sender, message2->priv->sender)
    && _tpl_entity_compare (message1->priv->receiver, message2->priv->receiver);
}


static void
tpl_event_class_init (TplEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  /* to be used by subclasses */
  object_class->finalize = tpl_event_finalize;
  object_class->dispose = tpl_event_dispose;
  object_class->get_property = tpl_event_get_property;
  object_class->set_property = tpl_event_set_property;

  klass->equal = tpl_event_equal_default;

  param_spec = g_param_spec_int64 ("timestamp",
      "Timestamp",
      "The timestamp (gint64) for the log event",
      G_MININT64, G_MAXINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TIMESTAMP, param_spec);

  param_spec = g_param_spec_object ("account",
      "TpAccount",
      "The TpAccount to which the log event is related",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_string ("account-path",
      "AccountPath",
      "The account path of the TpAccount to which the log event is related",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT_PATH, param_spec);

  param_spec = g_param_spec_string ("channel-path",
      "ChannelPath",
      "The channel path of the TpChannel to which the log event is related",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PATH, param_spec);

  param_spec = g_param_spec_object ("sender",
      "Sender",
      "TplEntity instance who originated the log event",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDER, param_spec);

  param_spec = g_param_spec_object ("receiver",
      "Receiver",
      "TplEntity instance destination for the log event "
      "(may be NULL with some log stores)",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECEIVER, param_spec);

  g_type_class_add_private (object_class, sizeof (TplEventPriv));
}


static void
tpl_event_init (TplEvent *self)
{
  TplEventPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_EVENT, TplEventPriv);
  self->priv = priv;
}

/**
 * tpl_event_get_timestamp:
 * @self: a #TplEvent
 *
 * Returns: the same timestamp as the #TplEvent:timestamp property
 */
gint64
tpl_event_get_timestamp (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), -1);

  return self->priv->timestamp;
}


/**
 * tpl_event_get_sender:
 * @self: a #TplEvent
 *
 * Returns: (transfer none): the same #TplEntity as the #TplEvent:sender property
 */
TplEntity *
tpl_event_get_sender (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->sender;
}

/**
 * tpl_event_get_receiver:
 * @self: a #TplEvent
 *
 * Returns: (transfer none): the same #TplEntity as the #TplEvent:receiver property
 */
TplEntity *
tpl_event_get_receiver (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->receiver;
}


TplEntity *
_tpl_event_get_target (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  if (_tpl_event_target_is_room (self)
      || tpl_entity_get_entity_type (self->priv->sender) == TPL_ENTITY_SELF)
    return self->priv->receiver;
  else
    return self->priv->sender;
}


const gchar *
_tpl_event_get_target_id (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return tpl_entity_get_identifier (_tpl_event_get_target (self));
}

gboolean
_tpl_event_target_is_room (TplEvent *self)
{
  /* Some log-store like Pidgin text mode does not know about receiver, so
   * having a NULL receiver is fine. */
  if (self->priv->receiver == NULL)
    return FALSE;

  return (tpl_entity_get_entity_type (self->priv->receiver) == TPL_ENTITY_ROOM);
}


/**
 * tpl_event_get_account_path:
 * @self: a #TplEvent
 *
 * <!-- no more to say -->
 *
 * Returns: the path as the #TplEvent:account property
 */
const gchar *
tpl_event_get_account_path (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (self->priv->account), NULL);

  return tp_proxy_get_object_path (self->priv->account);
}


const gchar *
_tpl_event_get_channel_path (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->channel_path;
}


/**
 * tpl_event_equal:
 * @self: TplEvent subclass instance
 * @data: an instance of the same TplEvent subclass of @self
 *
 * Checks if two instances of TplEvent represent the same data
 *
 * Returns: %TRUE if @data is the same type of @self and they hold the same
 * data, %FALSE otherwise
 */
gboolean
tpl_event_equal (TplEvent *self,
    TplEvent *data)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), FALSE);
  g_return_val_if_fail (TPL_IS_EVENT (data), FALSE);

  return TPL_EVENT_GET_CLASS (self)->equal (self, data);
}

/**
 * tpl_event_get_account:
 * @self: a #TplEvent
 *
 * <!-- no more to say -->
 *
 * Returns: (transfer none): the same account as the #TplEvent:account property
 */
TpAccount *
tpl_event_get_account (TplEvent *self)
{
  return self->priv->account;
}
