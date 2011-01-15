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

#define DEBUG_FLAG TPL_DEBUG_EVENT
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:event
 * @title: TplEvent
 * @short_description: Abstract representation of a log event
 * @see_also: #TplEventText and other subclasses when they'll exist
 *
 * The TPLogger log event represents a generic log event, which will be
 * specialized by subclasses of #TplEvent.
 */

/**
 * TplEvent:
 *
 * An object representing a generic log event.
 */

/**
 * TPL_EVENT_MSG_ID_IS_VALID:
 * @msg: a message ID
 *
 * Return whether a message ID is valid.
 *
 * If %FALSE is returned, it means that either an invalid input has been
 * passed, or the TplEvent is currently set to %TPL_EVENT_MSG_ID_UNKNOWN
 * or %TPL_EVENT_MSG_ID_ACKNOWLEDGED.
 *
 * Returns: %TRUE if the argument is a valid message ID or %FALSE otherwise.
 */

/**
 * TPL_EVENT_MSG_ID_UNKNOWN:
 *
 * Special value used instead of a message ID to indicate a message with an
 * unknown status (before _tpl_event_set_pending_msg_id() was called, or
 * when it wasn't possible to obtain the message ID).
 */

/**
 * TPL_EVENT_MSG_ID_ACKNOWLEDGED:
 *
 * Special value used instead of a message ID to indicate an acknowledged
 * message.
 */

G_DEFINE_ABSTRACT_TYPE (TplEvent, tpl_event, G_TYPE_OBJECT)

static void tpl_event_set_log_id (TplEvent *self, const gchar *data);

struct _TplEventPriv
{
  gchar *log_id;
  gint64 timestamp;
  gchar *id;
  TpAccount *account;
  gchar *channel_path;

  /* incoming/outgoing */
  TplEventDirection direction;

  /* message and receiver may be NULL depending on the signal. ie. status
   * changed signals set only the sender */
  TplEntity *sender;
  TplEntity *receiver;
};

enum {
    PROP_TIMESTAMP = 1,
    PROP_LOG_ID,
    PROP_DIRECTION,
    PROP_ID,
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

  tp_clear_pointer (&priv->log_id, g_free);
  tp_clear_pointer (&priv->id, g_free);
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
        g_value_set_uint (value, priv->timestamp);
        break;
      case PROP_LOG_ID:
        g_value_set_string (value, priv->log_id);
        break;
      case PROP_DIRECTION:
        g_value_set_uint (value, priv->direction);
        break;
      case PROP_ID:
        g_value_set_string (value, priv->id);
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

