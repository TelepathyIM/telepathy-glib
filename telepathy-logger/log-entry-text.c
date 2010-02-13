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
#include "log-entry-text.h"

#include <glib-object.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/util.h>
#include <telepathy-logger/log-entry.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug.h>

G_DEFINE_TYPE (TplLogEntryText, tpl_log_entry_text, TPL_TYPE_LOG_ENTRY)

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplLogEntryText)
struct _TplLogEntryTextPriv
{
  TplChannelText *tpl_text;
  TpChannelTextMessageType message_type;
  gchar *message;
  gboolean chatroom;
};

enum
{
  PROP_MESSAGE_TYPE = 1,
  PROP_MESSAGE,
  PROP_TPL_CHANNEL_TEXT
};


static void
tpl_log_entry_text_dispose (GObject * obj)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (obj);
  TplLogEntryTextPriv *priv = GET_PRIV (self);

  tpl_object_unref_if_not_null (priv->tpl_text);
  priv->tpl_text = NULL;

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->dispose (obj);
}


static void
tpl_log_entry_text_finalize (GObject * obj)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (obj);
  TplLogEntryTextPriv *priv = GET_PRIV (self);

  g_free (priv->message);
  priv->message = NULL;

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->finalize (obj);
}


static void
tpl_log_entry_text_get_prop (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogEntryTextPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_MESSAGE_TYPE:
        g_value_set_uint (value, priv->message_type);
        break;
      case PROP_MESSAGE:
        g_value_set_string (value, priv->message);
        break;
      case PROP_TPL_CHANNEL_TEXT:
        g_value_set_object (value, priv->tpl_text);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}


static void
tpl_log_entry_text_set_prop (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (object);

  switch (param_id) {
      case PROP_MESSAGE_TYPE:
        tpl_log_entry_text_set_message_type (self, g_value_get_uint (value));
        break;
      case PROP_MESSAGE:
        tpl_log_entry_text_set_message (self, g_value_get_string (value));
        break;
      case PROP_TPL_CHANNEL_TEXT:
        tpl_log_entry_text_set_tpl_channel_text (self,
            g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  }
}


static void tpl_log_entry_text_class_init (TplLogEntryTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplLogEntryClass *log_entry_class = TPL_LOG_ENTRY_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_log_entry_text_finalize;
  object_class->dispose = tpl_log_entry_text_dispose;
  object_class->get_property = tpl_log_entry_text_get_prop;
  object_class->set_property = tpl_log_entry_text_set_prop;
  log_entry_class->equal = tpl_log_entry_text_equal;

  param_spec = g_param_spec_uint ("message-type",
      "MessageType",
      "The message type for a Text log entry",
      0, G_MAXUINT32, TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE_TYPE, param_spec);

  param_spec = g_param_spec_string ("message",
      "Message",
      "The text message of the log entry",
      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE, param_spec);

  param_spec = g_param_spec_object ("tpl-channel-text",
      "TplChannelText",
      "The TplChannelText instance associated with the log entry, if any",
      TPL_TYPE_CHANNEL_TEXT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TPL_CHANNEL_TEXT, param_spec);

  g_type_class_add_private (object_class, sizeof (TplLogEntryTextPriv));

}


static void
tpl_log_entry_text_init (TplLogEntryText * self)
{
  TplLogEntryTextPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextPriv);
  self->priv = priv;
}


TplLogEntryText *
tpl_log_entry_text_new (guint log_id, const gchar *chat_id,
    TplLogEntryDirection direction)
{
  return g_object_new (TPL_TYPE_LOG_ENTRY_TEXT,
      "log-id", log_id,
      "chat-id", chat_id,
      "direction", direction,
      NULL);
}

static gchar *message_types[] = {
    "normal",
    "action",
    "notice",
    "auto-reply",
    "delivery-report",
    NULL };


/**
 * tpl_log_entry_text_message_type_to_str
 * @type_str: string to transform into a #TpChannelTextMessageType
 *
 * Maps strings into enum #TpChannelTextMessageType values.
 *
 * Returns: the relative value from enum #TpChannelTextMessageType if a
 * mapping is found, or defaults to %TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL if not.
 */
TpChannelTextMessageType
tpl_log_entry_text_message_type_from_str (const gchar *type_str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (message_types); ++i)
    if (!tp_strdiff (type_str, message_types[i]))
      return (TpChannelTextMessageType) i;

  /* default case */
  return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}


/**
 * tpl_log_entry_text_message_type_to_str
 * @msg_type: message type to transform into a string
 *
 * Maps enum #TpChannelTextMessageType values into strings
 *
 * Returns: a string representation for @msg_type or NULL if @msg_type is not
 * a legal value for %TpChannelTextMessageType.
 */
