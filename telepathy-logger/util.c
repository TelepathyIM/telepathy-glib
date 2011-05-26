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

#include "util-internal.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>


void
_tpl_rmdir_recursively (const gchar *dir_name)
{
  GDir *dir;
  const gchar *name;

  dir = g_dir_open (dir_name, 0, NULL);

  /* Directory does not exist, nothing to do */
  if (dir == NULL)
    return;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *filename = g_build_path (G_DIR_SEPARATOR_S,
          dir_name, name, NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
        _tpl_rmdir_recursively (filename);
      else if (g_unlink (filename) < 0)
        g_warning ("Could not unlink '%s': %s", filename, g_strerror (errno));

      g_free (filename);
    }

  g_dir_close (dir);

  if (g_rmdir (dir_name) < 0)
    g_warning ("Could not remove directory '%s': %s",
        dir_name, g_strerror (errno));
}

/* We leak a single tz struct as freeing them is not thread-safe,
 * see https://bugzilla.gnome.org/show_bug.cgi?id=646435 */
static GTimeZone *tz;

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

  if (G_UNLIKELY (tz == NULL))
    tz = g_time_zone_new_utc ();

  dt = g_date_time_new (tz, year, month, day, hour, min, sec);
  ts = g_date_time_to_unix (dt);

  g_date_time_unref (dt);

  return ts;
}


GList *
_tpl_event_queue_insert_sorted_after (GQueue *events,
    GList *index,
    TplEvent *event)
{
  if (g_queue_is_empty (events))
    {
      g_queue_push_tail (events, event);
      return events->tail;
    }

  /* The initial index might go before the first one */
  if (index == NULL)
    {
      index = events->head;

      if (tpl_event_get_timestamp (event) <
          tpl_event_get_timestamp (TPL_EVENT (index->data)))
        {
          g_queue_insert_before (events, index, event);
          return events->head;
        }
    }

  /* Find the last event that this event can go after */
  while (g_list_next (index) != NULL &&
      tpl_event_get_timestamp (event) >=
      tpl_event_get_timestamp (TPL_EVENT (g_list_next (index)->data)))
    index = g_list_next (index);

  g_queue_insert_after (events, index, event);
  return g_list_next (index);
}
