/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __TPL_TIME_H__
#define __TPL_TIME_H__

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#include <glib.h>

G_BEGIN_DECLS
#define TPL_TIME_FORMAT_DISPLAY_SHORT "%H:%M"
#define TPL_TIME_FORMAT_DISPLAY_LONG  "%a %d %b %Y"
  time_t tpl_time_get_current (void);
time_t tpl_time_get_local_time (struct tm *tm);
time_t tpl_time_parse (const gchar * str);
gchar *tpl_time_to_string_utc (time_t t, const gchar * format);
gchar *tpl_time_to_string_local (time_t t, const gchar * format);
gchar *tpl_time_to_string_relative (time_t t);

G_END_DECLS
#endif /* __TPL_TIME_H__ */
