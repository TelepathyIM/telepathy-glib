/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2010 Collabora Ltd.
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
 * Authors: Richard Hult <richard@imendio.com>
 */

#include "config.h"
#include "datetime-internal.h"

#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Note: time is always in UTC. */

time_t
_tpl_time_get_current (void)
{
  return time (NULL);
}


/* The format is: "20021209T23:51:30" and is in UTC. 0 is returned on
 * failure. The alternative format "20021209" is also accepted.
 */
gint64
_tpl_time_parse (const gchar *str)
{
  gint year = 0;
  gint month = 0;
  gint day = 0;
  gint hour = 0;
  gint min = 0;
  gint sec = 0;
  gint n_parsed;
  GDateTime *dt;
  gint64 ts;

  n_parsed = sscanf (str, "%4d%2d%2dT%2d:%2d:%2d",
      &year, &month, &day, &hour,
      &min, &sec);

  if (n_parsed != 3 && n_parsed != 6)
    return 0;

  dt = g_date_time_new_utc (year, month, day, hour, min, sec);
  ts = g_date_time_to_unix (dt);

  g_date_time_unref (dt);

  return ts;
}

/* Converts the UTC timestamp to a string, also in UTC. Returns NULL on failure. */
gchar *
_tpl_time_to_string_utc (time_t t,
    const gchar * format)
{
  gchar stamp[128];
  struct tm *tm;

  g_return_val_if_fail (format != NULL, NULL);

  tm = gmtime (&t);
  if (strftime (stamp, sizeof (stamp), format, tm) == 0)
    {
      return NULL;
    }

  return g_strdup (stamp);
}

/* Converts the UTC timestamp to a string, in local time. Returns NULL on failure. */
gchar *
_tpl_time_to_string_local (time_t t,
    const gchar *format)
{
  gchar stamp[128];
  struct tm *tm;

  g_return_val_if_fail (format != NULL, NULL);

  tm = localtime (&t);
  if (strftime (stamp, sizeof (stamp), format, tm) == 0)
    {
      return NULL;
    }

  return g_strdup (stamp);
}
