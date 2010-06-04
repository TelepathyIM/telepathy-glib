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
#include "log-entry-text-internal.h"

#include <glib-object.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/log-entry.h>
#include <telepathy-logger/log-entry-internal.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

G_DEFINE_TYPE (TplLogEntryText, tpl_log_entry_text, TPL_TYPE_LOG_ENTRY)

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
  TplLogEntryTextPriv *priv = self->priv;

  if (priv->tpl_text != NULL)
    {
      g_object_unref (priv->tpl_text);
      priv->tpl_text = NULL;
    }

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->dispose (obj);
}


static void
tpl_log_entry_text_finalize (GObject * obj)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (obj);
  TplLogEntryTextPriv *priv = self->priv;

  g_free (priv->message);
  priv->message = NULL;

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->finalize (obj);
}


static void
tpl_log_entry_text_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogEntryTextPriv *priv = TPL_LOG_ENTRY_TEXT (object)->priv;

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
tpl_log_entry_text_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (object);

  switch (param_id) {
      case PROP_MESSAGE_TYPE:
        _tpl_log_entry_text_set_message_type (self, g_value_get_uint (value));
        break;
      case PROP_MESSAGE:
        _tpl_log_entry_text_set_message (self, g_value_get_string (value));
        break;
      case PROP_TPL_CHANNEL_TEXT:
        _tpl_log_entry_text_set_tpl_channel_text (self,
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
  object_class->get_property = tpl_log_entry_text_get_property;
  object_class->set_property = tpl_log_entry_text_set_property;
  log_entry_class->equal = _tpl_log_entry_text_equal;

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
tpl_log_entry_text_init (TplLogEntryText *self)
{
  TplLogEntryTextPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextPriv);
  self->priv = priv;
}


TplLogEntryText *
_tpl_log_entry_text_new (const gchar *log_id,
    const gchar *account_path,
    TplLogEntryDirection direction)
{
  return g_object_new (TPL_TYPE_LOG_ENTRY_TEXT,
      "log-id", log_id,
      "account-path", account_path,
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
 * _tpl_log_entry_text_message_type_from_str
 * @type_str: string to transform into a #TpChannelTextMessageType
 *
 * Maps strings into enum #TpChannelTextMessageType values.
 *
 * Returns: the relative value from enum #TpChannelTextMessageType if a
 * mapping is found, or defaults to %TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL if not.
 */
TpChannelTextMessageType
_tpl_log_entry_text_message_type_from_str (const gchar *type_str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (message_types); ++i)
    if (!tp_strdiff (type_str, message_types[i]))
      return (TpChannelTextMessageType) i;

  /* default case */
  return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}


/**
 * _tpl_log_entry_text_message_type_to_str
 * @msg_type: message type to transform into a string
 *
 * Maps enum #TpChannelTextMessageType values into strings
 *
 * Returns: a string representation for @msg_type or NULL if @msg_type is not
 * a legal value for %TpChannelTextMessageType.
 */
const gchar *
_tpl_log_entry_text_message_type_to_str (TpChannelTextMessageType msg_type)
{
  g_return_val_if_fail (G_N_ELEMENTS (message_types) >= msg_type, NULL);

  return message_types[msg_type];
}


gboolean
_tpl_log_entry_text_is_chatroom (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), FALSE);

  return self->priv->chatroom;
}


TplChannelText *
_tpl_log_entry_text_get_tpl_channel_text (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);

  return self->priv->tpl_text;
}


const gchar *
tpl_log_entry_text_get_message (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);

  return self->priv->message;
}


TpChannelTextMessageType
_tpl_log_entry_text_get_message_type (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self),
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

  return self->priv->message_type;
}


void
_tpl_log_entry_text_set_tpl_channel_text (TplLogEntryText * self,
    TplChannelText *data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_CHANNEL_TEXT (data) || data == NULL);

  priv = self->priv;
  if (priv->tpl_text != NULL)
    g_object_unref (priv->tpl_text);
  priv->tpl_text = g_object_ref (data);
}


void
_tpl_log_entry_text_set_message (TplLogEntryText *self,
    const gchar *data)
{
  TplLogEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (data != NULL); /* allow zero length */

  priv = self->priv;

  g_free (priv->message);
  priv->message = g_strdup (data);
}


void
_tpl_log_entry_text_set_message_type (TplLogEntryText *self,
    TpChannelTextMessageType data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->priv->message_type = data;
}


void
_tpl_log_entry_text_set_chatroom (TplLogEntryText *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->priv->chatroom = data;
}

gboolean
_tpl_log_entry_text_equal (TplLogEntry *message1,
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
  return !tp_strdiff (_tpl_log_entry_get_log_id (message1),
      _tpl_log_entry_get_log_id (message2));
}
