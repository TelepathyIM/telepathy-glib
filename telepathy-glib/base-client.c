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

/**
 * SECTION:base-client
 * @title: TpBaseClient
 * @short_description: base class for Telepathy clients on D-Bus
 *
 * This base class makes it easier to write #TpSvcClient
 * implementations. Subclasses should usually pass the filters they
 * want and override the D-Bus methods they implement.
 *
 * See #TpSimpleObserver for a class implementing a simple observer using
 * #TpBaseClient.
 */

/**
 * TpBaseClient:
 *
 * Data structure representing a generic #TpSvcClient implementation.
 *
 * Since: 0.11.5
 */

/**
 * TpBaseClientClass:
 *
 * The class of a #TpBaseClient.
 *
 * Since: 0.11.5
 */

/**
 * TpBaseClientClassObserveChannelsImpl:
 * @client: a #TpBaseClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @dispatch_operation: (allow-none): a #TpChannelDispatchOperation or %NULL;
 *  the dispatch_operation is not guaranteed to be prepared
 * @requests: (element-type TelepathyGLib.ChannelRequest): a #GList of
 *  #TpChannelRequest having their object-path defined but are not guaranteed
 *  to be prepared.
 * @context: a #TpObserveChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the ObserveChannels method.
 *
 * This function must call either tp_observe_channels_context_accept(),
 * tp_observe_channels_context_delay() or tp_observe_channels_context_fail()
 * on @context before it returns.
 *
 * Since: 0.11.5
 */

/**
 * TpBaseClientClassAddDispatchOperationImpl:
 * @client: a #TpBaseClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @dispatch_operation: a #TpChannelDispatchOperation having
 * %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE prepared if possible
 * @context: a #TpObserveChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the AddDispatchOperation method.
 *
 * This function must call either tp_add_dispatch_operation_context_accept(),
 * tp_add_dispatch_operation_context_delay() or
 * tp_add_dispatch_operation_context_fail() on @context before it returns.
 *
 * The implementation can then use
 * tp_channel_dispatch_operation_handle_with_async() to approve handling of the
 * channels, or tp_channel_dispatch_operation_claim_async() to take
 * responsibility for handling or closing them".
 *
 * Since: 0.11.5
 */

/**
 * TpBaseClientClassHandleChannelsImpl:
 * @client: a #TpBaseClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @requests_satisfied: (element-type TelepathyGLib.ChannelRequest): a #GList of
 *  #TpChannelRequest having their object-path defined but are not guaranteed
 *  to be prepared.
 * @user_action_time: the time at which user action occurred, or 0 if this
 * channel is to be handled for some reason not involving user action.
 * @context: a #TpHandleChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the HandleChannels method.
 *
 * This function must call either tp_handle_channels_context_accept(),
 * tp_handle_channels_context_delay() or tp_handle_channels_context_fail()
 * on @context before it returns.
 *
 * Since: 0.11.6
 */

#include "telepathy-glib/base-client.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/add-dispatch-operation-context-internal.h>
#include <telepathy-glib/channel-dispatch-operation-internal.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus-internal.h>
#include <telepathy-glib/handle-channels-context-internal.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/observe-channels-context-internal.h>
#include <telepathy-glib/svc-client.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/_gen/signals-marshal.h"

struct _TpBaseClientClassPrivate {
    /*<private>*/
    TpBaseClientClassObserveChannelsImpl observe_channels_impl;
    TpBaseClientClassAddDispatchOperationImpl add_dispatch_operation_impl;
    TpBaseClientClassHandleChannelsImpl handle_channels_impl;
};

static void observer_iface_init (gpointer, gpointer);
static void approver_iface_init (gpointer, gpointer);
static void handler_iface_init (gpointer, gpointer);
static void requests_iface_init (gpointer, gpointer);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(TpBaseClient, tp_base_client, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT, NULL);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_OBSERVER, observer_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_APPROVER, approver_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_HANDLER, handler_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_INTERFACE_REQUESTS,
      requests_iface_init);
    g_type_add_class_private (g_define_type_id, sizeof (
        TpBaseClientClassPrivate)))

enum {
    PROP_DBUS_DAEMON = 1,
    PROP_NAME,
    PROP_UNIQUIFY_NAME,
    N_PROPS
};

enum {
  SIGNAL_REQUEST_ADDED,
  SIGNAL_REQUEST_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static dbus_int32_t clients_slot = -1;

typedef enum {
    CLIENT_IS_OBSERVER = 1 << 0,
    CLIENT_IS_APPROVER = 1 << 1,
    CLIENT_IS_HANDLER = 1 << 2,
    CLIENT_HANDLER_WANTS_REQUESTS = 1 << 3,
    CLIENT_HANDLER_BYPASSES_APPROVAL = 1 << 4,
    CLIENT_OBSERVER_RECOVER = 1 << 5,
} ClientFlags;

struct _TpBaseClientPrivate
{
  TpDBusDaemon *dbus;
  gchar *name;
  gboolean uniquify_name;
  /* reffed */
  DBusConnection *libdbus;

  gboolean registered;
  ClientFlags flags;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *observer_filters;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *approver_filters;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *handler_filters;
  /* array of g_strdup(token), plus NULL included in length */
  GPtrArray *handler_caps;

  GList *pending_requests;
  /* Channels actually handled by THIS observer.
   * borrowed path (gchar *) => reffed TpChannel */
  GHashTable *my_chans;

  gchar *bus_name;
  gchar *object_path;

  TpAccountManager *account_mgr;
};

static GHashTable *
_tp_base_client_copy_filter (GHashTable *filter)
{
  GHashTable *copy;

  copy = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);
  tp_g_hash_table_update (copy, filter, (GBoxedCopyFunc) g_strdup,
      (GBoxedCopyFunc) tp_g_value_slice_dup);
  return copy;
}

/**
 * tp_base_client_add_observer_filter:
 * @self: a #TpBaseClient
 * @filter: (transfer none) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS
 *
 * Register a new channel class as Observer.ObserverChannelFilter.
 * The @observe_channels virtual method set up using
 * tp_base_client_implement_observe_channels() will be called whenever
 * a new channel's properties match the ones in @filter.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_observe_channels().
 *
 * Since: 0.11.5
 */
void
tp_base_client_add_observer_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);
  tp_base_client_take_observer_filter (self,
      _tp_base_client_copy_filter (filter));
}

/**
 * tp_base_client_take_observer_filter: (skip)
 * @self: a client
 * @filter: (transfer full) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS, ownership of which is taken by @self
 *
 * The same as tp_base_client_add_observer_filter(), but ownership of @filter
 * is taken by @self. This makes it convenient to call using tp_asv_new():
 *
 * |[
 * tp_base_client_take_observer_filter (client,
 *    tp_asv_new (
 *        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *            TP_IFACE_CHANNEL_TYPE_TEXT,
 *        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
 *            TP_HANDLE_TYPE_CONTACT,
 *        ...));
 * ]|
 *
 * Since: 0.11.5
 */
