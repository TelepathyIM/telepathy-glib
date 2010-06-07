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

#ifndef __TPL_LOG_ENTRY_TEXT_INTERNAL_H__
#define __TPL_LOG_ENTRY_TEXT_INTERNAL_H__

#include <telepathy-logger/entry-text.h>
#include <telepathy-logger/entry-internal.h>
#include <telepathy-logger/channel-text-internal.h>

G_BEGIN_DECLS

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

struct _TplLogEntryText
{
  TplLogEntry parent;

  /* Private */
  TplLogEntryTextPriv *priv;
};

struct _TplLogEntryTextClass
{
  TplLogEntryClass parent_class;
};

TplLogEntryText * _tpl_log_entry_text_new (const gchar* log_id,
    const gchar *account_path,
    TplLogEntryDirection direction);

TpChannelTextMessageType _tpl_log_entry_text_message_type_from_str (
    const gchar *type_str);

const gchar * _tpl_log_entry_text_message_type_to_str (
    TpChannelTextMessageType msg_type);

TplChannelText * _tpl_log_entry_text_get_tpl_channel_text (
    TplLogEntryText *self);

void _tpl_log_entry_text_set_tpl_channel_text (TplLogEntryText *self,
    TplChannelText *data);

void _tpl_log_entry_text_set_message (TplLogEntryText *self,
    const gchar *data);

void _tpl_log_entry_text_set_message_type (TplLogEntryText *self,
    TpChannelTextMessageType data);

void _tpl_log_entry_text_set_chatroom (TplLogEntryText *self,
    gboolean data);

TpChannelTextMessageType _tpl_log_entry_text_get_message_type (
    TplLogEntryText *self);

gboolean _tpl_log_entry_text_is_chatroom (TplLogEntryText *self);

gboolean _tpl_log_entry_text_equal (TplLogEntry *message1,
    TplLogEntry *message2);

G_END_DECLS
#endif // __TPL_LOG_ENTRY_TEXT_INTERNAL_H__
