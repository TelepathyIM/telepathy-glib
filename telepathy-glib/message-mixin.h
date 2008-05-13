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

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/svc-channel.h>
#include "util.h"

G_BEGIN_DECLS

typedef struct _TpMessageMixin TpMessageMixin;
typedef struct _TpMessageMixinPrivate TpMessageMixinPrivate;

struct _TpMessageMixin {
  /*<private>*/
  TpMessageMixinPrivate *priv;
};


typedef struct _TpMessage TpMessage;

TpMessage *tp_message_new (TpBaseConnection *connection, guint initial_parts,
    guint size_hint);
void tp_message_unref (TpMessage *self);
TpMessage *tp_message_copy (const TpMessage *self);
guint tp_message_count_parts (TpMessage *self);
const GHashTable *tp_message_peek (TpMessage *self, guint part);
void tp_message_delete_part (TpMessage *self, guint part);
void tp_message_ref_handle (TpMessage *self, TpHandleType handle_type,
    TpHandle handle_or_0);

gboolean tp_message_delete_key (TpMessage *self, guint part, const gchar *key);
void tp_message_set_handle (TpMessage *self, guint part, const gchar *key,
    TpHandleType handle_type, TpHandle handle_or_0);
void tp_message_set_boolean (TpMessage *self, guint part, const gchar *key,
    gboolean b);
void tp_message_set_int32 (TpMessage *self, guint part, const gchar *key,
    gint32 i);
#define tp_message_set_int16(s, p, k, i) \
    tp_message_set_int32 (s, p, k, (gint16) i)
void tp_message_set_uint32 (TpMessage *self, guint part, const gchar *key,
    guint32 u);
#define tp_message_set_uint16(s, p, k, u) \
    tp_message_set_uint32 (s, p, k, (guint16) u)
void tp_message_set_string (TpMessage *self, guint part, const gchar *key,
    const gchar *s);
void tp_message_set_bytes (TpMessage *self, guint part, const gchar *key,
    guint len, gconstpointer bytes);
void tp_message_set (TpMessage *self, guint part, const gchar *key,
    const GValue *source);


/* Sending */

typedef struct _TpMessageMixinOutgoingMessagePrivate
    TpMessageMixinOutgoingMessagePrivate;

typedef struct _TpMessageMixinOutgoingMessage {
    TpMessageSendingFlags flags;
    GPtrArray *parts;
    TpMessageMixinOutgoingMessagePrivate *priv;
} TpMessageMixinOutgoingMessage;

typedef void (*TpMessageMixinSendImpl) (GObject *object,
    TpMessageMixinOutgoingMessage *message);

void tp_message_mixin_sent (GObject *object,
    TpMessageMixinOutgoingMessage *message, const gchar *token,
    const GError *error);

void tp_message_mixin_implement_sending (GObject *object,
    TpMessageMixinSendImpl send, guint n_types,
    const TpChannelTextMessageType *types);


/* Initialization */
void tp_message_mixin_text_iface_init (gpointer g_iface, gpointer iface_data);
void tp_message_mixin_messages_iface_init (gpointer g_iface,
    gpointer iface_data);

void tp_message_mixin_init (GObject *obj, gsize offset,
    TpMessageMixinCleanUpReceivedImpl clean_up_received);
void tp_message_mixin_finalize (GObject *obj);

G_END_DECLS

#endif
