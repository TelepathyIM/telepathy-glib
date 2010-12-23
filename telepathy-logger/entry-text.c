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
#include "entry-text.h"
#include "entry-text-internal.h"

#include <glib-object.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/entry.h>
#include <telepathy-logger/entry-internal.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:entry-text
 * @title: TplEntryText
 * @short_description: Representation of a text log entry
 *
 * A subclass of #TplEntry representing a text log entry.
 */

/**
 * TplEntryText:
 *
 * An object representing a text log entry.
 */

G_DEFINE_TYPE (TplEntryText, tpl_entry_text, TPL_TYPE_ENTRY)

struct _TplEntryTextPriv
{
  TplChannelText *tpl_text;
  TpChannelTextMessageType message_type;
  gchar *message;
  gboolean chatroom;
  /* in specs it's guint, TplEntry needs a way to represent ACK'd messages:
   * if pending_msg_id reachs G_MAXINT32, then the problem is elsewhere :-) */
  gint pending_msg_id;
};

enum
{
  PROP_MESSAGE_TYPE = 1,
  PROP_MESSAGE,
  PROP_PENDING_MSG_ID
};


static void
tpl_entry_text_dispose (GObject * obj)
{
  TplEntryText *self = TPL_ENTRY_TEXT (obj);
  TplEntryTextPriv *priv = self->priv;

  if (priv->tpl_text != NULL)
    {
      g_object_unref (priv->tpl_text);
      priv->tpl_text = NULL;
    }

  G_OBJECT_CLASS (tpl_entry_text_parent_class)->dispose (obj);
}


static void
tpl_entry_text_finalize (GObject * obj)
{
  TplEntryText *self = TPL_ENTRY_TEXT (obj);
  TplEntryTextPriv *priv = self->priv;

  g_free (priv->message);
  priv->message = NULL;

  G_OBJECT_CLASS (tpl_entry_text_parent_class)->finalize (obj);
}


static void
tpl_entry_text_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplEntryTextPriv *priv = TPL_ENTRY_TEXT (object)->priv;

  switch (param_id)
    {
      case PROP_MESSAGE_TYPE:
        g_value_set_uint (value, priv->message_type);
        break;
      case PROP_MESSAGE:
        g_value_set_string (value, priv->message);
        break;
      case PROP_PENDING_MSG_ID:
        g_value_set_int (value, priv->pending_msg_id);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}


static void
tpl_entry_text_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplEntryText *self = TPL_ENTRY_TEXT (object);

  switch (param_id) {
      case PROP_MESSAGE_TYPE:
        _tpl_entry_text_set_message_type (self, g_value_get_uint (value));
        break;
      case PROP_MESSAGE:
        _tpl_entry_text_set_message (self, g_value_get_string (value));
        break;
      case PROP_PENDING_MSG_ID:
        _tpl_entry_text_set_pending_msg_id (self, g_value_get_int (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  }
}


static void tpl_entry_text_class_init (TplEntryTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplEntryClass *entry_class = TPL_ENTRY_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_entry_text_finalize;
  object_class->dispose = tpl_entry_text_dispose;
  object_class->get_property = tpl_entry_text_get_property;
  object_class->set_property = tpl_entry_text_set_property;
  entry_class->equal = _tpl_entry_text_equal;

  param_spec = g_param_spec_uint ("message-type",
      "MessageType",
      "The message type for a Text log entry",
      0, G_MAXUINT32, TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE_TYPE, param_spec);

  param_spec = g_param_spec_string ("message",
      "Message",
      "The text message of the log entry",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE, param_spec);

  /**
   * TplEntryText::pending-msg-id:
   *
   * The pending message id for the current log entry.
   * The default value, is #TPL_ENTRY_MSG_ID_UNKNOWN,
   * meaning that it's not possible to know if the message is pending or has
   * been acknowledged.
   *
   * An object instantiating a TplEntry subclass should explicitly set it
   * to a valid msg-id number (id>=0) or to #TPL_ENTRY_MSG_ID_ACKNOWLEDGED
   * when acknowledged or if the entry is a result of
   * 'sent' signal.
   * In fact a sent entry is considered as 'automatically' ACK by TPL.
   *
   * The pending message id value is only meaningful when associated to the
   * #TplEntry::channel-path property.
   * The couple (channel-path, pending-msg-id) cannot be considered unique,
   * though, since a message-id might be reused over time.
   *
   * Use #TplEntry::log-id for a unique identifier within TPL.
   */
  param_spec = g_param_spec_int ("pending-msg-id",
      "PendingMessageId",
      "Pending Message ID, if set, the log entry is set as pending for ACK."
      " Default to -1 meaning not pending.",
      -1, G_MAXUINT32, TPL_ENTRY_MSG_ID_ACKNOWLEDGED,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PENDING_MSG_ID,
      param_spec);

  g_type_class_add_private (object_class, sizeof (TplEntryTextPriv));

}


static void
tpl_entry_text_init (TplEntryText *self)
{
  TplEntryTextPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_ENTRY_TEXT, TplEntryTextPriv);
  self->priv = priv;
}