void
tp_base_client_take_observer_filter (TpBaseClient *self,
    GHashTable *filter)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->observe_channels_impl != NULL);

  self->priv->flags |= CLIENT_IS_OBSERVER;
  g_ptr_array_add (self->priv->observer_filters, filter);
}

/**
 * tp_base_client_set_observer_recover:
 * @self: a #TpBaseClient
 * @recover: the value of the Observer.Recover property
 *
 * Set whether the channel dispatcher should attempt to recover
 * this Observer if it crashes. (This is implemented by setting
 * the value of its Recover D-Bus property.)
 *
 * Normally, Observers are only notified when new channels
 * appear. If an Observer is set to recover, when it registers with
 * tp_base_client_register(), it will also be told about any channels
 * that already existed before it started.
 *
 * For Observers that are activatable as a D-Bus service, if the
 * Observer exits or crashes while there are any channels that match
 * its filter, it will automatically be restarted by service-activation.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_observe_channels().
 *
 * Since: 0.11.5
 */
void
tp_base_client_set_observer_recover (TpBaseClient *self,
    gboolean recover)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->observe_channels_impl != NULL);

  self->priv->flags |= (CLIENT_IS_OBSERVER | CLIENT_OBSERVER_RECOVER);
}

/**
 * tp_base_client_add_approver_filter:
 * @self: a #TpBaseClient
 * @filter: (transfer none) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS
 *
 * Register a new channel class as Approver.ApproverChannelFilter.
 * The @add_dispatch_operation virtual method set up using
 * tp_base_client_implement_add_dispatch_operation() will be called whenever
 * a new channel's properties match the ones in @filter.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_add_dispatch_operation().
 *
 * Since: 0.11.5
 */
void
tp_base_client_add_approver_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);
  tp_base_client_take_approver_filter (self,
      _tp_base_client_copy_filter (filter));
}

/**
 * tp_base_client_take_approver_filter: (skip)
 * @self: a client
 * @filter: (transfer full) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS, ownership of which is taken by @self
 *
 * The same as tp_base_client_add_approver_filter(), but ownership of @filter
 * is taken by @self. This makes it convenient to call using tp_asv_new():
 *
 * |[
 * tp_base_client_take_approver_filter (client,
 *    tp_asv_new (
 *        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *            TP_IFACE_CHANNEL_TYPE_TEXT,
 *        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
 *            TP_HANDLE_TYPE_CONTACT,
 *        ...));
 * ]|
 *
 * Since: 0.11.5
 */
void
tp_base_client_take_approver_filter (TpBaseClient *self,
    GHashTable *filter)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->add_dispatch_operation_impl != NULL);

  self->priv->flags |= CLIENT_IS_APPROVER;
  g_ptr_array_add (self->priv->approver_filters, filter);
}

/**
 * tp_base_client_be_a_handler:
 * @self: a #TpBaseClient
 *
 * Register @self as a ChannelHandler with an empty list of filter.
 * This is useful if you want to create a client that only handle channels
 * for which it's the PreferredHandler.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_be_a_handler (TpBaseClient *self)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  self->priv->flags |= CLIENT_IS_HANDLER;
}

/**
 * tp_base_client_add_handler_filter:
 * @self: a #TpBaseClient
 * @filter: (transfer none) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS
 *
 * Register a new channel class as Handler.HandlerChannelFilter.
 * The @handle_channels virtual method set up using
 * tp_base_client_implement_handle_channels() will be called whenever
 * a new channel's properties match the ones in @filter.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_add_handler_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);

  tp_base_client_take_handler_filter (self,
      _tp_base_client_copy_filter (filter));
}

/**
 * tp_base_client_take_handler_filter: (skip)
 * @self: a #TpBaseClient
 * @filter: (transfer full) (element-type utf8 GObject.Value):
 * a %TP_HASH_TYPE_CHANNEL_CLASS, ownership of which is taken by @self
 *
 * The same as tp_base_client_add_handler_filter(), but ownership of @filter
 * is taken by @self. This makes it convenient to call using tp_asv_new():
 *
 * |[
 * tp_base_client_take_handler_filter (client,
 *    tp_asv_new (
 *        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *            TP_IFACE_CHANNEL_TYPE_TEXT,
 *        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
 *            TP_HANDLE_TYPE_CONTACT,
 *        ...));
 * ]|
 *
 * Since: 0.11.6
 */
void
tp_base_client_take_handler_filter (TpBaseClient *self,
    GHashTable *filter)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  self->priv->flags |= CLIENT_IS_HANDLER;
  g_ptr_array_add (self->priv->handler_filters, filter);
}

/**
 * tp_base_client_set_handler_bypass_approval:
 * @self: a #TpBaseClient
 * @bypass_approval: the value of the Handler.BypassApproval property
 *
 * Set whether the channels destined for this handler are automatically
 * handled, without invoking approvers.
 * (This is implemented by setting the value of its BypassApproval
 * D-Bus property.)
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_set_handler_bypass_approval (TpBaseClient *self,
    gboolean bypass_approval)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  if (bypass_approval)
    {
      self->priv->flags |= (CLIENT_IS_HANDLER |
          CLIENT_HANDLER_BYPASSES_APPROVAL);
    }
  else
    {
      self->priv->flags |= CLIENT_IS_HANDLER;
      self->priv->flags &= ~CLIENT_HANDLER_BYPASSES_APPROVAL;
    }
}

/**
 * tp_base_client_set_handler_request_notification:
 * @self: a #TpBaseClient
 *
 * Indicate that @self is a Handler willing to be notified about requests for
 * channels that it is likely to be asked to handle.
 * That means the TpBaseClient::request-added and TpBaseClient::request-removed:
 * signals will be fired and tp_base_client_get_pending_requests() will
 * return the list of pending requests.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_set_handler_request_notification (TpBaseClient *self)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  self->priv->flags |= (CLIENT_IS_HANDLER | CLIENT_HANDLER_WANTS_REQUESTS);
}

static void
_tp_base_client_add_handler_capability (TpBaseClient *self,
    const gchar *token)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  self->priv->flags |= CLIENT_IS_HANDLER;

  g_assert (g_ptr_array_index (self->priv->handler_caps,
        self->priv->handler_caps->len - 1) == NULL);
  g_ptr_array_index (self->priv->handler_caps,
      self->priv->handler_caps->len - 1) = g_strdup (token);
  g_ptr_array_add (self->priv->handler_caps, NULL);
}

/**
 * tp_base_client_add_handler_capability:
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @token: a capability token as defined by the Telepathy D-Bus API
 *  Specification
 *
 * Add one capability token to this client, as if via
 * tp_base_client_add_handler_capabilities().
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_add_handler_capability (TpBaseClient *self,
    const gchar *token)
{
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  g_return_if_fail (cls->priv->handle_channels_impl != NULL);

  _tp_base_client_add_handler_capability (self, token);
}

/**
 * tp_base_client_add_handler_capabilities:
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @tokens: (array zero-terminated=1) (element-type utf8): capability
 *  tokens as defined by the Telepathy D-Bus API Specification
 *
 * Add several capability tokens to this client. These are used to signal
 * that Telepathy connection managers should advertise certain capabilities
 * to other contacts, such as the ability to receive audio/video calls using
 * particular streaming protocols and codecs.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_add_handler_capabilities (TpBaseClient *self,
    const gchar * const *tokens)
{
  const gchar * const *iter;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  if (tokens == NULL)
    return;

  for (iter = tokens; *iter != NULL; iter++)
    _tp_base_client_add_handler_capability (self, *iter);
}

/**
 * tp_base_client_add_handler_capabilities_varargs: (skip)
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @first_token: a capability token from the Telepathy D-Bus API Specification
 * @...: more tokens, ending with %NULL
 *
 * Convenience C API equivalent to calling
 * tp_base_client_add_handler_capability() for each capability token.
 *
 * This method may only be called before tp_base_client_register() is
 * called, and may only be called on objects whose class has called
 * tp_base_client_implement_handle_channels().
 *
 * Since: 0.11.6
 */
