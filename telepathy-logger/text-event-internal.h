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

#ifndef __TPL_TEXT_EVENT_INTERNAL_H__
#define __TPL_TEXT_EVENT_INTERNAL_H__

#include <telepathy-logger/text-event.h>
#include <telepathy-logger/event-internal.h>
#include <telepathy-logger/text-channel-internal.h>

#define TPL_TEXT_EVENT_MSG_ID_IS_VALID(msg) (msg >= 0)

#define TPL_TEXT_EVENT_MSG_ID_UNKNOWN -2
#define TPL_TEXT_EVENT_MSG_ID_ACKNOWLEDGED -1

G_BEGIN_DECLS

struct _TplTextEvent
{
  TplEvent parent;

  /* Private */
  TplTextEventPriv *priv;
};

struct _TplTextEventClass
{
  TplEventClass parent_class;
};

TpChannelTextMessageType _tpl_text_event_message_type_from_str (
    const gchar *type_str);

const gchar * _tpl_text_event_message_type_to_str (
    TpChannelTextMessageType msg_type);

gint _tpl_text_event_get_pending_msg_id (TplTextEvent *self);

gboolean _tpl_text_event_is_pending (TplTextEvent *self);

void _tpl_text_event_add_supersedes (TplTextEvent *self,
    TplTextEvent *old_event);

G_END_DECLS
#endif // __TPL_TEXT_EVENT_INTERNAL_H__
