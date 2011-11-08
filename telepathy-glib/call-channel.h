/*
 * call-channel.h - high level API for Call channels
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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
 */

#ifndef __TP_CALL_CHANNEL_H__
#define __TP_CALL_CHANNEL_H__

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

#define TP_TYPE_CALL_CHANNEL (tp_call_channel_get_type ())
#define TP_CALL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CALL_CHANNEL, TpCallChannel))
#define TP_CALL_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_CALL_CHANNEL, TpCallChannelClass))
#define TP_IS_CALL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CALL_CHANNEL))
#define TP_IS_CALL_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_CALL_CHANNEL))
#define TP_CALL_CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CALL_CHANNEL, TpCallChannelClass))

typedef struct _TpCallChannel TpCallChannel;
typedef struct _TpCallChannelClass TpCallChannelClass;
typedef struct _TpCallChannelPrivate TpCallChannelPrivate;

struct _TpCallChannel
{
  /*<private>*/
  TpChannel parent;
  TpCallChannelPrivate *priv;
};

struct _TpCallChannelClass
{
  /*<private>*/
  TpChannelClass parent_class;
  GCallback _padding[7];
};

GType tp_call_channel_get_type (void);

G_END_DECLS

#endif
