/*
 * connection-manager.c - proxy for a Telepathy connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#include "telepathy-glib/connection-manager.h"

#include "telepathy-glib/defs.h"
#include "telepathy-glib/gtypes.h"

#define DEBUG_FLAG TP_DEBUG_MANAGER
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/proxy-internal.h"

/**
 * SECTION:connection-manager
 * @title: TpConnectionManager
 * @short_description: proxy object for a Telepathy connection manager
 * @see_also: #TpConnection
 *
 * #TpConnectionManager objects represent Telepathy connection managers. They
 * can be used to open connections.
 */

/**
 * TpConnectionManagerClass:
 *
 * The class of a #TpConnectionManager.
 */
struct _TpConnectionManagerClass {
    TpProxyClass parent_class;
    /*<private>*/
};

enum
{
  SIGNAL_ACTIVATED,
  SIGNAL_DISCOVERED,
  SIGNAL_EXITED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

/**
 * TpConnectionManager:
 *
 * A proxy object for a Telepathy connection manager.
 *
 * This might represent a connection manager which is currently running
 * (in which case it can be introspected) or not (in which case its
 * capabilities can be read from .manager files in the filesystem).
 * Accordingly, this object never emits #TpProxy::destroyed unless all
 * references to it are discarded.
 */
struct _TpConnectionManager {
    TpProxy parent;
    /*<private>*/

    /* GPtrArray of TpConnectionManagerProtocol * */
    GPtrArray *protocols;

    gboolean running:1;
    gboolean listing_protocols:1;

    /* GPtrArray of gchar * */
    GPtrArray *introspect_pending;
    /* GPtrArray of TpConnectionManagerProtocol * */
    GPtrArray *introspect_protocols;
};

G_DEFINE_TYPE (TpConnectionManager,
    tp_connection_manager,
    TP_TYPE_PROXY);

static void tp_connection_manager_continue_introspection
    (TpConnectionManager *self);

static void
tp_connection_manager_got_parameters (TpProxy *proxy,
                                      const GPtrArray *parameters,
                                      const GError *error,
                                      gpointer user_data)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (proxy);
  gchar *protocol = user_data;
  GArray *output;
  guint i;
  TpConnectionManagerProtocol *proto_struct;

  DEBUG ("Protocol name: %s", protocol);

  if (error != NULL)
    tp_connection_manager_continue_introspection (self);

   output = g_array_sized_new (TRUE, TRUE,
      sizeof (TpConnectionManagerParam), parameters->len);

  for (i = 0; i < parameters->len; i++)
    {
      GValue structure = { 0 };
      GValue *tmp;
      TpConnectionManagerParam *param = &g_array_index (output,
          TpConnectionManagerParam, output->len + 1);

      g_value_init (&structure, TP_STRUCT_TYPE_PARAM_SPEC);
      g_value_set_static_boxed (&structure, g_ptr_array_index (parameters, i));

      g_array_set_size (output, output->len + 1);

      if (!dbus_g_type_struct_get (&structure,
            0, &param->name,
            1, &param->flags,
            2, &param->dbus_signature,
            3, &tmp,
            G_MAXUINT))
        {
          DEBUG ("Bad parameter, ignoring");
          /* *shrug* that one didn't work, let's skip it */
          g_array_set_size (output, output->len - 1);
        }

      g_value_init (&param->default_value,
          G_VALUE_TYPE (tmp));
      g_value_copy (tmp, &param->default_value);
      g_value_unset (tmp);
      g_free (tmp);

      param->priv = NULL;

      DEBUG ("\tParam name: %s", param->name);
      DEBUG ("\tParam flags: 0x%x", param->flags);
      DEBUG ("\tParam sig: %s", param->dbus_signature);

#ifdef ENABLE_DEBUG
        {
          gchar *repr = g_strdup_value_contents (&(param->default_value));

          DEBUG ("\tParam default value: %s of type %s", repr,
              G_VALUE_TYPE_NAME (&(param->default_value)));
          g_free (repr);
        }
#endif
    }