void
tp_base_client_add_handler_capabilities_varargs (TpBaseClient *self,
    const gchar *first_token, ...)
{
  va_list ap;
  const gchar *token = first_token;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  va_start (ap, first_token);

  for (token = first_token; token != NULL; token = va_arg (ap, gchar *))
    _tp_base_client_add_handler_capability (self, token);

  va_end (ap);
}

/**
 * tp_base_client_register:
 * @self: a #TpBaseClient, which must not have been registered with
 *  tp_base_client_register() yet
 * @error: used to indicate the error if %FALSE is returned
 *
 * Publish @self as an available client. After this method is called, as long
 * as it continues to exist, it will receive and process whatever events were
 * requested via the various filters.
 *
 * Methods that set the filters and other immutable state, such as
 * tp_base_client_add_observer_filter(), cannot be called after this one.
 *
 * Returns: %TRUE if the client was registered successfully
 *
 * Since: 0.11.5
 */
gboolean
tp_base_client_register (TpBaseClient *self,
    GError **error)
{
  GHashTable *clients;

  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), FALSE);
  g_return_val_if_fail (!self->priv->registered, FALSE);
  /* Client should at least be an Observer, Approver or Handler */
  g_return_val_if_fail (self->priv->flags != 0, FALSE);

  DEBUG ("request name %s", self->priv->bus_name);

  if (!tp_dbus_daemon_request_name (self->priv->dbus, self->priv->bus_name,
        TRUE, error))
    {
      DEBUG ("Failed to register bus name %s", self->priv->bus_name);
      return FALSE;
    }

  tp_dbus_daemon_register_object (self->priv->dbus, self->priv->object_path,
      G_OBJECT (self));

  self->priv->registered = TRUE;

  if (!(self->priv->flags & CLIENT_IS_HANDLER))
    return TRUE;

  /* Client is an handler */
  self->priv->libdbus = dbus_connection_ref (
      dbus_g_connection_get_connection (
        tp_proxy_get_dbus_connection (self->priv->dbus)));

  /* one ref per TpBaseClient with CLIENT_IS_HANDLER, released
   * in tp_base_client_unregister() */
  if (!dbus_connection_allocate_data_slot (&clients_slot))
    ERROR ("Out of memory");

  clients = dbus_connection_get_data (self->priv->libdbus, clients_slot);

  if (clients == NULL)
    {
      /* Map DBusConnection to the self->priv->my_chans hash table owned by
       * the client using this DBusConnection.

       * borrowed client path => borrowed (GHashTable *) */
      clients = g_hash_table_new (g_str_hash, g_str_equal);

      dbus_connection_set_data (self->priv->libdbus, clients_slot, clients,
          (DBusFreeFunction) g_hash_table_unref);
    }

  g_hash_table_insert (clients, self->priv->object_path, self->priv->my_chans);

  return TRUE;
}

/**
 * tp_base_client_get_pending_requests:
 * @self: a #TpBaseClient
 *
 * Only works if tp_base_client_set_handler_request_notification() has been
 * called.
 * Returns the list of requests @self is likely be asked to handle.
 *
 * Returns: (transfer container) (element-type TelepathyGLib.ChannelRequest): a
 * #GList of #TpChannelRequest
 *
 * Since: 0.11.6
 */
GList *
tp_base_client_get_pending_requests (TpBaseClient *self)
{
  g_return_val_if_fail (self->priv->flags & CLIENT_IS_HANDLER, NULL);

  return g_list_copy (self->priv->pending_requests);
}

/**
 * tp_base_client_get_handled_channels:
 * @self: a #TpBaseClient
 *
 * Returns the set of channels currently handled by this base client or by any
 * other #TpBaseClient with which it shares a unique name.
 *
 * Returns: (transfer container) (element-type TelepathyGLib.Channel): the
 * handled channels
 *
 * Since: 0.11.6
 */
GList *
tp_base_client_get_handled_channels (TpBaseClient *self)
{
  GList *result = NULL;
  GHashTable *clients;
  GHashTableIter iter;
  gpointer value;
  GHashTable *set;

  g_return_val_if_fail (self->priv->flags & CLIENT_IS_HANDLER, NULL);

  if (clients_slot == -1)
    return NULL;

  set = g_hash_table_new (g_str_hash, g_str_equal);

  clients = dbus_connection_get_data (self->priv->libdbus, clients_slot);

  g_hash_table_iter_init (&iter, clients);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GHashTable *chans = value;

      tp_g_hash_table_update (set, chans, NULL, NULL);
    }

  result = g_hash_table_get_values (set);
  g_hash_table_unref (set);

  return result;
}

static void
tp_base_client_init (TpBaseClient *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_CLIENT,
      TpBaseClientPrivate);

  /* wild guess: most clients won't need more than one of each filter */
  self->priv->observer_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->approver_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->handler_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->handler_caps = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (self->priv->handler_caps, NULL);

  self->priv->my_chans = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, NULL);
}

static void
tp_base_client_dispose (GObject *object)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_base_client_parent_class)->dispose;

  tp_base_client_unregister (self);

  if (self->priv->dbus != NULL)
    {
      g_object_unref (self->priv->dbus);
      self->priv->dbus = NULL;
    }

  if (self->priv->account_mgr != NULL)
    {
      g_object_unref (self->priv->account_mgr);
      self->priv->account_mgr = NULL;
    }

  g_list_foreach (self->priv->pending_requests, (GFunc) g_object_unref, NULL);
  g_list_free (self->priv->pending_requests);
  self->priv->pending_requests = NULL;

  if (self->priv->my_chans != NULL)
    {
      g_hash_table_unref (self->priv->my_chans);
      self->priv->my_chans = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_base_client_finalize (GObject *object)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  void (*finalize) (GObject *) =
    G_OBJECT_CLASS (tp_base_client_parent_class)->finalize;

  g_free (self->priv->name);

  g_ptr_array_free (self->priv->observer_filters, TRUE);
  g_ptr_array_free (self->priv->approver_filters, TRUE);
  g_ptr_array_free (self->priv->handler_filters, TRUE);
  g_ptr_array_free (self->priv->handler_caps, TRUE);

  g_free (self->priv->bus_name);
  g_free (self->priv->object_path);

  if (finalize != NULL)
    finalize (object);
}

