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

#include <telepathy-logger/contact.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_ENTRY
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:entry
 * @title: TplLogEntry
 * @short_description: Abstract representation of a log entry
 * @see_also: #TplLogEntryText and other subclasses when they'll exist
 *
 * The TPLogger log entry represent a generic log entry, which will be
 * specialied by subclasses of #TplLogEntry.
 */

/**
 * TPL_LOG_ENTRY_MSG_ID_IS_VALID:
 * @msg: a message ID
 *
 * Return whether a message ID is valid.
 *
 * If %FALSE is returned, it means that either a invalid input has been
 * passed, or the TplLogEntry is currently set to %TPL_LOG_ENTRY_MSG_ID_UNKNOWN
 * or %TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED.
 *
 * Returns: %TRUE if the argument is a valid message ID or %FALSE otherwise.
 */

/**
 * TPL_LOG_ENTRY_MSG_ID_UNKNOWN:
 *
 * Special value used instead of a message ID to indicate a message with an
 * unknown status (before _tpl_log_entry_set_pending_msg_id() was called, or
 * when it wasn't possible to obtain the message ID).
 */

/**
 * TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED:
 *
 * Special value used instead of a message ID to indicate an acknowledged
 * message.
 */

G_DEFINE_ABSTRACT_TYPE (TplLogEntry, tpl_log_entry, G_TYPE_OBJECT)

static void tpl_log_entry_set_log_id (TplLogEntry *self, const gchar *data);
static void tpl_log_entry_set_account_path (TplLogEntry *self,
    const gchar *data);

struct _TplLogEntryPriv
{
  gchar *log_id;
  gint64 timestamp;
  TplLogEntrySignalType signal_type;
  gchar *chat_id;
  gchar *account_path;
  gchar *channel_path;
  /* in specs it's guint, TplLogEntry needs a way to represent ACK'd messages:
   * if pending_msg_id reachs G_MAXINT32, then the problem is elsewhere :-) */
  gint pending_msg_id;

  /* incoming/outgoing */
  TplLogEntryDirection direction;

  /* message and receiver may be NULL depending on the signal. ie. status
   * changed signals set only the sender */
  TplContact *sender;
  TplContact *receiver;
};

enum {
    PROP_TIMESTAMP = 1,
    PROP_SIGNAL_TYPE,
    PROP_LOG_ID,
    PROP_PENDING_MSG_ID,
    PROP_DIRECTION,
    PROP_CHAT_ID,
    PROP_ACCOUNT_PATH,
    PROP_CHANNEL_PATH,
    PROP_SENDER,
    PROP_RECEIVER
};


static void
tpl_log_entry_finalize (GObject *obj)
{
  TplLogEntry *self = TPL_LOG_ENTRY (obj);
  TplLogEntryPriv *priv = self->priv;

  g_free (priv->chat_id);
  priv->chat_id = NULL;
  g_free (priv->account_path);
  priv->account_path = NULL;

  G_OBJECT_CLASS (tpl_log_entry_parent_class)->finalize (obj);
}


static void
tpl_log_entry_dispose (GObject *obj)
{
  TplLogEntry *self = TPL_LOG_ENTRY (obj);
  TplLogEntryPriv *priv = self->priv;

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

  G_OBJECT_CLASS (tpl_log_entry_parent_class)->dispose (obj);
}


