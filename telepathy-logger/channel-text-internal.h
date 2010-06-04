/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_CHANNEL_TEXT_H__
#define __TPL_CHANNEL_TEXT_H__

/*
 * http://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-channel-text.html#tp-cli-channel-type-text-connect-to-received
 */

#include <glib-object.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>

#include <telepathy-logger/channel-internal.h>

G_BEGIN_DECLS
#define TPL_TYPE_CHANNEL_TEXT                  (_tpl_channel_text_get_type ())
#define TPL_CHANNEL_TEXT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CHANNEL_TEXT, TplChannelText))
#define TPL_CHANNEL_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CHANNEL_TEXT, TplChannelTextClass))
#define TPL_IS_CHANNEL_TEXT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CHANNEL_TEXT))
#define TPL_IS_CHANNEL_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CHANNEL_TEXT))
#define TPL_CHANNEL_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CHANNEL_TEXT, TplChannelTextClass))

typedef struct _TplChannelTextPriv TplChannelTextPriv;
typedef struct
{
  TplChannel parent;

  /* private */
  TplChannelTextPriv *priv;
} TplChannelText;

typedef struct
{
  TplChannelClass parent_class;
} TplChannelTextClass;

GType _tpl_channel_text_get_type (void);

TplChannelText * _tpl_channel_text_new (TpConnection *conn,
    const gchar *object_path, GHashTable *tp_chan_props, TpAccount *account,
    GError **error);

TplChannel * _tpl_channel_text_get_tpl_channel (TplChannelText * self);
TpContact * _tpl_channel_text_get_remote_contact (TplChannelText * self);
TpContact * _tpl_channel_text_get_my_contact (TplChannelText * self);
gboolean  _tpl_channel_text_is_chatroom (TplChannelText * self);
const gchar * _tpl_channel_text_get_chatroom_id (TplChannelText * self);

void _tpl_channel_text_set_tpl_channel (TplChannelText * self,
  TplChannel * tpl_chan);
void _tpl_channel_text_set_remote_contact (TplChannelText * self,
            TpContact * data);
void _tpl_channel_text_set_my_contact (TplChannelText * self,
              TpContact * data);
void _tpl_channel_text_set_chatroom (TplChannelText * self, gboolean data);
void _tpl_channel_text_set_chatroom_id (TplChannelText * self,
               const gchar * data);

void _tpl_channel_text_call_when_ready (TplChannelText *self,
    GAsyncReadyCallback cb, gpointer user_data);

G_END_DECLS
#endif /* __TPL_CHANNEL_TEXT_H__ */
