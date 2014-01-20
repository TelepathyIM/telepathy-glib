/*<private_header>*/
/*
 * Context objects for TpBaseClient calls (internal)
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

#ifndef __TP_OBSERVE_CHANNEL_CONTEXT_INTERNAL_H__
#define __TP_OBSERVE_CHANNEL_CONTEXT_INTERNAL_H__

#include <dbus/dbus-glib.h>

#include <telepathy-glib/account.h>
#include <telepathy-glib/channel-dispatch-operation.h>
#include <telepathy-glib/observe-channel-context.h>

G_BEGIN_DECLS

typedef enum
{
  TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE,
  TP_OBSERVE_CHANNEL_CONTEXT_STATE_DONE,
  TP_OBSERVE_CHANNEL_CONTEXT_STATE_FAILED,
  TP_OBSERVE_CHANNEL_CONTEXT_STATE_DELAYED,
} TpObserveChannelContextState;

struct _TpObserveChannelContext {
  /*<private>*/
  GObject parent;
  TpObserveChannelContextPrivate *priv;

  TpAccount *account;
  TpConnection *connection;
  TpChannel *channel;
  /* Reffed TpChannelDispatchOperation, or NULL */
  TpChannelDispatchOperation *dispatch_operation;
  /* Array of reffed TpChannelRequest */
  GPtrArray *requests;
  GHashTable *observer_info;
};

TpObserveChannelContext * _tp_observe_channel_context_new (
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *dispatch_operation,
    GPtrArray *requests,
    GHashTable *observer_info,
    DBusGMethodInvocation *dbus_context);

TpObserveChannelContextState _tp_observe_channel_context_get_state (
    TpObserveChannelContext *self);

void _tp_observe_channel_context_prepare_async (TpObserveChannelContext *self,
    const GQuark *account_features,
    const GQuark *connection_features,
    const GQuark *channel_features,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean _tp_observe_channel_context_prepare_finish (
    TpObserveChannelContext *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
