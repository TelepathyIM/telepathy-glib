/*
 * text-channel.h - high level API for Text channels
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_TEXT_CHANNEL_H__
#define __TP_TEXT_CHANNEL_H__

#include <telepathy-glib/channel.h>
#include <telepathy-glib/client-message.h>
#include <telepathy-glib/signalled-message.h>

G_BEGIN_DECLS


#define TP_TYPE_TEXT_CHANNEL (tp_text_channel_get_type ())
#define TP_TEXT_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_TEXT_CHANNEL, TpTextChannel))
#define TP_TEXT_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_TEXT_CHANNEL, TpTextChannelClass))
#define TP_IS_TEXT_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_TEXT_CHANNEL))
#define TP_IS_TEXT_CHANNEL_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_TEXT_CHANNEL))
#define TP_TEXT_CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_TEXT_CHANNEL, TpTextChannelClass))

typedef struct _TpTextChannel TpTextChannel;
typedef struct _TpTextChannelClass TpTextChannelClass;
typedef struct _TpTextChannelPrivate TpTextChannelPrivate;

struct _TpTextChannel
{
  /*<private>*/
  TpChannel parent;
  TpTextChannelPrivate *priv;
};

struct _TpTextChannelClass
{
  /*<private>*/
  TpChannelClass parent_class;
  GCallback _padding[7];
};

GType tp_text_channel_get_type (void);

TpTextChannel *tp_text_channel_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error);

const gchar * const *
/* gtk-doc sucks */
tp_text_channel_get_supported_content_types (TpTextChannel *self);

TpMessagePartSupportFlags tp_text_channel_get_message_part_support_flags (
    TpTextChannel *self);

TpDeliveryReportingSupportFlags tp_text_channel_get_delivery_reporting_support (
    TpTextChannel *self);

GArray * tp_text_channel_get_message_types (TpTextChannel *self);

gboolean tp_text_channel_supports_message_type (TpTextChannel *self,
    TpChannelTextMessageType message_type);

#define TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES \
  tp_text_channel_get_feature_quark_incoming_messages ()
GQuark tp_text_channel_get_feature_quark_incoming_messages (void) G_GNUC_CONST;

GList * tp_text_channel_get_pending_messages (TpTextChannel *self);

void tp_text_channel_send_message_async (TpTextChannel *self,
    TpMessage *message,
    TpMessageSendingFlags flags,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_text_channel_send_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    gchar **token,
    GError **error);

void tp_text_channel_ack_messages_async (TpTextChannel *self,
    const GList *messages,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_text_channel_ack_messages_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error);

void tp_text_channel_ack_message_async (TpTextChannel *self,
    TpMessage *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_text_channel_ack_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error);

void tp_text_channel_set_chat_state_async (TpTextChannel *self,
    TpChannelChatState state,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_text_channel_set_chat_state_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
