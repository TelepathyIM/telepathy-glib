/*
 * channel-dispatch-operation.c - proxy for incoming channels seeking approval
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#include "telepathy-glib/channel-dispatch-operation.h"

#include <telepathy-glib/account.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-internal.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_DISPATCHER
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-channel-dispatch-operation-body.h"

/**
 * SECTION:channel-dispatch-operation
 * @title: TpChannelDispatchOperation
 * @short_description: proxy object for a to the Telepathy channel
 *  dispatcher
 * @see_also: #TpChannelDispatcher
 *
 * One of the channel dispatcher's functions is to offer incoming channels to
 * Approver clients for approval. Approvers respond to the channel dispatcher
 * via a #TpChannelDispatchOperation object.
 */

/**
 * TpChannelDispatchOperation:
 *
 * One of the channel dispatcher's functions is to offer incoming channels to
 * Approver clients for approval. An approver should generally ask the user
 * whether they want to participate in the requested communication channels
 * (join the chat or chatroom, answer the call, accept the file transfer, or
 * whatever is appropriate). A collection of channels offered in this way
 * is represented by a ChannelDispatchOperation object.
 *
 * If the user wishes to accept the communication channels, the approver
 * should call tp_cli_channel_dispatch_operation_call_handle_with() to
 * indicate the user's or approver's preferred handler for the channels (the
 * empty string indicates no particular preference, and will cause any
 * suitable handler to be used).
 *
 * If the user wishes to reject the communication channels, or if the user
 * accepts the channels and the approver will handle them itself, the approver
 * should call tp_cli_channel_dispatch_operation_call_claim(). If this method
 * succeeds, the approver immediately has control over the channels as their
 * primary handler, and may do anything with them (in particular, it may close
 * them in whatever way seems most appropriate).
 *
 * There are various situations in which the channel dispatch operation will
 * be closed, causing the #TpProxy::invalidated signal to be emitted. If this
 * happens, the approver should stop prompting the user.
 *
 * Because all approvers are launched simultaneously, the user might respond
 * to another approver; if this happens, the invalidated signal will be
 * emitted with the domain %TP_DBUS_ERRORS and the error code
 * %TP_DBUS_ERROR_OBJECT_REMOVED.
 *
 * If a channel closes, the D-Bus signal ChannelLost is emitted; this class
 * doesn't (yet) have a GObject binding for this signal, but you can use
 * tp_cli_channel_dispatch_operation_connect_to_channel_lost(). If all channels
 * close, there is nothing more to dispatch, so the invalidated signal will be
 * emitted with the domain %TP_DBUS_ERRORS and the error code
 * %TP_DBUS_ERROR_OBJECT_REMOVED.
 *
 * If the channel dispatcher crashes or exits, the invalidated
 * signal will be emitted with the domain %TP_DBUS_ERRORS and the error code
 * %TP_DBUS_ERROR_NAME_OWNER_LOST. In a high-quality implementation, the
 * dispatcher should be restarted, at which point it will create new
 * channel dispatch operations for any undispatched channels, and the approver
 * will be notified again.
 *
 * This proxy is usable but incomplete: accessors for the D-Bus properties will
 * be added in a later version of telepathy-glib, along with a mechanism
 * similar to tp_connection_call_when_ready().
 *
 * Since: 0.7.32
 */

/**
 * TpChannelDispatchOperationClass:
 *
 * The class of a #TpChannelDispatchOperation.
 */

struct _TpChannelDispatchOperationPrivate {
  TpConnection *connection;
  TpAccount *account;
  GPtrArray *channels;
  GStrv possible_handlers;
  GHashTable *immutable_properties;
};

enum
{
  PROP_CONNECTION = 1,
  PROP_ACCOUNT,
  PROP_CHANNELS,
  PROP_POSSIBLE_HANDLERS,
  PROP_CHANNEL_DISPATCH_OPERATION_PROPERTIES,
  N_PROPS
};

G_DEFINE_TYPE (TpChannelDispatchOperation, tp_channel_dispatch_operation,
    TP_TYPE_PROXY);

static void
tp_channel_dispatch_operation_init (TpChannelDispatchOperation *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_CHANNEL_DISPATCH_OPERATION, TpChannelDispatchOperationPrivate);

  self->priv->immutable_properties = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free);
}

