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

#include "config.h"

#include "telepathy-glib/account-manager-internal.h"
#include "telepathy-glib/account-internal.h"

#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/variant-util.h>

#include "telepathy-glib/account-manager.h"

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/proxy-internal.h"
#include "telepathy-glib/client-factory-internal.h"

/**
 * SECTION:account-manager
 * @title: TpAccountManager
 * @short_description: proxy object for the Telepathy account manager
 * @see_also: #TpAccount
 *
 * The #TpAccountManager object is used to communicate with the Telepathy
 * AccountManager service.
 *
 * A new #TpAccountManager object can be created with
 * tp_account_manager_dup().
 *
 * To list the existing valid accounts, the client should first
 * prepare the %TP_ACCOUNT_MANAGER_FEATURE_CORE feature using
 * tp_proxy_prepare_async(), then call
 * tp_account_manager_dup_usable_accounts().
 *
 * The #TpAccountManager::account-validity-changed signal is emitted
 * to notify of the validity of an account changing. New accounts are
 * also indicated by the emission of this signal on an account that
 * did not previously exist. (The rationale behind indicating new
 * accounts by an account validity change signal is that clients
 * interested in this kind of thing should be connected to this signal
 * anyway: an account having just become valid is effectively a new
 * account to a client.)
 *
 * The #TpAccountManager::account-removed signal is emitted when
 * existing accounts are removed.
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
 * #TpAccountManager is the "top level" object. Since 0.16 it always has a
 * non-%NULL #TpProxy:factory, and its #TpProxy:factory will be
 * propagated to all other objects like #TpAccountManager -> #TpAccount ->
 * #TpConnection -> #TpContact and #TpChannel. This means that desired features
 * set on that factory will be prepared on all those objects.
 * If a #TpProxy:factory is not specified when the #TpAccountManager is
 * constructed, it will use a #TpAutomaticClientFactory.
 *
 * <example id="account-manager"><title>TpAccountManager example</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../examples/client/contact-list.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
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

  /* most available presence */
  TpAccount *most_available_account;

  TpConnectionPresenceType most_available_presence;
  gchar *most_available_status;
  gchar *most_available_status_message;

  /* requested presence, could be different
   * from the actual one. */
  TpConnectionPresenceType requested_presence;
  gchar *requested_status;
  gchar *requested_status_message;

  guint n_preparing_accounts;
  guint watch_name_id;
};

typedef struct {
  GQuark name;
  gboolean ready;
} TpAccountManagerFeature;

typedef struct {
  GSimpleAsyncResult *result;
  GArray *features;
} TpAccountManagerFeatureCallback;

enum {
  ACCOUNT_USABILITY_CHANGED,
  ACCOUNT_REMOVED,
  ACCOUNT_ENABLED,
  ACCOUNT_DISABLED,
  MOST_AVAILABLE_PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (TpAccountManager, tp_account_manager, TP_TYPE_PROXY)

/**
 * TP_ACCOUNT_MANAGER_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core" feature
 * on a #TpAccountManager.
 *
 * When this feature is prepared, the list of accounts have been retrieved and
 * are available for use, and change-notification has been set up.
 * Additionally, since 0.16 the #TpAccount objects returned by
 * tp_account_manager_dup_usable_accounts() have their
 * features prepared as configured by the #TpProxy:factory; in particular,
 * they will all have %TP_ACCOUNT_FEATURE_CORE.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.9.0
 */

/**
 * tp_account_manager_get_feature_quark_core:
 *
 * <!-- -->
 *
 * Returns: the quark used for representing the core feature of a
 *          #TpAccountManager
 *
 * Since: 0.9.0
 */
GQuark
tp_account_manager_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-account-manager-feature-core");
}

enum {
    FEAT_CORE,
    N_FEAT
};

static const TpProxyFeature *
_tp_account_manager_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_UNLIKELY (features[0].name == 0))
    {
      features[FEAT_CORE].name = TP_ACCOUNT_MANAGER_FEATURE_CORE;
      features[FEAT_CORE].core = TRUE;
      /* no need for a prepare_async function - the constructor starts it */

      /* assert that the terminator at the end is there */
      g_assert (features[N_FEAT].name == 0);
    }

  return features;
}

