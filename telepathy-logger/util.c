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

#include "util-internal.h"

#include "datetime-internal.h"
#include "log-store-sqlite-internal.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

/* Bug#26838 prevents us to trust Messages' iface message-token
 * header, so I need to create a token which TPL can trust to be unique
 * within itself */
gchar *
_tpl_create_message_token (const gchar *channel,
    gint64 timestamp,
    guint msgid)
{
  GChecksum *log_id = g_checksum_new (G_CHECKSUM_SHA1);
  gchar *retval;
  gchar *date = _tpl_time_to_string_local (timestamp,
      TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);

  g_checksum_update (log_id, (guchar *) channel, -1);
  g_checksum_update (log_id, (guchar *) date, -1);
  g_checksum_update (log_id, (guchar *) &msgid, sizeof (unsigned int));

  retval = g_strdup (g_checksum_get_string (log_id));

  g_checksum_free (log_id);
  g_free (date);

  return retval;
}


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