static void
tp_channel_dispatch_operation_finished_cb (TpChannelDispatchOperation *self,
    gpointer unused G_GNUC_UNUSED,
    GObject *object G_GNUC_UNUSED)
{
  GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
      "ChannelDispatchOperation finished and was removed" };

  tp_proxy_invalidate ((TpProxy *) self, &e);
}

static void
tp_channel_dispatch_operation_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpChannelDispatchOperation *self = TP_CHANNEL_DISPATCH_OPERATION (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->connection);
      break;

    case PROP_ACCOUNT:
      g_value_set_object (value, self->priv->account);
      break;

    case PROP_CHANNELS:
      g_value_set_boxed (value, self->priv->channels);
      break;

    case PROP_POSSIBLE_HANDLERS:
      g_value_set_boxed (value, self->priv->possible_handlers);
      break;

    case PROP_CHANNEL_DISPATCH_OPERATION_PROPERTIES:
      g_value_set_boxed (value, self->priv->immutable_properties);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
maybe_set_connection (TpChannelDispatchOperation *self,
    const gchar *path)
{
  TpDBusDaemon *dbus;
  GError *error = NULL;

  if (self->priv->connection != NULL)
    return;

  if (path == NULL)
    return;

  dbus = tp_proxy_get_dbus_daemon (self);

  self->priv->connection = tp_connection_new (dbus, NULL, path, &error);
  if (self->priv->connection == NULL)
    {
      DEBUG ("Failed to create connection %s: %s", path, error->message);
      g_error_free (error);
      return;
    }

  g_object_notify ((GObject *) self, "connection");

  if (g_hash_table_lookup (self->priv->immutable_properties,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION) != NULL)
    return;

  g_hash_table_insert (self->priv->immutable_properties,
      g_strdup (TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION),
      tp_g_value_slice_new_boxed (DBUS_TYPE_G_OBJECT_PATH, path));
}

static void
maybe_set_account (TpChannelDispatchOperation *self,
    const gchar *path)
{
  TpDBusDaemon *dbus;
  GError *error = NULL;

  if (self->priv->account != NULL)
    return;

  if (path == NULL)
    return;

  dbus = tp_proxy_get_dbus_daemon (self);

  self->priv->account = tp_account_new (dbus, path, &error);
  if (self->priv->account == NULL)
    {
      DEBUG ("Failed to create account %s: %s", path, error->message);
      g_error_free (error);
      return;
    }

  g_object_notify ((GObject *) self, "account");

  if (g_hash_table_lookup (self->priv->immutable_properties,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT) != NULL)
    return;

  g_hash_table_insert (self->priv->immutable_properties,
      g_strdup (TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT),
      tp_g_value_slice_new_boxed (DBUS_TYPE_G_OBJECT_PATH, path));
}

static void
maybe_set_possible_handlers (TpChannelDispatchOperation *self,
    GStrv handlers)
{
  if (self->priv->possible_handlers != NULL)
    return;

  if (handlers == NULL)
    return;

  self->priv->possible_handlers = g_strdupv (handlers);

  g_object_notify ((GObject *) self, "possible-handlers");

  if (g_hash_table_lookup (self->priv->immutable_properties,
        TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS) != NULL)
    return;

  g_hash_table_insert (self->priv->immutable_properties,
      g_strdup (TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS),
      tp_g_value_slice_new_boxed (G_TYPE_STRV, handlers));
}

static void
maybe_set_interfaces (TpChannelDispatchOperation *self,
    const gchar **interfaces)
{
  const gchar **iter;

  if (interfaces == NULL)
    return;

  for (iter = interfaces; *iter != NULL; iter++)
    {
      DEBUG ("- %s", *iter);

      if (tp_dbus_check_valid_interface_name (*iter, NULL))
        {
          GQuark q = g_quark_from_string (*iter);
          tp_proxy_add_interface_by_id ((TpProxy *) self, q);
        }
      else
        {
          DEBUG ("\tInterface %s not valid, ignoring it", *iter);
        }
    }

  g_hash_table_insert (self->priv->immutable_properties,
      g_strdup (TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES),
      tp_g_value_slice_new_boxed (G_TYPE_STRV, interfaces));
}

