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

#include <telepathy-logger/log-entry-text.h>

G_BEGIN_DECLS

void _tpl_log_entry_text_set_tpl_channel_text (TplLogEntryText *self,
    TplChannelText *data);

void _tpl_log_entry_text_set_message (TplLogEntryText *self,
    const gchar *data);

void _tpl_log_entry_text_set_message_type (TplLogEntryText *self,
    TpChannelTextMessageType data);

void _tpl_log_entry_text_set_chatroom (TplLogEntryText *self,
    gboolean data);

/* Methods inherited by TplLogEntry */
void _tpl_log_entry_text_set_timestamp (TplLogEntryText *self,
    gint64 data);

void _tpl_log_entry_text_set_signal_type (TplLogEntryText *self,
    TplLogEntrySignalType data);

void _tpl_log_entry_text_set_direction (TplLogEntryText *self,
    TplLogEntryDirection data);

void _tpl_log_entry_text_set_chat_id (TplLogEntryText *self,
    const gchar *data);

void _tpl_log_entry_text_set_sender (TplLogEntryText *self,
    TplContact *data);

void _tpl_log_entry_text_set_receiver (TplLogEntryText *self,
    TplContact *data);

void _tpl_log_entry_text_set_pending_msg_id (TplLogEntryText *self,
    gint64 data);

/* Methods inherited by TplLogEntry */

G_END_DECLS
#endif // __TPL_LOG_ENTRY_TEXT_INTERNAL_H__