TplEntryText *
_tpl_entry_text_new (const gchar *log_id,
    TpAccount *account,
    TplEntryDirection direction)
{
  return g_object_new (TPL_TYPE_ENTRY_TEXT,
      "log-id", log_id,
      "account", account,
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
 * _tpl_entry_text_message_type_from_str
 * @type_str: string to transform into a #TpChannelTextMessageType
 *
 * Maps strings into enum #TpChannelTextMessageType values.
 *
 * Returns: the relative value from enum #TpChannelTextMessageType if a
 * mapping is found, or defaults to %TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL if not.
 */
TpChannelTextMessageType
_tpl_entry_text_message_type_from_str (const gchar *type_str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (message_types); ++i)
    if (!tp_strdiff (type_str, message_types[i]))
      return (TpChannelTextMessageType) i;

  /* default case */
  return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}


/**
 * _tpl_entry_text_message_type_to_str
 * @msg_type: message type to transform into a string
 *
 * Maps enum #TpChannelTextMessageType values into strings
 *
 * Returns: a string representation for @msg_type or NULL if @msg_type is not
 * a legal value for %TpChannelTextMessageType.
 */
const gchar *
_tpl_entry_text_message_type_to_str (TpChannelTextMessageType msg_type)
{
  g_return_val_if_fail (G_N_ELEMENTS (message_types) >= msg_type, NULL);

  return message_types[msg_type];
}


gboolean
_tpl_entry_text_is_chatroom (TplEntryText * self)
{
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (self), FALSE);

  return self->priv->chatroom;
}


TplChannelText *
_tpl_entry_text_get_tpl_channel_text (TplEntryText * self)
{
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (self), NULL);

  return self->priv->tpl_text;
}

/**
 * tpl_entry_text_get_message
 * @self: a #TplEntryText
 *
 * Returns: the same message as the #TplEntryText:message property
 */
const gchar *
tpl_entry_text_get_message (TplEntryText * self)
{
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (self), NULL);

  return self->priv->message;
}

TpChannelTextMessageType
_tpl_entry_text_get_message_type (TplEntryText * self)
{
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (self),
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

  return self->priv->message_type;
}


void
_tpl_entry_text_set_tpl_channel_text (TplEntryText * self,
    TplChannelText *data)
{
  TplEntryTextPriv *priv;

  g_return_if_fail (TPL_IS_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_CHANNEL_TEXT (data) || data == NULL);

  priv = self->priv;
  if (priv->tpl_text != NULL)
    g_object_unref (priv->tpl_text);
  priv->tpl_text = g_object_ref (data);
}


void
_tpl_entry_text_set_message (TplEntryText *self,
    const gchar *data)
{
  TplEntryTextPriv *priv;

  if (data == NULL)
    return;

  g_return_if_fail (TPL_IS_ENTRY_TEXT (self));

  priv = self->priv;

  g_free (priv->message);
  priv->message = g_strdup (data);
}


void
_tpl_entry_text_set_message_type (TplEntryText *self,
    TpChannelTextMessageType data)
{
  g_return_if_fail (TPL_IS_ENTRY_TEXT (self));

  self->priv->message_type = data;
}


void
_tpl_entry_text_set_chatroom (TplEntryText *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_ENTRY_TEXT (self));

  self->priv->chatroom = data;
}

gboolean
_tpl_entry_text_equal (TplEntry *message1,
    TplEntry *message2)
{
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (message1), FALSE);
  g_return_val_if_fail (TPL_IS_ENTRY_TEXT (message2), FALSE);

  /*
  if (priv1->id == priv2->id && !tp_strdiff (priv1->body, priv2->body)) {
  if (priv1->type == priv2->type)
    if (!tp_strdiff (priv1->entry.text->message, priv2->entry.text->message)) {
    }
  */
  return !tp_strdiff (_tpl_entry_get_log_id (message1),
      _tpl_entry_get_log_id (message2));
}

/**
 * _tpl_entry_set_pending_msg_id:
 * @self: TplEntryText instance
 * @data: the pending message ID
 *
 * Sets @self to be associated to pending message id @data.
 *
 * @see_also: #TplEntry::pending-msg-id for special values.
 */
void
_tpl_entry_text_set_pending_msg_id (TplEntryText *self,
    gint data)
{
  g_return_if_fail (TPL_IS_ENTRY (self));

  self->priv->pending_msg_id = data;
  g_object_notify (G_OBJECT (self), "pending-msg-id");
}

/**
 * tpl_entry_text_get_pending_msg_id
 * @self: a #TplEntryText
 *
 * Returns: the id as the #TplEntryText:pending-msg-id property
 */
gint
tpl_entry_text_get_pending_msg_id (TplEntryText *self)
{
  g_return_val_if_fail (TPL_IS_ENTRY (self), -1);

  return self->priv->pending_msg_id;
}


gboolean
_tpl_entry_text_is_pending (TplEntryText *self)
{
  return TPL_ENTRY_MSG_ID_IS_VALID (
      tpl_entry_text_get_pending_msg_id (self));
}
