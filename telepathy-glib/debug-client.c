/*
 * debug-client.c - proxy for Telepathy debug objects
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include <telepathy-glib/debug-client.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_DEBUGGER
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-debug-body.h"

/**
 * SECTION:debug-client
 * @title: TpDebugClient
 * @short_description: proxy objects for Telepathy debug information
 * @see_also: #TpProxy
 *
 * This module provides access to the auxiliary objects used to
 * implement #TpSvcDebug.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpDebugClientClass:
 *
 * The class of a #TpDebugClient.
 *
 * Since: 0.UNRELEASED
 */
struct _TpDebugClientClass {
    TpProxyClass parent_class;
    /*<private>*/
    gpointer priv;
};

/**
 * TpDebugClient:
 *
 * A proxy object for a Telepathy connection manager.
 *
 * Since: 0.7.1
 */
struct _TpDebugClient {
    TpProxy parent;
    /*<private>*/
    TpDebugClientPrivate *priv;
};

static void name_owner_changed_cb (TpDBusDaemon *bus,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data);

G_DEFINE_TYPE (TpDebugClient, tp_debug_client, TP_TYPE_PROXY)

static void
tp_debug_client_init (TpDebugClient *self)
{
}

static void
tp_debug_client_constructed (GObject *object)
{
  TpProxy *proxy = TP_PROXY (object);

  tp_dbus_daemon_watch_name_owner (
      tp_proxy_get_dbus_daemon (proxy), tp_proxy_get_bus_name (proxy),
      name_owner_changed_cb, object, NULL);
}

static void
tp_debug_client_dispose (GObject *object)
{
  TpProxy *proxy = TP_PROXY (object);

  tp_dbus_daemon_cancel_name_owner_watch (
      tp_proxy_get_dbus_daemon (proxy), tp_proxy_get_bus_name (proxy),
      name_owner_changed_cb, object);
  G_OBJECT_CLASS (tp_debug_client_parent_class)->dispose (object);
}

static void
tp_debug_client_class_init (TpDebugClientClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;

  object_class->constructed = tp_debug_client_constructed;
  object_class->dispose = tp_debug_client_dispose;

  proxy_class->must_have_unique_name = TRUE;
  proxy_class->interface = TP_IFACE_QUARK_DEBUG;
  tp_debug_client_init_known_interfaces ();
}

static void
name_owner_changed_cb (
    TpDBusDaemon *bus,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data)
{
  TpDebugClient *self = TP_DEBUG_CLIENT (user_data);

  if (tp_str_empty (new_owner))
    {
      GError *error = g_error_new (TP_DBUS_ERRORS,
          TP_DBUS_ERROR_NAME_OWNER_LOST,
          "%s fell off the bus", name);

      DEBUG ("%s fell off the bus", name);
      tp_proxy_invalidate (TP_PROXY (self), error);
      g_error_free (error);
    }
}

/**
 * tp_debug_client_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpDebugClient have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_DEBUG_CLIENT.
 *
 * Since: 0.UNRELEASED
 */
void
tp_debug_client_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_DEBUG_CLIENT;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_debug_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

/**
 * tp_debug_client_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @unique_name: the unique name of the process to be debugged; may not be
 *  %NULL or a well-known name
 *
 * <!-- -->
 *
 * Returns: a new debug client proxy, or %NULL on invalid arguments
 *
 * Since: 0.UNRELEASED
 */
TpDebugClient *
tp_debug_client_new (
    TpDBusDaemon *dbus,
    const gchar *unique_name,
    GError **error)
{
  if (!tp_dbus_check_valid_bus_name (unique_name,
          TP_DBUS_NAME_TYPE_UNIQUE, error))
    return NULL;

  return TP_DEBUG_CLIENT (g_object_new (TP_TYPE_DEBUG_CLIENT,
      "dbus-daemon", dbus,
      "bus-name", unique_name,
      "object-path", TP_DEBUG_OBJECT_PATH,
      NULL));
}
