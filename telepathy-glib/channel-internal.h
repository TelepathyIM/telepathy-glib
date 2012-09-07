/*<private_header>*/
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

typedef void (*TpChannelProc) (TpChannel *self);

typedef struct {
    TpContact *actor_contact;
    TpChannelGroupChangeReason reason;
    gchar *message;
} LocalPendingInfo;

typedef struct _ContactsQueueItem ContactsQueueItem;

struct _TpChannelPrivate {
    gulong conn_invalidated_id;

    TpConnection *connection;

    /* GQueue of TpChannelProc */
    GQueue *introspect_needed;

    GQuark channel_type;
    TpHandleType handle_type;
    TpHandle handle;
    gchar *identifier;
    /* owned string (iface + "." + prop) => slice-allocated GValue */
    GHashTable *channel_properties;

    TpChannelGroupFlags group_flags;

    /* reason the self-handle left */
    GError *group_remove_error /* implicitly zero-initialized */ ;

    /* reffed TpContact */
    TpContact *target_contact;
    TpContact *initiator_contact;
    TpContact *group_self_contact;
    /* TpHandle -> reffed TpContact */
    GHashTable *group_members;
    GHashTable *group_local_pending;
    GHashTable *group_remote_pending;
    /* TpHandle -> reffed TpContact or NULL */
    GHashTable *group_contact_owners;
    /* TpHandle -> LocalPendingInfo */
    GHashTable *group_local_pending_info;
    gboolean group_properties_retrieved;

    /* Queue of GSimpleAsyncResult with ContactsQueueItem payload */
    GQueue *contacts_queue;
    /* Item currently being prepared, not part of contacts_queue anymore */
    GSimpleAsyncResult *current_contacts_queue_result;

    /* These are really booleans, but gboolean is signed. Thanks, GLib */

    /* Enough method calls have succeeded that we believe that the channel
     * exists (implied by ready) */
    unsigned exists:1;
    /* GetGroupFlags has returned */
    unsigned have_group_flags:1;

    TpChannelPasswordFlags password_flags;
};

/* channel.c internals */

void _tp_channel_continue_introspection (TpChannel *self);
void _tp_channel_abort_introspection (TpChannel *self,
    const gchar *debug,
    const GError *error);
GHashTable *_tp_channel_get_immutable_properties (TpChannel *self);

/* channel-group.c internals */

void _tp_channel_group_prepare_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data);

void _tp_channel_contacts_queue_prepare_async (TpChannel *self,
    GPtrArray *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean _tp_channel_contacts_queue_prepare_finish (TpChannel *self,
    GAsyncResult *result,
    GPtrArray **contacts,
    GError **error);

G_END_DECLS

#endif
