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
#include <glib.h>

#include <telepathy-glib/account.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/client-channel-factory.h>
#include <telepathy-glib/handle-channels-context.h>

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
    gint64 user_action_time) G_GNUC_WARN_UNUSED_RESULT;

TpAccount * tp_account_channel_request_get_account (
    TpAccountChannelRequest *self);

GHashTable * tp_account_channel_request_get_request (
    TpAccountChannelRequest *self);

gint64 tp_account_channel_request_get_user_action_time (
    TpAccountChannelRequest *self);

void tp_account_channel_request_set_channel_factory (
    TpAccountChannelRequest *self,
    TpClientChannelFactory *factory);


TpChannelRequest * tp_account_channel_request_get_channel_request (
    TpAccountChannelRequest *self);

void tp_account_channel_request_set_hints (TpAccountChannelRequest *self,
    GHashTable *hints);

/* Request and handle API */

void tp_account_channel_request_create_and_handle_channel_async (
    TpAccountChannelRequest *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

TpChannel * tp_account_channel_request_create_and_handle_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    TpHandleChannelsContext **context,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

void tp_account_channel_request_ensure_and_handle_channel_async (
    TpAccountChannelRequest *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

TpChannel * tp_account_channel_request_ensure_and_handle_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    TpHandleChannelsContext **context,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

/* Request and forget API */

void tp_account_channel_request_create_channel_async (
    TpAccountChannelRequest *self,
    const gchar *preferred_handler,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_account_channel_request_create_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    GError **error);

void tp_account_channel_request_ensure_channel_async (
    TpAccountChannelRequest *self,
    const gchar *preferred_handler,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_account_channel_request_ensure_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    GError **error);

/* Request and observe API */

void tp_account_channel_request_create_and_observe_channel_async (
    TpAccountChannelRequest *self,
    const gchar *preferred_handler,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

TpChannel * tp_account_channel_request_create_and_observe_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

void tp_account_channel_request_ensure_and_observe_channel_async (
    TpAccountChannelRequest *self,
    const gchar *preferred_handler,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

TpChannel * tp_account_channel_request_ensure_and_observe_channel_finish (
    TpAccountChannelRequest *self,
    GAsyncResult *result,
    GError **error) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif
