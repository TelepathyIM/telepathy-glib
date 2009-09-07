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

#include <telepathy-glib/defs.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#include "telepathy-glib/account-manager.h"
#include "telepathy-glib/_gen/signals-marshal.h"

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
 * Since: 0.7.32
 */

/**
 * TpAccountManagerClass:
 *
 * The class of a #TpAccount.
 */

struct _TpAccountManagerPrivate {
  /* (owned) object path -> (reffed) TpAccount */
  GHashTable *accounts;
  gboolean dispose_run;
  gboolean ready;

  /* global presence */
  TpAccount *global_account;

  TpConnectionPresenceType global_presence;
  gchar *global_status;
  gchar *global_status_message;

  /* requested global presence, could be different
   * from the actual global one. */
  TpConnectionPresenceType requested_presence;
  gchar *requested_status;
  gchar *requested_status_message;

  GHashTable *create_results;
};

#define MC5_BUS_NAME "org.freedesktop.Telepathy.MissionControl5"

enum {
  ACCOUNT_CREATED,
  ACCOUNT_DELETED,
  ACCOUNT_ENABLED,
  ACCOUNT_DISABLED,
  ACCOUNT_CHANGED,
  ACCOUNT_CONNECTION_CHANGED,
  GLOBAL_PRESENCE_CHANGED,
  NEW_CONNECTION,
  LAST_SIGNAL
};

enum {
  PROP_READY = 1,
};

static guint signals[LAST_SIGNAL];
static gpointer manager_singleton = NULL;

G_DEFINE_TYPE (TpAccountManager, tp_account_manager, TP_TYPE_PROXY);

static void
tp_account_manager_init (TpAccountManager *self)
{
  TpAccountManagerPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_ACCOUNT_MANAGER,
      TpAccountManagerPrivate);

  self->priv = priv;

  priv->global_presence = TP_CONNECTION_PRESENCE_TYPE_UNSET;

  priv->accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_object_unref);

  priv->create_results = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
_tp_account_manager_start_mc5 (TpDBusDaemon *bus)
{
  TpProxy *mc5_proxy;

  /* trigger MC5 starting */
  mc5_proxy = g_object_new (TP_TYPE_PROXY,
      "dbus-daemon", bus,
      "dbus-connection", tp_proxy_get_dbus_connection (TP_PROXY (bus)),
      "bus-name", MC5_BUS_NAME,
      "object-path", "/",
      NULL);

  tp_cli_dbus_peer_call_ping (mc5_proxy, -1, NULL, NULL, NULL, NULL);

  g_object_unref (mc5_proxy);
}

static void
_tp_account_manager_name_owner_cb (TpDBusDaemon *proxy,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data)
{
  DEBUG ("Name owner changed for %s, new name: %s", name, new_owner);

  if (new_owner == NULL || new_owner[0] == '\0')
    {
      /* MC5 quit or crashed for some reason, let's start it again */
      _tp_account_manager_start_mc5 (proxy);
      return;
    }
}

static void
_tp_account_manager_ensure_all_accounts (TpAccountManager *manager,
    GPtrArray *accounts)
{
  guint i, missing_accounts;
  GHashTableIter iter;
  TpAccountManagerPrivate *priv = manager->priv;
  gpointer value;
  TpAccount *account;
  gboolean found = FALSE;
  const gchar *name;

  /* ensure all accounts coming from MC5 first */
  for (i = 0; i < accounts->len; i++)
    {
      name = g_ptr_array_index (accounts, i);

      account = tp_account_manager_ensure_account (manager, name);
      tp_account_refresh_properties (account);
    }

  missing_accounts = g_hash_table_size (priv->accounts) - accounts->len;

  if (missing_accounts > 0)
    {
      /* look for accounts we have and the TpAccountManager doesn't,
       * and remove them from our cache. */

      DEBUG ("%d missing accounts", missing_accounts);

      g_hash_table_iter_init (&iter, priv->accounts);

      while (g_hash_table_iter_next (&iter, NULL, &value) && missing_accounts > 0)
        {
          account = value;

          /* look for this account in the AccountManager provided array */
          for (i = 0; i < accounts->len; i++)
            {
              name = g_ptr_array_index (accounts, i);

              if (!tp_strdiff (name, tp_proxy_get_object_path (account)))
                {
                  found = TRUE;
                  break;
                }
            }

          if (!found)
            {
              DEBUG ("Account %s was not found, remove it from the cache",
                  tp_proxy_get_object_path (account));

              g_object_ref (account);
              g_hash_table_iter_remove (&iter);
              g_signal_emit (manager, signals[ACCOUNT_DELETED], 0, account);
              g_object_unref (account);

              missing_accounts--;
            }

          found = FALSE;
        }
    }
}

