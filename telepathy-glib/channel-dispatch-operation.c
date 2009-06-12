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

#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

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
    gpointer dummy;
};

G_DEFINE_TYPE (TpChannelDispatchOperation, tp_channel_dispatch_operation,
    TP_TYPE_PROXY);

static void
tp_channel_dispatch_operation_init (TpChannelDispatchOperation *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_CHANNEL_DISPATCH_OPERATION, TpChannelDispatchOperationPrivate);
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
      g_critical ("Couldn't connect to Finished: %s", error->message);
      g_error_free (error);
      g_assert_not_reached ();
      return;
    }
}

static void
tp_channel_dispatch_operation_class_init (TpChannelDispatchOperationClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpChannelDispatchOperationPrivate));

  object_class->constructed = tp_channel_dispatch_operation_constructed;

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
    GHashTable *immutable_properties G_GNUC_UNUSED,
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
        NULL));

  g_free (unique_name);

  return self;
}
