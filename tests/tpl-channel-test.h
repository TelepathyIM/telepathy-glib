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

#ifndef __TPL_CHANNEL_TEST_H__
#define __TPL_CHANNEL_TEST_H__

/* 
 * http://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-channel-text.html#tp-cli-channel-type-text-connect-to-received
 */

#include <glib-object.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/svc-client.h>

#include <telepathy-logger/channel.h>
#include <telepathy-logger/observer.h>

G_BEGIN_DECLS
#define TPL_TYPE_CHANNEL_TEST                 (tpl_channel_test_get_type ())
#define TPL_CHANNEL_TEST(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CHANNEL_TEST, TplChannelTest))
#define TPL_CHANNEL_TEST_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CHANNEL_TEST, TplChannelTestClass))
#define TPL_IS_CHANNEL_TEST(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CHANNEL_TEST))
#define TPL_IS_CHANNEL_TEST_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CHANNEL_TEST))
#define TPL_CHANNEL_TEST_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CHANNEL_TEST, TplChannelTestClass))

typedef struct _TplChannelTestPriv TplChannelTestPriv;
typedef struct
{
  TplChannel parent;

  /* private */
  TplChannelTestPriv *priv;
} TplChannelTest;

typedef struct
{
  TplChannelClass parent_class;
} TplChannelTestClass;

GType tpl_channel_test_get_type (void);

TplChannelTest *tpl_channel_test_new (TpConnection *conn,
    const gchar *object_path, GHashTable *tp_chan_props, TpAccount *account,
    GError **error);

TplChannel *tpl_channel_test_get_tpl_channel (TplChannelTest * self);
TpContact *tpl_channel_test_get_remote_contact (TplChannelTest * self);
TpContact *tpl_channel_test_get_my_contact (TplChannelTest * self);
gboolean tpl_channel_test_is_chatroom (TplChannelTest * self);
const gchar *tpl_channel_test_get_chatroom_id (TplChannelTest * self);

void tpl_channel_test_set_tpl_channel (TplChannelTest * self,
				       TplChannel * tpl_chan);
void tpl_channel_test_set_remote_contact (TplChannelTest * self,
					  TpContact * data);
void tpl_channel_test_set_my_contact (TplChannelTest * self,
				      TpContact * data);
void tpl_channel_test_set_chatroom (TplChannelTest * self, gboolean data);
void tpl_channel_test_set_chatroom_id (TplChannelTest * self,
				       const gchar * data);

void tpl_channel_test_call_when_ready (TplChannelTest *self,
    GAsyncReadyCallback cb, gpointer user_data);

G_END_DECLS
#endif /* __TPL_CHANNEL_TEST_H__ */
