/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009-2011 Collabora Ltd.
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
 *          Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "channel-internal.h"

#include <glib.h>

G_DEFINE_INTERFACE (TplChannel, _tpl_channel, TP_TYPE_CHANNEL)


static void
_tpl_channel_default_init (TplChannelInterface *iface)
{
}


/**
 * tpl_channel_prepare_async
 * @self: a TplChannel instance
 * @cb: a callback
 * @user_data: user's data passed to the callback
 *
 * The TplObserver has no idea of what TplChannel subclass instance it's
 * dealing with.
 * In order to prepare the subclass instance this method has to
 * be called, which will call #TplChannelClass.prepare_async
 * virtual method, implemented (mandatory) by #TplChannel subclasses.
 */
void
_tpl_channel_prepare_async (TplChannel *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TPL_CHANNEL_GET_IFACE (self)->prepare_async != NULL);

  TPL_CHANNEL_GET_IFACE (self)->prepare_async (self, cb, user_data);
}


gboolean
_tpl_channel_prepare_finish (TplChannel *self,
    GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (TPL_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (TPL_CHANNEL_GET_IFACE (self)->prepare_finish != NULL,
      FALSE);

  return TPL_CHANNEL_GET_IFACE (self)->prepare_finish (self, result, error);
}
