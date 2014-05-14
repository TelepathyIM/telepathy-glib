/*
 * Context objects for TpBaseClient calls
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
 * SECTION:observe-channel-context
 * @title: TpObserveChannelContext
 * @short_description: context of a Observer.ObserveChannels() call
 *
 * Object used to represent the context of a Observer.ObserveChannels()
 * D-Bus call on a #TpBaseClient.
 */

/**
 * TpObserveChannelContext:
 *
 * Data structure representing the context of a Observer.ObserveChannels()
 * call.
 *
 * Since: 0.11.5
 */

/**
 * TpObserveChannelContextClass:
 *
 * The class of a #TpObserveChannelContext.
 *
 * Since: 0.11.5
 */

#include "config.h"

#include "telepathy-glib/observe-channel-context-internal.h"
#include "telepathy-glib/observe-channel-context.h"

#include <telepathy-glib/asv.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util-internal.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

struct _TpObserveChannelContextClass {
    /*<private>*/
    GObjectClass parent_class;
};

G_DEFINE_TYPE(TpObserveChannelContext, tp_observe_channel_context,
    G_TYPE_OBJECT)

enum {
    PROP_ACCOUNT = 1,
    PROP_CONNECTION,
    PROP_CHANNEL,
    PROP_DISPATCH_OPERATION,
    PROP_REQUESTS,
    PROP_OBSERVER_INFO,
    PROP_DBUS_CONTEXT,
    N_PROPS
};

struct _TpObserveChannelContextPrivate
{
  TpObserveChannelContextState state;
  GSimpleAsyncResult *result;
  GDBusMethodInvocation *dbus_context;

  /* Number of calls we are waiting they return. Once they have all returned
   * the context is considered as prepared */
  guint num_pending;
};

static void
tp_observe_channel_context_init (TpObserveChannelContext *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_OBSERVE_CHANNELS_CONTEXT, TpObserveChannelContextPrivate);

  self->priv->state = TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE;
}