static void
tp_account_manager_init (TpAccountManager *self)
{
  TpAccountManagerPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_ACCOUNT_MANAGER,
      TpAccountManagerPrivate);

  self->priv = priv;

  priv->most_available_presence = TP_CONNECTION_PRESENCE_TYPE_UNSET;

  priv->accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_object_unref);
}

static void
_tp_account_manager_start (TpAccountManager *self)
{
  tp_cli_dbus_peer_call_ping (self, -1, NULL, NULL, NULL, NULL);
}

static void
name_vanished_cb (GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
  /* AM quit or crashed for some reason, let's start it again */
  _tp_account_manager_start (user_data);
}

static void insert_account (TpAccountManager *self, TpAccount *account);

static void
usability_changed_account_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpAccountManager *self = user_data;
  TpAccount *account = (TpAccount *) object;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    {
      DEBUG ("Error preparing account: %s", error->message);
      g_clear_error (&error);
      goto OUT;
    }

  /* Account could have been invalidated while we were preparing it */
  if (tp_account_is_usable (account) &&
      tp_proxy_get_invalidated (account) == NULL)
    {
      insert_account (self, account);
      g_signal_emit (self, signals[ACCOUNT_USABILITY_CHANGED], 0,
          account, TRUE);
    }

OUT:
  g_object_unref (self);
}

static void
_tp_account_manager_usability_changed_cb (TpAccountManager *proxy,
    const gchar *path,
    gboolean usable,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account;
  GArray *features;
  GError *error = NULL;

  if (!usable)
    {
      /* If account became unusable, but we didn't have it anyway, ignore. */
      account = g_hash_table_lookup (priv->accounts, path);
      if (account == NULL)
        return;

      g_object_ref (account);
      g_hash_table_remove (priv->accounts, path);

      g_signal_emit (manager, signals[ACCOUNT_USABILITY_CHANGED], 0,
          account, FALSE);

      g_object_unref (account);

      return;
    }

  account = tp_client_factory_ensure_account (
      tp_proxy_get_factory (manager), path, NULL, &error);
  if (account == NULL)
    {
      DEBUG ("failed to create TpAccount: %s", error->message);
      g_clear_error (&error);
      return;
    }

  /* Delay signal emission until until account is prepared */
  features = tp_client_factory_dup_account_features (
      tp_proxy_get_factory (manager), account);

  tp_proxy_prepare_async (account, (GQuark *) features->data,
      usability_changed_account_prepared_cb, g_object_ref (manager));

  g_array_unref (features);
  g_object_unref (account);
}

static void
_tp_account_manager_update_most_available_presence (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;
  TpConnectionPresenceType presence = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
  TpAccount *account = NULL;
  GHashTableIter iter;
  gpointer value;
  TpAccount *has_unset_presence = NULL;

  /* this presence is equal to the presence of the account with the
   * highest availability */

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *a = TP_ACCOUNT (value);
      TpConnectionPresenceType p;

      p = tp_account_get_current_presence (a, NULL, NULL);

      if (p == TP_CONNECTION_PRESENCE_TYPE_UNSET)
        {
          has_unset_presence = a;
          /* There is no point comparing the presence as UNSET is the
           * 'smallest' presence of all */
          continue;
        }

      if (tp_connection_presence_type_cmp_availability (p, presence) > 0)
        {
          account = a;
          presence = p;
        }
    }

  if (presence == TP_CONNECTION_PRESENCE_TYPE_OFFLINE &&
      has_unset_presence != NULL)
    {
      /* Use an account having UNSET as presence as the 'best' one,
       * see tp_account_manager_get_most_available_presence() */
      account = has_unset_presence;
    }

  priv->most_available_account = account;
  g_free (priv->most_available_status);
  g_free (priv->most_available_status_message);

  if (account == NULL)
    {
      priv->most_available_presence = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
      priv->most_available_status = g_strdup ("offline");
      priv->most_available_status_message = g_strdup ("");
      return;
    }

  priv->most_available_presence = tp_account_get_current_presence (account,
      &(priv->most_available_status), &(priv->most_available_status_message));

  DEBUG ("Updated most available presence to: %s (%d) \"%s\"",
      priv->most_available_status, priv->most_available_presence,
      priv->most_available_status_message);
}

