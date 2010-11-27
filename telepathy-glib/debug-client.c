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
#include "telepathy-glib/proxy-internal.h"

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

struct _TpDebugClientPrivate {
    gboolean enabled;
};

static const TpProxyFeature *tp_debug_client_list_features (TpProxyClass *klass);
static void tp_debug_client_prepare_core (TpDebugClient *self);
static void name_owner_changed_cb (TpDBusDaemon *bus,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data);

G_DEFINE_TYPE (TpDebugClient, tp_debug_client, TP_TYPE_PROXY)

static void
tp_debug_client_init (TpDebugClient *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_DEBUG_CLIENT,
      TpDebugClientPrivate);
}

static void
tp_debug_client_constructed (GObject *object)
{
  TpDebugClient *self = TP_DEBUG_CLIENT (object);
  TpProxy *proxy = TP_PROXY (object);
  GObjectClass *parent_class = G_OBJECT_CLASS (tp_debug_client_parent_class);

  if (parent_class->constructed != NULL)
    parent_class->constructed (object);

  tp_dbus_daemon_watch_name_owner (
      tp_proxy_get_dbus_daemon (proxy), tp_proxy_get_bus_name (proxy),
      name_owner_changed_cb, object, NULL);
  tp_debug_client_prepare_core (self);
}

static void
tp_debug_client_dispose (GObject *object)
{
  TpProxy *proxy = TP_PROXY (object);
  GObjectClass *parent_class = G_OBJECT_CLASS (tp_debug_client_parent_class);

  tp_dbus_daemon_cancel_name_owner_watch (
      tp_proxy_get_dbus_daemon (proxy), tp_proxy_get_bus_name (proxy),
      name_owner_changed_cb, object);

  if (parent_class->dispose != NULL)
    parent_class->dispose (object);
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
  proxy_class->list_features = tp_debug_client_list_features;

  g_type_class_add_private (klass, sizeof (TpDebugClientPrivate));
  tp_debug_client_init_known_interfaces ();
}

GQuark
tp_debug_client_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-debug-client-feature-core");
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

static void
got_enabled_cb (
    TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpDebugClient *self = TP_DEBUG_CLIENT (proxy);

  if (error != NULL)
    {
      tp_proxy_invalidate (proxy, error);
    }
  else if (!G_VALUE_HOLDS_BOOLEAN (value))
    {
      GError *e = g_error_new (TP_ERRORS,
          TP_ERROR_NOT_IMPLEMENTED,
          "this service doesn't implement the Debug interface correctly "
          "(the Enabled property is not a boolean, but a %s)",
          G_VALUE_TYPE_NAME (value));

      tp_proxy_invalidate (proxy, e);
      g_error_free (e);
    }
  else
    {
      self->priv->enabled = g_value_get_boolean (value);
      /* FIXME: we have no change notification for Enabled. */
      _tp_proxy_set_feature_prepared (proxy, TP_DEBUG_CLIENT_FEATURE_CORE,
          TRUE);
    }
}

static void
tp_debug_client_prepare_core (TpDebugClient *self)
{
  tp_cli_dbus_properties_call_get (self, -1, TP_IFACE_DEBUG, "Enabled",
      got_enabled_cb, NULL, NULL, NULL);
}

static const TpProxyFeature *
tp_debug_client_list_features (TpProxyClass *klass)
{
  static gsize once = 0;
  static TpProxyFeature features[] = {
      { 0, TRUE },
      { 0 }
  };

  if (g_once_init_enter (&once))
    {
      features[0].name = TP_DEBUG_CLIENT_FEATURE_CORE;
      g_once_init_leave (&once, 1);
    }

  return features;
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
