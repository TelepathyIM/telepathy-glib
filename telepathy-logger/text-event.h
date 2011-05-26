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

#ifndef __TPL_TEXT_EVENT_H__
#define __TPL_TEXT_EVENT_H__

#include <glib-object.h>

#include <telepathy-logger/event.h>

G_BEGIN_DECLS
#define TPL_TYPE_TEXT_EVENT                  (tpl_text_event_get_type ())
#define TPL_TEXT_EVENT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_TEXT_EVENT, TplTextEvent))
#define TPL_TEXT_EVENT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_TEXT_EVENT, TplTextEventClass))
#define TPL_IS_TEXT_EVENT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_TEXT_EVENT))
#define TPL_IS_TEXT_EVENT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_TEXT_EVENT))
#define TPL_TEXT_EVENT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_TEXT_EVENT, TplTextEventClass))

typedef struct _TplTextEvent TplTextEvent;
typedef struct _TplTextEventClass TplTextEventClass;
typedef struct _TplTextEventPriv TplTextEventPriv;

GType tpl_text_event_get_type (void);

TpChannelTextMessageType tpl_text_event_get_message_type (TplTextEvent *self);
gint64 tpl_text_event_get_edit_timestamp (TplTextEvent *self);

const gchar *tpl_text_event_get_message (TplTextEvent *self);
const gchar *tpl_text_event_get_message_token (TplTextEvent *self);
const gchar *tpl_text_event_get_supersedes_token (TplTextEvent *self);

GList *tpl_text_event_get_supersedes (TplTextEvent *self);

G_END_DECLS
#endif // __TPL_TEXT_EVENT_H__