static void
_tp_account_manager_check_core_ready (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;

  DEBUG ("manager has %d accounts left to prepare",
    priv->n_preparing_accounts);
  if (tp_proxy_is_prepared (manager, TP_ACCOUNT_MANAGER_FEATURE_CORE))
    return;

  if (priv->n_preparing_accounts > 0)
    return;

  /* Rerequest most available presence on the initial set of accounts for cases
   * where a most available presence was requested before the manager was ready
   */
  if (priv->requested_presence != TP_CONNECTION_PRESENCE_TYPE_UNSET)
    {
      tp_account_manager_set_all_requested_presences (manager,
          priv->requested_presence, priv->requested_status,
          priv->requested_status_message);
    }

  _tp_account_manager_update_most_available_presence (manager);

  _tp_proxy_set_feature_prepared ((TpProxy *) manager,
      TP_ACCOUNT_MANAGER_FEATURE_CORE, TRUE);
}

static void
account_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpAccountManager *self = user_data;
  TpAccount *account = (TpAccount *) object;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    {
      DEBUG ("Error preparing account: %s", error->message);
      g_clear_error (&error);
      goto OUT;
    }

  /* Account could have been invalidated while we were preparing it */
  if (tp_account_is_usable (account) &&
      tp_proxy_get_invalidated (account) == NULL)
    {
      insert_account (self, account);
    }

  DEBUG ("Account %s was prepared",
      tp_proxy_get_object_path (object));

OUT:
  self->priv->n_preparing_accounts--;
  _tp_account_manager_check_core_ready (self);
  g_object_unref (self);
}

static void
_tp_account_manager_got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  GPtrArray *usable_accounts;
  guint i;

  if (error != NULL)
    {
      DEBUG ("Failed to get account manager properties: %s", error->message);
      tp_proxy_invalidate (proxy, error);
      return;
    }

  usable_accounts = tp_asv_get_boxed (properties, "UsableAccounts",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  for (i = 0; i < usable_accounts->len; i++)
    {
      const gchar *path = g_ptr_array_index (usable_accounts, i);
      TpAccount *account;
      GArray *features;
      GError *e = NULL;

      account = tp_client_factory_ensure_account (
          tp_proxy_get_factory (manager), path, NULL, &e);
      if (account == NULL)
        {
          DEBUG ("failed to create TpAccount: %s", e->message);
          g_clear_error (&e);
          continue;
        }

      features = tp_client_factory_dup_account_features (
          tp_proxy_get_factory (manager), account);

      manager->priv->n_preparing_accounts++;
      tp_proxy_prepare_async (account, (GQuark *) features->data,
          account_prepared_cb, g_object_ref (manager));

      g_array_unref (features);
      g_object_unref (account);
    }

  _tp_account_manager_check_core_ready (manager);
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

  tp_cli_account_manager_connect_to_account_usability_changed (self,
      _tp_account_manager_usability_changed_cb, NULL,
      NULL, G_OBJECT (self), NULL);

  tp_cli_dbus_properties_call_get_all (self, -1, TP_IFACE_ACCOUNT_MANAGER,
      _tp_account_manager_got_all_cb, NULL, NULL, G_OBJECT (self));
}

static void
_tp_account_manager_finalize (GObject *object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (object);
  TpAccountManagerPrivate *priv = manager->priv;

  g_free (priv->most_available_status);
  g_free (priv->most_available_status_message);

  g_free (priv->requested_status);
  g_free (priv->requested_status_message);

  G_OBJECT_CLASS (tp_account_manager_parent_class)->finalize (object);
}