  switch (param_id) {
      case PROP_TIMESTAMP:
        _tpl_event_set_timestamp (self, g_value_get_uint (value));
        break;
      case PROP_LOG_ID:
        tpl_event_set_log_id (self, g_value_get_string (value));
        break;
      case PROP_DIRECTION:
        _tpl_event_set_direction (self, g_value_get_uint (value));
        break;
      case PROP_ID:
        _tpl_event_set_id (self, g_value_get_string (value));
        break;
      case PROP_ACCOUNT:
        self->priv->account = g_value_dup_object (value);
        break;
      case PROP_CHANNEL_PATH:
        _tpl_event_set_channel_path (self, g_value_get_string (value));
        break;
      case PROP_SENDER:
        _tpl_event_set_sender (self, g_value_get_object (value));
        break;
      case PROP_RECEIVER:
        _tpl_event_set_receiver (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
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

  klass->equal = NULL;

  param_spec = g_param_spec_uint ("timestamp",
      "Timestamp",
      "The timestamp (gint64) for the log event",
      0, G_MAXUINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TIMESTAMP, param_spec);

  /**
   * TplEvent::log-id:
   *
   * A token identifying the event.
   *
   */
  param_spec = g_param_spec_string ("log-id",
      "LogId",
      "Log identification token, it's unique among existing event, if two "
      "messages have the same token, they are the same event (maybe logged "
      "by two different TplLogStore)",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOG_ID, param_spec);

  param_spec = g_param_spec_uint ("direction",
      "Direction",
      "The direction of the log event (in/out)",
      0, G_MAXUINT32, TPL_EVENT_DIRECTION_NONE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  param_spec = g_param_spec_string ("id",
      "Id",
      "The event identifier to which the log event is related.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY  | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ID, param_spec);

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
      "TplEntity instance destination for the log event",
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
 * tpl_event_get_timestamp
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

const gchar *
_tpl_event_get_log_id (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), 0);

  return self->priv->log_id;
}


TplEventDirection
_tpl_event_get_direction (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self),
      TPL_EVENT_DIRECTION_NONE);

  return self->priv->direction;
}

/**
 * tpl_event_get_sender
 * @self: a #TplEvent
 *
 * Returns: the same #TplEntity as the #TplEvent:sender property
 */
TplEntity *
tpl_event_get_sender (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->sender;
}

/**
 * tpl_event_get_receiver
 * @self: a #TplEvent
 *
 * Returns: the same #TplEntity as the #TplEvent:receiver property
 */
TplEntity *
tpl_event_get_receiver (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->receiver;
}


const gchar *
_tpl_event_get_id (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->id;
}


/**
 * tpl_event_get_account
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

  return tp_proxy_get_object_path (self->priv->account);
}


const gchar *
_tpl_event_get_channel_path (TplEvent *self)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), NULL);

  return self->priv->channel_path;
}


void
_tpl_event_set_timestamp (TplEvent *self,
    gint64 data)
{
  g_return_if_fail (TPL_IS_EVENT (self));

  self->priv->timestamp = data;
  g_object_notify (G_OBJECT (self), "timestamp");
}


/* set just on construction time */
static void
tpl_event_set_log_id (TplEvent *self,
    const gchar* data)
{
  g_return_if_fail (TPL_IS_EVENT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->log_id == NULL);

  self->priv->log_id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "log-id");
}


void
_tpl_event_set_direction (TplEvent *self,
    TplEventDirection data)
{
  g_return_if_fail (TPL_IS_EVENT (self));

  self->priv->direction = data;
  g_object_notify (G_OBJECT (self), "direction");
}


void
_tpl_event_set_sender (TplEvent *self,
    TplEntity *data)
{
  TplEventPriv *priv;

  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_EVENT (self));
  g_return_if_fail (TPL_IS_ENTITY (data));

  priv = self->priv;

  if (priv->sender != NULL)
    g_object_unref (priv->sender);
  priv->sender = g_object_ref (data);
  g_object_notify (G_OBJECT (self), "sender");
}


void
_tpl_event_set_receiver (TplEvent *self,
    TplEntity *data)
{
  TplEventPriv *priv;

  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_EVENT (self));
  g_return_if_fail (TPL_IS_ENTITY (data));

  priv = self->priv;

  if (priv->receiver != NULL)
    g_object_unref (priv->receiver);

  priv->receiver = g_object_ref (data);

  g_object_notify (G_OBJECT (self), "receiver");
}


void
_tpl_event_set_id (TplEvent *self,
    const gchar *data)
{
  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_EVENT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->id == NULL);

  self->priv->id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "id");
}

void
_tpl_event_set_channel_path (TplEvent *self,
    const gchar *data)
{
  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_EVENT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->channel_path == NULL);

  self->priv->channel_path = g_strdup (data);
  g_object_notify (G_OBJECT (self), "channel-path");
}

/**
 * _tpl_event_equal:
 * @self: TplEvent subclass instance
 * @data: an instance of the same TplEvent subclass of @self
 *
 * Checks if two instances of TplEvent represent the same data
 *
 * Returns: %TRUE if @data is the same type of @self and they hold the same
 * data, %FALSE otherwise
 */
gboolean
_tpl_event_equal (TplEvent *self,
    TplEvent *data)
{
  g_return_val_if_fail (TPL_IS_EVENT (self), FALSE);
  g_return_val_if_fail (TPL_IS_EVENT (data), FALSE);

  return TPL_EVENT_GET_CLASS (self)->equal (self, data);
}

/**
 * tpl_event_get_account
 * @self: a #TplEvent
 *
 * <!-- no more to say -->
 *
 * Returns: the same account as the #TplEvent:account property
 */
TpAccount *
tpl_event_get_account (TplEvent *self)
{
  return self->priv->account;
}
