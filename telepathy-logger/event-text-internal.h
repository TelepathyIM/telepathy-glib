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

#ifndef __TPL_EVENT_TEXT_INTERNAL_H__
#define __TPL_EVENT_TEXT_INTERNAL_H__

#include <telepathy-logger/event-text.h>
#include <telepathy-logger/event-internal.h>
#include <telepathy-logger/channel-text-internal.h>

#define TPL_EVENT_TEXT_MSG_ID_IS_VALID(msg) (msg >= 0)

#define TPL_EVENT_TEXT_MSG_ID_UNKNOWN -2
#define TPL_EVENT_TEXT_MSG_ID_ACKNOWLEDGED -1

G_BEGIN_DECLS

struct _TplEventText
{
  TplEvent parent;

  /* Private */
  TplEventTextPriv *priv;
};

struct _TplEventTextClass
{
  TplEventClass parent_class;
};

TpChannelTextMessageType _tpl_event_text_message_type_from_str (
    const gchar *type_str);

const gchar * _tpl_event_text_message_type_to_str (
    TpChannelTextMessageType msg_type);

gint _tpl_event_text_get_pending_msg_id (TplEventText *self);

gboolean _tpl_event_text_is_pending (TplEventText *self);

G_END_DECLS
#endif // __TPL_EVENT_TEXT_INTERNAL_H__