static void
_tp_account_manager_dispose (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  TpAccountManagerPrivate *priv = self->priv;

  g_clear_pointer (&priv->accounts, g_hash_table_unref);

  if (priv->watch_name_id != 0)
    g_bus_unwatch_name (priv->watch_name_id);
  priv->watch_name_id = 0;

  G_OBJECT_CLASS (tp_account_manager_parent_class)->dispose (object);
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

  proxy_class->interface = TP_IFACE_QUARK_ACCOUNT_MANAGER;
  proxy_class->list_features = _tp_account_manager_list_features;

  /**
   * TpAccountManager::account-usability-changed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   * @usable: %TRUE if the account is now usable
   *
   * Emitted when the usability on @account changes.
   *
   * This signal is also used to indicate a new account that did not
   * previously exist has been added (with @usable set to %TRUE).
   *
   * If @usable is %TRUE,
   * @account is guaranteed to have %TP_ACCOUNT_FEATURE_CORE prepared, along
   * with all the features previously passed to the #TpProxy:factory<!-- -->'s
   * tp_client_factory_add_account_features().
   *
   * Since: 0.9.0
   */
  signals[ACCOUNT_USABILITY_CHANGED] = g_signal_new ("account-usability-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE,
      2, TP_TYPE_ACCOUNT, G_TYPE_BOOLEAN);

  /**
   * TpAccountManager::account-removed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account is removed from @manager.
   *
   * Since: 0.9.0
   */
  signals[ACCOUNT_REMOVED] = g_signal_new ("account-removed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-enabled:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account from @manager is enabled.
   *
   * @account is guaranteed to have %TP_ACCOUNT_FEATURE_CORE prepared, along
   * with all the features previously passed to the #TpProxy:factory<!-- -->'s
   * tp_client_factory_add_account_features().
   *
   * Since: 0.9.0
   */
  signals[ACCOUNT_ENABLED] = g_signal_new ("account-enabled",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::account-disabled:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   *
   * Emitted when an account from @manager is disabled.
   *
   * Since: 0.9.0
   */
  signals[ACCOUNT_DISABLED] = g_signal_new ("account-disabled",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_ACCOUNT);

  /**
   * TpAccountManager::most-available-presence-changed:
   * @manager: a #TpAccountManager
   * @presence: new presence type
   * @status: new status
   * @message: new status message
   *
   * Emitted when the most available presence on @manager changes.
   *
   * Since: 0.9.0
   */
  signals[MOST_AVAILABLE_PRESENCE_CHANGED] =
    g_signal_new ("most-available-presence-changed",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE,
        3, G_TYPE_UINT, /* Presence type */
        G_TYPE_STRING,  /* status */
        G_TYPE_STRING); /* stauts message*/
}

TpAccountManager *
_tp_account_manager_new (TpClientFactory *factory)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (factory), NULL);

  return TP_ACCOUNT_MANAGER (g_object_new (TP_TYPE_ACCOUNT_MANAGER,
          "dbus-daemon", tp_client_factory_get_dbus_daemon (factory),
          "bus-name", TP_ACCOUNT_MANAGER_BUS_NAME,
          "object-path", TP_ACCOUNT_MANAGER_OBJECT_PATH,
          "factory", factory,
          NULL));
}

/**
 * tp_account_manager_dup:
 *
 * Returns the default #TpClientFactory's #TpAccountManager. It will use
 * tp_client_factory_dup(), print a warning and return %NULL if it fails.
 *
 * Returns: (transfer full): a reference on a #TpAccountManager singleton.
 *
 * Since: 0.9.0
 */
