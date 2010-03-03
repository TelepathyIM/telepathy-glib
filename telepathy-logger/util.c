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

#include "util.h"

/* Bug#26838 prevents us to trust Messages' iface message-token
 * header, so I need to create a token which TPL can trust to be unique
 * within itself */
gchar *
create_message_token (const gchar *channel,
    const gchar *date,
    guint msgid)
{
  GChecksum *log_id = g_checksum_new (G_CHECKSUM_SHA1);
  gchar *retval;

  g_checksum_update (log_id, (guchar *) channel, -1);
  g_checksum_update (log_id, (guchar *) date, -1);
  g_checksum_update (log_id, (guchar *) &msgid, sizeof (unsigned int));

  retval = g_strdup (g_checksum_get_string (log_id));

  g_checksum_free (log_id);

  return retval;
}