static void
tp_base_client_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);

  switch (property_id)
    {
      case PROP_DBUS_DAEMON:
        g_value_set_object (value, self->priv->dbus);
        break;

      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;

      case PROP_UNIQUIFY_NAME:
        g_value_set_boolean (value, self->priv->uniquify_name);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_base_client_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);

  switch (property_id)
    {
      case PROP_DBUS_DAEMON:
        g_assert (self->priv->dbus == NULL);    /* construct-only */
        self->priv->dbus = g_value_dup_object (value);
        break;

      case PROP_NAME:
        g_assert (self->priv->name == NULL);    /* construct-only */
        self->priv->name = g_value_dup_string (value);
        break;

      case PROP_UNIQUIFY_NAME:
        self->priv->uniquify_name = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_base_client_constructed (GObject *object)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_base_client_parent_class)->constructed;
  GString *string;
  static guint unique_counter = 0;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->dbus != NULL);
  g_assert (self->priv->name != NULL);

  /* Bus name */
  string = g_string_new (TP_CLIENT_BUS_NAME_BASE);
  g_string_append (string, self->priv->name);

  if (self->priv->uniquify_name)
    {
      gchar *unique;

      unique = tp_escape_as_identifier (tp_dbus_daemon_get_unique_name (
            self->priv->dbus));

      g_string_append_printf (string, ".%s.n%u", unique, unique_counter++);
      g_free (unique);
    }

  /* Object path */
  self->priv->object_path = g_strdup_printf ("/%s", string->str);
  g_strdelimit (self->priv->object_path, ".", '/');

  self->priv->bus_name = g_string_free (string, FALSE);

  if (_tp_dbus_daemon_is_the_shared_one (self->priv->dbus))
    {
      /* The AM is guaranteed to be the one from tp_account_manager_dup() */
      self->priv->account_mgr = tp_account_manager_dup ();
    }
  else
    {
      /* No guarantee, create a new AM */
      self->priv->account_mgr = tp_account_manager_new (self->priv->dbus);
    }
}

typedef enum {
    DP_INTERFACES,
    DP_APPROVER_CHANNEL_FILTER,
    DP_HANDLER_CHANNEL_FILTER,
    DP_BYPASS_APPROVAL,
    DP_CAPABILITIES,
    DP_HANDLED_CHANNELS,
    DP_OBSERVER_CHANNEL_FILTER,
    DP_OBSERVER_RECOVER,
} ClientDBusProp;

static void
tp_base_client_get_dbus_properties (GObject *object,
    GQuark iface,
    GQuark name,
    GValue *value,
    gpointer getter_data)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  ClientDBusProp which = GPOINTER_TO_INT (getter_data);

  switch (which)
    {
    case DP_INTERFACES:
        {
          GPtrArray *arr = g_ptr_array_sized_new (5);

          if (self->priv->flags & CLIENT_IS_OBSERVER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_OBSERVER));

          if (self->priv->flags & CLIENT_IS_APPROVER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_APPROVER));

          if (self->priv->flags & CLIENT_IS_HANDLER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_HANDLER));

          if (self->priv->flags & CLIENT_HANDLER_WANTS_REQUESTS)
            g_ptr_array_add (arr, g_strdup (
                  TP_IFACE_CLIENT_INTERFACE_REQUESTS));

          g_ptr_array_add (arr, NULL);
          g_value_take_boxed (value, g_ptr_array_free (arr, FALSE));
        }
      break;

    case DP_OBSERVER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->observer_filters);
      break;

    case DP_APPROVER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->approver_filters);
      break;

    case DP_HANDLER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->handler_filters);
      break;

    case DP_BYPASS_APPROVAL:
      g_value_set_boolean (value,
          (self->priv->flags & CLIENT_HANDLER_BYPASSES_APPROVAL) != 0);
      break;

    case DP_CAPABILITIES:
      /* this is already NULL-terminated */
      g_value_set_boxed (value, (GStrv) self->priv->handler_caps->pdata);
      break;

    case DP_HANDLED_CHANNELS:
        {
          GList *channels = tp_base_client_get_handled_channels (self);
          GList *iter;
          GPtrArray *arr = g_ptr_array_sized_new (g_list_length (channels));

          for (iter = channels; iter != NULL; iter = iter->next)
            g_ptr_array_add (arr,
                g_strdup (tp_proxy_get_object_path (iter->data)));

          g_value_take_boxed (value, arr);
          g_list_free (channels);
        }
      break;

    case DP_OBSERVER_RECOVER:
      g_value_set_boolean (value,
          (self->priv->flags & CLIENT_OBSERVER_RECOVER) != 0);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
