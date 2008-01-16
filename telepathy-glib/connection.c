/*
 * connection.c - proxy for a Telepathy connection
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#include "telepathy-glib/connection.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"

#include "_gen/tp-cli-connection-body.h"

/**
 * SECTION:connection
 * @title: TpConnection
 * @short_description: proxy object for a Telepathy connection
 * @see_also: #TpConnectionManager, #TpChannel
 *
 * #TpConnection objects represent Telepathy instant messaging connections
 * accessed via D-Bus.
 *
 * Since: 0.7.1
 */

/**
 * TP_ERRORS_DISCONNECTED:
 *
 * #GError domain representing a Telepathy connection becoming disconnected.
 * The @code in a #GError with this domain must be a member of
 * #TpConnectionStatusReason.
 *
 * This macro expands to a function call returning a #GQuark.
 */
GQuark
tp_errors_disconnected_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string ("tp_errors_disconnected_quark");

  return q;
}

/**
 * TP_UNKNOWN_CONNECTION_STATUS:
 *
 * An invalid connection status used in #TpConnection to indicate that the
 * status has not yet been discovered.
 */

/**
 * TpConnectionClass:
 * @parent_class: the parent class
 * @priv: pointer to opaque private data
 *
 * The class of a #TpConnection.
 */
struct _TpConnectionClass {
    TpProxyClass parent_class;
    gpointer priv;
};

/**
 * TpConnection:
 * @parent: the parent class instance
 * @status: same as #TpConnection:status, should be considered read-only
 * @status_reason: same as #TpConnection:status-reason, should be considered
 *  read-only
 * @_reserved_for_self_handle: reserved, currently always 0
 * @ready: the same as #TpChannel:channel-ready; should be considered
 *  read-only
 * @_reserved_flags: (private, reserved for future use)
 * @priv: pointer to opaque private data
 *
 * A proxy object for a Telepathy connection.
 */
struct _TpConnection {
    TpProxy parent;

    TpConnectionStatus status;
    TpConnectionStatusReason status_reason;
    TpHandle _reserved_for_self_handle;

    gboolean ready:1;
    gboolean _reserved_flags:31;

    TpConnectionPrivate *priv;
};

typedef void (*TpConnectionProc) (TpConnection *self);

struct _TpConnectionPrivate {
    /* GArray of TpConnectionProc */
    GArray *introspect_needed;

    TpConnectionAliasFlags alias_flags;
    /* other stuff could go here */
};

enum
{
  PROP_STATUS = 1,
  PROP_STATUS_REASON,
  PROP_CONNECTION_READY,
  N_PROPS
};

G_DEFINE_TYPE (TpConnection,
    tp_connection,
    TP_TYPE_PROXY);