TpAccountManager *
tp_account_manager_dup (void)
{
  TpAccountManager *self;
  TpClientFactory *factory;
  GError *error = NULL;

  factory = tp_client_factory_dup (&error);
  if (factory == NULL)
    {
      WARNING ("Error getting default TpClientFactory: %s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  self = tp_client_factory_ensure_account_manager (factory);

  g_object_unref (factory);

  return self;
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
_tp_account_manager_account_presence_changed_cb (TpAccount *account,
    TpConnectionPresenceType presence,
    const gchar *status,
    const gchar *status_message,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;
  TpConnectionPresenceType p;
  gchar *s;
  gchar *msg;

  if (tp_connection_presence_type_cmp_availability (presence,
          priv->most_available_presence) > 0)
    {
      priv->most_available_account = account;

      priv->most_available_presence = presence;

      g_free (priv->most_available_status);
      priv->most_available_status = g_strdup (status);

      g_free (priv->most_available_status_message);
      priv->most_available_status_message = g_strdup (status_message);

      goto signal;
    }
  else if (priv->most_available_account == account)
    {
      _tp_account_manager_update_most_available_presence (manager);
      goto signal;
    }

  return;

signal:
  /* Use tp_account_manager_get_most_available_presence() as the effective
   * most available presence may differ of the one stored in
   * priv->most_available_presence. */
  p = tp_account_manager_get_most_available_presence (manager,
      &s, &msg);

  g_signal_emit (manager, signals[MOST_AVAILABLE_PRESENCE_CHANGED], 0,
      p, s, msg);

  g_free (s);
  g_free (msg);
}

static void
_tp_account_manager_account_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account = TP_ACCOUNT (proxy);

  /* We only want to deal with accounts being removed here. */
  if (domain != TP_DBUS_ERRORS || code != TP_DBUS_ERROR_OBJECT_REMOVED)
    return;

  g_object_ref (account);
  g_hash_table_remove (priv->accounts,
      tp_proxy_get_object_path (account));

  g_signal_emit (manager, signals[ACCOUNT_REMOVED], 0, account);
  g_object_unref (account);
}

static void
insert_account (TpAccountManager *self,
    TpAccount *account)
{
  g_hash_table_insert (self->priv->accounts,
      g_strdup (tp_proxy_get_object_path (account)),
      g_object_ref (account));

  /* If a global presence has been requested, set in on new accounts as well */
  if (self->priv->requested_presence != TP_CONNECTION_PRESENCE_TYPE_UNSET)
    {
      tp_account_request_presence_async (account,
          self->priv->requested_presence,
          self->priv->requested_status,
          self->priv->requested_status_message,
          NULL, NULL);
    }

  tp_g_signal_connect_object (account, "notify::enabled",
      G_CALLBACK (_tp_account_manager_account_enabled_cb),
      G_OBJECT (self), 0);

  tp_g_signal_connect_object (account, "presence-changed",
      G_CALLBACK (_tp_account_manager_account_presence_changed_cb),
      G_OBJECT (self), 0);

  tp_g_signal_connect_object (account, "invalidated",
      G_CALLBACK (_tp_account_manager_account_invalidated_cb),
      G_OBJECT (self), 0);
}

/**
 * tp_account_manager_dup_usable_accounts:
 * @manager: a #TpAccountManager
 *
 * Returns a newly allocated #GList of reffed valid accounts in @manager.
 * The list must be freed with g_list_free_full() and g_object_unref() after
 * used.
 *
 * The returned #TpAccount<!-- -->s are guaranteed to have
 * %TP_ACCOUNT_FEATURE_CORE prepared, along with all features previously passed
 * to tp_simple_client_factory_add_account_features() for the account
 * manager's #TpProxy:factory.
 *
 * The list of valid accounts returned is not guaranteed to have been retrieved
 * until %TP_ACCOUNT_MANAGER_FEATURE_CORE is prepared
 * (tp_proxy_prepare_async() has returned). Until this feature has
 * been prepared, an empty list (%NULL) will be returned.
 *
 * Returns: (element-type TelepathyGLib.Account) (transfer full): a newly
 *  allocated #GList of reffed valid accounts in @manager
 *
 * Since: 0.19.9
 */
GList *
tp_account_manager_dup_usable_accounts (TpAccountManager *manager)
{
  GList *ret;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  ret = g_hash_table_get_values (manager->priv->accounts);
  g_list_foreach (ret, (GFunc) g_object_ref, NULL);

  return ret;
}

/**
 * tp_account_manager_set_all_requested_presences:
 * @manager: a #TpAccountManager
 * @type: a presence type to request
 * @status: a status to request
 * @message: a status message to request
 *
 * Iterates through the accounts in @manager and requests the presence
 * (@type, @status and @message). Note that the presence requested here is
 * merely a request, and if might not be satisfiable.
 *
 * You can find the most available presence across all accounts by calling
 * tp_account_manager_get_most_available_presence().
 *
 * Setting a requested presence on all accounts will have no effect
 * until tp_proxy_prepare_async()
 * (or the older tp_account_manager_prepare_async()) has finished.
 *
 * Since: 0.9.0
 */
void
tp_account_manager_set_all_requested_presences (TpAccountManager *manager,
    TpConnectionPresenceType type,
    const gchar *status,
    const gchar *message)
{
  TpAccountManagerPrivate *priv;
  GHashTableIter iter;
  gpointer value;

  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));

  priv = manager->priv;

  DEBUG ("request most available presence, type: %d, status: %s, message: %s",
      type, status, message);

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *account = TP_ACCOUNT (value);

      if (tp_proxy_is_prepared (account, TP_ACCOUNT_FEATURE_CORE))
        tp_account_request_presence_async (account, type, status, message,
            NULL, NULL);
    }

  /* save the requested presence, to use it in case we create new accounts or
   * some accounts become ready. */
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
 * tp_account_manager_get_most_available_presence:
 * @manager: a #TpAccountManager
 * @status: (out) (transfer full): a string to fill with the actual status
 * @message: (out) (transfer full): a string to fill with the actual status
 *  message
 *
 * Gets the most available presence over all accounts in @manager. This
 * function does not average presences across all accounts, but it merely
 * finds the "most available" presence. As a result, there is a guarantee
 * that there exists at least one account in @manager with the returned
 * presence.
 *
 * If no accounts are enabled or usable the output will be
 * (%TP_CONNECTION_PRESENCE_TYPE_OFFLINE, "offline", "").
 *
 * Since 0.17.5, if the only connected accounts does not implement
 * %TP_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE, the output will be
 * (%TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, "available", "").
 *
 * The return value of this function is not guaranteed to have been retrieved
 * until tp_proxy_prepare_async() has finished; until then, the
 * value will be the same as if no accounts are enabled or usable.
 *
 * Returns: the most available presence across all accounts
 *
 * Since: 0.9.0
 */

