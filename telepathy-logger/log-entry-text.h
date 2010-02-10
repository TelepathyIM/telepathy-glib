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

#ifndef __TPL_LOG_ENTRY_TEXT_H__
#define __TPL_LOG_ENTRY_TEXT_H__

#include <glib-object.h>

#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/log-entry.h>

G_BEGIN_DECLS
#define TPL_TYPE_LOG_ENTRY_TEXT                  (tpl_log_entry_text_get_type ())
#define TPL_LOG_ENTRY_TEXT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryText))
#define TPL_LOG_ENTRY_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextClass))
#define TPL_IS_LOG_ENTRY_TEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY_TEXT))
#define TPL_IS_LOG_ENTRY_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY_TEXT))
#define TPL_LOG_ENTRY_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY_TEXT, TplLogEntryTextClass))

typedef enum
{
  TPL_LOG_ENTRY_TEXT_SIGNAL_NONE = 0,
  TPL_LOG_ENTRY_TEXT_SIGNAL_SENT,
  TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED,
  TPL_LOG_ENTRY_TEXT_SIGNAL_SEND_ERROR,
  TPL_LOG_ENTRY_TEXT_SIGNAL_LOST_MESSAGE,
  TPL_LOG_ENTRY_TEXT_SIGNAL_CHAT_STATUS_CHANGED,
  TPL_LOG_ENTRY_SIGNAL_CHANNEL_CLOSED
} TplLogEntryTextSignalType;


typedef struct _TplLogEntryTextPriv TplLogEntryTextPriv;

typedef struct
{
  TplLogEntry parent;

  /* Private */
  TplLogEntryTextPriv *priv;
} TplLogEntryText;

typedef struct
{
  TplLogEntryClass parent_class;
} TplLogEntryTextClass;

GType tpl_log_entry_text_get_type (void);

TplLogEntryText *tpl_log_entry_text_new (guint log_id, const gchar *chat_id,
    TplLogEntryDirection direction);

TpChannelTextMessageType tpl_log_entry_text_message_type_from_str (
    const gchar *type_str);

const gchar *tpl_log_entry_text_message_type_to_str (
    TpChannelTextMessageType msg_type);

TplChannelText *tpl_log_entry_text_get_tpl_channel_text (
    TplLogEntryText *self);
const gchar *tpl_log_entry_text_get_message (TplLogEntryText *self);
TpChannelTextMessageType tpl_log_entry_text_get_message_type (TplLogEntryText *self);
gboolean tpl_log_entry_text_is_chatroom (TplLogEntryText *self);

void tpl_log_entry_text_set_tpl_channel_text (TplLogEntryText *self,
    TplChannelText *data);
void tpl_log_entry_text_set_message (TplLogEntryText *self,
    const gchar *data);
void tpl_log_entry_text_set_message_type (TplLogEntryText *self,
    TpChannelTextMessageType data);
void tpl_log_entry_text_set_chatroom (TplLogEntryText *self, gboolean data);


/* Methods inherited by TplLogEntry */
time_t tpl_log_entry_text_get_timestamp (TplLogEntryText *self);
TplLogEntrySignalType tpl_log_entry_text_get_signal_type (
    TplLogEntryText *self);
guint tpl_log_entry_text_get_log_id (TplLogEntryText *self);
const gchar *tpl_log_entry_text_get_chat_id (TplLogEntryText *self);
TplLogEntryDirection tpl_log_entry_text_get_direction (TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_sender (TplLogEntryText *self);
TplContact *tpl_log_entry_text_get_receiver (TplLogEntryText *self);

void tpl_log_entry_text_set_timestamp (TplLogEntryText *self, time_t data);
void tpl_log_entry_text_set_signal_type (TplLogEntryText *self,
    TplLogEntrySignalType data);
void tpl_log_entry_text_set_direction (TplLogEntryText *self,
    TplLogEntryDirection data);
void tpl_log_entry_text_set_chat_id (TplLogEntryText *self,
    const gchar *data);
void tpl_log_entry_text_set_sender (TplLogEntryText *self, TplContact *data);
void tpl_log_entry_text_set_receiver (TplLogEntryText *self,
    TplContact *data);

gboolean tpl_log_entry_text_equal (TplLogEntry *message1, TplLogEntry *message2);
/* Methods inherited by TplLogEntry */

G_END_DECLS
#endif // __TPL_LOG_ENTRY_TEXT_H__