const gchar *
tpl_log_entry_text_message_type_to_str (TpChannelTextMessageType msg_type)
{
  g_return_val_if_fail (G_N_ELEMENTS (message_types) >= msg_type, NULL);

  return message_types[msg_type];
}


gboolean
tpl_log_entry_text_is_chatroom (TplLogEntryText * self)
{
  TplLogEntryTextPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), FALSE);

  priv = GET_PRIV (self);
  return priv->chatroom;
}


TplChannelText *
tpl_log_entry_text_get_tpl_channel_text (TplLogEntryText * self)
{
  TplLogEntryTextPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);

  priv = GET_PRIV (self);
  return priv->tpl_text;
}


const gchar *
tpl_log_entry_text_get_message (TplLogEntryText * self)
{
  TplLogEntryTextPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);

  priv = GET_PRIV (self);
  return priv->message;
}


TpChannelTextMessageType
tpl_log_entry_text_get_message_type (TplLogEntryText * self)
{
  TplLogEntryTextPriv *priv;

  /* TODO is TYPE_NORMAL the right value to return in case of error? I doubt
   * :) */
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self),
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

  priv = GET_PRIV (self);
  return priv->message_type;
}


void
tpl_log_entry_text_set_tpl_channel_text (TplLogEntryText * self,
    TplChannelText *data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_CHANNEL_TEXT (data) || data == NULL);

  priv = GET_PRIV (self);
  tpl_object_unref_if_not_null (priv->tpl_text);
  priv->tpl_text = data;
  tpl_object_ref_if_not_null (data);
}


void
tpl_log_entry_text_set_message (TplLogEntryText *self, const gchar *data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  priv = GET_PRIV (self);

  g_free (priv->message);
  priv->message = g_strdup (data);
}


void
tpl_log_entry_text_set_message_type (TplLogEntryText *self,
    TpChannelTextMessageType data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  priv = GET_PRIV (self);

  priv->message_type = data;
}


void
tpl_log_entry_text_set_chatroom (TplLogEntryText *self,
    gboolean data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  priv = GET_PRIV (self);

  priv->chatroom = data;
}

/* Methods inherited by TplLogEntry */

time_t
tpl_log_entry_text_get_timestamp (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_timestamp (logentry);
}


TplLogEntrySignalType
tpl_log_entry_text_get_signal_type (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_signal_type (logentry);
}


guint
tpl_log_entry_text_get_log_id (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_log_id (logentry);
}


const gchar *
tpl_log_entry_text_get_chat_id (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_chat_id (logentry);
}


TplLogEntryDirection
tpl_log_entry_text_get_direction (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_direction (logentry);
}


TplContact *
tpl_log_entry_text_get_sender (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_sender (logentry);
}


TplContact *
tpl_log_entry_text_get_receiver (TplLogEntryText *self)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  return TPL_LOG_ENTRY_GET_CLASS (self)->get_receiver (logentry);
}


void
tpl_log_entry_text_set_timestamp (TplLogEntryText *self, time_t data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_timestamp (logentry, data);
}


void
tpl_log_entry_text_set_signal_type (TplLogEntryText *self,
  TplLogEntrySignalType data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_signal_type (logentry, data);
}

void
tpl_log_entry_text_set_direction (TplLogEntryText *self,
    TplLogEntryDirection data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_direction (logentry, data);
}


void
tpl_log_entry_text_set_chat_id (TplLogEntryText *self, const gchar *data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_chat_id (logentry, data);
}


void
tpl_log_entry_text_set_sender (TplLogEntryText *self, TplContact *data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_sender (logentry, data);
}


void
tpl_log_entry_text_set_receiver (TplLogEntryText *self, TplContact *data)
{
  TplLogEntry *logentry = TPL_LOG_ENTRY (self);
  TPL_LOG_ENTRY_GET_CLASS (self)->set_receiver (logentry, data);
}


gboolean
tpl_log_entry_text_equal (TplLogEntry *message1,
    TplLogEntry *message2)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (message1), FALSE);
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (message2), FALSE);

  /*
  if (priv1->id == priv2->id && !tp_strdiff (priv1->body, priv2->body)) {
  if (priv1->type == priv2->type)
    if (!tp_strdiff (priv1->entry.text->message, priv2->entry.text->message)) {
    }
  */
  DEBUG ("TODO: do a tpl_log_entry_equal rewrite!");
  return tpl_log_entry_text_get_log_id (TPL_LOG_ENTRY_TEXT (message1)) ==
      tpl_log_entry_text_get_log_id ( TPL_LOG_ENTRY_TEXT (message2));
}