static void
tpl_log_entry_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogEntry *self = TPL_LOG_ENTRY (object);
  TplLogEntryPriv *priv = self->priv;

  switch (param_id)
    {
      case PROP_TIMESTAMP:
        g_value_set_uint (value, priv->timestamp);
        break;
      case PROP_SIGNAL_TYPE:
        g_value_set_uint (value, priv->signal_type);
        break;
      case PROP_PENDING_MSG_ID:
        g_value_set_int (value, priv->pending_msg_id);
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
tpl_log_entry_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogEntry *self = TPL_LOG_ENTRY (object);

  switch (param_id) {
      case PROP_TIMESTAMP:
        _tpl_log_entry_set_timestamp (self, g_value_get_uint (value));
        break;
      case PROP_SIGNAL_TYPE:
        _tpl_log_entry_set_signal_type (self, g_value_get_uint (value));
        break;
      case PROP_PENDING_MSG_ID:
        _tpl_log_entry_set_pending_msg_id (self, g_value_get_int (value));
        break;
      case PROP_LOG_ID:
        tpl_log_entry_set_log_id (self, g_value_get_string (value));
        break;
      case PROP_DIRECTION:
        _tpl_log_entry_set_direction (self, g_value_get_uint (value));
        break;
      case PROP_CHAT_ID:
        _tpl_log_entry_set_chat_id (self, g_value_get_string (value));
        break;
      case PROP_ACCOUNT_PATH:
        tpl_log_entry_set_account_path (self, g_value_get_string (value));
        break;
      case PROP_CHANNEL_PATH:
        _tpl_log_entry_set_channel_path (self, g_value_get_string (value));
        break;
      case PROP_SENDER:
        _tpl_log_entry_set_sender (self, g_value_get_object (value));
        break;
      case PROP_RECEIVER:
        _tpl_log_entry_set_receiver (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}


static void
tpl_log_entry_class_init (TplLogEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  /* to be used by subclasses */
  object_class->finalize = tpl_log_entry_finalize;
  object_class->dispose = tpl_log_entry_dispose;
  object_class->get_property = tpl_log_entry_get_property;
  object_class->set_property = tpl_log_entry_set_property;

  klass->equal = NULL;

  param_spec = g_param_spec_uint ("timestamp",
      "Timestamp",
      "The timestamp (gint64) for the log entry",
      0, G_MAXUINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TIMESTAMP, param_spec);

  param_spec = g_param_spec_uint ("signal-type",
      "SignalType",
      "The signal type which caused the log entry",
      0, G_MAXUINT32, TPL_LOG_ENTRY_SIGNAL_NONE, G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIGNAL_TYPE, param_spec);

  /**
   * TplLogEntry::pending-msg-id:
   *
   * The pending message id for the current log entry.
   * The default value, is #TPL_LOG_ENTRY_MSG_ID_UNKNOWN,
   * meaning that it's not possible to know if the message is pending or has
   * been acknowledged.
   *
   * An object instantiating a TplLogEntry subclass should explicitly set it
   * to a valid msg-id number (id>=0) or to #TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED
   * when acknowledged or if the entry is a result of
   * 'sent' signal.
   * In fact a sent entry is considered as 'automatically' ACK by TPL.
   *
   * The pending message id value is only meaningful when associated to the
   * #TplLogEntry::channel-path property.
   * The couple (channel-path, pending-msg-id) cannot be considered unique,
   * though, since a message-id might be reused over time.
   *
   * Use #TplLogEntry::log-id for a unique identifier within TPL.
   */
  param_spec = g_param_spec_int ("pending-msg-id",
      "PendingMessageId",
      "Pending Message ID, if set, the log entry is set as pending for ACK."
      " Default to -1 meaning not pending.",
      -1, G_MAXUINT32, TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PENDING_MSG_ID,
      param_spec);

  /**
   * TplLogEntry::log-id:
   *
   * A token which can be trusted as unique over time within TPL.
   *
   * Always use this token if you need to identify a TplLogEntry uniquely.
   *
   * @see_also: #Util:create_message_token for more information about how it's
   * built.
   */
  param_spec = g_param_spec_string ("log-id",
      "LogId",
      "Log identification token, it's unique among existing LogEntry, if two "
      "messages have the same token, they are the same entry (maybe logged "
      "by two different TplLogStore)",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOG_ID, param_spec);

  param_spec = g_param_spec_uint ("direction",
      "Direction",
      "The direction of the log entry (in/out)",
      0, G_MAXUINT32, TPL_LOG_ENTRY_DIRECTION_NONE, G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  /* FIXME: G_PARAM_CONSTRUCT and not G_PARAM_CONSTRUCT_ONLY because it needs to be
   * set not at instance time in channel_text.c on_received_signal.
   * It would be much better using G_PARAM_CONSTRUCT_ONLY */
  param_spec = g_param_spec_string ("chat-id",
      "ChatId",
      "The chat identifier to which the log entry is related.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
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
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PATH, param_spec);

  param_spec = g_param_spec_object ("sender",
      "Sender",
      "TplContact instance who originated the log entry",
      TPL_TYPE_CONTACT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDER, param_spec);

  param_spec = g_param_spec_object ("receiver",
      "Receiver",
      "TplContact instance destination for the log entry",
      TPL_TYPE_CONTACT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECEIVER, param_spec);

  g_type_class_add_private (object_class, sizeof (TplLogEntryPriv));
  }


static void
tpl_log_entry_init (TplLogEntry *self)
{
  TplLogEntryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_ENTRY, TplLogEntryPriv);
  self->priv = priv;
}


gint64
tpl_log_entry_get_timestamp (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), -1);

  return self->priv->timestamp;
}


