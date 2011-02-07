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
 * Authors: Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *          Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#ifndef __TPL_STREAMED_MEDIA_CHANNEL_H__
#define __TPL_STREAMED_MEDIA_CHANNEL_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/channel-internal.h>

G_BEGIN_DECLS
#define TPL_TYPE_STREAMED_MEDIA_CHANNEL            (_tpl_streamed_media_channel_get_type ())
#define TPL_STREAMED_MEDIA_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_STREAMED_MEDIA_CHANNEL, TplStreamedMediaChannel))
#define TPL_STREAMED_MEDIA_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_STREAMED_MEDIA_CHANNEL, TplStreamedMediaChannelClass))
#define TPL_IS_CHANNEL_STREAMED_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_STREAMED_MEDIA_CHANNEL))
#define TPL_IS_CHANNEL_STREAMED_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_STREAMED_MEDIA_CHANNEL))
#define TPL_STREAMED_MEDIA_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_STREAMED_MEDIA_CHANNEL, TplStreamedMediaChannelClass))

typedef struct _TplStreamedMediaChannelPriv TplStreamedMediaChannelPriv;
typedef struct
{
  TpChannel parent;

  /* private */
  TplStreamedMediaChannelPriv *priv;
} TplStreamedMediaChannel;

typedef struct
{
  TpChannelClass parent_class;
} TplStreamedMediaChannelClass;

GType _tpl_streamed_media_channel_get_type (void);

TplStreamedMediaChannel * _tpl_streamed_media_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error);

G_END_DECLS
#endif /* __TPL_STREAMED_MEDIA_CHANNEL_H__ */
