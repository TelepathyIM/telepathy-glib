/*
 * account-manager.c - proxy for the Telepathy account manager
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

#include "telepathy-glib/account-manager.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-account-manager-body.h"

/**
 * SECTION:account-manager
 * @title: TpAccountManager
 * @short_description: proxy object for the Telepathy account manager
 * @see_also: #TpAccount
 *
 * The #TpAccountManager object is used to communicate with the Telepathy
 * AccountManager service.
 *
 * Since: 0.7.32
 */

/**
 * TpAccountManager:
 *
 * The Telepathy Account Manager stores real-time communication accounts and
 * their configuration, places accounts online on request, and manipulates
 * accounts' presence, nicknames and avatars.
 *
 * This proxy is usable but incomplete: GObject signals and accessors for the
 * D-Bus properties will be added in a later version of telepathy-glib, along
 * with a mechanism similar to tp_connection_call_when_ready().
 *
 * Until suitable convenience methods are implemented, the generic
 * tp_cli_dbus_properties_call_get_all() method can be used to get the D-Bus
 * properties.
 *
 * Since: 0.7.32
 */

/**
 * TpAccountManagerClass:
 *
 * The class of a #TpAccount.
 */

struct _TpAccountManagerPrivate {
    gpointer dummy;
};

G_DEFINE_TYPE (TpAccountManager, tp_account_manager, TP_TYPE_PROXY);

static void
tp_account_manager_init (TpAccountManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_ACCOUNT_MANAGER,
      TpAccountManagerPrivate);
}

static void
tp_account_manager_constructed (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_account_manager_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);
}

static void
tp_account_manager_class_init (TpAccountManagerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpAccountManagerPrivate));

  object_class->constructed = tp_account_manager_constructed;

  proxy_class->interface = TP_IFACE_QUARK_ACCOUNT_MANAGER;
  tp_account_manager_init_known_interfaces ();
}

/**
 * tp_account_manager_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpAccountManager have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_ACCOUNT_MANAGER.
 *
 * Since: 0.7.32
 */
void
tp_account_manager_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType tp_type = TP_TYPE_ACCOUNT_MANAGER;

      tp_proxy_init_known_interfaces ();
      tp_proxy_or_subclass_hook_on_interface_add (tp_type,
          tp_cli_account_manager_add_signals);
      tp_proxy_subclass_add_error_mapping (tp_type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

/**
 * tp_account_manager_new:
 * @bus_daemon: Proxy for the D-Bus daemon
 *
 * Convenience function to create a new account manager proxy.
 *
 * Returns: a new reference to an account manager proxy
 */
TpAccountManager *
tp_account_manager_new (TpDBusDaemon *bus_daemon)
{
  TpAccountManager *self;

  g_return_val_if_fail (bus_daemon != NULL, NULL);

  self = TP_ACCOUNT_MANAGER (g_object_new (TP_TYPE_ACCOUNT_MANAGER,
        "dbus-daemon", bus_daemon,
        "dbus-connection", ((TpProxy *) bus_daemon)->dbus_connection,
        "bus-name", TP_ACCOUNT_MANAGER_BUS_NAME,
        "object-path", TP_ACCOUNT_MANAGER_OBJECT_PATH,
        NULL));

  return self;
}
