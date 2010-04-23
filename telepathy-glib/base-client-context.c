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

#include "telepathy-glib/base-client-context.h"
#include "telepathy-glib/base-client-context-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

struct _TpObserveChannelsContextClass {
    /*<private>*/
    GObjectClass parent_class;
};

G_DEFINE_TYPE(TpObserveChannelsContext, tp_observe_channels_context,
    G_TYPE_OBJECT)

enum {
    PROP_ACCOUNT = 1,
    PROP_CONNECTION,
    PROP_CHANNELS,
    PROP_DISPATCH_OPERATION,
    PROP_REQUESTS,
    PROP_OBSERVER_INFO,
    PROP_DBUS_CONTEXT,
    N_PROPS
};

struct _TpObserveChannelsContextPrivate
{
  TpBaseClientContextState state;
  GSimpleAsyncResult *result;
};

static void
tp_observe_channels_context_init (TpObserveChannelsContext *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_OBSERVE_CHANNELS_CONTEXT, TpObserveChannelsContextPrivate);

  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_NONE;
}

static void
tp_observe_channels_context_dispose (GObject *object)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_observe_channels_context_parent_class)->dispose;

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

  if (self->channels != NULL)
    {
      g_ptr_array_foreach (self->channels, (GFunc) g_object_unref, NULL);
      g_ptr_array_unref (self->channels);
      self->channels = NULL;
    }

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

  if (self->observer_info != NULL)
    {
      g_hash_table_unref (self->observer_info);
      self->observer_info = NULL;
    }

  if (self->priv->result != NULL)
    {
      g_object_unref (self->priv->result);
      self->priv->result = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_observe_channels_context_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, self->account);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value, self->connection);
        break;
      case PROP_CHANNELS:
        g_value_set_boxed (value, self->channels);
        break;
      case PROP_DISPATCH_OPERATION:
        g_value_set_object (value, self->dispatch_operation);
        break;
      case PROP_REQUESTS:
        g_value_set_boxed (value, self->requests);
        break;
      case PROP_DBUS_CONTEXT:
        g_value_set_pointer (value, self->dbus_context);
        break;
      case PROP_OBSERVER_INFO:
        g_value_set_boxed (value, self->observer_info);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channels_context_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        self->account = g_value_dup_object (value);
        break;
      case PROP_CONNECTION:
        self->connection = g_value_dup_object (value);
        break;
      case PROP_CHANNELS:
        self->channels = g_value_dup_boxed (value);
        g_ptr_array_foreach (self->channels, (GFunc) g_object_ref, NULL);
        break;
      case PROP_DISPATCH_OPERATION:
        self->dispatch_operation = g_value_dup_object (value);
        break;
      case PROP_REQUESTS:
        self->requests = g_value_dup_boxed (value);
        g_ptr_array_foreach (self->requests, (GFunc) g_object_ref, NULL);
        break;
      case PROP_DBUS_CONTEXT:
        self->dbus_context = g_value_get_pointer (value);
        break;
      case PROP_OBSERVER_INFO:
        self->observer_info = g_value_dup_boxed (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channels_context_constructed (GObject *object)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_observe_channels_context_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->account != NULL);
  g_assert (self->connection != NULL);
  g_assert (self->channels != NULL);
  g_assert (self->requests != NULL);
  g_assert (self->observer_info != NULL);
  g_assert (self->dbus_context != NULL);

  /* self->dispatch_operation may be NULL (channels were requested) */
}

static void
tp_observe_channels_context_class_init (TpObserveChannelsContextClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpObserveChannelsContextPrivate));

  object_class->get_property = tp_observe_channels_context_get_property;
  object_class->set_property = tp_observe_channels_context_set_property;
  object_class->constructed = tp_observe_channels_context_constructed;
  object_class->dispose = tp_observe_channels_context_dispose;

  param_spec = g_param_spec_object ("account", "TpAccount",
      "The TpAccount that has been passed to ObserveChannels",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT,
      param_spec);

  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The TpConnection that has been passed to ObserveChannels",
      TP_TYPE_CONNECTION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  param_spec = g_param_spec_boxed ("channels", "GPtrArray of TpChannel",
      "The TpChannels that have been passed to ObserveChannels",
      G_TYPE_PTR_ARRAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNELS,
      param_spec);

  param_spec = g_param_spec_object ("dispatch-operation",
     "TpChannelDispatchOperation",
     "The TpChannelDispatchOperation that has been passed to ObserveChannels",
     TP_TYPE_CHANNEL_DISPATCH_OPERATION,
     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPATCH_OPERATION,
      param_spec);

  param_spec = g_param_spec_boxed ("requests", "GPtrArray of TpChannelRequest",
      "The TpChannelRequest that have been passed to ObserveChannels",
      G_TYPE_PTR_ARRAY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTS,
      param_spec);

  param_spec = g_param_spec_pointer ("dbus-context", "D-Bus context",
      "The DBusGMethodInvocation associated with the ObserveChannels call",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_CONTEXT,
      param_spec);

  param_spec = g_param_spec_boxed ("observer-info", "Observer info",
      "The Observer_Info that has been passed to ObserveChannels",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBSERVER_INFO,
      param_spec);
}

