/*
 * connection.c - proxy for a Telepathy connection
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#include <string.h>

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

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
 * Compared with a simple proxy for method calls, they add the following
 * features:
 *
 * * connection status tracking
 *
 * * calling GetInterfaces() automatically
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
 *
 * Since: 0.7.1
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
 *
 * Since: 0.7.1
 */

/**
 * TpConnectionClass:
 * @parent_class: the parent class
 *
 * The class of a #TpConnection. In addition to @parent_class there are four
 * pointers reserved for possible future use.
 *
 * Since: 0.7.1; structure layout visible since 0.7.12
 */

/**
 * TpConnection:
 * @parent: the parent class instance
 * @priv: pointer to opaque private data
 *
 * A proxy object for a Telepathy connection.
 *
 * Since: 0.7.1; structure layout visible since 0.7.12
 */

typedef void (*TpConnectionProc) (TpConnection *self);

struct _TpConnectionPrivate {
    /* GArray of TpConnectionProc */
    GArray *introspect_needed;

    TpConnectionStatus status;
    TpConnectionStatusReason status_reason;
    TpConnectionAliasFlags alias_flags;

    gboolean ready:1;
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
      g_value_set_boolean (value, self->priv->ready);
      break;
    case PROP_STATUS:
      g_value_set_uint (value, self->priv->status);
      break;
    case PROP_STATUS_REASON:
      g_value_set_uint (value, self->priv->status_reason);
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
      self->priv->ready = TRUE;
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

  g_assert (self->priv->introspect_needed == NULL);
  self->priv->introspect_needed = g_array_new (FALSE, FALSE,
      sizeof (TpConnectionProc));

