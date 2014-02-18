/*
 * Base class for Client implementations
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_BASE_CLIENT_H__
#define __TP_BASE_CLIENT_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include <telepathy-glib/account.h>
#include <telepathy-glib/add-dispatch-operation-context.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/handle-channel-context.h>
#include <telepathy-glib/observe-channel-context.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

/* forward declaration, see channel-dispatch-operation.h for the rest */
typedef struct _TpChannelDispatchOperation
    TpChannelDispatchOperation;

typedef struct _TpBaseClient TpBaseClient;
typedef struct _TpBaseClientClass TpBaseClientClass;
typedef struct _TpBaseClientPrivate TpBaseClientPrivate;
typedef struct _TpBaseClientClassPrivate TpBaseClientClassPrivate;

typedef void (*TpBaseClientClassObserveChannelImpl) (
    TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelContext *context);

typedef void (*TpBaseClientClassAddDispatchOperationImpl) (
    TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *dispatch_operation,
    TpAddDispatchOperationContext *context);

typedef void (*TpBaseClientClassHandleChannelImpl) (
    TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelContext *context);

struct _TpBaseClientClass {
    /*<public>*/
    GObjectClass parent_class;
    TpBaseClientClassObserveChannelImpl observe_channel;
    TpBaseClientClassAddDispatchOperationImpl add_dispatch_operation;
    TpBaseClientClassHandleChannelImpl handle_channel;
    /*<private>*/
    GCallback _padding[4];
    TpDBusPropertiesMixinClass dbus_properties_class;
    TpBaseClientClassPrivate *priv;
};

struct _TpBaseClient {
    /*<private>*/
    GObject parent;
    TpBaseClientPrivate *priv;
};

GType tp_base_client_get_type (void);

/* Protected methods; should be called only by subclasses */

void tp_base_client_implement_observe_channel (TpBaseClientClass *klass,
    TpBaseClientClassObserveChannelImpl impl);

void tp_base_client_implement_add_dispatch_operation (TpBaseClientClass *klass,
    TpBaseClientClassAddDispatchOperationImpl impl);

void tp_base_client_implement_handle_channel (TpBaseClientClass *klass,
    TpBaseClientClassHandleChannelImpl impl);

/* setup functions which can only be called before register() */

_TP_AVAILABLE_IN_0_20
void tp_base_client_add_observer_filter_vardict (TpBaseClient *self,
    GVariant *filter);

void tp_base_client_set_observer_recover (TpBaseClient *self,
    gboolean recover);
void tp_base_client_set_observer_delay_approvers (TpBaseClient *self,
    gboolean delay);

_TP_AVAILABLE_IN_0_20
void tp_base_client_add_approver_filter_vardict (TpBaseClient *self,
    GVariant *filter);

void tp_base_client_be_a_handler (TpBaseClient *self);

_TP_AVAILABLE_IN_0_20
void tp_base_client_add_handler_filter_vardict (TpBaseClient *self,
    GVariant *filter);
void tp_base_client_set_handler_bypass_approval (TpBaseClient *self,
    gboolean bypass_approval);

void tp_base_client_set_handler_request_notification (TpBaseClient *self);

void tp_base_client_add_handler_capability (TpBaseClient *self,
    const gchar *token);
void tp_base_client_add_handler_capabilities (TpBaseClient *self,
    const gchar * const *tokens);
void tp_base_client_add_handler_capabilities_varargs (TpBaseClient *self,
    const gchar *first_token, ...) G_GNUC_NULL_TERMINATED;

typedef void (*TpBaseClientDelegatedChannelsCb) (
    TpBaseClient *client,
    GPtrArray *channels,
    gpointer user_data);

_TP_AVAILABLE_IN_0_16
void tp_base_client_set_delegated_channels_callback (TpBaseClient *self,
    TpBaseClientDelegatedChannelsCb callback,
    gpointer user_data,
    GDestroyNotify destroy);

/* future, potentially (currently in spec as a draft):
void tp_base_client_set_handler_related_conferences_bypass_approval (
    TpBaseClient *self, gboolean bypass_approval);
    */

gboolean tp_base_client_register (TpBaseClient *self,
    GError **error);

/* Normal methods, can be called at any time */

_TP_AVAILABLE_IN_0_20
GList *tp_base_client_dup_pending_requests (TpBaseClient *self);
_TP_AVAILABLE_IN_0_20
GList *tp_base_client_dup_handled_channels (TpBaseClient *self);

gboolean tp_base_client_is_handling_channel (TpBaseClient *self,
    TpChannel *channel);

_TP_AVAILABLE_IN_0_16
void tp_base_client_delegate_channels_async (TpBaseClient *self,
    GList *channels,
    gint64 user_action_time,
    const gchar *preferred_handler,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_0_16
gboolean tp_base_client_delegate_channels_finish (TpBaseClient *self,
    GAsyncResult *result,
    GPtrArray **delegated,
    GHashTable **not_delegated,
    GError **error);

const gchar *tp_base_client_get_name (TpBaseClient *self);
gboolean tp_base_client_get_uniquify_name (TpBaseClient *self);
const gchar *tp_base_client_get_bus_name (TpBaseClient *self);
const gchar *tp_base_client_get_object_path (TpBaseClient *self);
TpDBusDaemon *tp_base_client_get_dbus_daemon (TpBaseClient *self);

void tp_base_client_unregister (TpBaseClient *self);

#define TP_TYPE_BASE_CLIENT \
  (tp_base_client_get_type ())
#define TP_BASE_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_BASE_CLIENT, \
                               TpBaseClient))
#define TP_BASE_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_BASE_CLIENT, \
                            TpBaseClientClass))
#define TP_IS_BASE_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_BASE_CLIENT))
#define TP_IS_BASE_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_BASE_CLIENT))
#define TP_BASE_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_CLIENT, \
                              TpBaseClientClass))

G_END_DECLS

#endif