static void
_tp_account_manager_check_ready (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;
  GHashTableIter iter;
  gpointer value;

  if (priv->ready)
    return;

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *account = TP_ACCOUNT (value);
      gboolean ready;

      g_object_get (account, "ready", &ready, NULL);

      if (!ready)
        return;
    }

  /* Rerequest global presence on the initial set of accounts for cases where a
   * global presence was requested before the manager was ready */
  if (priv->requested_presence != TP_CONNECTION_PRESENCE_TYPE_UNSET)
    {
      tp_account_manager_request_global_presence (manager,
          priv->requested_presence, priv->requested_status,
          priv->requested_status_message);
    }

  priv->ready = TRUE;
  g_object_notify (G_OBJECT (manager), "ready");
}

static void
_tp_account_manager_got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  GPtrArray *accounts;

  if (error != NULL)
    {
      DEBUG ("Failed to get account manager properties: %s", error->message);
      return;
    }

  accounts = tp_asv_get_boxed (properties, "ValidAccounts",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  if (accounts != NULL)
    _tp_account_manager_ensure_all_accounts (manager, accounts);

  _tp_account_manager_check_ready (manager);
}

static void
_tp_account_manager_validity_changed_cb (TpAccountManager *proxy,
    const gchar *path,
    gboolean valid,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);

  if (!valid)
    return;

  tp_account_manager_ensure_account (manager, path);
}

static void
_tp_account_manager_constructed (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_account_manager_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);

  tp_dbus_daemon_watch_name_owner (tp_proxy_get_dbus_daemon (self),
      TP_ACCOUNT_MANAGER_BUS_NAME, _tp_account_manager_name_owner_cb,
      self, NULL);

  tp_cli_account_manager_connect_to_account_validity_changed (self,
      _tp_account_manager_validity_changed_cb, NULL,
      NULL, G_OBJECT (self), NULL);

  tp_cli_dbus_properties_call_get_all (self, -1, TP_IFACE_ACCOUNT_MANAGER,
      _tp_account_manager_got_all_cb, NULL, NULL, G_OBJECT (self));

  _tp_account_manager_start_mc5 (tp_proxy_get_dbus_daemon (self));
}

static void
_tp_account_manager_finalize (GObject *object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (object);
  TpAccountManagerPrivate *priv = manager->priv;

  g_hash_table_destroy (priv->create_results);
  g_hash_table_destroy (priv->accounts);

  g_free (priv->global_status);
  g_free (priv->global_status_message);

  g_free (priv->requested_status);
  g_free (priv->requested_status_message);

  G_OBJECT_CLASS (tp_account_manager_parent_class)->finalize (object);
}

static void
_tp_account_manager_dispose (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  TpAccountManagerPrivate *priv = self->priv;
  GHashTableIter iter;
  GSimpleAsyncResult *result;

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  /* the manager is being destroyed while there are account creation
   * processes pending; this should not happen, but emit the callbacks
   * with an error anyway. */
  g_hash_table_iter_init (&iter, priv->create_results);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &result))
    {
      g_simple_async_result_set_error (result, G_IO_ERROR,
          G_IO_ERROR_CANCELLED, "The account manager was disposed while "
          "creating the account");
      g_simple_async_result_complete (result);
      g_object_unref (result);
    }
  g_hash_table_remove_all (priv->create_results);

  tp_dbus_daemon_cancel_name_owner_watch (tp_proxy_get_dbus_daemon (self),
      TP_ACCOUNT_MANAGER_BUS_NAME, _tp_account_manager_name_owner_cb, self);

  G_OBJECT_CLASS (tp_account_manager_parent_class)->dispose (object);
}

