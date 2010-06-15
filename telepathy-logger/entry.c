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
#include "entry.h"
#include "entry-internal.h"

#include <glib.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TPL_DEBUG_ENTRY
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:entry
 * @title: TplEntry
 * @short_description: Abstract representation of a log entry
 * @see_also: #TplEntryText and other subclasses when they'll exist
 *
 * The TPLogger log entry represent a generic log entry, which will be
 * specialied by subclasses of #TplEntry.
 */

/**
 * TplEntry:
 *
 * An object representing a generic log entry.
 */

/**
 * TPL_ENTRY_MSG_ID_IS_VALID:
 * @msg: a message ID
 *
 * Return whether a message ID is valid.
 *
 * If %FALSE is returned, it means that either a invalid input has been
 * passed, or the TplEntry is currently set to %TPL_ENTRY_MSG_ID_UNKNOWN
 * or %TPL_ENTRY_MSG_ID_ACKNOWLEDGED.
 *
 * Returns: %TRUE if the argument is a valid message ID or %FALSE otherwise.
 */

/**
 * TPL_ENTRY_MSG_ID_UNKNOWN:
 *
 * Special value used instead of a message ID to indicate a message with an
 * unknown status (before _tpl_entry_set_pending_msg_id() was called, or
 * when it wasn't possible to obtain the message ID).
 */

/**
 * TPL_ENTRY_MSG_ID_ACKNOWLEDGED:
 *
 * Special value used instead of a message ID to indicate an acknowledged
 * message.
 */

G_DEFINE_ABSTRACT_TYPE (TplEntry, tpl_entry, G_TYPE_OBJECT)

static void tpl_entry_set_log_id (TplEntry *self, const gchar *data);
static void tpl_entry_set_account_path (TplEntry *self,
    const gchar *data);

struct _TplEntryPriv
{
  gchar *log_id;
  gint64 timestamp;
  TplEntrySignalType signal_type;
  gchar *chat_id;
  gchar *account_path;
  gchar *channel_path;

  /* incoming/outgoing */
  TplEntryDirection direction;

  /* message and receiver may be NULL depending on the signal. ie. status
   * changed signals set only the sender */
  TplEntity *sender;
  TplEntity *receiver;
};

enum {
    PROP_TIMESTAMP = 1,
    PROP_LOG_ID,
    PROP_DIRECTION,
    PROP_CHAT_ID,
    PROP_ACCOUNT_PATH,
    PROP_CHANNEL_PATH,
    PROP_SENDER,
    PROP_RECEIVER
};


static void
tpl_entry_finalize (GObject *obj)
{
  TplEntry *self = TPL_ENTRY (obj);
  TplEntryPriv *priv = self->priv;

  g_free (priv->chat_id);
  priv->chat_id = NULL;
  g_free (priv->account_path);
  priv->account_path = NULL;

  G_OBJECT_CLASS (tpl_entry_parent_class)->finalize (obj);
}


static void
tpl_entry_dispose (GObject *obj)
{
  TplEntry *self = TPL_ENTRY (obj);
  TplEntryPriv *priv = self->priv;

  if (priv->sender != NULL)
    {
      g_object_unref (priv->sender);
      priv->sender = NULL;
    }
  if (priv->receiver != NULL)
    {
      g_object_unref (priv->receiver);
      priv->receiver = NULL;
    }

  G_OBJECT_CLASS (tpl_entry_parent_class)->dispose (obj);
}


static void
tpl_entry_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplEntry *self = TPL_ENTRY (object);
  TplEntryPriv *priv = self->priv;

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
      case PROP_CHAT_ID:
        g_value_set_string (value, priv->chat_id);
        break;
      case PROP_ACCOUNT_PATH:
        g_value_set_string (value, priv->account_path);
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
tpl_entry_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplEntry *self = TPL_ENTRY (object);

  switch (param_id) {
      case PROP_TIMESTAMP:
        _tpl_entry_set_timestamp (self, g_value_get_uint (value));
        break;
      case PROP_LOG_ID:
        tpl_entry_set_log_id (self, g_value_get_string (value));
        break;
      case PROP_DIRECTION:
        _tpl_entry_set_direction (self, g_value_get_uint (value));
        break;
      case PROP_CHAT_ID:
        _tpl_entry_set_chat_id (self, g_value_get_string (value));
        break;
      case PROP_ACCOUNT_PATH:
        tpl_entry_set_account_path (self, g_value_get_string (value));
        break;
      case PROP_CHANNEL_PATH:
        _tpl_entry_set_channel_path (self, g_value_get_string (value));
        break;
      case PROP_SENDER:
        _tpl_entry_set_sender (self, g_value_get_object (value));
        break;
      case PROP_RECEIVER:
        _tpl_entry_set_receiver (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}


static void
tpl_entry_class_init (TplEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  /* to be used by subclasses */
  object_class->finalize = tpl_entry_finalize;
  object_class->dispose = tpl_entry_dispose;
  object_class->get_property = tpl_entry_get_property;
  object_class->set_property = tpl_entry_set_property;

  klass->equal = NULL;

  param_spec = g_param_spec_uint ("timestamp",
      "Timestamp",
      "The timestamp (gint64) for the log entry",
      0, G_MAXUINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TIMESTAMP, param_spec);

  /**
   * TplEntry::log-id:
   *
   * A token identifying the entry.
   *
   */
  param_spec = g_param_spec_string ("log-id",
      "LogId",
      "Log identification token, it's unique among existing Entry, if two "
      "messages have the same token, they are the same entry (maybe logged "
      "by two different TplLogStore)",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOG_ID, param_spec);

  param_spec = g_param_spec_uint ("direction",
      "Direction",
      "The direction of the log entry (in/out)",
      0, G_MAXUINT32, TPL_ENTRY_DIRECTION_NONE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  param_spec = g_param_spec_string ("chat-id",
      "ChatId",
      "The chat identifier to which the log entry is related.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY  | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHAT_ID, param_spec);

  param_spec = g_param_spec_string ("account-path",
      "AccountPath",
      "The account path of the TpAccount to which the log entry is related",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT_PATH, param_spec);

  param_spec = g_param_spec_string ("channel-path",
      "ChannelPath",
      "The channel path of the TpChannel to which the log entry is related",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PATH, param_spec);

  param_spec = g_param_spec_object ("sender",
      "Sender",
      "TplEntity instance who originated the log entry",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDER, param_spec);

  param_spec = g_param_spec_object ("receiver",
      "Receiver",
      "TplEntity instance destination for the log entry",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECEIVER, param_spec);

  g_type_class_add_private (object_class, sizeof (TplEntryPriv));
  }


static void
tpl_entry_init (TplEntry *self)
{
  TplEntryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_ENTRY, TplEntryPriv);
  self->priv = priv;
}

/**
 * tpl_entry_get_timestamp
 * @self: a #TplEntry
 *
 * Returns: the same timestamp as the #TplEntry:timestamp property
 */
gint64
tpl_entry_get_timestamp (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), -1);

  return self->priv->timestamp;
}