tp_base_client_class_init (TpBaseClientClass *cls)
{
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl client_properties[] = {
        { "Interfaces", GINT_TO_POINTER (DP_INTERFACES) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl handler_properties[] = {
        { "HandlerChannelFilter",
          GINT_TO_POINTER (DP_HANDLER_CHANNEL_FILTER) },
        { "BypassApproval",
          GINT_TO_POINTER (DP_BYPASS_APPROVAL) },
        { "Capabilities",
          GINT_TO_POINTER (DP_CAPABILITIES) },
        { "HandledChannels",
          GINT_TO_POINTER (DP_HANDLED_CHANNELS) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl approver_properties[] = {
        { "ApproverChannelFilter",
          GINT_TO_POINTER (DP_APPROVER_CHANNEL_FILTER) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl observer_properties[] = {
        { "ObserverChannelFilter",
          GINT_TO_POINTER (DP_OBSERVER_CHANNEL_FILTER) },
        { "Recover",
          GINT_TO_POINTER (DP_OBSERVER_RECOVER) },
        { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_ifaces[] = {
        { TP_IFACE_CLIENT, tp_base_client_get_dbus_properties, NULL,
          client_properties },
        { TP_IFACE_CLIENT_OBSERVER, tp_base_client_get_dbus_properties, NULL,
          observer_properties },
        { TP_IFACE_CLIENT_APPROVER, tp_base_client_get_dbus_properties, NULL,
          approver_properties },
        { TP_IFACE_CLIENT_HANDLER, tp_base_client_get_dbus_properties, NULL,
          handler_properties },
        { NULL }
  };
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpBaseClientPrivate));

  object_class->get_property = tp_base_client_get_property;
  object_class->set_property = tp_base_client_set_property;
  object_class->constructed = tp_base_client_constructed;
  object_class->dispose = tp_base_client_dispose;
  object_class->finalize = tp_base_client_finalize;

  /**
   * TpBaseClient:dbus-daemon:
   *
   * #TpDBusDaemon object encapsulating this object's connection to D-Bus.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("dbus-daemon", "TpDBusDaemon object",
      "The dbus daemon associated with this client",
      TP_TYPE_DBUS_DAEMON,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_DAEMON, param_spec);

  /**
   * TpBaseClient:name:
   *
   * The name of the client. This is used to register the D-Bus service name
   * and object path of the service.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
 param_spec = g_param_spec_string ("name", "name",
      "The name of the client",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, param_spec);

  /**
   * TpBaseClient:uniquify-name:
   *
   * If %TRUE, tp_base_client_register() will append an unique token to the
   * service bus name and object path to ensure they are unique.
   *
   * Since: 0.11.5
   */
 param_spec = g_param_spec_boolean ("uniquify-name", "Uniquify name",
      "if TRUE, append a unique token to the name",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_UNIQUIFY_NAME,
      param_spec);

 /**
   * TpBaseClient::request-added:
   * @self: a #TpBaseClient
   * @account: the #TpAccount on which the request was made
   * having %TP_ACCOUNT_FEATURE_CORE prepared if possible
   * @request: a #TpChannelRequest having its object-path defined but
   * is not guaranteed to be prepared.
   *
   * Emitted when a channels have been requested, and that if the
   * request is successful, they will probably be handled by this Handler.
   *
   * This signal is only fired if
   * tp_base_client_set_handler_request_notification() has been called
   * on @self previously.
   *
   * Since: 0.11.6
   */
  signals[SIGNAL_REQUEST_ADDED] = g_signal_new (
      "request-added", G_OBJECT_CLASS_TYPE (cls),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__OBJECT_OBJECT,
      G_TYPE_NONE, 2,
      TP_TYPE_ACCOUNT, TP_TYPE_CHANNEL_REQUEST);

 /**
   * TpBaseClient::request-removed:
   * @self: a #TpBaseClient
   * @request: the #TpChannelRequest being removed
   * @error: the name of the D-Bus error with which the request failed.
   * @message: any message supplied with the D-Bus error.
   *
   * Emitted when a request has failed and should be disregarded.
   *
   * This signal is only fired if
   * tp_base_client_set_handler_request_notification() has been called
   * on @self previously.
   *
   * Since: 0.11.6
   */
  signals[SIGNAL_REQUEST_REMOVED] = g_signal_new (
      "request-removed", G_OBJECT_CLASS_TYPE (cls),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__OBJECT_STRING_STRING,
      G_TYPE_NONE, 3,
      TP_TYPE_CHANNEL_REQUEST, G_TYPE_STRING, G_TYPE_STRING);

  cls->dbus_properties_class.interfaces = prop_ifaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseClientClass, dbus_properties_class));

  cls->priv = G_TYPE_CLASS_GET_PRIVATE (cls, TP_TYPE_BASE_CLIENT,
      TpBaseClientClassPrivate);
}

static GList *
ptr_array_to_list (GPtrArray *arr)
{
  guint i;
  GList *result = NULL;

  for (i = 0; i < arr->len; i++)
    result = g_list_prepend (result, g_ptr_array_index (arr, i));

  return g_list_reverse (result);
}

static void
context_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseClient *self = user_data;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  TpObserveChannelsContext *ctx = TP_OBSERVE_CHANNELS_CONTEXT (source);
  GError *error = NULL;
  GList *channels_list, *requests_list;

  if (!_tp_observe_channels_context_prepare_finish (ctx, result, &error))
    {
      DEBUG ("Failed to prepare TpObserveChannelsContext: %s", error->message);
      tp_observe_channels_context_fail (ctx, error);
      g_error_free (error);
      return;
    }

  channels_list = ptr_array_to_list (ctx->channels);
  requests_list = ptr_array_to_list (ctx->requests);

  cls->priv->observe_channels_impl (self, ctx->account, ctx->connection,
      channels_list, ctx->dispatch_operation, requests_list, ctx);

  g_list_free (channels_list);
  g_list_free (requests_list);

  if (_tp_observe_channels_context_get_state (ctx) ==
      TP_OBSERVE_CHANNELS_CONTEXT_STATE_NONE)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Implementation of ObserveChannels in %s didn't call "
          "tp_observe_channels_context_{accept,fail,delay}",
          G_OBJECT_TYPE_NAME (self));

      CRITICAL ("%s", error->message);

      tp_observe_channels_context_fail (ctx, error);
      g_error_free (error);
    }
}

static void
_tp_base_client_observe_channels (TpSvcClientObserver *iface,
    const gchar *account_path,
    const gchar *connection_path,
    const GPtrArray *channels_arr,
    const gchar *dispatch_operation_path,
    const GPtrArray *requests_arr,
    GHashTable *observer_info,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpObserveChannelsContext *ctx;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  GError *error = NULL;
  TpAccount *account = NULL;
  TpConnection *connection = NULL;
  GPtrArray *channels = NULL, *requests = NULL;
  TpChannelDispatchOperation *dispatch_operation = NULL;
  guint i;

  if (!(self->priv->flags & CLIENT_IS_OBSERVER))
    {
      /* Pretend that the method is not implemented if we are not supposed to
       * be an Observer. */
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (cls->priv->observe_channels_impl == NULL)
    {
      WARNING ("class %s does not implement ObserveChannels",
          G_OBJECT_TYPE_NAME (self));

      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (channels_arr->len == 0)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Channels should contain at least one channel");
      DEBUG ("%s", error->message);
      goto out;
    }

  account = tp_account_manager_ensure_account (self->priv->account_mgr,
      account_path);

  connection = tp_account_ensure_connection (account, connection_path);
  if (connection == NULL)
    {
      DEBUG ("Failed to create TpConnection");
      goto out;
    }

  channels = g_ptr_array_sized_new (channels_arr->len);
  g_ptr_array_set_free_func (channels, g_object_unref);
  for (i = 0; i < channels_arr->len; i++)
    {
      const gchar *chan_path;
      GHashTable *chan_props;
      TpChannel *channel;

      tp_value_array_unpack (g_ptr_array_index (channels_arr, i), 2,
          &chan_path, &chan_props);

      channel = tp_channel_new_from_properties (connection,
          chan_path, chan_props, &error);
      if (channel == NULL)
        {
          DEBUG ("Failed to create TpChannel: %s", error->message);
          goto out;
        }

      g_ptr_array_add (channels, channel);
    }

  if (!tp_strdiff (dispatch_operation_path, "/"))
    {
      dispatch_operation = NULL;
    }
  else
    {
      dispatch_operation = tp_channel_dispatch_operation_new (self->priv->dbus,
            dispatch_operation_path, NULL, &error);
     if (dispatch_operation == NULL)
       {
         DEBUG ("Failed to create TpChannelDispatchOperation: %s",
             error->message);
         goto out;
        }
    }

  requests = g_ptr_array_sized_new (requests_arr->len);
  g_ptr_array_set_free_func (requests, g_object_unref);
  for (i = 0; i < requests_arr->len; i++)
    {
      const gchar *req_path = g_ptr_array_index (requests_arr, i);
      TpChannelRequest *request;

      request = tp_channel_request_new (self->priv->dbus, req_path, NULL,
          &error);
      if (request == NULL)
        {
          DEBUG ("Failed to create TpChannelRequest: %s", error->message);
          goto out;
        }

      g_ptr_array_add (requests, request);
    }

  ctx = _tp_observe_channels_context_new (account, connection, channels,
      dispatch_operation, requests, observer_info, context);

  _tp_observe_channels_context_prepare_async (ctx, context_prepare_cb, self);

  g_object_unref (ctx);

out:
  if (channels != NULL)
    g_ptr_array_unref (channels);

  if (dispatch_operation != NULL)
    g_object_unref (dispatch_operation);

  if (requests != NULL)
    g_ptr_array_unref (requests);

  if (error == NULL)
    return;

  dbus_g_method_return_error (context, error);
  g_error_free (error);
}

static void
observer_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_observer_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (observe_channels);
#undef IMPLEMENT
}

static void
add_dispatch_context_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseClient *self = user_data;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  TpAddDispatchOperationContext *ctx = TP_ADD_DISPATCH_OPERATION_CONTEXT (
      source);
  GError *error = NULL;
  GList *channels_list;

  if (!_tp_add_dispatch_operation_context_prepare_finish (ctx, result, &error))
    {
      DEBUG ("Failed to prepare TpAddDispatchOperationContext: %s",
          error->message);

      tp_add_dispatch_operation_context_fail (ctx, error);

      g_error_free (error);
      return;
    }

  channels_list = ptr_array_to_list (ctx->channels);

  cls->priv->add_dispatch_operation_impl (self, ctx->account, ctx->connection,
      channels_list, ctx->dispatch_operation, ctx);

  g_list_free (channels_list);

  if (_tp_add_dispatch_operation_context_get_state (ctx) ==
      TP_ADD_DISPATCH_OPERATION_CONTEXT_STATE_NONE)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Implementation of AddDispatchOperation in %s didn't call "
          "tp_add_dispatch_operation_context_{accept,fail,delay}",
          G_OBJECT_TYPE_NAME (self));

      g_critical ("%s", error->message);

      tp_add_dispatch_operation_context_fail (ctx, error);
      g_error_free (error);
    }
}