static GObject *
_tp_account_manager_constructor (GType type,
    guint n_construct_params,
    GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (manager_singleton == NULL)
    {
      retval = G_OBJECT_CLASS (tp_account_manager_parent_class)->constructor (
          type, n_construct_params, construct_params);
      manager_singleton = retval;
      g_object_add_weak_pointer (retval, &manager_singleton);
    }
  else
    {
      retval = g_object_ref (G_OBJECT (manager_singleton));
    }

  return retval;
}

static void
_tp_account_manager_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);

  switch (prop_id)
    {
    case PROP_READY:
      g_value_set_boolean (value, self->priv->ready);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tp_account_manager_class_init (TpAccountManagerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpAccountManagerPrivate));

  object_class->constructed = _tp_account_manager_constructed;
  object_class->finalize = _tp_account_manager_finalize;
  object_class->dispose = _tp_account_manager_dispose;
  object_class->constructor = _tp_account_manager_constructor;
  object_class->get_property = _tp_account_manager_get_property;

  proxy_class->interface = TP_IFACE_QUARK_ACCOUNT_MANAGER;
  tp_account_manager_init_known_interfaces ();

  /**
   * TpAccountManager:ready:
   *
   * Initially FALSE; changes to TRUE when every account in the manager is
   * individually ready.
   *
   * Since: 0.7.UNRELEASED
   */
  g_object_class_install_property (object_class, PROP_READY,
      g_param_spec_boolean ("ready",
          "Ready",
          "Whether the initial state dump from the account manager is finished",
          FALSE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccountManager::account-created:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account is created on @manager.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_CREATED] = g_signal_new ("account-created",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-deleted:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account is deleted from @manager.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_DELETED] = g_signal_new ("account-deleted",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-enabled:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account from @manager is enabled.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_ENABLED] = g_signal_new ("account-enabled",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-disabled.
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account from @manager is disabled.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_DISABLED] = g_signal_new ("account-disabled",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-changed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account @manager is changed.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_CHANGED] = g_signal_new ("account-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-connection-changed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   * @reason: the change reason
   * @actual: the actual connection type
   * @previous: the previous connection type
   *
   * Emitted when the connection of an account in @manager changes.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[ACCOUNT_CONNECTION_CHANGED] =
    g_signal_new ("account-connection-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        _tp_marshal_VOID__OBJECT_INT_UINT_UINT,
        G_TYPE_NONE,
        4, TP_TYPE_ACCOUNT,
        G_TYPE_INT,   /* reason */
        G_TYPE_UINT,  /* actual connection */
        G_TYPE_UINT); /* previous connection */

  /**
   * TpAccountManager::global-presence-changed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   * @presence: new presence type
   * @status: new status
   * @message: new status message
   *
   * Emitted when the global presence on @manager changes.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[GLOBAL_PRESENCE_CHANGED] =
    g_signal_new ("global-presence-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL,
        _tp_marshal_VOID__UINT_STRING_STRING,
        G_TYPE_NONE,
        3, G_TYPE_UINT, /* Presence type */
        G_TYPE_STRING,  /* status */
        G_TYPE_STRING); /* stauts message*/

  /**
   * TpAccountManager::new-connection
   * @manager: a #TpAccountManager
   * @account: a #TpConnection
   *
   * Emitted when an account in @manager makes a new connection.
   *
   * Since: 0.7.UNRELEASED
   */
  signals[NEW_CONNECTION] = g_signal_new ("new-connection",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_CONNECTION);
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
 * The returned #TpAccountManager is cached; the same #TpAccountManager object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Returns: a reference to the cached #TpAccountManager object, or a new
 *          instance
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

static void
_tp_account_manager_account_connection_cb (TpAccount *account,
    GParamSpec *spec,
    gpointer manager)
{
  TpConnection *connection = tp_account_get_connection (account);

  DEBUG ("Signalling connection %p of account %s",
      connection, tp_proxy_get_object_path (account));

  if (connection != NULL)
    g_signal_emit (manager, signals[NEW_CONNECTION], 0, connection);
}

static void
_tp_account_manager_account_enabled_cb (TpAccount *account,
    GParamSpec *spec,
    gpointer manager)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (manager);

  if (tp_account_is_enabled (account))
    g_signal_emit (self, signals[ACCOUNT_ENABLED], 0, account);
  else
    g_signal_emit (self, signals[ACCOUNT_DISABLED], 0, account);
}

