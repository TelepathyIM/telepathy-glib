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
#include "text-event.h"
#include "text-event-internal.h"

#include <glib-object.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/event.h>
#include <telepathy-logger/event-internal.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:text-event
 * @title: TplTextEvent
 * @short_description: Representation of a text log event
 *
 * A subclass of #TplEvent representing a text log event.
 */

/**
 * TplTextEvent:
 *
 * An object representing a text log event.
 */

/**
 * TPL_TEXT_EVENT_MSG_ID_IS_VALID:
 * @msg: a message ID
 *
 * Return whether a message ID is valid.
 *
 * If %FALSE is returned, it means that either an invalid input has been
 * passed, or the TplEvent is currently set to %TPL_TEXT_EVENT_MSG_ID_UNKNOWN
 * or %TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED.
 *
 * Returns: %TRUE if the argument is a valid message ID or %FALSE otherwise.
 */

/**
 * TPL_TEXT_EVENT_MSG_ID_UNKNOWN:
 *
 * Special value used instead of a message ID to indicate a message with an
 * unknown status (before _tpl_event_set_pending_msg_id() was called, or
 * when it wasn't possible to obtain the message ID).
 */

/**
 * TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED:
 *
 * Special value used instead of a message ID to indicate an acknowledged
 * message.
 */

G_DEFINE_TYPE (TplTextEvent, tpl_text_event, TPL_TYPE_EVENT)

struct _TplTextEventPriv
{
  TpChannelTextMessageType message_type;
  gchar *message;

  /* in specs it's guint, TplEvent needs a way to represent ACK'd messages:
   * if pending_msg_id reachs G_MAXINT32, then the problem is elsewhere :-) */
  gint pending_msg_id;
};

enum
{
  PROP_MESSAGE_TYPE = 1,
  PROP_MESSAGE,
  PROP_PENDING_MSG_ID
};

static gchar *message_types[] = {
    "normal",
    "action",
    "notice",
    "auto-reply",
    "delivery-report",
    NULL };


static void
tpl_text_event_finalize (GObject *obj)
{
  TplTextEventPriv *priv = TPL_TEXT_EVENT (obj)->priv;

  g_free (priv->message);
  priv->message = NULL;

  G_OBJECT_CLASS (tpl_text_event_parent_class)->finalize (obj);
}


static void
tpl_text_event_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplTextEventPriv *priv = TPL_TEXT_EVENT (object)->priv;

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
tpl_text_event_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplTextEventPriv *priv = TPL_TEXT_EVENT (object)->priv;

  switch (param_id) {
      case PROP_MESSAGE_TYPE:
        priv->message_type = g_value_get_uint (value);
        break;
      case PROP_MESSAGE:
        g_assert (priv->message == NULL);
        priv->message = g_value_dup_string (value);
        break;
      case PROP_PENDING_MSG_ID:
        priv->pending_msg_id = g_value_get_int (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  }
}


static gboolean tpl_text_event_equal (TplEvent *event1,
    TplEvent *event2)
{
  TplTextEvent *text_event1 = TPL_TEXT_EVENT (event1);
  TplTextEvent *text_event2 = TPL_TEXT_EVENT (event2);

  return TPL_EVENT_CLASS (tpl_text_event_parent_class)->equal (event1, event2)
    && text_event1->priv->message_type == text_event2->priv->message_type
    && !tp_strdiff (text_event1->priv->message, text_event2->priv->message);
}


static void tpl_text_event_class_init (TplTextEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplEventClass *event_class = TPL_EVENT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_text_event_finalize;
  object_class->get_property = tpl_text_event_get_property;
  object_class->set_property = tpl_text_event_set_property;

  event_class->equal = tpl_text_event_equal;

  param_spec = g_param_spec_uint ("message-type",
      "MessageType",
      "The message type for a Text log event",
      0, G_MAXUINT32, TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE_TYPE, param_spec);

  param_spec = g_param_spec_string ("message",
      "Message",
      "The text message of the log event",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE, param_spec);

  /**
   * TplTextEvent::pending-msg-id:
   *
   * The pending message id for the current log event.
   * The default value, is #TPL_TEXT_EVENT_MSG_ID_UNKNOWN,
   * meaning that it's not possible to know if the message is pending or has
   * been acknowledged.
   *
   * An object instantiating a TplEvent subclass should explicitly set it
   * to a valid msg-id number (id>=0) or to #TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED
   * when acknowledged or if the event is a result of
   * 'sent' signal.
   * In fact a sent event is considered as 'automatically' ACK by TPL.
   *
   * The pending message id value is only meaningful when associated to the
   * #TplEvent::channel-path property.
   * The couple (channel-path, pending-msg-id) cannot be considered unique,
   * though, since a message-id might be reused over time.
   *
   * Use #TplEvent::log-id for a unique identifier within TPL.
   */
  param_spec = g_param_spec_int ("pending-msg-id",
      "PendingMessageId",
      "Pending Message ID, if set, the log event is set as pending for ACK."
      " Default to -1 meaning not pending.",
      -1, G_MAXINT, TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PENDING_MSG_ID,
      param_spec);

  g_type_class_add_private (object_class, sizeof (TplTextEventPriv));
}


static void
tpl_text_event_init (TplTextEvent *self)
{
  TplTextEventPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_TEXT_EVENT, TplTextEventPriv);
  self->priv = priv;
}


/**
 * _tpl_text_event_message_type_from_str
 * @type_str: string to transform into a #TpChannelTextMessageType
 *
 * Maps strings into enum #TpChannelTextMessageType values.
 *
 * Returns: the relative value from enum #TpChannelTextMessageType if a
 * mapping is found, or defaults to %TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL if not.
 */
TpChannelTextMessageType
_tpl_text_event_message_type_from_str (const gchar *type_str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (message_types); ++i)
    if (!tp_strdiff (type_str, message_types[i]))
      return (TpChannelTextMessageType) i;

  /* default case */
  return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}


/**
 * _tpl_text_event_message_type_to_str
 * @msg_type: message type to transform into a string
 *
 * Maps enum #TpChannelTextMessageType values into strings
 *
 * Returns: a string representation for @msg_type or NULL if @msg_type is not
 * a legal value for %TpChannelTextMessageType.
 */
const gchar *
_tpl_text_event_message_type_to_str (TpChannelTextMessageType msg_type)
{
  g_return_val_if_fail (G_N_ELEMENTS (message_types) >= msg_type, NULL);

  return message_types[msg_type];
}


/**
 * tpl_text_event_get_message
 * @self: a #TplTextEvent
 *
 * Returns: the same message as the #TplTextEvent:message property
 */
const gchar *
tpl_text_event_get_message (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self), NULL);

  return self->priv->message;
}


/**
 * tpl_text_event_get_message_type
 * @self: a #TplTextEvent
 *
 * Returns: the same message as the #TplTextEvent:message-type property
 */
TpChannelTextMessageType
tpl_text_event_get_message_type (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self),
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

  return self->priv->message_type;
}


/*
 * tpl_text_event_get_pending_msg_id
 * @self: a #TplTextEvent
 *
 * Returns: the id as the #TplTextEvent:pending-msg-id property
 */
gint
_tpl_text_event_get_pending_msg_id (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self), -1);

  return self->priv->pending_msg_id;
}


gboolean
_tpl_text_event_is_pending (TplTextEvent *self)
{
  return TPL_TEXT_EVENT_MSG_ID_IS_VALID (
      _tpl_text_event_get_pending_msg_id (self));
}