TpConnectionPresenceType
tp_account_manager_get_most_available_presence (TpAccountManager *manager,
    gchar **status,
    gchar **message)
{
  TpAccountManagerPrivate *priv;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager),
      TP_CONNECTION_PRESENCE_TYPE_UNSET);

  priv = manager->priv;

  if (priv->most_available_presence == TP_CONNECTION_PRESENCE_TYPE_UNSET)
    {
      /* The best we have is an account having UNSET as its presence, which
       * means it's connected but does not implement SimplePresence; pretend
       * we are available. */
      if (status != NULL)
        *status = g_strdup ("available");

      if (message != NULL)
        *message = g_strdup ("");

      return TP_CONNECTION_PRESENCE_TYPE_AVAILABLE;
    }

  if (status != NULL)
    *status = g_strdup (priv->most_available_status);

  if (message != NULL)
    *message = g_strdup (priv->most_available_status_message);

  return priv->most_available_presence;
}

static void
create_account_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  GSimpleAsyncResult *my_res = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    {
      DEBUG ("Error preparing account: %s", error->message);
      g_simple_async_result_take_error (my_res, error);
    }

  g_simple_async_result_complete (my_res);
  g_object_unref (my_res);
}

static void
_tp_account_manager_created_cb (TpAccountManager *proxy,
    const gchar *account_path,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  GSimpleAsyncResult *my_res = user_data;
  TpAccount *account;
  GArray *features;
  GError *e = NULL;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (my_res, error);
      g_simple_async_result_complete (my_res);
      return;
    }

  account = tp_client_factory_ensure_account (
      tp_proxy_get_factory (manager), account_path, NULL, &e);
  if (account == NULL)
    {
      g_simple_async_result_take_error (my_res, e);
      g_simple_async_result_complete (my_res);
      return;
    }

  /* Give account's ref to the result */
  g_simple_async_result_set_op_res_gpointer (my_res, account, g_object_unref);

  features = tp_client_factory_dup_account_features (
      tp_proxy_get_factory (manager), account);

  tp_proxy_prepare_async (account, (GQuark *) features->data,
      create_account_prepared_cb, g_object_ref (my_res));

  g_array_unref (features);
}

