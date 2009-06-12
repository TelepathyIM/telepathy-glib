/*
 * account.c - proxy for an account in the Telepathy account manager
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

#include "telepathy-glib/account.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-account-body.h"

/**
 * SECTION:account
 * @title: TpAccount
 * @short_description: proxy object for an account in the Telepathy account
 *  manager
 * @see_also: #TpAccountManager
 *
 * The Telepathy Account Manager stores the user's configured real-time
 * communication accounts. The #TpAccount object represents a stored account.
 *
 * Since: 0.7.32
 */

/**
 * TpAccount:
 *
 * The Telepathy Account Manager stores the user's configured real-time
 * communication accounts. This object represents a stored account.
 *
 * If this account is deleted from the account manager, the
 * #TpProxy::invalidated signal will be emitted
 * with the domain %TP_DBUS_ERRORS and the error code
 * %TP_DBUS_ERROR_OBJECT_REMOVED.
 *
 * This proxy is usable but very incomplete: accessors for the
 * Account's D-Bus properties will be added in a later version of
 * telepathy-glib, along with a mechanism similar to
 * tp_connection_call_when_ready().
 *
 * Most operations performed on an Account are done via D-Bus properties.
 * Until convenience methods for this are implemented, use of the generic
 * tp_cli_dbus_properties_call_get_all() and tp_cli_dbus_properties_call_set()
 * methods is recommended.
 *
 * Other useful auto-generated method wrappers on an Account include
 * tp_cli_account_call_remove(), tp_cli_account_call_update_parameters() and
 * tp_cli_account_call_reconnect().
 *
 * Since: 0.7.32
 */

/**
 * TpAccountClass:
 *
 * The class of a #TpAccount.
 */

struct _TpAccountPrivate {
    gpointer dummy;
};

G_DEFINE_TYPE (TpAccount, tp_account, TP_TYPE_PROXY);

static void
tp_account_init (TpAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_ACCOUNT,
      TpAccountPrivate);
}

static void
tp_account_removed_cb (TpAccount *self,
    gpointer unused G_GNUC_UNUSED,
    GObject *object G_GNUC_UNUSED)
{
  GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
      "Account removed" };

  tp_proxy_invalidate ((TpProxy *) self, &e);
}

static void
tp_account_constructed (GObject *object)
{
  TpAccount *self = TP_ACCOUNT (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_account_parent_class)->constructed;
  GError *error = NULL;
  TpProxySignalConnection *sc;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);

  sc = tp_cli_account_connect_to_removed (self, tp_account_removed_cb,
      NULL, NULL, NULL, &error);

  if (sc == NULL)
    {
      g_critical ("Couldn't connect to Removed: %s", error->message);
      g_error_free (error);
      g_assert_not_reached ();
      return;
    }
}

static void
tp_account_class_init (TpAccountClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpAccountPrivate));

  object_class->constructed = tp_account_constructed;

  proxy_class->interface = TP_IFACE_QUARK_ACCOUNT;
  tp_account_init_known_interfaces ();
}

/**
 * tp_account_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpAccount have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_ACCOUNT.
 *
 * Since: 0.7.32
 */
void
tp_account_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_ACCOUNT;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_account_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

/**
 * tp_account_new:
 * @bus_daemon: Proxy for the D-Bus daemon
 * @object_path: The non-NULL object path of this account
 * @error: Used to raise an error if @object_path is not valid
 *
 * Convenience function to create a new account proxy.
 *
 * Returns: a new reference to an account proxy, or %NULL if @object_path is
 *    not valid
 */
TpAccount *
tp_account_new (TpDBusDaemon *bus_daemon,
    const gchar *object_path,
    GError **error)
{
  TpAccount *self;

  g_return_val_if_fail (bus_daemon != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  if (!g_str_has_prefix (object_path, TP_ACCOUNT_OBJECT_PATH_BASE))
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Account path does not start with the right prefix: %s",
          object_path);
      return NULL;
    }

  self = TP_ACCOUNT (g_object_new (TP_TYPE_ACCOUNT,
        "dbus-daemon", bus_daemon,
        "dbus-connection", ((TpProxy *) bus_daemon)->dbus_connection,
        "bus-name", TP_ACCOUNT_MANAGER_BUS_NAME,
        "object-path", object_path,
        NULL));

  return self;
}