static void
_tp_base_client_add_dispatch_operation (TpSvcClientApprover *iface,
    const GPtrArray *channels_arr,
    const gchar *dispatch_operation_path,
    GHashTable *properties,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpAddDispatchOperationContext *ctx;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  GError *error = NULL;
  TpAccount *account = NULL;
  TpConnection *connection = NULL;
  GPtrArray *channels = NULL;
  TpChannelDispatchOperation *dispatch_operation = NULL;
  guint i;
  const gchar *path;

  if (!(self->priv->flags & CLIENT_IS_APPROVER))
    {
      /* Pretend that the method is not implemented if we are not supposed to
       * be an Approver. */
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (cls->priv->add_dispatch_operation_impl == NULL)
    {
      WARNING ("class %s does not implement AddDispatchOperation",
          G_OBJECT_TYPE_NAME (self));

      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  path = tp_asv_get_object_path (properties,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT);
  if (path == NULL)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Properties doesn't contain 'Account'");
      DEBUG ("%s", error->message);
      goto out;
    }

  account = tp_account_manager_ensure_account (self->priv->account_mgr,
      path);

  path = tp_asv_get_object_path (properties,
      TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION);
  if (path == NULL)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Properties doesn't contain 'Connection'");
      DEBUG ("%s", error->message);
      goto out;
    }

  connection = tp_account_ensure_connection (account, path);
  if (connection == NULL)
    {
      DEBUG ("Failed to create TpConnection");
      goto out;
    }

  if (channels_arr->len == 0)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Channels should contain at least one channel");
      DEBUG ("%s", error->message);
      goto out;
    }

  channels = g_ptr_array_sized_new (channels_arr->len);
  g_ptr_array_set_free_func (channels, g_object_unref);
  for (i = 0; i < channels_arr->len; i++)
    {
      const gchar *chan_path;
      GHashTable *chan_props;
      TpChannel *channel;

      tp_value_array_unpack (g_ptr_array_index (channels_arr, i), 2,
          &chan_path, &chan_props);

      channel = tp_channel_new_from_properties (connection,
          chan_path, chan_props, &error);
      if (channel == NULL)
        {
          DEBUG ("Failed to create TpChannel: %s", error->message);
          goto out;
        }

      g_ptr_array_add (channels, channel);
    }

  dispatch_operation = _tp_channel_dispatch_operation_new_with_objects (
      self->priv->dbus, dispatch_operation_path, properties,
      account, connection, channels, &error);
  if (dispatch_operation == NULL)
    {
      DEBUG ("Failed to create TpChannelDispatchOperation: %s", error->message);
      goto out;
    }

  ctx = _tp_add_dispatch_operation_context_new (account, connection, channels,
      dispatch_operation, context);

  _tp_add_dispatch_operation_context_prepare_async (ctx,
      add_dispatch_context_prepare_cb, self);

  g_object_unref (ctx);

out:
  if (channels != NULL)
    g_ptr_array_unref (channels);

  if (dispatch_operation != NULL)
    g_object_unref (dispatch_operation);

  if (error == NULL)
    return;

  dbus_g_method_return_error (context, error);
  g_error_free (error);

}

static void
approver_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_approver_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (add_dispatch_operation);
#undef IMPLEMENT
}

static void
chan_invalidated_cb (TpChannel *channel,
    guint domain,
    gint code,
    gchar *message,
    TpBaseClient *self)
{
  DEBUG ("Channel %s has been invalidated", tp_proxy_get_object_path (channel));

  g_hash_table_remove (self->priv->my_chans, tp_proxy_get_object_path (
        channel));
}

static void
ctx_done_cb (TpHandleChannelsContext *context,
    TpBaseClient *self)
{
  guint i;

  for (i = 0; i < context->channels->len; i++)
    {
      TpChannel *channel = g_ptr_array_index (context->channels, i);

      if (tp_proxy_get_invalidated (channel) == NULL)
        {
          g_hash_table_insert (self->priv->my_chans,
              (gchar *) tp_proxy_get_object_path (channel),
              g_object_ref (channel));

          tp_g_signal_connect_object (channel, "invalidated",
              G_CALLBACK (chan_invalidated_cb), self, 0);
        }
    }
}

