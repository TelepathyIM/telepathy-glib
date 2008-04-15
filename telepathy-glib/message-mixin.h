/*
 * message-mixin.h - Header for TpMessageMixin
 * Copyright (C) 2006-2008 Collabora Ltd.
 * Copyright (C) 2006-2008 Nokia Corporation
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

#ifndef TP_MESSAGE_MIXIN_H
#define TP_MESSAGE_MIXIN_H

#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/svc-channel.h>
#include "util.h"

G_BEGIN_DECLS

typedef struct _TpMessageMixinClass TpMessageMixinClass;
typedef struct _TpMessageMixinClassPrivate TpMessageMixinClassPrivate;
typedef struct _TpMessageMixin TpMessageMixin;
typedef struct _TpMessageMixinPrivate TpMessageMixinPrivate;

struct _TpMessageMixinClass {
    /*<private>*/
    TpMessageMixinClassPrivate *priv;
};

struct _TpMessageMixin {
  /*<private>*/
  TpMessageMixinPrivate *priv;
};


/* Receiving */

guint tp_message_mixin_take_received (GObject *object,
    time_t timestamp, TpHandle sender, TpChannelTextMessageType message_type,
    GPtrArray *content);


/* Sending */

typedef struct _TpMessageMixinOutgoingMessagePrivate
    TpMessageMixinOutgoingMessagePrivate;

typedef struct _TpMessageMixinOutgoingMessage {
    guint flags;
    guint message_type;
    GPtrArray *parts;
    TpMessageMixinOutgoingMessagePrivate *priv;
} TpMessageMixinOutgoingMessage;

typedef gboolean (*TpMessageMixinSendImpl) (GObject *object,
    TpMessageMixinOutgoingMessage *message);

void tp_message_mixin_sent (GObject *object,
    TpMessageMixinOutgoingMessage *message, const gchar *token,
    const GError *error);

void tp_message_mixin_implement_sending (GObject *object,
    TpMessageMixinSendImpl send);


/* Initialization */
void tp_message_mixin_text_iface_init (gpointer g_iface, gpointer iface_data);
void tp_message_mixin_message_parts_iface_init (gpointer g_iface,
    gpointer iface_data);

void tp_message_mixin_class_init (GObjectClass *obj_cls, gsize offset);

void tp_message_mixin_init (GObject *obj, gsize offset,
    TpHandleRepoIface *contact_repo);
void tp_message_mixin_finalize (GObject *obj);

G_END_DECLS

#endif