TplEntrySignalType
_tpl_entry_get_signal_type (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), TPL_ENTRY_SIGNAL_NONE);

  return self->priv->signal_type;
}


const gchar *
_tpl_entry_get_log_id (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), 0);

  return self->priv->log_id;
}


TplEntryDirection
_tpl_entry_get_direction (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self),
      TPL_ENTRY_DIRECTION_NONE);

  return self->priv->direction;
}

/**
 * tpl_entry_get_sender
 * @self: a #TplEntry
 *
 * Returns: the same #TplEntity as the #TplEntry:sender property
 */
TplEntity *
tpl_entry_get_sender (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), NULL);

  return self->priv->sender;
}

/**
 * tpl_entry_get_receiver
 * @self: a #TplEntry
 *
 * Returns: the same #TplEntity as the #TplEntry:receiver property
 */
TplEntity *
tpl_entry_get_receiver (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), NULL);

  return self->priv->receiver;
}


const gchar *
_tpl_entry_get_chat_id (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), NULL);

  return self->priv->chat_id;
}


/**
 * tpl_entry_get_account_path
 * @self: a #TplEntry
 *
 * Returns: the same path as the #TplEntry:account-path property
 */
const gchar *
tpl_entry_get_account_path (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), NULL);

  return self->priv->account_path;
}


const gchar *
_tpl_entry_get_channel_path (TplEntry *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), NULL);

  return self->priv->channel_path;
}


void
_tpl_entry_set_timestamp (TplEntry *self,
    gint64 data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));

  self->priv->timestamp = data;
  g_object_notify (G_OBJECT (self), "timestamp");
}


void
_tpl_entry_set_signal_type (TplEntry *self,
    TplEntrySignalType data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));

  self->priv->signal_type = data;
  g_object_notify (G_OBJECT (self), "signal-type");
}

/* set just on construction time */
static void
tpl_entry_set_log_id (TplEntry *self,
    const gchar* data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->log_id == NULL);

  self->priv->log_id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "log-id");
}


void
_tpl_entry_set_direction (TplEntry *self,
    TplEntryDirection data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));

  self->priv->direction = data;
  g_object_notify (G_OBJECT (self), "direction");
}


void
_tpl_entry_set_sender (TplEntry *self,
    TplEntity *data)
{
  TplEntryPriv *priv;

  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (TPL_IS_ENTITY (data));

  priv = self->priv;

  if (priv->sender != NULL)
    g_object_unref (priv->sender);
  priv->sender = g_object_ref (data);
  g_object_notify (G_OBJECT (self), "sender");
}


void
_tpl_entry_set_receiver (TplEntry *self,
    TplEntity *data)
{
  TplEntryPriv *priv;

  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (TPL_IS_ENTITY (data));

  priv = self->priv;

  if (priv->receiver != NULL)
    g_object_unref (priv->receiver);

  priv->receiver = g_object_ref (data);

  g_object_notify (G_OBJECT (self), "receiver");
}


void
_tpl_entry_set_chat_id (TplEntry *self,
    const gchar *data)
{
  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->chat_id == NULL);

  self->priv->chat_id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "chat-id");
}


static void
tpl_entry_set_account_path (TplEntry *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->account_path == NULL);

  self->priv->account_path = g_strdup (data);
  g_object_notify (G_OBJECT (self), "account-path");
}


void
_tpl_entry_set_channel_path (TplEntry *self,
    const gchar *data)
{
  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->channel_path == NULL);

  self->priv->channel_path = g_strdup (data);
  g_object_notify (G_OBJECT (self), "channel-path");
}

/**
 * _tpl_entry_equal:
 * @self: TplEntry subclass instance
 * @data: an instance of the same TplEntry subclass of @self
 *
 * Checks if two instances of TplEntry represent the same data
 *
 * Returns: %TRUE if @data is the same type of @self and they hold the same
 * data, %FALSE otherwise
 */
gboolean
_tpl_entry_equal (TplEntry *self,
    TplEntry *data)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), FALSE);
  g_return_val_if_fail (TPL_IS_ENTRY (data), FALSE);

  return TPL_ENTRY_GET_CLASS (self)->equal (self, data);
}
