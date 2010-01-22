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

#include "log-entry.h"

#include <glib.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/contact.h>
#include <telepathy-logger/debug.h>
#include <telepathy-logger/util.h>

G_DEFINE_TYPE (TplLogEntry, tpl_log_entry, G_TYPE_OBJECT)

static void _set_log_id (TplLogEntry * self, guint data);

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplLogEntry)
struct _TplLogEntryPriv
{
  guint log_id;
  time_t timestamp;
  TplLogEntrySignalType signal_type;
  gchar *chat_id;

  /* incoming/outgoing */
  TplLogEntryDirection direction;

  /* message and receiver may be NULL depending on the signal. ie. status
   * changed signals set only the sender */
  TplContact *sender;
  TplContact *receiver;
};

enum {
    PROP0,
    PROP_TIMESTAMP,
    PROP_SIGNAL_TYPE,
    PROP_LOG_ID,
    PROP_DIRECTION,
    PROP_CHAT_ID,
    PROP_SENDER,
    PROP_RECEIVER
};

static void tpl_log_entry_finalize (GObject * obj)
{
  TplLogEntryPriv *priv = GET_PRIV (obj);

  g_free (priv->chat_id);
  priv->chat_id = NULL;

  G_OBJECT_CLASS (tpl_log_entry_parent_class)->finalize (obj);
}

static void
tpl_log_entry_dispose (GObject * obj)
{
  TplLogEntry *self = TPL_LOG_ENTRY (obj);
  TplLogEntryPriv *priv = GET_PRIV (self);

  tpl_object_unref_if_not_null (priv->sender);
  priv->sender = NULL;
  tpl_object_unref_if_not_null (priv->receiver);
  priv->receiver = NULL;

  G_OBJECT_CLASS (tpl_log_entry_parent_class)->dispose (obj);
}

static void
tpl_log_entry_get_prop (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec)
{
  TplLogEntryPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_TIMESTAMP:
        g_value_set_uint (value, priv->timestamp);
        break;
      case PROP_SIGNAL_TYPE:
        g_value_set_uint (value, priv->signal_type);
        break;
      case PROP_LOG_ID:
        g_value_set_uint (value, priv->log_id);
        break;
      case PROP_DIRECTION:
        g_value_set_uint (value, priv->direction);
        break;
      case PROP_CHAT_ID:
        g_value_set_string (value, priv->chat_id);
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
tpl_log_entry_set_prop (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec)
{
  TplLogEntry *self = TPL_LOG_ENTRY (object);

  switch (param_id) {
      case PROP_TIMESTAMP:
        tpl_log_entry_set_timestamp (self, g_value_get_uint (value));
        break;
      case PROP_SIGNAL_TYPE:
        tpl_log_entry_set_signal_type (self, g_value_get_uint (value));
        break;
      case PROP_LOG_ID:
        _set_log_id (self, g_value_get_uint (value));
        break;
      case PROP_DIRECTION:
        tpl_log_entry_set_direction (self, g_value_get_uint (value));
        break;
      case PROP_CHAT_ID:
        tpl_log_entry_set_chat_id (self, g_value_get_string (value));
        break;
      case PROP_SENDER:
        tpl_log_entry_set_sender (self, g_value_get_object (value));
        break;
      case PROP_RECEIVER:
        tpl_log_entry_set_receiver (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}


static void
tpl_log_entry_class_init (TplLogEntryClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = tpl_log_entry_get_prop;
  object_class->set_property = tpl_log_entry_set_prop;
  object_class->finalize = tpl_log_entry_finalize;
  object_class->dispose = tpl_log_entry_dispose;

  param_spec = g_param_spec_uint ("timestamp",
      "Timestamp",
      "The timestamp (time_t) for the log entry",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TIMESTAMP, param_spec);

  param_spec = g_param_spec_uint ("signal-type",
      "SignalType",
      "The signal type which caused the log entry",
      0, G_MAXUINT32, TPL_LOG_ENTRY_SIGNAL_NONE,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIGNAL_TYPE, param_spec);

  param_spec = g_param_spec_uint ("log-id",
      "LogId",
      "Log identification number: the triple LogId+AccountName+ChatId is unique",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOG_ID, param_spec);

  param_spec = g_param_spec_uint ("direction",
      "Direction",
      "The direction of the log entry (in/out)",
      0, G_MAXUINT32, TPL_LOG_ENTRY_DIRECTION_NONE,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  param_spec = g_param_spec_string ("chat-id",
      "ChatId",
      "The chat id relative to the log entry's account name",
      NULL,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHAT_ID, param_spec);

  param_spec = g_param_spec_object ("sender",
      "Sender",
      "TplContact instance who originated the log entry",
      TPL_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDER, param_spec);

  param_spec = g_param_spec_object ("receiver",
      "Receiver",
      "TplContact instance destination for the log entry",
      TPL_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECEIVER, param_spec);

  g_type_class_add_private (object_class, sizeof (TplLogEntryPriv));
}

static void
tpl_log_entry_init (TplLogEntry * self)
{
  TplLogEntryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_ENTRY, TplLogEntryPriv);
  self->priv = priv;
}

TplLogEntry *
tpl_log_entry_new (guint log_id, const gchar *chat_id,
    TplLogEntryDirection direction)
{
  return g_object_new (TPL_TYPE_LOG_ENTRY,
      "log-id", log_id,
	    "chat-id", chat_id,
      "direction", direction,
      NULL);
}

time_t
tpl_log_entry_get_timestamp (TplLogEntry * self)
{
  TplLogEntryPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), -1);

  priv = GET_PRIV (self);
  return priv->timestamp;
}

TplLogEntrySignalType
tpl_log_entry_get_signal_type (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self),
      TPL_LOG_ENTRY_SIGNAL_NONE);

  priv = GET_PRIV (self);
  return priv->signal_type;
}

