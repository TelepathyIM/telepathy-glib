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
#include <telepathy-glib/telepathy-glib.h>

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


G_DEFINE_TYPE (TplTextEvent, tpl_text_event, TPL_TYPE_EVENT)

struct _TplTextEventPriv
{
  TpChannelTextMessageType message_type;
  gint64 edit_timestamp;
  gchar *message;
  gchar *token;
  gchar *supersedes_token;
  /* A list of TplTextEvent that we supersede.
   * This is only populated when reading logs (not storing them). */
  GQueue supersedes;
};

enum
{
  PROP_MESSAGE_TYPE = 1,
  PROP_EDIT_TIMESTAMP,
  PROP_MESSAGE,
  PROP_TOKEN,
  PROP_SUPERSEDES
};

static gchar *message_types[] = {
    "normal",
    "action",
    "notice",
    "auto-reply",
    "delivery-report",
    NULL
};


static void
tpl_text_event_dispose (GObject *obj)
{
  TplTextEventPriv *priv = TPL_TEXT_EVENT (obj)->priv;

  g_list_foreach (priv->supersedes.head, (GFunc) g_object_unref, NULL);
  g_list_free (priv->supersedes.head);
  g_queue_init (&priv->supersedes);
}


static void
tpl_text_event_finalize (GObject *obj)
{
  TplTextEventPriv *priv = TPL_TEXT_EVENT (obj)->priv;

  g_free (priv->message);
  priv->message = NULL;

  g_free (priv->token);
  priv->token = NULL;

  g_free (priv->supersedes_token);
  priv->supersedes_token = NULL;

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
      case PROP_EDIT_TIMESTAMP:
        g_value_set_int64 (value, priv->edit_timestamp);
        break;
      case PROP_MESSAGE:
        g_value_set_string (value, priv->message);
        break;
      case PROP_TOKEN:
        g_value_set_string (value, priv->token);
        break;
      case PROP_SUPERSEDES:
        g_value_set_string (value, priv->supersedes_token);
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
      case PROP_EDIT_TIMESTAMP:
        priv->edit_timestamp = g_value_get_int64 (value);
        break;
      case PROP_MESSAGE:
        g_assert (priv->message == NULL);
        priv->message = g_value_dup_string (value);
        break;
      case PROP_TOKEN:
        g_assert (priv->token == NULL);
        priv->token = g_value_dup_string (value);
        break;
      case PROP_SUPERSEDES:
        g_assert (priv->supersedes_token == NULL);
        priv->supersedes_token = g_value_dup_string (value);
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

  object_class->dispose = tpl_text_event_dispose;
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

  param_spec = g_param_spec_int64 ("edit-timestamp",
      "Timestamp of edit message",
      "message-{sent,received} if this is an edit, or 0 otherwise.",
      G_MININT64, G_MAXINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_EDIT_TIMESTAMP,
      param_spec);

  param_spec = g_param_spec_string ("message",
      "Message",
      "The text message of the log event",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MESSAGE, param_spec);

  param_spec = g_param_spec_string ("message-token",
      "Message Token",
      "The message-token field of this message.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TOKEN, param_spec);

  param_spec = g_param_spec_string ("supersedes-token",
      "Message Token",
      "The message-token field of the message that this one supersedes.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SUPERSEDES, param_spec);

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
 * _tpl_text_event_message_type_from_str:
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
 * _tpl_text_event_message_type_to_str:
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
 * tpl_text_event_get_message:
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
 * tpl_text_event_get_message_token:
 * @self: a #TplTextEvent
 *
 * Returns: the same message as the #TplTextEvent:message-token property
 */
const gchar *
tpl_text_event_get_message_token (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self), NULL);

  return self->priv->token;
}


/**
 * tpl_text_event_get_supersedes_token:
 * @self: a #TplTextEvent
 *
 * Returns: the same message as the #TplTextEvent:supersedes-token property
 */
const gchar *
tpl_text_event_get_supersedes_token (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self), NULL);

  return self->priv->supersedes_token;
}


/**
 * _tpl_text_event_add_supersedes:
 * @self: a #TplTextEvent
 * @old_event: (transfer none): an #TplTextEvent which this one supersedes
 *
 * If there are other known entries in the message edit/succession chain,
 * they should be added to old_event before linking these two events,
 * as they will be copied onto this event for convenience.
 */
void
_tpl_text_event_add_supersedes (TplTextEvent *self,
    TplTextEvent *old_event)
{
  GList *l;

  g_object_ref (old_event);
  g_queue_push_tail (&self->priv->supersedes, old_event);

  for (l = old_event->priv->supersedes.head; l != NULL; l = g_list_next (l))
    g_queue_push_tail (&self->priv->supersedes, g_object_ref (l->data));

  if (self->priv->supersedes_token == NULL)
    self->priv->supersedes_token = g_strdup (old_event->priv->token);
}


/**
 * tpl_text_event_get_supersedes:
 * @self: a #TplTextEvent
 *
 * Returns: (transfer none) (element-type TelepathyLogger.TextEvent): A #GList
 *  of #TplTextEvent that this event
 * supersedes.
 */
GList *
tpl_text_event_get_supersedes (TplTextEvent *self)
{
  return self->priv->supersedes.head;
}


/**
 * tpl_text_event_get_message_type:
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


/**
 * tpl_text_event_get_edit_timestamp:
 * @self: a #TplTextEvent
 *
 * Returns: the same value as the #TplTextEvent:edit-timestamp property
 */
gint64
tpl_text_event_get_edit_timestamp (TplTextEvent *self)
{
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (self), 0);

  return self->priv->edit_timestamp;
}