/**
 * tp_account_manager_create_account_async:
 * @manager: a #TpAccountManager
 * @connection_manager: the name of a connection manager
 * @protocol: the name of a protocol
 * @display_name: the display name for the account
 * @parameters: a #G_VARIANT_TYPE_VARDICT #GVariant containing the parameters
 *  for the new account
 * @properties: a #G_VARIANT_TYPE_VARDICT #GVariant containing the properties
 *  for the new account
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous create of an account on the account manager
 * @manager. When the operation is finished, @callback will be called. You can
 * then call tp_account_manager_create_account_finish() to get the result of
 * the operation.
 *
 * The #TpAccount returned by tp_account_manager_create_account_finish()
 * will already have %TP_ACCOUNT_FEATURE_CORE prepared, along with all
 * features previously passed to
 * tp_simple_client_factory_add_account_features() for the account
 * manager's #TpProxy:factory.
 *
 * @parameters and @properties are consumed if they are floating.
 *
 * It is usually better to use #TpAccountRequest instead, particularly when
 * using high-level language bindings.
 *
 * Since: 0.9.0
 */
void
tp_account_manager_create_account_async (TpAccountManager *manager,
    const gchar *connection_manager,
    const gchar *protocol,
    const gchar *display_name,
    GVariant *parameters,
    GVariant *properties,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *res;
  GHashTable *params_asv, *props_asv;

  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));
  g_return_if_fail (connection_manager != NULL);
  g_return_if_fail (protocol != NULL);
  g_return_if_fail (display_name != NULL);
  g_return_if_fail (parameters != NULL);
  g_return_if_fail (properties != NULL);
  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));

  res = g_simple_async_result_new (G_OBJECT (manager), callback, user_data,
      tp_account_manager_create_account_finish);

  g_variant_ref_sink (parameters);
  params_asv = tp_asv_from_vardict (parameters);

  g_variant_ref_sink (properties);
  props_asv = tp_asv_from_vardict (properties);

  tp_cli_account_manager_call_create_account (manager,
      -1, connection_manager, protocol, display_name, params_asv,
      props_asv, _tp_account_manager_created_cb, res, g_object_unref,
      G_OBJECT (manager));

  g_variant_unref (parameters);
  g_variant_unref (properties);
  g_hash_table_unref (params_asv);
  g_hash_table_unref (props_asv);
}

/**
 * tp_account_manager_create_account_finish:
 * @manager: a #TpAccountManager
 * @result: a #GAsyncResult
 * @error: a #GError to be filled
 *
 * Finishes an async create account operation, and returns a new #TpAccount
 * object. It has %TP_ACCOUNT_FEATURE_CORE prepared, along with all
 * features previously passed to
 * tp_simple_client_factory_add_account_features() for the account
 * manager's #TpProxy:factory.
 *
 * The caller must keep a ref to the returned object using g_object_ref() if
 * it is to be kept beyond the lifetime of @result.
 *
 * Returns: (transfer none): a new #TpAccount which was just created on
 *  success, otherwise %NULL
 *
 * Since: 0.9.0
 */
TpAccount *
tp_account_manager_create_account_finish (TpAccountManager *manager,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_return_copy_pointer (manager,
      tp_account_manager_create_account_finish, /* do not copy */);
}

/**
 * tp_account_manager_enable_restart:
 * @manager: a #TpAccountManager
 *
 * Enable autostarting the account manager D-Bus service. This means
 * that the account manager will be restarted if it disappears from
 * the bus.
 */
void
tp_account_manager_enable_restart (TpAccountManager *manager)
{
  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));

  if (manager->priv->watch_name_id != 0)
    return;

  manager->priv->watch_name_id = g_bus_watch_name_on_connection (
      tp_proxy_get_dbus_connection (manager),
      TP_ACCOUNT_MANAGER_BUS_NAME,
      G_BUS_NAME_WATCHER_FLAGS_NONE,
      NULL,
      name_vanished_cb,
      manager, NULL);

  _tp_account_manager_start (manager);
}