guint
tpl_log_entry_get_log_id (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), 0);

  priv = GET_PRIV (self);
  return priv->log_id;
}

TplLogEntryDirection
tpl_log_entry_get_direction (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self),
      TPL_LOG_ENTRY_DIRECTION_NONE);

  priv = GET_PRIV (self);
  return priv->direction;
}

TplContact *
tpl_log_entry_get_sender (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  priv = GET_PRIV (self);
  return priv->sender;
}

TplContact *
tpl_log_entry_get_receiver (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  priv = GET_PRIV (self);
  return priv->receiver;
}

const gchar *
tpl_log_entry_get_chat_id (TplLogEntry * self)
{
  TplLogEntryPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  priv = GET_PRIV (self);
  return priv->chat_id;
}
void
tpl_log_entry_set_timestamp (TplLogEntry * self, time_t data)
{
  TplLogEntryPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  priv = GET_PRIV (self);
  priv->timestamp = data;
  g_object_notify (G_OBJECT(self), "timestamp");
}

void
tpl_log_entry_set_signal_type (TplLogEntry * self,
    TplLogEntrySignalType data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  priv = GET_PRIV (self);
  priv->signal_type = data;
  g_object_notify (G_OBJECT(self), "signal-type");
}

static void
_set_log_id (TplLogEntry * self, guint data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  priv = GET_PRIV (self);
  priv->log_id = data;
  g_object_notify (G_OBJECT(self), "log-id");
}

void
tpl_log_entry_set_direction (TplLogEntry * self,
    TplLogEntryDirection data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  priv = GET_PRIV (self);
  priv->direction = data;
  g_object_notify (G_OBJECT(self), "direction");
}

void
tpl_log_entry_set_sender (TplLogEntry * self, TplContact * data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  priv = GET_PRIV (self);

  tpl_object_unref_if_not_null (priv->sender);
  priv->sender = data;
  tpl_object_ref_if_not_null (data);
  g_object_notify (G_OBJECT(self), "sender");
}

void
tpl_log_entry_set_receiver (TplLogEntry * self, TplContact * data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  priv = GET_PRIV (self);
  tpl_object_unref_if_not_null (priv->receiver);
  priv->receiver = data;
  tpl_object_ref_if_not_null (data);
  g_object_notify (G_OBJECT(self), "receiver");
}

void
tpl_log_entry_set_chat_id (TplLogEntry * self, const gchar * data)
{
  TplLogEntryPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  priv = GET_PRIV (self);
  g_free (priv->chat_id);
  priv->chat_id = g_strdup (data);
}

gboolean
tpl_log_entry_equal (TplLogEntry *message1, TplLogEntry *message2)
{
  TplLogEntryPriv *priv1 = GET_PRIV (message1);
  TplLogEntryPriv *priv2 = GET_PRIV (message2);

  g_return_val_if_fail (TPL_IS_LOG_ENTRY (message1), FALSE);
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (message2), FALSE);

  //if (priv1->id == priv2->id && !tp_strdiff (priv1->body, priv2->body)) {
  //if (priv1->type == priv2->type)
    //if (!tp_strdiff (priv1->entry.text->message, priv2->entry.text->message)) {
    //}
  g_debug ("TODO: tpl_log_entry_equal update!");
  return priv1->log_id == priv2->log_id;

  return FALSE;
}
