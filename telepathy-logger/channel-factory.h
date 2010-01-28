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

#ifndef __TPL_CHANNEL_FACTORY_H__
#define __TPL_CHANNEL_FACTORY_H__

#include <glib-object.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/channel.h>

typedef TplChannel* (*TplChannelConstructor) (TpConnection *conn,
    const gchar *object_path, GHashTable *tp_chan_props, TpAccount *tp_acc,
    GError **error);
TplChannel *tpl_channel_factory (const gchar *channel_type,
    TpConnection *conn, const gchar *object_path, GHashTable *tp_chan_props,
    TpAccount *tp_acc, GError **error);

#endif /* __TPL_CHANNEL_FACTORY_H__ */
