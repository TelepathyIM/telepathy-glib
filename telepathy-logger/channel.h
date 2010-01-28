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

#ifndef __TPL_CHANNEL_H__
#define __TPL_CHANNEL_H__

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>

#include <telepathy-logger/util.h>

G_BEGIN_DECLS
#define TPL_TYPE_CHANNEL                  (tpl_channel_get_type ())
#define TPL_CHANNEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CHANNEL, TplChannel))
#define TPL_CHANNEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CHANNEL, TplChannelClass))
#define TPL_IS_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CHANNEL))
#define TPL_IS_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CHANNEL))
#define TPL_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CHANNEL, TplChannelClass))

typedef struct _TplChannelPriv TplChannelPriv;

typedef struct
{
  TpChannel parent;

  /* private */
  TplChannelPriv *priv;
} TplChannel;

typedef struct
{
  TpChannelClass parent_class;

  void (*call_when_ready) (TplChannel *self, GAsyncReadyCallback cb,
      gpointer user_data);
} TplChannelClass;


GType tpl_channel_get_type (void);

TpAccount *tpl_channel_get_account (TplChannel * self);
const gchar *tpl_channel_get_account_path (TplChannel * self);

typedef TplChannel* (*TplChannelConstructor) (TpConnection *conn,
    const gchar *object_path, GHashTable *tp_chan_props, GError **error);
TplChannelConstructor *tpl_channel_factory (const gchar *channel_type);

G_END_DECLS
#endif // __TPL_CHANNEL_H__