static void
_tp_account_manager_account_status_changed_cb (TpAccount *account,
    TpConnectionStatus old,
    TpConnectionStatus new,
    TpConnectionStatusReason reason,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);

  g_signal_emit (manager, signals[ACCOUNT_CONNECTION_CHANGED], 0,
      account, reason, new, old);
}

static void
_tp_account_manager_update_global_presence (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;
  TpConnectionPresenceType presence = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
  TpAccount *account = NULL;
  GHashTableIter iter;
  gpointer value;

  /* Make the global presence is equal to the presence of the account with the
   * highest availability */

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *a = TP_ACCOUNT (value);
      TpConnectionPresenceType p;

      g_object_get (a, "presence", &p, NULL);

      if (tp_connection_presence_type_cmp_availability (p, presence) > 0)
        {
          account = a;
          presence = p;
        }
    }

  priv->global_account = account;
  g_free (priv->global_status);
  g_free (priv->global_status_message);

  if (account == NULL)
    {
      priv->global_presence = presence;
      priv->global_status = NULL;
      priv->global_status_message = NULL;
      return;
    }

  g_object_get (account,
      "presence", &priv->global_presence,
      "status", &priv->global_status,
      "status-message", &priv->global_status_message,
      NULL);

  DEBUG ("Updated global presence to: %s (%d) \"%s\"",
      priv->global_status, priv->global_presence, priv->global_status_message);
}

static void
_tp_account_manager_account_presence_changed_cb (TpAccount *account,
    TpConnectionPresenceType presence,
    const gchar *status,
    const gchar *status_message,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;

  if (tp_connection_presence_type_cmp_availability (presence,
          priv->global_presence) > 0)
    {
      priv->global_account = account;

      priv->global_presence = presence;

      g_free (priv->global_status);
      priv->global_status = g_strdup (status);

      g_free (priv->global_status_message);
      priv->global_status_message = g_strdup (status_message);

      goto signal;
    }
  else if (priv->global_account == account)
    {
      _tp_account_manager_update_global_presence (manager);
      goto signal;
    }

  return;
signal:
  g_signal_emit (manager, signals[GLOBAL_PRESENCE_CHANGED], 0,
      priv->global_presence, priv->global_status, priv->global_status_message);
}

static void
_tp_account_manager_account_removed_cb (TpAccount *account,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;

  g_object_ref (account);
  g_hash_table_remove (priv->accounts,
      tp_proxy_get_object_path (account));

  g_signal_emit (manager, signals[ACCOUNT_DELETED], 0, account);
  g_object_unref (account);
}

static void
_tp_account_manager_account_ready_cb (GObject *object,
    GParamSpec *spec,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account = TP_ACCOUNT (object);
  GSimpleAsyncResult *result;
  gboolean ready;

  g_object_get (account, "ready", &ready, NULL);

  if (!ready)
    return;

  /* see if there's any pending callbacks for this account */
  result = g_hash_table_lookup (priv->create_results, account);
  if (result != NULL)
    {
      g_simple_async_result_set_op_res_gpointer (
          G_SIMPLE_ASYNC_RESULT (result), account, NULL);

      g_simple_async_result_complete (result);

      g_hash_table_remove (priv->create_results, account);
      g_object_unref (result);
    }

  g_signal_emit (manager, signals[ACCOUNT_CREATED], 0, account);

  g_signal_connect (account, "notify::connection",
      G_CALLBACK (_tp_account_manager_account_connection_cb), manager);

  g_signal_connect (account, "notify::enabled",
      G_CALLBACK (_tp_account_manager_account_enabled_cb), manager);

  g_signal_connect (account, "status-changed",
      G_CALLBACK (_tp_account_manager_account_status_changed_cb), manager);

  g_signal_connect (account, "presence-changed",
      G_CALLBACK (_tp_account_manager_account_presence_changed_cb), manager);

  g_signal_connect (account, "removed",
      G_CALLBACK (_tp_account_manager_account_removed_cb), manager);

  _tp_account_manager_check_ready (manager);
}

/**
 * tp_account_manager_get_account:
 * @manager: a #TpAccountManager
 * @path: the object path for an account
 *
 * Lookup an #TpAccount in the account manager @manager. If the desired account
 * has already been ensured then the same object will be returned, otherwise
 * %NULL will be returned.
 *
 * Returns: the desired #TpAccount, or %NULL if no such account has been
 *          ensured
 *
 * Since: 0.7.UNRELEASED
 */
