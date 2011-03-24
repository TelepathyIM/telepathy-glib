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

#ifndef __TPL_EVENT_H__
#define __TPL_EVENT_H__

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/entity.h>

G_BEGIN_DECLS
#define TPL_TYPE_EVENT (tpl_event_get_type ())
#define TPL_EVENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_EVENT, TplEvent))
#define TPL_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_EVENT, TplEventClass))
#define TPL_IS_EVENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_EVENT))
#define TPL_IS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_EVENT))
#define TPL_EVENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_EVENT, TplEventClass))

typedef struct _TplEvent TplEvent;
typedef struct _TplEventClass TplEventClass;
typedef struct _TplEventPriv TplEventPriv;

GType tpl_event_get_type (void);

gint64 tpl_event_get_timestamp (TplEvent *self);

const gchar *tpl_event_get_account_path (TplEvent *self);
TpAccount * tpl_event_get_account (TplEvent *self);

TplEntity * tpl_event_get_sender (TplEvent *self);
TplEntity * tpl_event_get_receiver (TplEvent *self);

gboolean tpl_event_equal (TplEvent *self, TplEvent *data);

G_END_DECLS
#endif // __TPL_EVENT_H__