static void
tp_observe_channel_context_dispose (GObject *object)
{
  TpObserveChannelContext *self = TP_OBSERVE_CHANNEL_CONTEXT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_observe_channel_context_parent_class)->dispose;

  if (self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE ||
      self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_DELAYED)
    {
      GError error = { TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "Disposing the TpObserveChannelContext" };

      WARNING ("Disposing a context in the %s state",
          self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE ?
          "none": "delayed");

      tp_observe_channel_context_fail (self, &error);
    }

  if (self->account != NULL)
    {
      g_object_unref (self->account);
      self->account = NULL;
    }

  if (self->connection != NULL)
    {
      g_object_unref (self->connection);
      self->connection = NULL;
    }

  g_clear_object (&self->channel);

  if (self->dispatch_operation != NULL)
    {
      g_object_unref (self->dispatch_operation);
      self->dispatch_operation = NULL;
    }

  if (self->requests != NULL)
    {
      g_ptr_array_foreach (self->requests, (GFunc) g_object_unref, NULL);
      g_ptr_array_unref (self->requests);
      self->requests = NULL;
    }

  g_clear_pointer (&self->observer_info, g_variant_unref);

  if (self->priv->result != NULL)
    {
      g_object_unref (self->priv->result);
      self->priv->result = NULL;
    }

  g_clear_object (&self->priv->dbus_context);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_observe_channel_context_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelContext *self = TP_OBSERVE_CHANNEL_CONTEXT (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, self->account);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value, self->connection);
        break;
      case PROP_CHANNEL:
        g_value_set_object (value, self->channel);
        break;
      case PROP_DISPATCH_OPERATION:
        g_value_set_object (value, self->dispatch_operation);
        break;
      case PROP_REQUESTS:
        g_value_set_boxed (value, self->requests);
        break;
      case PROP_OBSERVER_INFO:
        g_value_set_variant (value, self->observer_info);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channel_context_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelContext *self = TP_OBSERVE_CHANNEL_CONTEXT (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        self->account = g_value_dup_object (value);
        break;
      case PROP_CONNECTION:
        self->connection = g_value_dup_object (value);
        break;
      case PROP_CHANNEL:
        self->channel = g_value_dup_object (value);
        break;
      case PROP_DISPATCH_OPERATION:
        self->dispatch_operation = g_value_dup_object (value);
        break;
      case PROP_REQUESTS:
        self->requests = g_value_dup_boxed (value);
        g_ptr_array_foreach (self->requests, (GFunc) g_object_ref, NULL);
        break;
      case PROP_DBUS_CONTEXT:
        self->priv->dbus_context = g_value_dup_object (value);
        break;
      case PROP_OBSERVER_INFO:
        self->observer_info = g_value_dup_variant (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channel_context_constructed (GObject *object)
{
  TpObserveChannelContext *self = TP_OBSERVE_CHANNEL_CONTEXT (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_observe_channel_context_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->account != NULL);
  g_assert (self->connection != NULL);
  g_assert (self->channel != NULL);
  g_assert (self->requests != NULL);
  g_assert (self->observer_info != NULL);
  g_assert (self->priv->dbus_context != NULL);

  /* self->dispatch_operation may be NULL (channel was requested) */
}

static void
tp_observe_channel_context_class_init (TpObserveChannelContextClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpObserveChannelContextPrivate));

  object_class->get_property = tp_observe_channel_context_get_property;
  object_class->set_property = tp_observe_channel_context_set_property;
  object_class->constructed = tp_observe_channel_context_constructed;
  object_class->dispose = tp_observe_channel_context_dispose;

  /**
   * TpObserveChannelContext:account:
   *
   * A #TpAccount object representing the Account that has been passed to
   * ObserveChannels.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("account", "TpAccount",
      "The TpAccount that has been passed to ObserveChannels",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT,
      param_spec);

  /**
   * TpObserveChannelContext:connection:
   *
   * A #TpConnection object representing the Connection that has been passed
   * to ObserveChannels.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The TpConnection that has been passed to ObserveChannels",
      TP_TYPE_CONNECTION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  /**
   * TpObserveChannelContext:channel:
   *
   * A #TpChannel object representing the channel
   * that has been passed to ObserveChannel.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("channel", "TpChannel",
      "The TpChannes that has been passed to ObserveChannel",
      TP_TYPE_CHANNEL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL,
      param_spec);

  /**
   * TpObserveChannelContext:dispatch-operation:
   *
   * A #TpChannelDispatchOperation object representing the
   * ChannelDispatchOperation that has been passed to ObserveChannels,
   * or %NULL if none has been passed.
   * Read-only except during construction.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("dispatch-operation",
     "TpChannelDispatchOperation",
     "The TpChannelDispatchOperation that has been passed to ObserveChannels",
     TP_TYPE_CHANNEL_DISPATCH_OPERATION,
     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPATCH_OPERATION,
      param_spec);

  /**
   * TpObserveChannelContext:requests:
   *
   * A #GPtrArray containing #TpChannelRequest objects representing the
   * requests that have been passed to ObserveChannels.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_boxed ("requests", "GPtrArray of TpChannelRequest",
      "The TpChannelRequest that have been passed to ObserveChannels",
      G_TYPE_PTR_ARRAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTS,
      param_spec);

  /**
   * TpObserveChannelContext:dbus-context: (skip)
   *
   * The #GDBusMethodInvocation representing the D-Bus context of the
   * ObserveChannels call.
   * Can only be written during construction.
   *
   * Since: 0.11.5
   */
  param_spec = g_param_spec_object ("dbus-context", "D-Bus context",
      "The GDBusMethodInvocation associated with the ObserveChannels call",
      G_TYPE_DBUS_METHOD_INVOCATION,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_CONTEXT,
      param_spec);

  /**
   * TpObserveChannelContext:observer-info:
   *
   * A %G_VARIANT_TYPE_VARDICT.
   * It represents the Observer_Info hash table that has been passed to
   * ObserveChannels.
   * It's recommended to use high-level method such as
   * tp_observe_channel_context_is_recovering() to access to its content.
   *
   * This property can't be %NULL at runtime.
   */
  param_spec = g_param_spec_variant ("observer-info", "Observer info",
      "The Observer_Info that has been passed to ObserveChannels",
      G_VARIANT_TYPE_VARDICT, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBSERVER_INFO,
      param_spec);
}

