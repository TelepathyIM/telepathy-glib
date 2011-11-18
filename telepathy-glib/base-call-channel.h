/*
 * base-call-channel.h - Header for TpBaseCallChannel
 * Copyright © 2009–2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __TP_BASE_CALL_CHANNEL_H__
#define __TP_BASE_CALL_CHANNEL_H__

#include <telepathy-glib/base-channel.h>
#include <telepathy-glib/base-call-content.h>

G_BEGIN_DECLS

typedef struct _TpBaseCallChannel TpBaseCallChannel;
typedef struct _TpBaseCallChannelPrivate TpBaseCallChannelPrivate;
typedef struct _TpBaseCallChannelClass TpBaseCallChannelClass;

typedef void (*TpBaseCallChannelVoidFunc) (TpBaseCallChannel *self);
typedef TpBaseCallContent * (*TpBaseCallChannelAddContentFunc) (
    TpBaseCallChannel *self,
    const gchar *name,
    TpMediaStreamType media,
    GError **error);
typedef void (*TpBaseCallChannelHangupFunc) (TpBaseCallChannel *self,
    TpCallStateChangeReason reason,
    const gchar *detailed_reason,
    const gchar *message);

struct _TpBaseCallChannelClass {
  /*<private>*/
  TpBaseChannelClass parent_class;

  /*< public >*/
  TpBaseCallChannelVoidFunc set_ringing;
  TpBaseCallChannelVoidFunc set_queued;
  TpBaseCallChannelVoidFunc accept;
  TpBaseCallChannelAddContentFunc add_content;
  TpBaseCallChannelHangupFunc hangup;

  /*<private>*/
  gpointer future[4];
};

struct _TpBaseCallChannel {
  /*<private>*/
  TpBaseChannel parent;

  TpBaseCallChannelPrivate *priv;
};

GType tp_base_call_channel_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_BASE_CALL_CHANNEL \
  (tp_base_call_channel_get_type ())
#define TP_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   TP_TYPE_BASE_CALL_CHANNEL, TpBaseCallChannel))
#define TP_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
   TP_TYPE_BASE_CALL_CHANNEL, TpBaseCallChannelClass))
#define TP_IS_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_BASE_CALL_CHANNEL))
#define TP_IS_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_BASE_CALL_CHANNEL))
#define TP_BASE_CALL_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   TP_TYPE_BASE_CALL_CHANNEL, TpBaseCallChannelClass))

TpCallState tp_base_call_channel_get_state (TpBaseCallChannel *self);
void tp_base_call_channel_set_state (TpBaseCallChannel *self,
    TpCallState state,
    guint actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);

gboolean tp_base_call_channel_has_initial_audio (TpBaseCallChannel *self,
    const gchar **initial_audio_name);
gboolean tp_base_call_channel_has_initial_video (TpBaseCallChannel *self,
    const gchar **initial_video_name);

gboolean tp_base_call_channel_has_mutable_contents (TpBaseCallChannel *self);
GList * tp_base_call_channel_get_contents (TpBaseCallChannel *self);
void tp_base_call_channel_add_content (TpBaseCallChannel *self,
    TpBaseCallContent *content);
void tp_base_call_channel_remove_content (TpBaseCallChannel *self,
    TpBaseCallContent *content,
    TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);

void tp_base_call_channel_update_member_flags (TpBaseCallChannel *self,
    TpHandle contact,
    TpCallMemberFlags new_flags,
    TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);
void tp_base_call_channel_remove_member (TpBaseCallChannel *self,
    TpHandle contact,
    TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);

G_END_DECLS

#endif /* #ifndef __TP_BASE_CALL_CHANNEL_H__*/
