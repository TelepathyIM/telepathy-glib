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

#ifndef __TPL_LOG_ENTRY_H__
#define __TPL_LOG_ENTRY_H__

#include <glib-object.h>

#include <telepathy-logger/contact.h>

G_BEGIN_DECLS
#define TPL_TYPE_LOG_ENTRY (tpl_log_entry_get_type ())
#define TPL_LOG_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntry))
#define TPL_LOG_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))
#define TPL_IS_LOG_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY))
#define TPL_IS_LOG_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY))
#define TPL_LOG_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))

#define TPL_LOG_ENTRY_MSG_ID_IS_VALID(msg) (msg >= 0)

#define TPL_LOG_ENTRY_MSG_ID_UNKNOWN -2

#define TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED -1

typedef enum
{
  TPL_LOG_ENTRY_DIRECTION_NONE = 0,

  TPL_LOG_ENTRY_DIRECTION_IN,
  TPL_LOG_ENTRY_DIRECTION_OUT
} TplLogEntryDirection;

typedef enum
{
  TPL_LOG_ENTRY_SIGNAL_NONE = 0,

  TPL_LOG_ENTRY_CHANNEL_TEXT_SIGNAL_SENT,
  TPL_LOG_ENTRY_CHANNEL_TEXT_SIGNAL_RECEIVED,
  TPL_LOG_ENTRY_CHANNEL_TEXT_SIGNAL_SEND_ERROR,
  TPL_LOG_ENTRY_CHANELL_TEXT_SIGNAL_LOST_MESSAGE,
  TPL_LOG_ENTRY_CHANNEL_TEXT_SIGNAL_CHAT_STATUS_CHANGED,

  TPL_LOG_ENTRY_CHANNEL_SIGNAL_CHANNEL_CLOSED

} TplLogEntrySignalType;

typedef enum
{
  TPL_LOG_ENTRY_ERROR,
  TPL_LOG_ENTRY_TEXT
} TplLogEntryType;

typedef struct _TplLogEntry TplLogEntry;
typedef struct _TplLogEntryClass TplLogEntryClass;
typedef struct _TplLogEntryPriv TplLogEntryPriv;

GType tpl_log_entry_get_type (void);

gint64 tpl_log_entry_get_timestamp (TplLogEntry *self);

const gchar *tpl_log_entry_get_account_path (TplLogEntry *self);

G_END_DECLS
#endif // __TPL_LOG_ENTRY_H__