  proto_struct = g_slice_new (TpConnectionManagerProtocol);
  proto_struct->name = g_strdup (protocol);
  proto_struct->params =
      (TpConnectionManagerParam *) g_array_free (output, FALSE);
  g_ptr_array_add (self->introspect_protocols, proto_struct);

  tp_connection_manager_continue_introspection (self);
}

static void
tp_connection_manager_continue_introspection (TpConnectionManager *self)
{
  gchar *next_protocol;

  if (self->introspect_pending->len == 0)
    {
      g_ptr_array_free (self->introspect_pending, TRUE);

      if (self->protocols != NULL)
        {
          guint i;

          for (i = 0; i < self->protocols->len; i++)
            {
              TpConnectionManagerProtocol *proto =
                  g_ptr_array_index (self->protocols, i);

              g_free (proto->name);
              g_free (proto->params);
            }
        }
      g_ptr_array_add (self->introspect_protocols, NULL);

      self->protocols = self->introspect_protocols;
      self->introspect_protocols = NULL;

      g_signal_emit (self, signals[SIGNAL_DISCOVERED], 0, TRUE);
      return;
    }

  next_protocol = g_ptr_array_remove_index_fast (self->introspect_pending, 0);
  tp_cli_connection_manager_call_get_parameters (self, -1, next_protocol,
      tp_connection_manager_got_parameters, next_protocol, g_free);
}

static void
tp_connection_manager_got_protocols (TpProxy *proxy,
                                     const gchar **protocols,
                                     const GError *error,
                                     gpointer user_data)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (proxy);
  guint i = 0;
  const gchar **iter;

  self->listing_protocols = FALSE;

  if (error != NULL)
    return;

  g_assert (self->introspect_protocols == NULL);
  self->introspect_protocols = g_ptr_array_new ();

  for (iter = protocols; *iter != NULL; iter++)
    i++;

  g_assert (self->introspect_pending == NULL);
  self->introspect_pending = g_ptr_array_sized_new (i);

  for (iter = protocols; *iter != NULL; iter++)
    {
      g_ptr_array_add (self->introspect_pending, g_strdup (*iter));
    }

  tp_connection_manager_continue_introspection (self);
}

static void
tp_connection_manager_name_owner_changed_cb (TpDBusDaemon *bus,
                                             const gchar *name,
                                             const gchar *new_owner,
                                             gpointer user_data)
{
  TpConnectionManager *self = user_data;

  if (new_owner[0] == '\0')
    {
      self->running = FALSE;
      self->listing_protocols = FALSE;

      if (self->introspect_pending != NULL)
        {
          g_strfreev ((gchar **) g_ptr_array_free (self->introspect_pending,
                FALSE));
          self->introspect_pending = NULL;
        }

      if (self->introspect_protocols != NULL)
        {
          guint i;

          for (i = 0; i < self->introspect_protocols->len; i++)
            {
              TpConnectionManagerProtocol *proto =
                  g_ptr_array_index (self->introspect_protocols, i);

              g_free (proto->name);
              g_free (proto->params);
            }

          g_ptr_array_free (self->introspect_protocols, TRUE);
          self->introspect_protocols = NULL;
        }

      g_signal_emit (self, signals[SIGNAL_EXITED], 0);
    }
  else
    {
      /* represent an atomic change of ownership as if it was an exit and
       * restart */
      if (self->running)
        tp_connection_manager_name_owner_changed_cb (bus, name, "", self);

      self->running = TRUE;
      g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);

      /* Start introspecting if we're not already */
      if (!self->listing_protocols)
        {
          tp_cli_connection_manager_call_list_protocols (self, -1,
              tp_connection_manager_got_protocols, g_object_ref (self),
              g_object_unref);
        }
    }
}

static GObject *
tp_connection_manager_constructor (GType type,
                                   guint n_params,
                                   GObjectConstructParam *params)
{
  GObjectClass *object_class =
      (GObjectClass *) tp_connection_manager_parent_class;
  TpConnectionManager *self =
      TP_CONNECTION_MANAGER (object_class->constructor (type, n_params,
            params));
  TpProxy *as_proxy = (TpProxy *) self;

  /* Watch my D-Bus name */
  tp_dbus_daemon_watch_name_owner (TP_DBUS_DAEMON (as_proxy->dbus_daemon),
      as_proxy->bus_name, tp_connection_manager_name_owner_changed_cb, self,
      NULL);

  return (GObject *) self;
}