TpObserveChannelsContext *
_tp_observe_channels_context_new (
    TpAccount *account,
    TpConnection *connection,
    GPtrArray *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GPtrArray *requests,
    GHashTable *observer_info,
    DBusGMethodInvocation *dbus_context)
{
  return g_object_new (TP_TYPE_OBSERVE_CHANNELS_CONTEXT,
      "account", account,
      "connection", connection,
      "channels", channels,
      "dispatch-operation", dispatch_operation,
      "requests", requests,
      "observer-info", observer_info,
      "dbus-context", dbus_context,
      NULL);
}

void
tp_observe_channels_context_accept (TpObserveChannelsContext *self)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_DONE;
  dbus_g_method_return (self->dbus_context);
}

void
tp_observe_channels_context_fail (TpObserveChannelsContext *self,
    const GError *error)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_FAILED;
  dbus_g_method_return_error (self->dbus_context, error);
}

void
tp_observe_channels_context_delay (TpObserveChannelsContext *self)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_DELAYED;
}

gboolean
tp_observe_channels_context_get_recovering (TpObserveChannelsContext *self)
{
  /* tp_asv_get_boolean returns FALSE if the key is not set which is what we
   * want */
  return tp_asv_get_boolean (self->observer_info, "recovering", NULL);
}

TpBaseClientContextState
_tp_observe_channels_context_get_state (
    TpObserveChannelsContext *self)
{
  return self->priv->state;
}

static gboolean
context_is_prepared (TpObserveChannelsContext *self)
{
  if (!tp_proxy_is_prepared (self->account, TP_ACCOUNT_FEATURE_CORE))
    return FALSE;

  /* TODO */
  return TRUE;
}

static void
context_check_prepare (TpObserveChannelsContext *self)
{
  if (!context_is_prepared (self))
    return;

  /* Context is prepared */
  g_simple_async_result_complete (self->priv->result);

  g_object_unref (self->priv->result);
  self->priv->result = NULL;
}

static void
failed_to_prepare (TpObserveChannelsContext *self,
    const GError *error)
{
  g_return_if_fail (self->priv->result != NULL);

  g_simple_async_result_set_from_error (self->priv->result, error);
  g_simple_async_result_complete (self->priv->result);

  g_object_unref (self->priv->result);
  self->priv->result = NULL;
}

static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpObserveChannelsContext *self = user_data;
  GError *error = NULL;

  if (self->priv->result == NULL)
    goto out;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      DEBUG ("Failed to prepare account: %s", error->message);
      failed_to_prepare (self, error);
      g_error_free (error);
      goto out;
    }

  context_check_prepare (self);

out:
  g_object_unref (self);
}

static void
context_prepare (TpObserveChannelsContext *self)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (self->account, account_features,
      account_prepare_cb, g_object_ref (self));

  /* FIXME: prepare other objects */
}

void
tp_observe_channels_context_prepare_async (TpObserveChannelsContext *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TP_IS_OBSERVE_CHANNELS_CONTEXT (self));
  g_return_if_fail (self->priv->result == NULL);

  self->priv->result = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, tp_observe_channels_context_prepare_async);

  if (context_is_prepared (self))
    {
      g_simple_async_result_complete_in_idle (self->priv->result);
      g_object_unref (self->priv->result);
      self->priv->result = NULL;
      return;
    }

  context_prepare (self);
}

gboolean
tp_observe_channels_context_prepare_finish (
    TpObserveChannelsContext *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_OBSERVE_CHANNELS_CONTEXT (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (self), tp_observe_channels_context_prepare_async), FALSE);

  return TRUE;
}
