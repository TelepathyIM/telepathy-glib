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

#include "channel-factory.h"

#include <telepathy-glib/util.h>

#include <telepathy-logger/channel-text.h>

/*
static TplChannel *
text_new_wrap (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  g_debug ("FOO");
  return TPL_CHANNEL (tpl_channel_text_new (conn, object_path, tp_chan_props,
        account, error));
}
*/
static gchar *channel_types[] = {
  "org.freedesktop.Telepathy.Channel.Type.Text",
  NULL
};
static TplChannelConstructor channel_constructors[] = {
    (TplChannelConstructor) tpl_channel_text_new,
    NULL
};



TplChannel *
tpl_channel_factory (const gchar *channel_type,
    TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *tp_acc,
    GError **error)
{
  guint i;
  TplChannelConstructor chan_constructor = NULL;

  if (G_N_ELEMENTS (channel_types) != G_N_ELEMENTS (channel_constructors))
    g_critical ("channel_types and channel_constructors have different sizes."
        " An update to the channel factory's data is needed.");

  for(i=0; i < G_N_ELEMENTS (channel_types); ++i)
    if (!tp_strdiff (channel_type, channel_types[i])) {
      chan_constructor = channel_constructors[i];
      continue;
    }

  if (chan_constructor == NULL)
    {
      g_debug ("%s: channel type not handled by this logger", channel_type);
      return NULL;
    }

  return chan_constructor (conn, object_path, tp_chan_props, tp_acc, error);
}

