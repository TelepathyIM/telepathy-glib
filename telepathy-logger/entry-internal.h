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

#ifndef __TPL_ENTRY_INTERNAL_H__
#define __TPL_ENTRY_INTERNAL_H__

#include <telepathy-logger/entry.h>

G_BEGIN_DECLS

#define TPL_ENTRY_MSG_ID_IS_VALID(msg) (msg >= 0)

#define TPL_ENTRY_MSG_ID_UNKNOWN -2
#define TPL_ENTRY_MSG_ID_ACKNOWLEDGED -1

typedef enum
{
  TPL_ENTRY_SIGNAL_NONE = 0,

  TPL_ENTRY_CHANNEL_TEXT_SIGNAL_SENT,
  TPL_ENTRY_CHANNEL_TEXT_SIGNAL_RECEIVED,
  TPL_ENTRY_CHANNEL_TEXT_SIGNAL_SEND_ERROR,
  TPL_ENTRY_CHANELL_TEXT_SIGNAL_LOST_MESSAGE,
  TPL_ENTRY_CHANNEL_TEXT_SIGNAL_CHAT_STATUS_CHANGED,

  TPL_ENTRY_CHANNEL_SIGNAL_CHANNEL_CLOSED

} TplEntrySignalType;

typedef enum
{
  TPL_ENTRY_ERROR,
  TPL_ENTRY_TEXT
} TplEntryType;


struct _TplEntry
{
  GObject parent;

  /* Private */
  TplEntryPriv *priv;
};

struct _TplEntryClass {
  GObjectClass parent_class;

  /* to be implemented only by subclasses */
  gboolean (*equal) (TplEntry *entry1, TplEntry *entry2);
};


void _tpl_entry_set_timestamp (TplEntry *self,
    gint64 data);

void _tpl_entry_set_signal_type (TplEntry *self,
    TplEntrySignalType data);

void _tpl_entry_set_direction (TplEntry *self,
    TplEntryDirection data);

void _tpl_entry_set_chat_id (TplEntry *self,
    const gchar *data);

void _tpl_entry_set_channel_path (TplEntry *self,
    const gchar *data);

void _tpl_entry_set_sender (TplEntry *self,
    TplContact *data);

void _tpl_entry_set_receiver (TplEntry *self,
    TplContact *data);

TplEntrySignalType _tpl_entry_get_signal_type (TplEntry *self);
const gchar * _tpl_entry_get_chat_id (TplEntry * self);
const gchar * _tpl_entry_get_channel_path (TplEntry *self);

TplEntryDirection _tpl_entry_get_direction (TplEntry *self);

gboolean _tpl_entry_equal (TplEntry *self,
    TplEntry *data);

const gchar * _tpl_entry_get_log_id (TplEntry *self);

G_END_DECLS
#endif // __TPL_ENTRY_INTERNAL_H__