static void
tp_connection_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  TpConnection *self = TP_CONNECTION (object);

  switch (property_id)
    {
    case PROP_CONNECTION_READY:
      g_value_set_boolean (value, self->ready);
      break;
    case PROP_STATUS:
      g_value_set_uint (value, self->status);
      break;
    case PROP_STATUS_REASON:
      g_value_set_uint (value, self->status_reason);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_connection_continue_introspection (TpConnection *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  if (self->priv->introspect_needed->len == 0)
    {
      g_array_free (self->priv->introspect_needed, TRUE);
      self->priv->introspect_needed = NULL;

      DEBUG ("%p: connection ready", self);
      self->ready = TRUE;
      g_object_notify ((GObject *) self, "connection-ready");
    }
  else
    {
      guint i = self->priv->introspect_needed->len - 1;
      TpConnectionProc next = g_array_index (self->priv->introspect_needed,
          TpConnectionProc, i);

      g_array_remove_index (self->priv->introspect_needed, i);
      next (self);
    }
}

static void
got_aliasing_flags (TpConnection *self,
                    guint flags,
                    const GError *error,
                    gpointer user_data,
                    GObject *weak_object)
{
  if (error == NULL)
    {
      DEBUG ("Introspected aliasing flags: 0x%x", flags);
      self->priv->alias_flags = flags;
    }
  else
    {
      DEBUG ("GetAliasFlags(): %s", error->message);
    }

  tp_connection_continue_introspection (self);
}

static void
introspect_aliasing (TpConnection *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  tp_cli_connection_interface_aliasing_call_get_alias_flags
      (self, -1, got_aliasing_flags, NULL, NULL, NULL);
}

static void
tp_connection_got_interfaces_cb (TpConnection *self,
                                 const gchar **interfaces,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *user_object)
{
  if (error != NULL)
    {
      DEBUG ("%p: GetInterfaces() failed, assuming no interfaces: %s",
          self, error->message);
      interfaces = NULL;
    }

  DEBUG ("%p: Introspected interfaces", self);
  if (interfaces != NULL)
    {
      const gchar **iter;

      g_assert (self->priv->introspect_needed == NULL);
      self->priv->introspect_needed = g_array_new (FALSE, FALSE,
          sizeof (TpConnectionProc));

      for (iter = interfaces; *iter != NULL; iter++)
        {
          if (tp_dbus_check_valid_interface_name (*iter, NULL))
            {
              GQuark q = g_quark_from_string (*iter);

              tp_proxy_add_interface_by_id ((TpProxy *) self,
                  g_quark_from_string (*iter));

              if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_ALIASING)
                {
                  /* call GetAliasFlags */
                  TpConnectionProc func = introspect_aliasing;

                  g_array_append_val (self->priv->introspect_needed,
                      func);
                }
#if 0
              else if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS)
                {
                  /* call GetAvatarRequirements */
                  TpConnectionProc func = introspect_avatars;

                  g_array_append_val (self->priv->introspect_needed,
                      func);
                }
              else if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_PRESENCE)
                {
                  /* call GetStatuses */
                  TpConnectionProc func = introspect_presence;

                  g_array_append_val (self->priv->introspect_needed,
                      func);
                }
              /* if Privacy was stable, we'd also queue GetPrivacyModes
               * here */
#endif
            }
          else
            {
              DEBUG ("\t\tInterface %s not valid", *iter);
            }
        }
    }

  tp_connection_continue_introspection (self);
}

static void
tp_connection_status_changed (TpConnection *self,
                              guint status,
                              guint reason)
{
  DEBUG ("%p: %d -> %d because %d", self, self->status, status, reason);

  self->status = status;
  self->status_reason = reason;
  g_object_notify ((GObject *) self, "status");
  g_object_notify ((GObject *) self, "status-reason");

  if (status == TP_CONNECTION_STATUS_CONNECTED)
    {
      tp_cli_connection_call_get_interfaces (self, -1,
          tp_connection_got_interfaces_cb, NULL, NULL, NULL);
    }
}

static void
tp_connection_status_changed_cb (TpConnection *self,
                                 guint status,
                                 guint reason,
                                 gpointer user_data,
                                 GObject *weak_object)
{
  tp_connection_status_changed (self, status, reason);

  /* we only want to run this in response to a StatusChanged signal,
   * not if the initial status is DISCONNECTED */

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      GError *error = g_error_new (TP_ERRORS_DISCONNECTED, reason,
          "Disconnected: reason %d", reason);

      tp_proxy_invalidate ((TpProxy *) self, error);
      g_error_free (error);
    }
}

static void
tp_connection_got_status_cb (TpConnection *self,
                             guint status,
                             const GError *error,
                             gpointer unused,
                             GObject *user_object)
{
  DEBUG ("%p", self);

  if (error == NULL)
    {
      DEBUG ("%p: Initial status is %d", self, status);
      tp_connection_status_changed (self, status,
          TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED);
    }
  else
    {
      DEBUG ("%p: GetStatus() failed with %s %d \"%s\"",
          self, g_quark_to_string (error->domain), error->code,
          error->message);
    }
}

static GObject *
tp_connection_constructor (GType type,
                           guint n_params,
                           GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_connection_parent_class;
  TpConnection *self = TP_CONNECTION (object_class->constructor (type,
        n_params, params));

  /* Connect to my own StatusChanged signal.
   * The connection hasn't had a chance to become invalid yet, so we can
   * assume that this signal connection will work */
  DEBUG ("Connecting to StatusChanged");
  tp_cli_connection_connect_to_status_changed (self,
      tp_connection_status_changed_cb, NULL, NULL, NULL, NULL);

  /* get my initial status */
  DEBUG ("Calling GetStatus");
  tp_cli_connection_call_get_status (self, -1,
      tp_connection_got_status_cb, NULL, NULL, NULL);

  DEBUG ("Returning %p", self);
  return (GObject *) self;
}