TpObserveChannelContext *
_tp_observe_channel_context_new (
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *dispatch_operation,
    GPtrArray *requests,
    GVariant *observer_info,
    GDBusMethodInvocation *dbus_context)
{
  TpObserveChannelContext *self;

  g_variant_ref_sink (observer_info);
  self = g_object_new (TP_TYPE_OBSERVE_CHANNELS_CONTEXT,
      "account", account,
      "connection", connection,
      "channel", channel,
      "dispatch-operation", dispatch_operation,
      "requests", requests,
      "observer-info", observer_info,
      "dbus-context", dbus_context,
      NULL);
  g_variant_unref (observer_info);
  return self;
}

/**
 * tp_observe_channel_context_accept:
 * @self: a #TpObserveChannelContext
 *
 * Called by #TpBaseClientClassObserveChannelImpl when it's done so the D-Bus
 * method can return.
 *
 * Since: 0.11.5
 */
void
tp_observe_channel_context_accept (TpObserveChannelContext *self)
{
  g_return_if_fail (self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE
      || self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_DELAYED);
  g_return_if_fail (self->priv->dbus_context != NULL);

  self->priv->state = TP_OBSERVE_CHANNEL_CONTEXT_STATE_DONE;
  g_dbus_method_invocation_return_value (self->priv->dbus_context, NULL);

  g_clear_object (&self->priv->dbus_context);
}

/**
 * tp_observe_channel_context_fail:
 * @self: a #TpObserveChannelContext
 * @error: the error to return from the method
 *
 * Called by #TpBaseClientClassObserveChannelImpl to raise a D-Bus error.
 *
 * Since: 0.11.5
 */
void
tp_observe_channel_context_fail (TpObserveChannelContext *self,
    const GError *error)
{
  g_return_if_fail (self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE
      || self->priv->state == TP_OBSERVE_CHANNEL_CONTEXT_STATE_DELAYED);
  g_return_if_fail (self->priv->dbus_context != NULL);

  self->priv->state = TP_OBSERVE_CHANNEL_CONTEXT_STATE_FAILED;
  g_dbus_method_invocation_return_gerror (self->priv->dbus_context, error);

  g_clear_object (&self->priv->dbus_context);
}

/**
 * tp_observe_channel_context_delay:
 * @self: a #TpObserveChannelContext
 *
 * Called by #TpBaseClientClassObserveChannelImpl to indicate that it
 * implements the method in an async way. The caller must take a reference
 * to the #TpObserveChannelContext before calling this function, and
 * is responsible for calling either tp_observe_channel_context_accept() or
 * tp_observe_channel_context_fail() later.
 *
 * Since: 0.11.5
 */
void
tp_observe_channel_context_delay (TpObserveChannelContext *self)
{
  g_return_if_fail (self->priv->state ==
      TP_OBSERVE_CHANNEL_CONTEXT_STATE_NONE);

  self->priv->state = TP_OBSERVE_CHANNEL_CONTEXT_STATE_DELAYED;
}

/**
 * tp_observe_channel_context_is_recovering:
 * @self: a #TpObserveChannelContext
 *
 * If this call to ObserveChannels is for channels that already
 * existed before this observer started (because the observer used
 * tp_base_client_set_observer_recover()), return %TRUE.
 *
 * In most cases, the result is %FALSE.
 *
 * Returns: %TRUE for pre-existing channels, %FALSE for new channels
 *
 * Since: 0.11.5
 */
gboolean
tp_observe_channel_context_is_recovering (TpObserveChannelContext *self)
{
  /* tp_asv_get_boolean returns FALSE if the key is not set which is what we
   * want */
  return tp_vardict_get_boolean (self->observer_info, "recovering", NULL);
}

TpObserveChannelContextState
_tp_observe_channel_context_get_state (
    TpObserveChannelContext *self)
{
  return self->priv->state;
}