gint
tpl_log_entry_get_pending_msg_id (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), -1);

  return self->priv->pending_msg_id;
}


gboolean
_tpl_log_entry_is_pending (TplLogEntry *self)
{
  return TPL_LOG_ENTRY_MSG_ID_IS_VALID (
      tpl_log_entry_get_pending_msg_id (self));
}


TplLogEntrySignalType
_tpl_log_entry_get_signal_type (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), TPL_LOG_ENTRY_SIGNAL_NONE);

  return self->priv->signal_type;
}


const gchar *
_tpl_log_entry_get_log_id (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), 0);

  return self->priv->log_id;
}


TplLogEntryDirection
_tpl_log_entry_get_direction (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self),
      TPL_LOG_ENTRY_DIRECTION_NONE);

  return self->priv->direction;
}


TplContact *
tpl_log_entry_get_sender (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  return self->priv->sender;
}


TplContact *
tpl_log_entry_get_receiver (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  return self->priv->receiver;
}


const gchar *
_tpl_log_entry_get_chat_id (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  return self->priv->chat_id;
}


const gchar *
tpl_log_entry_get_account_path (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  return self->priv->account_path;
}


const gchar *
_tpl_log_entry_get_channel_path (TplLogEntry *self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  return self->priv->channel_path;
}


void
_tpl_log_entry_set_timestamp (TplLogEntry *self,
    gint64 data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  self->priv->timestamp = data;
  g_object_notify (G_OBJECT (self), "timestamp");
}


void
_tpl_log_entry_set_signal_type (TplLogEntry *self,
    TplLogEntrySignalType data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  self->priv->signal_type = data;
  g_object_notify (G_OBJECT (self), "signal-type");
}

/**
 * _tpl_log_entry_set_pending_msg_id:
 * @self: TplLogentry instance
 * @data: the pending message ID
 *
 * Sets @self to be associated to pending message id @data.
 *
 * @see_also: #TplLogEntry::pending-msg-id for special values.
 */
void
_tpl_log_entry_set_pending_msg_id (TplLogEntry *self,
    gint data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  self->priv->pending_msg_id = data;
  g_object_notify (G_OBJECT (self), "pending-msg-id");
}


/* set just on construction time */
static void
tpl_log_entry_set_log_id (TplLogEntry *self,
    const gchar* data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->log_id == NULL);

  self->priv->log_id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "log-id");
}


void
_tpl_log_entry_set_direction (TplLogEntry *self,
    TplLogEntryDirection data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  self->priv->direction = data;
  g_object_notify (G_OBJECT (self), "direction");
}


void
_tpl_log_entry_set_sender (TplLogEntry *self,
    TplContact *data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  priv = self->priv;

  if (priv->sender != NULL)
    g_object_unref (priv->sender);
  priv->sender = g_object_ref (data);
  g_object_notify (G_OBJECT (self), "sender");
}


void
_tpl_log_entry_set_receiver (TplLogEntry *self,
    TplContact *data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  priv = self->priv;

  if (priv->receiver != NULL)
    g_object_unref (priv->receiver);

  priv->receiver = g_object_ref (data);

  g_object_notify (G_OBJECT (self), "receiver");
}


void
_tpl_log_entry_set_chat_id (TplLogEntry *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->chat_id == NULL);

  self->priv->chat_id = g_strdup (data);
  g_object_notify (G_OBJECT (self), "chat-id");
}


static void
tpl_log_entry_set_account_path (TplLogEntry *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->account_path == NULL);

  self->priv->account_path = g_strdup (data);
  g_object_notify (G_OBJECT (self), "account-path");
}


void
_tpl_log_entry_set_channel_path (TplLogEntry *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->channel_path == NULL);

  self->priv->channel_path = g_strdup (data);
  g_object_notify (G_OBJECT (self), "channel-path");
}

/**
 * _tpl_log_entry_equal:
 * @self: TplLogEntry subclass instance
 * @data: an instance of the same TplLogEntry subclass of @self
 *
 * Checks if two instances of TplLogEntry represent the same data
 *
 * Returns: %TRUE if @data is the same type of @self and they hold the same
 * data, %FALSE otherwise
 */
gboolean
_tpl_log_entry_equal (TplLogEntry *self,
    TplLogEntry *data)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), FALSE);
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (data), FALSE);

  return TPL_LOG_ENTRY_GET_CLASS (self)->equal (self, data);
}
