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

#ifndef __TPL_EVENT_INTERNAL_H__
#define __TPL_EVENT_INTERNAL_H__

#include <telepathy-logger/event.h>

G_BEGIN_DECLS

#define TPL_EVENT_MSG_ID_IS_VALID(msg) (msg >= 0)

#define TPL_EVENT_MSG_ID_UNKNOWN -2
#define TPL_EVENT_MSG_ID_ACKNOWLEDGED -1

typedef enum
{
  TPL_EVENT_ERROR,
  TPL_EVENT_TEXT
} TplEventType;


struct _TplEvent
{
  GObject parent;

  /* Private */
  TplEventPriv *priv;
};

struct _TplEventClass {
  GObjectClass parent_class;

  /* to be implemented only by subclasses */
  gboolean (*equal) (TplEvent *event1, TplEvent *event2);
};


void _tpl_event_set_timestamp (TplEvent *self,
    gint64 data);

void _tpl_event_set_direction (TplEvent *self,
    TplEventDirection data);

void _tpl_event_set_id (TplEvent *self,
    const gchar *data);

void _tpl_event_set_channel_path (TplEvent *self,
    const gchar *data);

void _tpl_event_set_sender (TplEvent *self,
    TplEntity *data);

void _tpl_event_set_receiver (TplEvent *self,
    TplEntity *data);

const gchar * _tpl_event_get_id (TplEvent * self);
const gchar * _tpl_event_get_channel_path (TplEvent *self);

TplEventDirection _tpl_event_get_direction (TplEvent *self);

gboolean _tpl_event_equal (TplEvent *self,
    TplEvent *data);

const gchar * _tpl_event_get_log_id (TplEvent *self);

G_END_DECLS
#endif // __TPL_EVENT_INTERNAL_H__