static void
tp_connection_init (TpConnection *self)
{
  DEBUG ("%p", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CONNECTION,
      TpConnectionPrivate);

  self->status = TP_UNKNOWN_CONNECTION_STATUS;
  self->status_reason = TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
}

static void
tp_connection_dispose (GObject *object)
{
  DEBUG ("%p", object);

  ((GObjectClass *) tp_connection_parent_class)->dispose (object);
}

static void
tp_connection_class_init (TpConnectionClass *klass)
{
  GType tp_type = TP_TYPE_CONNECTION;
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpConnectionPrivate));

  object_class->constructor = tp_connection_constructor;
  object_class->get_property = tp_connection_get_property;
  object_class->dispose = tp_connection_dispose;

  proxy_class->interface = TP_IFACE_QUARK_CONNECTION;
  proxy_class->must_have_unique_name = TRUE;
  tp_proxy_or_subclass_hook_on_interface_add (tp_type,
      tp_cli_connection_add_signals);
  tp_proxy_subclass_add_error_mapping (tp_type,
      TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

  /**
   * TpConnection:status:
   *
   * This connection's status, or TP_UNKNOWN_CONNECTION_STATUS if we don't
   * know yet.
   */
  param_spec = g_param_spec_uint ("status", "Status",
      "The status of this connection", 0, G_MAXUINT32,
      TP_UNKNOWN_CONNECTION_STATUS,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_STATUS,
      param_spec);

  /**
   * TpConnection:status-reason:
   *
   * The reason why #TpConnection:status changed to its current value,
   * or TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED if unknown.
   * know yet.
   */
  param_spec = g_param_spec_uint ("status-reason", "Last status change reason",
      "The reason why #TpConnection:status changed to its current value",
      0, G_MAXUINT32, TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_STATUS_REASON,
      param_spec);

  /**
   * TpConnection:connection-ready:
   *
   * Initially %FALSE; changes to %TRUE when the connection has gone to
   * CONNECTED status, introspection has finished and it's ready for use.
   *
   * By the time this property becomes %TRUE, any extra interfaces will
   * have been set up and the #TpProxy:interfaces property will have been
   * populated.
   */
  param_spec = g_param_spec_boolean ("connection-ready", "Connection ready?",
      "Initially FALSE; changes to TRUE when introspection finishes", FALSE,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CONNECTION_READY,
      param_spec);
}

/**
 * tp_connection_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @bus_name: the well-known or unique name of the connection process;
 *  if well-known, this function will make a blocking call to the bus daemon
 *  to resolve the unique name. May be %NULL if @object_path is not, in which
 *  case a well-known name will be derived from @object_path.
 * @object_path: the object path of the connection process. May be %NULL
 *  if @bus_name is a well-known name, in which case the object path will
 *  be derived from @bus_name.
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new connection proxy, or %NULL if unique-name resolution
 *  fails or on invalid arguments
 */
