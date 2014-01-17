/*<private_header>*/
/*
 * object for HandleChannels calls context (internal)
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_HANDLE_CHANNEL_CONTEXT_INTERNAL_H__
#define __TP_HANDLE_CHANNEL_CONTEXT_INTERNAL_H__

#include <dbus/dbus-glib.h>

#include <telepathy-glib/account.h>
#include <telepathy-glib/handle-channel-context.h>

G_BEGIN_DECLS

typedef enum
{
  TP_HANDLE_CHANNEL_CONTEXT_STATE_NONE,
  TP_HANDLE_CHANNEL_CONTEXT_STATE_DONE,
  TP_HANDLE_CHANNEL_CONTEXT_STATE_FAILED,
  TP_HANDLE_CHANNEL_CONTEXT_STATE_DELAYED,
} TpHandleChannelContextState;

struct _TpHandleChannelContext {
  /*<private>*/
  GObject parent;
  TpHandleChannelContextPrivate *priv;

  TpAccount *account;
  TpConnection *connection;
  /* array of reffed TpChannel */
  GPtrArray *channels;
  /* array of reffed TpChannelRequest */
  GPtrArray *requests_satisfied;
  guint64 user_action_time;
  GHashTable *handler_info;
};

TpHandleChannelContext * _tp_handle_channel_context_new (
    TpAccount *account,
    TpConnection *connection,
    GPtrArray *channels,
    GPtrArray *requests_satisfied,
    guint64 user_action_time,
    GHashTable *handler_info,
    DBusGMethodInvocation *dbus_context);

TpHandleChannelContextState _tp_handle_channel_context_get_state
    (TpHandleChannelContext *self);

void _tp_handle_channel_context_prepare_async (
    TpHandleChannelContext *self,
    const GQuark *account_features,
    const GQuark *connection_features,
    const GQuark *channel_features,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean _tp_handle_channel_context_prepare_finish (
    TpHandleChannelContext *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
