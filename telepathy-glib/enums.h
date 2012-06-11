/*
 * enums.h - numeric constants
 *
 * Copyright © 2007-2012 Collabora Ltd.
 * Copyright © 2007-2009 Nokia Corporation
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
 */

#ifndef __TP_ENUMS_H__
#define __TP_ENUMS_H__

#if !defined (_TP_IN_META_HEADER) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> and <telepathy-glib/telepathy-glib-dbus.h> can be included directly."
#endif

#include <telepathy-glib/_gen/telepathy-enums.h>
#include <telepathy-glib/_gen/genums.h>

#define TP_USER_ACTION_TIME_NOT_USER_ACTION (G_GINT64_CONSTANT (0))

#define TP_USER_ACTION_TIME_CURRENT_TIME (G_MAXINT64)

#endif