static void
tp_connection_manager_init (TpConnectionManager *self)
{
}

static void
tp_connection_manager_dispose (GObject *object)
{
  TpProxy *as_proxy = TP_PROXY (object);

  tp_dbus_daemon_cancel_name_owner_watch (
      TP_DBUS_DAEMON (as_proxy->dbus_daemon), as_proxy->bus_name,
      tp_connection_manager_name_owner_changed_cb, object);

  G_OBJECT_CLASS (tp_connection_manager_parent_class)->dispose (object);
}

static void
tp_connection_manager_class_init (TpConnectionManagerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructor = tp_connection_manager_constructor;
  object_class->dispose = tp_connection_manager_dispose;

  proxy_class->interface = TP_IFACE_QUARK_CONNECTION_MANAGER;
  proxy_class->on_interface_added = g_slist_prepend
      (proxy_class->on_interface_added, tp_cli_connection_manager_add_signals);

  /**
   * TpConnectionManager::activated:
   * @self: the connection manager proxy
   *
   * Emitted when the connection manager's well-known name appears on the bus.
   */
  signals[SIGNAL_ACTIVATED] = g_signal_new ("activated",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * TpConnectionManager::exited:
   * @self: the connection manager proxy
   *
   * Emitted when the connection manager's well-known name disappears from
   * the bus or when activation fails.
   */
  signals[SIGNAL_EXITED] = g_signal_new ("exited",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * TpConnectionManager::discovered:
   * @self: the connection manager proxy
   * @live: %TRUE if the CM is actually running, %FALSE if its capabilities
   *  were discovered from a .manager file
   *
   * Emitted when the connection manager's capabilities have been discovered.
   */
  signals[SIGNAL_DISCOVERED] = g_signal_new ("discovered",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__BOOLEAN,
      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/**
 * tp_connection_manager_new:
 * @dbus: Proxy for the D-Bus daemon
 * @name: The connection manager name
 *
 * Convenience function to create a new connection manager proxy.
 *
 * Returns: a new reference to a connection manager proxy
 */
TpConnectionManager *
tp_connection_manager_new (TpDBusDaemon *dbus,
                           const gchar *name)
{
  TpConnectionManager *cm;
  gchar *object_path, *bus_name;

  g_return_val_if_fail (dbus != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  object_path = g_strdup_printf ("%s%s", TP_CM_OBJECT_PATH_BASE, name);
  bus_name = g_strdup_printf ("%s%s", TP_CM_BUS_NAME_BASE, name);

  cm = TP_CONNECTION_MANAGER (g_object_new (TP_TYPE_CONNECTION_MANAGER,
        "dbus-daemon", dbus,
        "dbus-connection", ((TpProxy *) dbus)->dbus_connection,
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

  g_free (object_path);
  g_free (bus_name);

  return cm;
}

/**
 * tp_connection_manager_activate:
 *
 * Attempt to run and introspect the connection manager, asynchronously.
 *
 * If the CM was already running, do nothing and return %FALSE.
 *
 * On success, emit #TpConnectionManager::activated when the CM appears
 * on the bus, and #TpConnectionManager::discovered when its capabilities
 * have been (re-)discovered.
 *
 * On failure, emit #TpConnectionManager::exited without first emitting
 * activated.
 *
 * Returns: %TRUE if activation was needed and is now in progress, %FALSE
 *  if the connection manager was already running and no additional signals
 *  will be emitted.
 */
gboolean
tp_connection_manager_activate (TpConnectionManager *self)
{
  if (self->running)
    return FALSE;

  self->listing_protocols = TRUE;
  tp_cli_connection_manager_call_list_protocols (self, -1,
      tp_connection_manager_got_protocols,
      g_object_ref (self), g_object_unref);

  return TRUE;
}
