/*
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#ifndef __TPL_CALL_EVENT_H__
#define __TPL_CALL_EVENT_H__

#include <glib-object.h>

#include <telepathy-logger/event.h>

G_BEGIN_DECLS
#define TPL_TYPE_CALL_EVENT                  (tpl_call_event_get_type ())
#define TPL_CALL_EVENT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CALL_EVENT, TplCallEvent))
#define TPL_CALL_EVENT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CALL_EVENT, TplCallEventClass))
#define TPL_IS_CALL_EVENT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CALL_EVENT))
#define TPL_IS_CALL_EVENT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CALL_EVENT))
#define TPL_CALL_EVENT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CALL_EVENT, TplCallEventClass))

typedef struct _TplCallEvent TplCallEvent;
typedef struct _TplCallEventClass TplCallEventClass;
typedef struct _TplCallEventPriv TplCallEventPriv;

GType tpl_call_event_get_type (void);

GTimeSpan tpl_call_event_get_duration (TplCallEvent *self);
TplEntity * tpl_call_event_get_end_actor (TplCallEvent *self);
TpCallStateChangeReason tpl_call_event_get_end_reason (TplCallEvent *self);
const gchar * tpl_call_event_get_detailed_end_reason (TplCallEvent *self);


G_END_DECLS
#endif // __TPL_CALL_EVENT_H__
