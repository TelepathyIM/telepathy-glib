/*
 * room-list-channel.h - High level API for RoomList channels
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_ROOM_LIST_CHANNEL_H__
#define __TP_ROOM_LIST_CHANNEL_H__

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

#define TP_TYPE_ROOM_LIST_CHANNEL (tp_room_list_channel_get_type ())
#define TP_ROOM_LIST_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_ROOM_LIST_CHANNEL, TpRoomListChannel))
#define TP_ROOM_LIST_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_ROOM_LIST_CHANNEL, TpRoomListChannelClass))
#define TP_IS_ROOM_LIST_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_ROOM_LIST_CHANNEL))
#define TP_IS_ROOM_LIST_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_ROOM_LIST_CHANNEL))
#define TP_ROOM_LIST_CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_ROOM_LIST_CHANNEL, TpRoomListChannelClass))

typedef struct _TpRoomListChannel TpRoomListChannel;
typedef struct _TpRoomListChannelClass TpRoomListChannelClass;
typedef struct _TpRoomListChannelPrivate TpRoomListChannelPrivate;

struct _TpRoomListChannel
{
  /*<private>*/
  TpChannel parent;
  TpRoomListChannelPrivate *priv;
};

struct _TpRoomListChannelClass
{
  /*<private>*/
  TpChannelClass parent_class;
  GCallback _padding[7];
};

GType tp_room_list_channel_get_type (void);

const gchar * tp_room_list_channel_get_server (TpRoomListChannel *self);

gboolean tp_room_list_channel_get_listing (TpRoomListChannel *self);

#define TP_ROOM_LIST_CHANNEL_FEATURE_LISTING \
  tp_room_list_channel_get_feature_quark_listing ()

GQuark tp_room_list_channel_get_feature_quark_listing (void) G_GNUC_CONST;

void tp_room_list_channel_start_listing_async (TpRoomListChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_room_list_channel_start_listing_finish (TpRoomListChannel *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
