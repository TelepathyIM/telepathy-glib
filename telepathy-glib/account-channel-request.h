/*
 * object used to request a channel from a TpAccount
 *
 * Copyright © 2010 Collabora Ltd.
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

#ifndef __TP_ACCOUNT_CHANNEL_REQUEST_H__
#define __TP_ACCOUNT_CHANNEL_REQUEST_H__

#include <gio/gio.h>
#include <glib-object.h>

#include <telepathy-glib/account.h>

G_BEGIN_DECLS

typedef struct _TpAccountChannelRequest TpAccountChannelRequest;
typedef struct _TpAccountChannelRequestClass \
          TpAccountChannelRequestClass;
typedef struct _TpAccountChannelRequestPrivate \
          TpAccountChannelRequestPrivate;

GType tp_account_channel_request_get_type (void);

#define TP_TYPE_ACCOUNT_CHANNEL_REQUEST \
  (tp_account_channel_request_get_type ())
#define TP_ACCOUNT_CHANNEL_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_ACCOUNT_CHANNEL_REQUEST, \
                               TpAccountChannelRequest))
#define TP_ACCOUNT_CHANNEL_REQUEST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_ACCOUNT_CHANNEL_REQUEST, \
                            TpAccountChannelRequestClass))
#define TP_IS_ACCOUNT_CHANNEL_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_ACCOUNT_CHANNEL_REQUEST))
#define TP_IS_ACCOUNT_CHANNEL_REQUEST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_ACCOUNT_CHANNEL_REQUEST))
#define TP_ACCOUNT_CHANNEL_REQUEST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_ACCOUNT_CHANNEL_REQUEST, \
                              TpAccountChannelRequestClass))

TpAccountChannelRequest * tp_account_channel_request_new (
    TpAccount *account,
    GHashTable *request,
    gint64 user_action_time);

TpAccount * tp_account_channel_request_get_account (
    TpAccountChannelRequest *self);

GHashTable * tp_account_channel_request_get_request (
    TpAccountChannelRequest *self);

gint64 tp_account_channel_request_get_user_action_time (
    TpAccountChannelRequest *self);

G_END_DECLS

#endif
