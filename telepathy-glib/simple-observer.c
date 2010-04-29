/*
 * Simple implementation of an Observer
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
 * SECTION: simple-observer
 * @title: TpSimpleObserver
 * @short_description: a subclass of #TpBaseClient implementing
 * a simple Observer
 *
 * This class makes it easier to write #TpSvcClient implementing the
 * TpSvcClientObserver interface.
 *
 * A typical simple observer would look liks this:
 * |[
 * static void
 * my_observe_channels (TpSimpleObserver *self,
 *    TpAccount *account,
 *    TpConnection *connection,
 *    GList *channels,
 *    TpChannelDispatchOperation *dispatch_operation,
 *    GList *requests,
 *    TpObserveChannelsContext *context,
 *    gpointer user_data)
 * {
 *  /<!-- -->* do something useful with the channels here *<!-- -->/
 *
 *  tp_observe_channels_context_accept (context);
 * }
 *
 * client = tp_simple_observer_new (dbus, TRUE, "MyObserver", FALSE,
 *    my_observe_channels, user_data);
 *
 * tp_base_client_take_observer_filter (client, tp_asv_new (
 *      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
 *      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
 *      NULL));
 *
 * tp_base_client_register (client, NULL);
 * ]|
 *
 * See examples/client/media-observer.c for a complete example.
 */

/**
 * TpSimpleObserver:
 *
 * Data structure representing a simple Observer implementation.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpSimpleObserverClass:
 *
 * The class of a #TpSimpleObserver.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpSimpleObserverObserveChannelsImpl:
 * @self: a #TpSimpleObserver instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * @channels: (element-type Tp.Channel): a #GList of #TpChannel,
 * all having %TP_CHANNEL_FEATURE_CORE prepared
 * @dispatch_operation: a #TpChannelDispatchOperation or %NULL; the
 * dispatch_operation is not guaranteed to be prepared
 * @requests: (element-type Tp.ChannelRequest): a #GList of #TpChannelRequest,
 * all having their object-path defined
 * but are not guaranteed to be prepared.
 * @context: a #TpObserveChannelsContext representing the context of this
 * D-Bus call
 * @user_data: arbitrary user-supplied data passed to tp_simple_observer_new()
 *
 * Signature of the implementation of the ObserveChannels method.
 *
 * This function must call either tp_observe_channels_context_accept,
 * tp_observe_channels_context_delay or tp_observe_channels_context_fail
 * on @context before it returns.
 *
 * Since: 0.11.UNRELEASED
 */

#include "telepathy-glib/simple-observer.h"

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

G_DEFINE_TYPE(TpSimpleObserver, tp_simple_observer, TP_TYPE_BASE_CLIENT)

enum {
    PROP_RECOVER = 1,
    PROP_OBSERV_IMPL,
    PROP_USER_DATA,
    N_PROPS
};

struct _TpSimpleObserverPrivate
{
  TpSimpleObserverObserveChannelsImpl observe_channels_impl;
  gpointer user_data;
};

static void
tp_simple_observer_init (TpSimpleObserver *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_SIMPLE_OBSERVER,
      TpSimpleObserverPrivate);
}

static void
tp_simple_observer_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseClient *base = TP_BASE_CLIENT (object);
  TpSimpleObserver *self = TP_SIMPLE_OBSERVER (object);

  switch (property_id)
    {
      case PROP_RECOVER:
        tp_base_client_set_observer_recover (base, g_value_get_boolean (value));
        break;
      case PROP_OBSERV_IMPL:
        self->priv->observe_channels_impl = g_value_get_pointer (value);
        break;
      case PROP_USER_DATA:
        self->priv->user_data = g_value_get_pointer (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_simple_observer_constructed (GObject *object)
{
  TpSimpleObserver *self = TP_SIMPLE_OBSERVER (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_simple_observer_parent_class)->constructed;

  g_assert (self->priv->observe_channels_impl != NULL);

  if (chain_up != NULL)
    chain_up (object);
}

static void
observe_channels (
    TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context)
{
  TpSimpleObserver *self = TP_SIMPLE_OBSERVER (client);

  self->priv->observe_channels_impl (self, account, connection, channels,
      dispatch_operation, requests, context, self->priv->user_data);
}

static void
tp_simple_observer_class_init (TpSimpleObserverClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  TpBaseClientClass *base_clt_cls = TP_BASE_CLIENT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpSimpleObserverPrivate));

  object_class->set_property = tp_simple_observer_set_property;
  object_class->constructed = tp_simple_observer_constructed;

  /**
   * TpSimpleObserver:recover:
   *
   * The value of the Observer.Recover D-Bus property.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boolean ("recover", "Recover",
      "Observer.Recover",
      FALSE,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECOVER,
      param_spec);

  /**
   * TpSimpleObserver:observe-channels-impl:
   *
   * The TpSimpleObserverObserveChannelsImpl callback implementing the
   * ObserverChannels D-Bus method.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_pointer ("observe-channels-impl",
      "implementation of ObserverChannels",
      "Function called when ObserverChannels is called",
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBSERV_IMPL,
      param_spec);

  /**
   * TpSimpleObserver:user-data:
   *
   * The user-data pointer passed to the callback implementing the
   * ObserverChannels D-Bus method.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_pointer ("user-data", "user data",
      "pointer passed as user-data when ObserverChannels is called",
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_USER_DATA,
      param_spec);

  tp_base_client_implement_observe_channels (base_clt_cls, observe_channels);
}

/**
 * tp_simple_observer_new:
 * @dbus: a #TpDBusDaemon object, may not be %NULL
 * @recover: the value of the Observer.Recover D-Bus property
 * @name: the name of the Observer (see #TpBaseClient:name: for details)
 * @unique: the value of the TpBaseClient:uniquify-name: property
 * @observe_channels_impl: the function called when ObserverChannels is called
 * @user_data: arbitrary user-supplied data passed to @observe_channels_impl
 *
 * Convenient function to create a new #TpSimpleObserver instance.
 *
 * Returns: a new #TpSimpleObserver
 *
 * Since: 0.11.UNRELEASED
 */
TpBaseClient *
tp_simple_observer_new (TpDBusDaemon *dbus,
    gboolean recover,
    const gchar *name,
    gboolean unique,
    TpSimpleObserverObserveChannelsImpl observe_channels_impl,
    gpointer user_data)
{
  return g_object_new (TP_TYPE_SIMPLE_OBSERVER,
      "dbus-daemon", dbus,
      "recover", recover,
      "name", name,
      "uniquify-name", unique,
      "observe-channels-impl", observe_channels_impl,
      "user-data", user_data,
      NULL);
}
