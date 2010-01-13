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

#include "log-entry-text.h"

#include <telepathy-logger/channel.h>
#include <telepathy-logger/contact.h>
#include <telepathy-logger/utils.h>

G_DEFINE_TYPE (TplLogEntryText, tpl_log_entry_text, G_TYPE_OBJECT)
     static void tpl_log_entry_text_finalize (GObject * obj);
     static void tpl_log_entry_text_dispose (GObject * obj);

     static void tpl_log_entry_text_class_init (TplLogEntryTextClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = tpl_log_entry_text_finalize;
  object_class->dispose = tpl_log_entry_text_dispose;
}

static void
tpl_log_entry_text_init (TplLogEntryText * self)
{
#define TPL_SET_NULL(x) tpl_log_entry_text_set_##x(self, NULL)
  TPL_SET_NULL (tpl_text_channel);
  TPL_SET_NULL (sender);
  TPL_SET_NULL (receiver);
  TPL_SET_NULL (message);
  TPL_SET_NULL (chat_id);
#undef TPL_SET_NULL
}

static void
tpl_log_entry_text_dispose (GObject * obj)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (obj);
  g_debug ("TplLogEntryText: disposing\n");

  tpl_object_unref_if_not_null (self->tpl_text);
  self->tpl_text = NULL;
  tpl_object_unref_if_not_null (self->sender);
  self->sender = NULL;
  tpl_object_unref_if_not_null (self->receiver);
  self->receiver = NULL;

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->finalize (obj);

  g_debug ("TplLogEntryText: disposed\n");
}

static void
tpl_log_entry_text_finalize (GObject * obj)
{
  TplLogEntryText *self = TPL_LOG_ENTRY_TEXT (obj);

  g_debug ("TplLogEntryText: finalizing\n");

  g_free ((gchar *) self->message);
  self->message = NULL;
  g_free ((gchar *) self->chat_id);
  self->chat_id = NULL;

  G_OBJECT_CLASS (tpl_log_entry_text_parent_class)->dispose (obj);

  g_debug ("TplLogEntryText: finalized\n");
}


TplLogEntryText *
tpl_log_entry_text_new (void)
{
  return g_object_new (TPL_TYPE_LOG_ENTRY_TEXT, NULL);
}



TpChannelTextMessageType
tpl_log_entry_text_message_type_from_str (const gchar * type_str)
{
  if (g_strcmp0 (type_str, "normal") == 0)
    {
      return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
    }
  else if (g_strcmp0 (type_str, "action") == 0)
    {
      return TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION;
    }
  else if (g_strcmp0 (type_str, "notice") == 0)
    {
      return TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE;
    }
  else if (g_strcmp0 (type_str, "auto-reply") == 0)
    {
      return TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY;
    }

  return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}


const gchar *
tpl_log_entry_text_message_type_to_str (TpChannelTextMessageType msg_type)
{
  switch (msg_type)
    {
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION:
      return "action";
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE:
      return "notice";
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY:
      return "auto-reply";
    default:
      return "normal";
    }
}


TplChannel *
tpl_log_entry_text_get_tpl_channel (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);

  return
    tpl_text_channel_get_tpl_channel (tpl_log_entry_text_get_tpl_text_channel
				      (self));
}

TplTextChannel *
tpl_log_entry_text_get_tpl_text_channel (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);
  return self->tpl_text;
}

TplContact *
tpl_log_entry_text_get_sender (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);
  return self->sender;
}

TplContact *
tpl_log_entry_text_get_receiver (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);
  return self->receiver;
}

const gchar *
tpl_log_entry_text_get_message (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);
  return self->message;
}

TpChannelTextMessageType
tpl_log_entry_text_get_message_type (TplLogEntryText * self)
{
  return self->message_type;
}

TplLogEntryTextSignalType
tpl_log_entry_text_get_signal_type (TplLogEntryText * self)
{
  return self->signal_type;
}

TplLogEntryTextDirection
tpl_log_entry_text_get_direction (TplLogEntryText * self)
{
  return self->direction;
}

guint
tpl_log_entry_text_get_message_id (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), 0);
  return self->message_id;
}

const gchar *
tpl_log_entry_text_get_chat_id (TplLogEntryText * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY_TEXT (self), NULL);
  return self->chat_id;
}


void
tpl_log_entry_text_set_tpl_text_channel (TplLogEntryText * self,
					 TplTextChannel * data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_TEXT_CHANNEL (data) || data == NULL);

  tpl_object_unref_if_not_null (self->tpl_text);
  self->tpl_text = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_log_entry_text_set_sender (TplLogEntryText * self, TplContact * data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  tpl_object_unref_if_not_null (self->sender);
  self->sender = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_log_entry_text_set_receiver (TplLogEntryText * self, TplContact * data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));
  g_return_if_fail (TPL_IS_CONTACT (data) || data == NULL);

  tpl_object_unref_if_not_null (self->receiver);
  self->receiver = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_log_entry_text_set_message (TplLogEntryText * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  g_free ((gchar *) self->message);
  self->message = g_strdup (data);
}

void
tpl_log_entry_text_set_message_type (TplLogEntryText * self,
				     TpChannelTextMessageType data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->message_type = data;
}

void
tpl_log_entry_text_set_signal_type (TplLogEntryText * self,
				    TplLogEntryTextSignalType data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->signal_type = data;
}

void
tpl_log_entry_text_set_direction (TplLogEntryText * self,
				  TplLogEntryTextDirection data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->direction = data;
}

void
tpl_log_entry_text_set_message_id (TplLogEntryText * self, guint data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  self->message_id = data;
}

void
tpl_log_entry_text_set_chat_id (TplLogEntryText * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY_TEXT (self));

  g_free ((gchar *) self->chat_id);
  self->chat_id = g_strdup (data);
}
