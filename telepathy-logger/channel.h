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

#ifndef __TPL_CHANNEL_H__
#define __TPL_CHANNEL_H__

#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/svc-client.h>

#include <telepathy-logger/observer.h>
#include <telepathy-logger/util.h>

G_BEGIN_DECLS
#define TPL_TYPE_CHANNEL                  (tpl_channel_get_type ())
#define TPL_CHANNEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CHANNEL, TplChannel))
#define TPL_CHANNEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CHANNEL, TplChannelClass))
#define TPL_IS_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CHANNEL))
#define TPL_IS_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CHANNEL))
#define TPL_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CHANNEL, TplChannelClass))

typedef struct _TplChannelPriv TplChannelPriv;

typedef struct
{
  GObject parent;

  /* private */
  TplChannelPriv *priv;

  TpChannel *channel;
  gchar *channel_path;
  gchar *channel_type;
  GHashTable *channel_properties;

  TpAccount *account;
  gchar *account_path;
  TpConnection *connection;
  gchar *connection_path;

  TpSvcClientObserver *observer;
} TplChannel;

typedef struct
{
  GObjectClass parent_class;
} TplChannelClass;


GType tpl_channel_get_type (void);

TplChannel *tpl_channel_new (TpSvcClientObserver * observer);
void tpl_channel_free (TplChannel * tpl_chan);

TpSvcClientObserver *tpl_channel_get_observer (TplChannel * self);
TpAccount *tpl_channel_get_account (TplChannel * self);
const gchar *tpl_channel_get_account_path (TplChannel * self);
TpConnection *tpl_channel_get_connection (TplChannel * self);
const gchar *tpl_channel_get_connection_path (TplChannel * self);
TpChannel *tpl_channel_get_channel (TplChannel * self);
const gchar *tpl_channel_get_channel_path (TplChannel * self);
const gchar *tpl_channel_get_channel_type (TplChannel * self);
GHashTable *tpl_channel_get_channel_properties (TplChannel * self);


void tpl_channel_set_observer (TplChannel * self, TpSvcClientObserver * data);
void tpl_channel_set_account (TplChannel * self, TpAccount * data);
void tpl_channel_set_account_path (TplChannel * self, const gchar * data);
void tpl_channel_set_connection (TplChannel * self, TpConnection * data);
void tpl_channel_set_connection_path (TplChannel * self, const gchar * data);
void tpl_channel_set_channel (TplChannel * self, TpChannel * data);
void tpl_channel_set_channel_path (TplChannel * self, const gchar * data);
void tpl_channel_set_channel_type (TplChannel * self, const gchar * data);
void tpl_channel_set_channel_properties (TplChannel * self,
					 GHashTable * data);

gboolean tpl_channel_register_to_observer (TplChannel * self);
gboolean tpl_channel_unregister_from_observer (TplChannel * self);

G_END_DECLS
#endif // __TPL_CHANNEL_H__