  if (interfaces != NULL)
    {
      const gchar **iter;

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

  /* FIXME: give subclasses a chance to influence the definition of "ready"
   * now that we have our interfaces? */

  tp_connection_continue_introspection (self);
}

static void
tp_connection_status_changed (TpConnection *self,
                              guint status,
                              guint reason)
{
  DEBUG ("%p: %d -> %d because %d", self, self->priv->status, status, reason);

  self->priv->status = status;
  self->priv->status_reason = reason;
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
  /* GetStatus is called in the TpConnection constructor. If we don't have the
   * reply for this GetStatus call yet, ignore this signal StatusChanged in
   * order to run the interface introspection only one time. We will get the
   * GetStatus reply later anyway. */
  if (self->priv->status != TP_UNKNOWN_CONNECTION_STATUS)
    {
      tp_connection_status_changed (self, status, reason);
    }

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

  self->priv->status = TP_UNKNOWN_CONNECTION_STATUS;
  self->priv->status_reason = TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
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
  /* If you change this, you must also change TpChannel to stop asserting
   * that its connection has a unique name */
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
 *
 * Since: 0.7.1
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
 * tp_connection_get_status:
 * @self: a connection
 * @reason: a TpConnectionStatusReason, or %NULL
 *
 * If @reason is not %NULL it is set to the reason why "status" changed to its
 * current value, or %TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED if unknown.
 *
 * Returns: This connection's status, or %TP_UNKNOWN_CONNECTION_STATUS if we
 * don't know yet.
 *
 * Since: 0.7.14
 */
TpConnectionStatus
tp_connection_get_status (TpConnection *self,
                          TpConnectionStatusReason *reason)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), TP_UNKNOWN_CONNECTION_STATUS);

  if (reason != NULL)
    *reason = self->priv->status_reason;

  return self->priv->status;
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
 *
 * Since: 0.7.1
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

  if (self->priv->ready)
    return TRUE;

  data.loop = g_main_loop_new (NULL, FALSE);

  invalidated_id = g_signal_connect_swapped (self, "invalidated",
      G_CALLBACK (g_main_loop_quit), data.loop);
  ready_id = g_signal_connect_swapped (self, "notify::connection-ready",
      G_CALLBACK (g_main_loop_quit), data.loop);

  if (self->priv->status != TP_CONNECTION_STATUS_CONNECTED &&
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

  if (self->priv->ready)
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

/**
 * TpConnectionNameListCb:
 * @names: %NULL-terminated array of connection bus names,
 *   or %NULL on error
 * @n: number of names (not including the final %NULL), or 0 on error
 * @cms: %NULL-terminated array of connection manager names in the same order
 *   as @names, or %NULL on error
 * @protocols: %NULL-terminated array of protocol names in the same order
 *   as @names, or %NULL on error
 * @error: %NULL on success, or an error that occurred
 * @user_data: user-supplied data
 * @weak_object: user-supplied weakly referenced object
 *
 * Signature of the callback supplied to tp_list_connection_managers().
 *
 * Since: 0.7.1
 */

typedef struct {
    TpConnectionNameListCb callback;
    gpointer user_data;
    GDestroyNotify destroy;
    size_t base_len;
} _ListContext;

static void
tp_list_connection_names_helper (TpDBusDaemon *bus_daemon,
                                 const gchar **names,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *user_object)
{
  _ListContext *list_context = user_data;
  const gchar **iter;
  /* array of borrowed strings */
  GPtrArray *bus_names;
  /* array of dup'd strings */
  GPtrArray *cms;
  /* array of borrowed strings */
  GPtrArray *protocols;

  if (error != NULL)
    {
      list_context->callback (NULL, 0, NULL, NULL, error,
          list_context->user_data, user_object);
      return;
    }

  bus_names = g_ptr_array_new ();
  cms = g_ptr_array_new ();
  protocols = g_ptr_array_new ();

  for (iter = names; iter != NULL && *iter != NULL; iter++)
    {
      gchar *dup, *proto, *dot;

      if (strncmp (TP_CONN_BUS_NAME_BASE, *iter, list_context->base_len) != 0)
        continue;

      dup = g_strdup (*iter + list_context->base_len);
      dot = strchr (dup, '.');

      if (dot == NULL)
        goto invalid;

      *dot = '\0';

      if (!tp_connection_manager_check_valid_name (dup, NULL))
        goto invalid;

      proto = dot + 1;
      dot = strchr (proto, '.');

      if (dot == NULL)
        goto invalid;

      *dot = '\0';

      if (!tp_strdiff (proto, "local_2dxmpp"))
        {
          /* the CM's telepathy-glib is older than 0.7.x, work around it.
           * FIXME: Remove this workaround in 0.9.x */
          proto = "local-xmpp";
        }
      else
        {
          /* the real protocol name may have "-" in; bus names may not, but
           * they may have "_", so the Telepathy spec specifies replacement.
           * Here we need to undo that replacement */
          g_strdelimit (proto, "_", '-');
        }

      if (!tp_connection_manager_check_valid_protocol_name (proto, NULL))
        {
          goto invalid;
        }

      /* the casts here are because g_ptr_array contains non-const pointers -
       * but in this case I'll only be passing pdata to a callback with const
       * arguments, so it's fine */
      g_ptr_array_add (bus_names, (gpointer) *iter);
      g_ptr_array_add (cms, dup);
      g_ptr_array_add (protocols, (gpointer) proto);

      continue;

invalid:
      DEBUG ("invalid name: %s", *iter);
      g_free (dup);
    }

  g_ptr_array_add (bus_names, NULL);
  g_ptr_array_add (cms, NULL);
  g_ptr_array_add (protocols, NULL);

  list_context->callback ((const gchar * const *) bus_names->pdata,
      bus_names->len - 1, (const gchar * const *) cms->pdata,
      (const gchar * const *) protocols->pdata,
      NULL, list_context->user_data, user_object);

  g_ptr_array_free (bus_names, TRUE);
  g_strfreev ((char **) g_ptr_array_free (cms, FALSE));
  g_ptr_array_free (protocols, TRUE);
}

static void
list_context_free (gpointer p)
{
  _ListContext *list_context = p;

  if (list_context->destroy != NULL)
    list_context->destroy (list_context->user_data);

  g_slice_free (_ListContext, list_context);
}

/**
 * tp_list_connection_names:
 * @bus_daemon: proxy for the D-Bus daemon
 * @callback: callback to be called when listing the connections succeeds or
 *   fails; not called if the D-Bus connection fails completely or if the
 *   @weak_object goes away
 * @user_data: user-supplied data for the callback
 * @destroy: callback to destroy the user-supplied data, called after
 *   @callback, but also if the D-Bus connection fails or if the @weak_object
 *   goes away
 * @weak_object: if not %NULL, will be weakly referenced; the callback will
 *   not be called, and the call will be cancelled, if the object has vanished
 *
 * List the available (running or installed) connection managers. Call the
 * callback when done.
 *
 * Since: 0.7.1
 */
void
tp_list_connection_names (TpDBusDaemon *bus_daemon,
                          TpConnectionNameListCb callback,
                          gpointer user_data,
                          GDestroyNotify destroy,
                          GObject *weak_object)
{
  _ListContext *list_context = g_slice_new0 (_ListContext);

  list_context->base_len = strlen (TP_CONN_BUS_NAME_BASE);
  list_context->callback = callback;
  list_context->user_data = user_data;

  tp_cli_dbus_daemon_call_list_names (bus_daemon, 2000,
      tp_list_connection_names_helper, list_context,
      list_context_free, weak_object);
}

static gpointer
tp_connection_once (gpointer data G_GNUC_UNUSED)
{
  GType type = TP_TYPE_CONNECTION;

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (type,
      tp_cli_connection_add_signals);
  tp_proxy_subclass_add_error_mapping (type,
      TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

  return NULL;
}

/**
 * tp_connection_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpConnection have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CONNECTION.
 *
 * Since: 0.7.6
 */
void
tp_connection_init_known_interfaces (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tp_connection_once, NULL);
}

typedef struct {
    TpConnectionWhenReadyCb callback;
    gpointer user_data;
    gulong invalidated_id;
    gulong ready_id;
} CallWhenReadyContext;

static void
cwr_invalidated (TpConnection *self,
                 guint domain,
                 gint code,
                 gchar *message,
                 gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;
  GError e = { domain, code, message };

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, &e, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

static void
cwr_ready (TpConnection *self,
           GParamSpec *unused G_GNUC_UNUSED,
           gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, NULL, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

/**
 * TpConnectionWhenReadyCb:
 * @connection: the connection (which may be in the middle of being disposed,
 *  if error is non-%NULL, error->domain is TP_DBUS_ERRORS and error->code is
 *  TP_DBUS_ERROR_PROXY_UNREFERENCED)
 * @error: %NULL if the connection is ready for use, or the error with which
 *  it was invalidated if it is now invalid
 * @user_data: whatever was passed to tp_connection_call_when_ready()
 *
 * Signature of a callback passed to tp_connection_call_when_ready(), which
 * will be called exactly once, when the connection becomes ready or
 * invalid (whichever happens first)
 */

/**
 * tp_connection_call_when_ready:
 * @self: a connection
 * @callback: called when the connection becomes ready or invalidated,
 *  whichever happens first
 * @user_data: arbitrary user-supplied data passed to the callback
 *
 * If @self is ready for use or has been invalidated, call @callback
 * immediately, then return. Otherwise, arrange
 * for @callback to be called when @self either becomes ready for use
 * or becomes invalid.
 *
 * Note that if the connection is not in state CONNECTED, the callback will
 * not be called until the connection either goes to state CONNECTED
 * or is invalidated (e.g. by going to state DISCONNECTED or by becoming
 * unreferenced). In particular, this method does not call Connect().
 * Call tp_cli_connection_call_connect() too, if you want to do that.
 *
 * Since: 0.7.7
 */
void
tp_connection_call_when_ready (TpConnection *self,
                               TpConnectionWhenReadyCb callback,
                               gpointer user_data)
{
  TpProxy *as_proxy = (TpProxy *) self;

  g_return_if_fail (callback != NULL);

  if (self->priv->ready || as_proxy->invalidated != NULL)
    {
      DEBUG ("already ready or invalidated");
      callback (self, as_proxy->invalidated, user_data);
    }
  else
    {
      CallWhenReadyContext *ctx = g_slice_new (CallWhenReadyContext);

      DEBUG ("arranging callback later");

      ctx->callback = callback;
      ctx->user_data = user_data;
      ctx->invalidated_id = g_signal_connect (self, "invalidated",
          G_CALLBACK (cwr_invalidated), ctx);
      ctx->ready_id = g_signal_connect (self, "notify::connection-ready",
          G_CALLBACK (cwr_ready), ctx);
    }
}