TpAccount *
tp_account_manager_get_account (TpAccountManager *manager,
    const gchar *path)
{
  TpAccountManagerPrivate *priv = manager->priv;

  return g_hash_table_lookup (priv->accounts, path);
}

/**
 * tp_account_manager_ensure_account:
 * @manager: a #TpAccountManager
 * @path: the object path for an account
 *
 * Lookup an account in the account manager *manager. If the desired account
 * has already been ensured then the same object will be returned, otherwise
 * %NULL will be returned.
 *
 * The caller must keep a ref to the returned object using g_object_ref() if
 * it is to be kept.
 *
 * Returns: a new #TpAccount at @path
 *
 * Since: 0.7.UNRELEASED
 */
TpAccount *
tp_account_manager_ensure_account (TpAccountManager *manager,
    const gchar *path)
{
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account;

  account = g_hash_table_lookup (priv->accounts, path);
  if (account != NULL)
    return account;

  account = tp_account_new (tp_proxy_get_dbus_daemon (manager), path, NULL);
  g_hash_table_insert (priv->accounts, g_strdup (path), account);

  g_signal_connect (account, "notify::ready",
      G_CALLBACK (_tp_account_manager_account_ready_cb), manager);

  return account;
}

/**
 * tp_account_manager_is_ready:
 * @manager: a #TpAccountManager
 *
 * <!-- -->
 *
 * Returns: %TRUE if the @manager and all its accounts are ready, otherwise
 *          %FALSE
 *
 * Since: 0.7.UNRELEASED
 */
gboolean
tp_account_manager_is_ready (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;

  return priv->ready;
}

/**
 * tp_account_manager_get_account_for_connection:
 * @manager: a #TpAccountManager
 * @connection: a #TpConnection
 *
 * Looks up what #TpAccount @connection belongs to, and returns it. If
 * no appropriate #TpAccount is found, %NULL is returned.
 *
 * Returns: the #TpAccount that @connection belongs to, otherwise %NULL
 *
 * Since: 0.7.UNRELEASED
 */
TpAccount *
tp_account_manager_get_account_for_connection (TpAccountManager *manager,
    TpConnection *connection)
{
  TpAccountManagerPrivate *priv;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), 0);

  priv = manager->priv;

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *account = TP_ACCOUNT (value);

      if (connection == tp_account_get_connection (account))
        return account;
    }

  return NULL;
}

/**
 * tp_account_manager_get_accounts:
 * @manager: a #TpAccountManager
 *
 * Returns a newly allocated #GList of accounts in @manager. The list must be
 * freed with g_list_free() after used.
 *
 * Note that the #TpAccount<!-- -->s in the returned #GList are not reffed
 * before returning from this function. One could ref every item in the list
 * like the following example:
 * |[
 * GList *accounts;
 * account = tp_account_manager_get_accounts (manager);
 * g_list_foreach (accounts, (GFunc) g_object_ref, NULL);
 * ]|
 *
 * Returns: a newly allocated #GList of accounts in @manager
 *
 * Since: 0.7.UNRELEASED
 */
GList *
tp_account_manager_get_accounts (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv;
  GList *ret;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  priv = manager->priv;

  ret = g_hash_table_get_values (priv->accounts);

  return ret;
}

/**
 * tp_account_manager_request_global_presence:
 * @manager: a #TpAccountManager
 * @type: a presence type to request
 * @status: a status to request
 * @message: a status message to request
 *
 * Requests a global presence among all accounts in @manager. Note that
 * the presence requested here is merely a request, and if might not be
 * satisfiable.
 *
 * You can find the actual global presence across all accounts by calling
 * tp_account_manager_get_global_presence().
 *
 * Since: 0.7.UNRELEASED
 */