TpConnection *
tp_connection_new (TpDBusDaemon *dbus,
                   const gchar *bus_name,
                   const gchar *object_path,
                   GError **error)
{
  gchar *dup_path = NULL;
  gchar *dup_name = NULL;
  gchar *dup_unique_name = NULL;
  TpConnection *ret = NULL;

  g_return_val_if_fail (dbus != NULL, NULL);
  g_return_val_if_fail (object_path != NULL ||
                        (bus_name != NULL && bus_name[0] != ':'), NULL);

  if (object_path == NULL)
    {
      dup_path = g_strdelimit (g_strdup_printf ("/%s", bus_name), ".", '/');
      object_path = dup_path;
    }
  else if (bus_name == NULL)
    {
      dup_name = g_strdelimit (g_strdup (object_path + 1), "/", '.');
      bus_name = dup_name;
    }

  if (!tp_dbus_check_valid_bus_name (bus_name,
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, error))
    goto finally;

  /* Resolve unique name if necessary */
  if (bus_name[0] != ':')
    {
      if (!_tp_dbus_daemon_get_name_owner (dbus, 2000, bus_name,
          &dup_unique_name, error))
        goto finally;

      bus_name = dup_unique_name;

      if (!tp_dbus_check_valid_bus_name (bus_name,
          TP_DBUS_NAME_TYPE_UNIQUE, error))
        goto finally;
    }

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  ret = TP_CONNECTION (g_object_new (TP_TYPE_CONNECTION,
        "dbus-daemon", dbus,
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

finally:
  g_free (dup_path);
  g_free (dup_name);
  g_free (dup_unique_name);

  return ret;
}

/**
 * tp_connection_run_until_ready:
 * @self: a connection
 * @connect: if %TRUE, call Connect() if it appears to be necessary;
 *  if %FALSE, rely on Connect() to be called by another client
 * @error: if not %NULL and %FALSE is returned, used to raise an error
 * @loop: if not %NULL, a #GMainLoop is placed here while it is being run
 *  (so calling code can call g_main_loop_quit() to abort), and %NULL is
 *  placed here after the loop has been run
 *
 * If @self is connected and ready for use, return immediately. Otherwise,
 * call Connect() (unless @connect is %FALSE) and re-enter the main loop
 * until the connection becomes invalid, the connection connects successfully
 * and is introspected, or the main loop stored via @loop is cancelled.
 *
 * Returns: %TRUE if the connection is now connected and ready for use,
 *  %FALSE if the connection has become invalid.
 */

typedef struct {
    GMainLoop *loop;
    TpProxyPendingCall *pc;
    GError *connect_error;
} RunUntilReadyData;

static void
run_until_ready_ret (TpConnection *self,
                     const GError *error,
                     gpointer user_data,
                     GObject *weak_object)
{
  RunUntilReadyData *data = user_data;

  if (error != NULL)
    {
      g_main_loop_quit (data->loop);
      data->connect_error = g_error_copy (error);
    }
}

static void
run_until_ready_destroy (gpointer p)
{
  RunUntilReadyData *data = p;

  data->pc = NULL;
}

gboolean
tp_connection_run_until_ready (TpConnection *self,
                               gboolean connect,
                               GError **error,
                               GMainLoop **loop)
{
  TpProxy *as_proxy = (TpProxy *) self;
  gulong invalidated_id, ready_id;
  RunUntilReadyData data = { NULL, NULL, NULL };

  if (as_proxy->invalidated)
    goto raise_invalidated;

  if (self->ready)
    return TRUE;

  data.loop = g_main_loop_new (NULL, FALSE);

  invalidated_id = g_signal_connect_swapped (self, "invalidated",
      G_CALLBACK (g_main_loop_quit), data.loop);
  ready_id = g_signal_connect_swapped (self, "notify::connection-ready",
      G_CALLBACK (g_main_loop_quit), data.loop);

  if (self->status != TP_CONNECTION_STATUS_CONNECTED &&
      connect)
    {
      data.pc = tp_cli_connection_call_connect (self, -1,
          run_until_ready_ret, &data,
          run_until_ready_destroy, NULL);
    }

  if (data.connect_error == NULL)
    {
      if (loop != NULL)
        *loop = data.loop;

      g_main_loop_run (data.loop);

      if (loop != NULL)
        *loop = NULL;
    }

  if (data.pc != NULL)
    tp_proxy_pending_call_cancel (data.pc);

  g_signal_handler_disconnect (self, invalidated_id);
  g_signal_handler_disconnect (self, ready_id);
  g_main_loop_unref (data.loop);

  if (data.connect_error != NULL)
    {
      g_propagate_error (error, data.connect_error);
      return FALSE;
    }

  if (as_proxy->invalidated != NULL)
    goto raise_invalidated;

  if (self->ready)
    return TRUE;

  g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_CANCELLED,
      "tp_connection_run_until_ready() cancelled");
  return FALSE;

raise_invalidated:
  if (error != NULL)
    {
      g_return_val_if_fail (*error == NULL, FALSE);
      *error = g_error_copy (as_proxy->invalidated);
    }

  return FALSE;
}