static void
handle_channels_context_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseClient *self = user_data;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  TpHandleChannelsContext *ctx = TP_HANDLE_CHANNELS_CONTEXT (source);
  GError *error = NULL;
  GList *channels_list, *requests_list;

  if (!_tp_handle_channels_context_prepare_finish (ctx, result, &error))
    {
      DEBUG ("Failed to prepare TpHandleChannelsContext: %s", error->message);
      tp_handle_channels_context_fail (ctx, error);
      g_error_free (error);
      return;
    }

  channels_list = ptr_array_to_list (ctx->channels);
  requests_list = ptr_array_to_list (ctx->requests_satisfied);

  tp_g_signal_connect_object (ctx, "done", G_CALLBACK (ctx_done_cb),
      self, 0);

  cls->priv->handle_channels_impl (self, ctx->account, ctx->connection,
      channels_list, requests_list, ctx->user_action_time, ctx);

  g_list_free (channels_list);
  g_list_free (requests_list);

  if (_tp_handle_channels_context_get_state (ctx) ==
      TP_OBSERVE_CHANNELS_CONTEXT_STATE_NONE)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Implementation of HandledChannels in %s didn't call "
          "tp_handle_channels_context_{accept,fail,delay}",
          G_OBJECT_TYPE_NAME (self));

      CRITICAL ("%s", error->message);

      tp_handle_channels_context_fail (ctx, error);
      g_error_free (error);
    }
}

static TpChannelRequest *
find_request_by_path (TpBaseClient *self,
    const gchar *path)
{
  GList *l;

  for (l = self->priv->pending_requests; l != NULL; l = g_list_next (l))
    {
      TpChannelRequest *request = l->data;

      if (!tp_strdiff (tp_proxy_get_object_path (request), path))
        return request;
    }

  return NULL;
}

static void
_tp_base_client_handle_channels (TpSvcClientHandler *iface,
    const gchar *account_path,
    const gchar *connection_path,
    const GPtrArray *channels_arr,
    const GPtrArray *requests_arr,
    guint64 user_action_time,
    GHashTable *handler_info,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpHandleChannelsContext *ctx;
  TpBaseClientClass *cls = TP_BASE_CLIENT_GET_CLASS (self);
  GError *error = NULL;
  TpAccount *account = NULL;
  TpConnection *connection = NULL;
  GPtrArray *channels = NULL, *requests = NULL;
  guint i;

  if (!(self->priv->flags & CLIENT_IS_HANDLER))
    {
      /* Pretend that the method is not implemented if we are not supposed to
       * be an Handler. */
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (cls->priv->handle_channels_impl == NULL)
    {
      DEBUG ("class %s does not implement HandleChannels",
          G_OBJECT_TYPE_NAME (self));

      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (channels_arr->len == 0)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Channels should contain at least one channel");
      DEBUG ("%s", error->message);
      goto out;
    }

  account = tp_account_manager_ensure_account (self->priv->account_mgr,
      account_path);

  connection = tp_account_ensure_connection (account, connection_path);
  if (connection == NULL)
    {
      DEBUG ("Failed to create TpConnection");
      goto out;
    }

  channels = g_ptr_array_sized_new (channels_arr->len);
  g_ptr_array_set_free_func (channels, g_object_unref);
  for (i = 0; i < channels_arr->len; i++)
    {
      const gchar *chan_path;
      GHashTable *chan_props;
      TpChannel *channel;

      tp_value_array_unpack (g_ptr_array_index (channels_arr, i), 2,
          &chan_path, &chan_props);

      channel = tp_channel_new_from_properties (connection,
          chan_path, chan_props, &error);
      if (channel == NULL)
        {
          DEBUG ("Failed to create TpChannel: %s", error->message);
          goto out;
        }

      g_ptr_array_add (channels, channel);
    }

  requests = g_ptr_array_sized_new (requests_arr->len);
  g_ptr_array_set_free_func (requests, g_object_unref);
  for (i = 0; i < requests_arr->len; i++)
    {
      const gchar *req_path = g_ptr_array_index (requests_arr, i);
      TpChannelRequest *request;

      request = find_request_by_path (self, req_path);
      if (request != NULL)
        {
          g_object_ref (request);
        }
      else
        {
          request = tp_channel_request_new (self->priv->dbus, req_path, NULL,
              &error);
          if (request == NULL)
            {
              DEBUG ("Failed to create TpChannelRequest: %s", error->message);
              goto out;
            }
        }

      g_ptr_array_add (requests, request);
    }

  ctx = _tp_handle_channels_context_new (account, connection, channels,
      requests, user_action_time, handler_info, context);

  _tp_handle_channels_context_prepare_async (ctx,
      handle_channels_context_prepare_cb, self);

  g_object_unref (ctx);

out:
  if (channels != NULL)
    g_ptr_array_unref (channels);

  if (requests != NULL)
    g_ptr_array_unref (requests);

  if (error == NULL)
    return;

  dbus_g_method_return_error (context, error);
  g_error_free (error);
}

static void
handler_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_handler_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (handle_channels);
#undef IMPLEMENT
}

typedef struct
{
  TpBaseClient *self;
  TpChannelRequest *request;
} channel_request_prepare_account_ctx;

static channel_request_prepare_account_ctx *
channel_request_prepare_account_ctx_new (TpBaseClient *self,
    TpChannelRequest *request)
{
  channel_request_prepare_account_ctx *ctx = g_slice_new (
      channel_request_prepare_account_ctx);

  ctx->self = g_object_ref (self);
  ctx->request = g_object_ref (request);
  return ctx;
}

static void
channel_request_prepare_account_ctx_free (
    channel_request_prepare_account_ctx *ctx)
{
  g_object_unref (ctx->self);
  g_object_unref (ctx->request);

  g_slice_free (channel_request_prepare_account_ctx, ctx);
}

static void
channel_request_account_prepare_cb (GObject *account,
    GAsyncResult *result,
    gpointer user_data)
{
  channel_request_prepare_account_ctx *ctx = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (account, result, &error))
    {
      DEBUG ("Failed to prepare account: %s", error->message);
      g_error_free (error);
    }

  g_signal_emit (ctx->self, signals[SIGNAL_REQUEST_ADDED], 0, account,
      ctx->request);

  channel_request_prepare_account_ctx_free (ctx);
}