void
tp_account_manager_request_global_presence (TpAccountManager *manager,
    TpConnectionPresenceType type,
    const gchar *status,
    const gchar *message)
{
  TpAccountManagerPrivate *priv = manager->priv;
  GHashTableIter iter;
  gpointer value;

  DEBUG ("request global presence, type: %d, status: %s, message: %s",
      type, status, message);

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *account = TP_ACCOUNT (value);
      gboolean ready;

      g_object_get (account, "ready", &ready, NULL);

      if (ready)
        tp_account_request_presence_async (account, type, status, message,
            NULL, NULL);
    }

  /* save the requested global presence, to use it in case we create
   * new accounts or some accounts become ready. */
  priv->requested_presence = type;

  if (tp_strdiff (priv->requested_status, status))
    {
      g_free (priv->requested_status);
      priv->requested_status = g_strdup (status);
    }

  if (tp_strdiff (priv->requested_status_message, message))
    {
      g_free (priv->requested_status_message);
      priv->requested_status_message = g_strdup (message);
    }
}

/**
 * tp_account_manager_get_requested_global_presence:
 * @manager: a #TpAccountManager
 * @status: a string to fill with the requested status
 * @message: a string to fill with the requested status message
 *
 * <!-- -->
 *
 * Returns: the requested global presence
 *
 * Since: 0.7.UNRELEASED
 */
TpConnectionPresenceType
tp_account_manager_get_requested_global_presence (TpAccountManager *manager,
    gchar **status,
    gchar **message)
{
  TpAccountManagerPrivate *priv = manager->priv;

  if (status != NULL)
    *status = g_strdup (priv->requested_status);

  if (message != NULL)
    *message = g_strdup (priv->requested_status_message);

  return priv->requested_presence;
}

/**
 * tp_account_manager_get_global_presence:
 * @manager: a #TpAccountManager
 * @status: a string to fill with the actual status
 * @message: a string to fill with the actual status message
 *
 * <!-- -->
 *
 * Returns: the actual global presence
 *
 * Since: 0.7.UNRELEASED
 */

TpConnectionPresenceType
tp_account_manager_get_global_presence (TpAccountManager *manager,
    gchar **status,
    gchar **message)
{
  TpAccountManagerPrivate *priv = manager->priv;

  if (status != NULL)
    *status = g_strdup (priv->global_status);

  if (message != NULL)
    *message = g_strdup (priv->global_status_message);

  return priv->global_presence;
}

static void
_tp_account_manager_created_cb (TpAccountManager *proxy,
    const gchar *account_path,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  TpAccountManagerPrivate *priv = manager->priv;
  GSimpleAsyncResult *my_res = user_data;
  TpAccount *account;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (my_res,
          (GError *) error);
      g_simple_async_result_complete (my_res);
      g_object_unref (my_res);

      return;
    }

  account = tp_account_manager_ensure_account (manager, account_path);

  g_hash_table_insert (priv->create_results, account, my_res);
}

/**
 * tp_account_manager_create_account_async:
 * @manager: a #TpAccountManager
 * @connection_manager: the name of a connection manager
 * @protocol: the name of a protocol
 * @display_name: the display name for the account
 * @parameters: parameters for the new account
 * @properties: properties for the new account
 * @callback: a callback to bcall when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous create of an account on the account manager
 * @manager. When the operation is finished, @callback will be called. You can
 * then call tp_account_manager_create_account_finish() to get the result of
 * the operation.
 *
 * Since: 0.7.UNRELEASED
 */
void
tp_account_manager_create_account_async (TpAccountManager *manager,
    const gchar *connection_manager,
    const gchar *protocol,
    const gchar *display_name,
    GHashTable *parameters,
    GHashTable *properties,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (manager), callback, user_data,
      tp_account_manager_create_account_finish);

  tp_cli_account_manager_call_create_account (manager,
      -1, connection_manager, protocol, display_name, parameters,
      properties, _tp_account_manager_created_cb, res, NULL,
      G_OBJECT (manager));
}

/**
 * tp_account_manager_create_account_finish:
 * @manager: a #TpAccountManager
 * @result: a #GAsyncResult
 * @error: a #GError to be filled
 *
 * Finishes an async create account operation.
 *
 * Returns: %TRUE if the reconnect call was successful, otherwise %FALSE
 *
 * Since: 0.7.UNRELEASED
 */
TpAccount *
tp_account_manager_create_account_finish (TpAccountManager *manager,
    GAsyncResult *result,
    GError **error)
{
  TpAccount *retval;

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (manager), tp_account_manager_create_account_finish), NULL);

  retval = TP_ACCOUNT (g_simple_async_result_get_op_res_gpointer (
          G_SIMPLE_ASYNC_RESULT (result)));

  return retval;
}