static void
tp_channel_dispatch_operation_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpChannelDispatchOperation *self = TP_CHANNEL_DISPATCH_OPERATION (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      self->priv->connection = g_value_dup_object (value);
      break;

    case PROP_ACCOUNT:
      self->priv->account = g_value_dup_object (value);
      break;

    case PROP_POSSIBLE_HANDLERS:
      self->priv->possible_handlers = g_value_dup_boxed (value);
      break;

    case PROP_CHANNEL_DISPATCH_OPERATION_PROPERTIES:
      {
        GHashTable *asv = g_value_get_boxed (value);

        if (asv == NULL)
          return;

        tp_g_hash_table_update (self->priv->immutable_properties,
            asv, (GBoxedCopyFunc) g_strdup,
            (GBoxedCopyFunc) tp_g_value_slice_dup);

        maybe_set_connection (self, tp_asv_get_boxed (asv,
              TP_PROP_CHANNEL_DISPATCH_OPERATION_CONNECTION,
              DBUS_TYPE_G_OBJECT_PATH));

        maybe_set_account (self, tp_asv_get_boxed (asv,
              TP_PROP_CHANNEL_DISPATCH_OPERATION_ACCOUNT,
              DBUS_TYPE_G_OBJECT_PATH));

        maybe_set_possible_handlers (self, tp_asv_get_boxed (asv,
              TP_PROP_CHANNEL_DISPATCH_OPERATION_POSSIBLE_HANDLERS,
              G_TYPE_STRV));

        maybe_set_interfaces (self, tp_asv_get_boxed (asv,
              TP_PROP_CHANNEL_DISPATCH_OPERATION_INTERFACES,
              G_TYPE_STRV));
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_channel_dispatch_operation_constructed (GObject *object)
{
  TpChannelDispatchOperation *self = TP_CHANNEL_DISPATCH_OPERATION (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_channel_dispatch_operation_parent_class)->constructed;
  GError *error = NULL;
  TpProxySignalConnection *sc;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);

  sc = tp_cli_channel_dispatch_operation_connect_to_finished (self,
      tp_channel_dispatch_operation_finished_cb, NULL, NULL, NULL, &error);

  if (sc == NULL)
    {
      CRITICAL ("Couldn't connect to Finished: %s", error->message);
      g_error_free (error);
      g_assert_not_reached ();
      return;
    }
}

static void
tp_channel_dispatch_operation_dispose (GObject *object)
{
  TpChannelDispatchOperation *self = TP_CHANNEL_DISPATCH_OPERATION (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_channel_dispatch_operation_parent_class)->dispose;

  if (self->priv->connection != NULL)
    {
      g_object_unref (self->priv->connection);
      self->priv->connection = NULL;
    }

  if (self->priv->account != NULL)
    {
      g_object_unref (self->priv->account);
      self->priv->account = NULL;
    }

  if (self->priv->channels != NULL)
    {
      g_ptr_array_foreach (self->priv->channels, (GFunc) g_object_unref, NULL);
      g_ptr_array_free (self->priv->channels, TRUE);
      self->priv->channels = NULL;
    }

  g_strfreev (self->priv->possible_handlers);
  self->priv->possible_handlers = NULL;

  if (self->priv->immutable_properties != NULL)
    {
      g_hash_table_unref (self->priv->immutable_properties);
      self->priv->immutable_properties = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_channel_dispatch_operation_class_init (TpChannelDispatchOperationClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpChannelDispatchOperationPrivate));

  object_class->get_property = tp_channel_dispatch_operation_get_property;
  object_class->set_property = tp_channel_dispatch_operation_set_property;
  object_class->constructed = tp_channel_dispatch_operation_constructed;
  object_class->dispose = tp_channel_dispatch_operation_dispose;

  /**
   * TpChannelDispatchOperation:connection:
   *
   * The #TpConnection with which the channels are associated.
   *
   * Read-only except during construction.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The TpConnection of this channel dispatch operation",
      TP_TYPE_CONNECTION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  /**
   * TpChannelDispatchOperation:account:
   *
   * The #TpAccount with which the connection and channels are associated.
   *
   * Read-only except during construction.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_object ("account", "TpAccount",
      "The TpAccount of this channel dispatch operation",
      TP_TYPE_ACCOUNT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT,
      param_spec);

  /**
   * TpChannelDispatchOperation:channels:
   *
   * A #GPtrArray containing the #TpChannel to be dispatched.
   *
   * Read-only.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("channels", "GPtrArray of TpChannel",
      "The TpChannel to be dispatched",
      G_TYPE_PTR_ARRAY,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNELS,
      param_spec);

  /**
   * TpChannelDispatchOperation:possible-handlers:
   *
   * A #GStrv containing the well known bus names (starting
   * with TP_CLIENT_BUS_NAME_BASE) of the possible Handlers for
   * the channels
   *
   * Read-only except during construction.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("possible-handlers", "Possible handlers",
      "Possible handlers for the channels",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_POSSIBLE_HANDLERS,
      param_spec);

  /**
   * TpChannelDispatchOperation:channel-dispatch-operation-properties:
   *
   * The immutable D-Bus properties of this ChannelDispatchOperation,
   * represented by a #GHashTable where the keys are D-Bus
   * interface name + "." + property name, and the values are #GValue instances.
   *
   * Read-only except during construction. If this is not provided
   * during construction, it is not guaranteed to be set until
   * tp_proxy_prepare_async() has finished preparing
   * %TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("channel-dispatch-operation-properties",
      "Immutable D-Bus properties",
      "A map D-Bus interface + \".\" + property name => GValue",
      TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_CHANNEL_DISPATCH_OPERATION_PROPERTIES, param_spec);

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL_DISPATCH_OPERATION;
  proxy_class->must_have_unique_name = TRUE;

  tp_channel_dispatch_operation_init_known_interfaces ();
}

/**
 * tp_channel_dispatch_operation_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpChannelDispatchOperation have been
 * set up. This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CHANNEL_DISPATCH_OPERATION.
 *
 * Since: 0.7.32
 */
void
tp_channel_dispatch_operation_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_CHANNEL_DISPATCH_OPERATION;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_channel_dispatch_operation_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

/**
 * tp_channel_dispatch_operation_new:
 * @bus_daemon: Proxy for the D-Bus daemon
 * @object_path: The non-NULL object path of this channel dispatch operation
 * @immutable_properties: As many as are known of the immutable D-Bus
 *  properties of this channel dispatch operation, or %NULL if none are known
 * @error: Used to raise an error if %NULL is returned
 *
 * Convenience function to create a new channel dispatch operation proxy.
 *
 * The @immutable_properties argument is not yet used.
 *
 * Returns: a new reference to an channel dispatch operation proxy, or %NULL if
 *    @object_path is not syntactically valid or the channel dispatcher is not
 *    running
 */
TpChannelDispatchOperation *
tp_channel_dispatch_operation_new (TpDBusDaemon *bus_daemon,
    const gchar *object_path,
    GHashTable *immutable_properties,
    GError **error)
{
  TpChannelDispatchOperation *self;
  gchar *unique_name;

  g_return_val_if_fail (bus_daemon != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  if (!_tp_dbus_daemon_get_name_owner (bus_daemon, -1,
      TP_CHANNEL_DISPATCHER_BUS_NAME, &unique_name, error))
    return NULL;

  self = TP_CHANNEL_DISPATCH_OPERATION (g_object_new (
        TP_TYPE_CHANNEL_DISPATCH_OPERATION,
        "dbus-daemon", bus_daemon,
        "dbus-connection", ((TpProxy *) bus_daemon)->dbus_connection,
        "bus-name", unique_name,
        "object-path", object_path,
        "channel-dispatch-operation-properties", immutable_properties,
        NULL));

  g_free (unique_name);

  return self;
}

/**
 * TP_CHANNEL_DISPATCH_OPERATION_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core" feature
 * on a #TpChannelDispatchOperation.
 *
 * When this feature is prepared, the basic properties of the
 * ChannelDispatchOperation have been retrieved and are available for use.
 *
 * Specifically, this implies that:
 *
 * - #TpChannelDispatchOperation:connection is set (but
 *   TP_CONNECTION_FEATURE_CORE is not necessarily prepared)
 * - #TpChannelDispatchOperation:account is set (but
 *   TP_ACCOUNT_FEATURE_CORE is not necessarily prepared)
 * - #TpChannelDispatchOperation:channels is set (but
 *   TP_CHANNEL_FEATURE_CORE is not necessarily prepared)
 * - #TpChannelDispatchOperation:possible-handlers is set
 * - any extra interfaces will have been set up in TpProxy (i.e.
 *   #TpProxy:interfaces contains at least all extra ChannelDispatchOperation
 *   interfaces)
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.11.UNRELEASED
 */
GQuark
tp_channel_dispatch_operation_get_feature_quark_core (void)
{
  return g_quark_from_static_string (
      "tp-channel-dispatch-operation-feature-core");
}
