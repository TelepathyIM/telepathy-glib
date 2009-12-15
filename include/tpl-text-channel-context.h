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

#ifndef __TPL_TEXT_CHANNEL_H__
#define __TPL_TEXT_CHANNEL_H__

/* 
 * http://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-channel-text.html#tp-cli-channel-type-text-connect-to-received
 */

#include <glib-object.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/svc-client.h>

#include <tpl-channel.h>
#include <tpl-observer.h>
#include <tpl-utils.h>

G_BEGIN_DECLS

#define TPL_TYPE_TEXT_CHANNEL                  (tpl_text_channel_get_type ())
#define TPL_TEXT_CHANNEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannel))
#define TPL_TEXT_CHANNEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))
#define TPL_IS_TEXT_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_TEXT_CHANNEL))
#define TPL_IS_TEXT_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_TEXT_CHANNEL))
#define TPL_TEXT_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))


typedef struct {
	GObject		parent;

	/* private */
	TplChannel	*tpl_channel;
	gboolean	chatroom;
	TpContact	*my_contact;
	TpContact	*remote_contact;	// only set if chatroom==FALSE
	const gchar	*chatroom_id;		// only set if chatroom==TRUE

	GQueue		*chain;			// queue of TplPendingProc

	// only used as metadata in CB data passing
	guint		selector;
} TplTextChannel;

typedef struct {
	GObjectClass	parent_class;
} TplTextChannelClass;

GType tpl_text_channel_get_type (void);

TplTextChannel* tpl_text_channel_new(TplChannel* tpl_channel);

TplChannel *tpl_text_channel_get_tpl_channel(TplTextChannel *self);
TpContact *tpl_text_channel_get_remote_contact(TplTextChannel *self);
TpContact *tpl_text_channel_get_my_contact(TplTextChannel *self);
gboolean tpl_text_channel_is_chatroom(TplTextChannel *self);
const gchar *tpl_text_channel_get_chatroom_id(TplTextChannel *self);

void tpl_text_channel_set_tpl_channel(TplTextChannel *self, TplChannel *tpl_chan);
void tpl_text_channel_set_remote_contact(TplTextChannel *self, TpContact *data);
void tpl_text_channel_set_my_contact(TplTextChannel *self, TpContact *data);
void tpl_text_channel_set_chatroom(TplTextChannel *self, gboolean data);
void tpl_text_channel_set_chatroom_id(TplTextChannel *self, const gchar *data);

G_END_DECLS

#endif // __TPL_TEXT_CHANNEL_H__
