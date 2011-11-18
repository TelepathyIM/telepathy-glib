/*
 * base-call-internal.h - Header for TpBaseCall* (internals)
 * Copyright © 2011 Collabora Ltd.
 * @author Olivier Crete <olivier.crete@collabora.com>
 * @author Xavier Claessens <xavier.claessens@collabora.co.uk>
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

#ifndef __TP_BASE_CALL_INTERNAL_H__
#define __TP_BASE_CALL_INTERNAL_H__

G_BEGIN_DECLS

typedef struct _TpBaseCallChannel TpBaseCallChannel;
typedef struct _TpBaseCallContent TpBaseCallContent;
typedef struct _TpBaseCallStream  TpBaseCallStream;

/* Implemented in base-call-content.c */
void _tp_base_call_content_set_channel (TpBaseCallContent *self,
    TpBaseCallChannel *channel);
void _tp_base_call_content_accepted (TpBaseCallContent *self,
    TpHandle actor_handle);
void _tp_base_call_content_deinit (TpBaseCallContent *self);

/* Implemented in base-call-stream.c */
void _tp_base_call_stream_set_channel (TpBaseCallStream *self,
    TpBaseCallChannel *channel);
gboolean _tp_base_call_stream_set_sending (TpBaseCallStream *self,
    gboolean send,
    TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message,
    GError **error);

/* Implemented in base-call-channel.c */
GHashTable *_tp_base_call_dup_member_identifiers (TpBaseConnection *conn,
    GHashTable *source);
GValueArray *_tp_base_call_state_reason_new (TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);

G_END_DECLS

#endif /* #ifndef __TP_BASE_CALL_INTERNAL_H__*/