static void
_tp_base_client_add_request (TpSvcClientInterfaceRequests *iface,
    const gchar *path,
    GHashTable *properties,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpChannelRequest *request;
  TpAccount *account;
  GError *error = NULL;
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  channel_request_prepare_account_ctx *ctx;

  request = tp_channel_request_new (self->priv->dbus, path, properties, &error);
  if (request == NULL)
    {
      DEBUG ("Failed to create TpChannelRequest: %s", error->message);

      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  path = tp_asv_get_object_path (properties, TP_PROP_CHANNEL_REQUEST_ACCOUNT);
  if (path == NULL)
    {
      error = g_error_new_literal (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Mandatory 'Account' property is missing");

      DEBUG ("%s", error->message);

      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  account = tp_account_manager_ensure_account (self->priv->account_mgr,
      path);

  self->priv->pending_requests = g_list_append (self->priv->pending_requests,
      request);

  ctx = channel_request_prepare_account_ctx_new (self, request);

  tp_proxy_prepare_async (account, account_features,
      channel_request_account_prepare_cb, ctx);

  tp_svc_client_interface_requests_return_from_add_request (context);
}

static void
_tp_base_client_remove_request (TpSvcClientInterfaceRequests *iface,
    const gchar *path,
    const gchar *error,
    const gchar *reason,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpChannelRequest *request;

  request = find_request_by_path (self, path);
  if (request == NULL)
    {
      GError err = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Uknown ChannelRequest" };

      dbus_g_method_return_error (context, &err);
      return;
    }

  self->priv->pending_requests = g_list_remove (self->priv->pending_requests,
      request);

  g_signal_emit (self, signals[SIGNAL_REQUEST_REMOVED], 0, request,
      error, reason);

  tp_svc_client_interface_requests_return_from_remove_request (context);
}

static void
requests_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_interface_requests_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (add_request);
  IMPLEMENT (remove_request);
#undef IMPLEMENT
}

/**
 * tp_base_client_implement_observe_channels: (skip)
 * @klass: the #TpBaseClientClass of the object
 * @impl: the #TpBaseClientClassObserveChannelsImpl function implementing
 * ObserveChannels()
 *
 * Called by subclasses to define the actual implementation of the
 * ObserveChannels() D-Bus method.
 *
 * Since: 0.11.5
 */
void
tp_base_client_implement_observe_channels (TpBaseClientClass *cls,
    TpBaseClientClassObserveChannelsImpl impl)
{
  cls->priv->observe_channels_impl = impl;
}

/**
 * tp_base_client_get_bus_name:
 * @self: a #TpBaseClient
 *
 * Return the bus name of @self. Note that doesn't mean the client is
 * actually owning this name; for example if tp_base_client_register()
 * has not been called yet or failed.
 *
 * Returns: the bus name of the client
 *
 * Since: 0.11.5
 */
const gchar *
tp_base_client_get_bus_name (TpBaseClient *self)
{
  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), NULL);
  return self->priv->bus_name;
}

/**
 * tp_base_client_get_object_path:
 * @self: a #TpBaseClient
 *
 * Return the object path of @self. Note that doesn't mean the client is
 * actually registered on this path; for example if tp_base_client_register()
 * has not been called yet or failed.
 *
 * Returns: the object path of the client
 *
 * Since: 0.11.5
 */
const gchar *
tp_base_client_get_object_path (TpBaseClient *self)
{
  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), NULL);
  return self->priv->object_path;
}

/**
 * tp_base_client_get_name: (skip)
 * @self: a #TpBaseClient
 *
 * Return the #TpBaseClient:name construct-only property, which is used as
 * part of the bus name and object path.
 *
 * Returns: the value of #TpBaseClient:name
 * Since: 0.11.11
 */
const gchar *
tp_base_client_get_name (TpBaseClient *self)
{
  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), NULL);
  return self->priv->name;
}

/**
 * tp_base_client_get_uniquify_name: (skip)
 * @self: a #TpBaseClient
 *
 * Return the #TpBaseClient:uniquify-name construct-only property; if this
 * is true, the bus name and object path will be made unique by appending
 * a suffix that includes the D-Bus unique name and a per-process counter.
 *
 * Returns: the value of #TpBaseClient:uniquify-name
 * Since: 0.11.11
 */
gboolean
tp_base_client_get_uniquify_name (TpBaseClient *self)
{
  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), FALSE);
  return self->priv->uniquify_name;
}

/**
 * tp_base_client_get_dbus_daemon: (skip)
 * @self: a #TpBaseClient
 *
 * Return the #TpBaseClient:dbus-daemon construct-only property, which
 * represents the D-Bus connection used to export this client object.
 *
 * The returned object's reference count is not incremented, so it is not
 * necessarily valid after @self is destroyed.
 *
 * Returns: (transfer none): the value of #TpBaseClient:dbus-daemon
 * Since: 0.11.11
 */
TpDBusDaemon *
tp_base_client_get_dbus_daemon (TpBaseClient *self)
{
  g_return_val_if_fail (TP_IS_BASE_CLIENT (self), NULL);
  return self->priv->dbus;
}

/**
 * tp_base_client_implement_add_dispatch_operation: (skip)
 * @klass: the #TpBaseClientClass of the object
 * @impl: the #TpBaseClientClassAddDispatchOperationImpl function implementing
 * AddDispatchOperation()
 *
 * Called by subclasses to define the actual implementation of the
 * AddDispatchOperation() D-Bus method.
 *
 * Since: 0.11.5
 */
void
tp_base_client_implement_add_dispatch_operation (TpBaseClientClass *cls,
    TpBaseClientClassAddDispatchOperationImpl impl)
{
  cls->priv->add_dispatch_operation_impl = impl;
}

/**
 * tp_base_client_implement_handle_channels: (skip)
 * @klass: the #TpBaseClientClass of the object
 * @impl: the #TpBaseClientClassHandleChannelsImpl function implementing
 * HandleCHannels()
 *
 * Called by subclasses to define the actual implementation of the
 * HandleChannels() D-Bus method.
 *
 * Since: 0.11.6
 */
void
tp_base_client_implement_handle_channels (TpBaseClientClass *cls,
    TpBaseClientClassHandleChannelsImpl impl)
{
  cls->priv->handle_channels_impl = impl;
}

/**
 * tp_base_client_unregister:
 * @self: a client, which may already have been registered with
 *  tp_base_client_register(), or not
 *
 * Remove this client object from D-Bus, if tp_base_client_register()
 * has already been called.
 *
 * If the object is not registered, this method may be called, but has
 * no effect.
 *
 * Releasing the last reference to the object also has the same effect
 * as calling this method, but this method should be preferred, as it
 * has more deterministic behaviour.
 *
 * If the object still exists, tp_base_client_register() may be used to
 * attempt to register it again.
 *
 * Since: 0.11.6
 */
void
tp_base_client_unregister (TpBaseClient *self)
{
  GError *error = NULL;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));

  if (!self->priv->registered)
    return;

  if (!tp_dbus_daemon_release_name (self->priv->dbus, self->priv->bus_name,
        &error))
    {
      WARNING ("Failed to release bus name (%s): %s", self->priv->bus_name,
          error->message);

      g_error_free (error);
    }

  tp_dbus_daemon_unregister_object (self->priv->dbus, self);

  if (self->priv->flags & CLIENT_IS_HANDLER)
    {
      GHashTable *clients;

      clients = dbus_connection_get_data (self->priv->libdbus, clients_slot);
      if (clients != NULL)
        g_hash_table_remove (clients, self->priv->object_path);

      dbus_connection_unref (self->priv->libdbus);
      self->priv->libdbus = NULL;

      dbus_connection_free_data_slot (&clients_slot);
    }

  self->priv->registered = FALSE;
}
