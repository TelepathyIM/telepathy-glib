/*
 * TpChannel - proxy for a Telepathy channel (internals)
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#ifndef TP_CHANNEL_INTERNAL_H
#define TP_CHANNEL_INTERNAL_H

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

struct _TpChannelClass {
    TpProxyClass parent_class;
    gpointer priv;
};

struct _TpChannel {
    TpProxy parent;

    TpConnection *connection;

    gboolean ready:1;
    gboolean _reserved_flags:31;
    GQuark channel_type;
    TpHandleType handle_type;
    TpHandle handle;

    TpHandle group_self_handle;
    TpChannelGroupFlags group_flags;

    TpChannelPrivate *priv;
};

typedef void (*TpChannelProc) (TpChannel *self);

typedef struct {
    TpHandle actor;
    TpChannelGroupChangeReason reason;
    gchar *message;
} LocalPendingInfo;

struct _TpChannelPrivate {
    gulong conn_invalidated_id;

    /* GQueue of TpChannelProc */
    GQueue *introspect_needed;

    /* NULL if members not discovered yet */
    TpIntSet *group_members;
    TpIntSet *group_local_pending;
    TpIntSet *group_remote_pending;
    /* (TpHandle => LocalPendingInfo), or NULL if members not discovered yet */
    GHashTable *group_local_pending_info;

    /* reason the self-handle left, message == NULL if not removed */
    gchar *group_remove_message;
    TpChannelGroupChangeReason group_remove_reason;
    /* guint => guint, NULL if not discovered yet */
    GHashTable *group_handle_owners;

    gboolean have_group_flags:1;
};

/* channel.c internals */

void _tp_channel_continue_introspection (TpChannel *self);

/* channel-group.c internals */

void _tp_channel_get_group_properties (TpChannel *self);

G_END_DECLS

#endif