static gboolean
context_is_prepared (TpObserveChannelContext *self)
{
  return self->priv->num_pending == 0;
}

static void
context_check_prepare (TpObserveChannelContext *self)
{
  if (!context_is_prepared (self))
    return;

  /* Context is prepared */
  g_simple_async_result_complete (self->priv->result);

  g_object_unref (self->priv->result);
  self->priv->result = NULL;
}

static void
cdo_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpObserveChannelContext *self = user_data;
  GError *error = NULL;

  if (self->priv->result == NULL)
    goto out;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      DEBUG ("Failed to prepare ChannelDispatchOperation: %s", error->message);

      g_error_free (error);
    }

  self->priv->num_pending--;
  context_check_prepare (self);

out:
  g_object_unref (self);
}

static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpObserveChannelContext *self = user_data;
  GError *error = NULL;

  if (self->priv->result == NULL)
    goto out;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      DEBUG ("Failed to prepare account: %s", error->message);
      g_error_free (error);
    }

  self->priv->num_pending--;
  context_check_prepare (self);

out:
  g_object_unref (self);
}

static void
conn_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpObserveChannelContext *self = user_data;
  GError *error = NULL;

  if (self->priv->result == NULL)
    goto out;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      DEBUG ("Failed to prepare connection: %s", error->message);
      g_error_free (error);
    }

  self->priv->num_pending--;
  context_check_prepare (self);

out:
  g_object_unref (self);
}

static void
occ_channel_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpObserveChannelContext *self = user_data;
  GError *error = NULL;

  if (self->priv->result == NULL)
    goto out;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      DEBUG ("Failed to prepare channel: %s", error->message);
      g_error_free (error);
    }

  self->priv->num_pending--;
  context_check_prepare (self);

out:
  g_object_unref (self);
}

static void
context_prepare (TpObserveChannelContext *self,
    const GQuark *account_features,
    const GQuark *connection_features,
    const GQuark *channel_features)
{
  GQuark cdo_features[] = { TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE, 0 };

  self->priv->num_pending = 3;

  tp_proxy_prepare_async (self->account, account_features,
      account_prepare_cb, g_object_ref (self));

  tp_proxy_prepare_async (self->connection, connection_features,
      conn_prepare_cb, g_object_ref (self));

  if (self->dispatch_operation != NULL)
    {
      self->priv->num_pending++;
      tp_proxy_prepare_async (self->dispatch_operation, cdo_features,
          cdo_prepare_cb, g_object_ref (self));
    }

  tp_proxy_prepare_async (self->channel, channel_features,
      occ_channel_prepare_cb, g_object_ref (self));
}

void
_tp_observe_channel_context_prepare_async (TpObserveChannelContext *self,
    const GQuark *account_features,
    const GQuark *connection_features,
    const GQuark *channel_features,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TP_IS_OBSERVE_CHANNELS_CONTEXT (self));
  /* This is only used once, by TpBaseClient, so for simplicity, we only
   * allow one asynchronous preparation */
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, _tp_observe_channel_context_prepare_async);

  context_prepare (self, account_features, connection_features,
      channel_features);
}

gboolean
_tp_observe_channel_context_prepare_finish (
    TpObserveChannelContext *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, _tp_observe_channel_context_prepare_async);
}

/**
 * tp_observe_channel_context_get_requests:
 * @self: a #TpObserveChannelContext
 *
 * Return a list of the #TpChannelRequest which have been satisfied by the
 * channels associated with #self.
 *
 * Returns: (transfer full) (element-type TelepathyGLib.ChannelRequest):
 *  a newly allocated #GList of reffed #TpChannelRequest.
 *
 * Since: 0.13.14
 */
GList *
tp_observe_channel_context_get_requests (TpObserveChannelContext *self)
{
  GList *list = NULL;
  guint i;

  for (i = 0; i < self->requests->len; i++)
    {
      list = g_list_prepend (list, g_object_ref (g_ptr_array_index (
              self->requests, i)));
    }

  return list;
}
