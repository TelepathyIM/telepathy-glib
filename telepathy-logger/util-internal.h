/*
 * Copyright (C) 2009-2011 Collabora Ltd.
 * Copyright (C) 2003-2007 Imendio AB
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
 *          Richard Hult <richard@imendio.com>
 */

#ifndef __TPL_UTIL_H__
#define __TPL_UTIL_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "event.h"

#define TPL_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

void _tpl_rmdir_recursively (const gchar *dir_name);

gint64 _tpl_time_parse (const gchar * str);

GList *_tpl_event_queue_insert_sorted_after (GQueue *events,
    GList *index,
    TplEvent *event);

#endif // __TPL_UTIL_H__
