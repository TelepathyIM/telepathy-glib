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

#include "telepathy-glib/account-manager-internal.h"
#include "telepathy-glib/account-internal.h"

#include <telepathy-glib/defs.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util-internal.h>
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

  GHashTable *create_results;

  /* Features */
  GList *features;
  GList *callbacks;
  GArray *requested_features;
  GArray *actual_features;
  GArray *missing_features;
};

typedef struct {
  GQuark name;
  gboolean ready;
} TpAccountManagerFeature;

typedef struct {
  GSimpleAsyncResult *result;
  GArray *features;
} TpAccountManagerFeatureCallback;

#define MC5_BUS_NAME "org.freedesktop.Telepathy.MissionControl5"

enum {
  ACCOUNT_VALIDITY_CHANGED,
  ACCOUNT_REMOVED,
  ACCOUNT_ENABLED,
  ACCOUNT_DISABLED,
  MOST_AVAILABLE_PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (TpAccountManager, tp_account_manager, TP_TYPE_PROXY);

/**
 * TP_ACCOUNT_MANAGER_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core" feature
 * on a #TpAccountManager.
 *
 * When this feature is prepared, the list of accounts have been retrieved and
 * are available for use, and change-notification has been set up.
 *
 * One can ask for a feature to be prepared using the
 * tp_account_manager_prepare_async() function, and waiting for it to callback.
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

static const GQuark *
_tp_account_manager_get_known_features (void)
{
  static GQuark features[] = { 0, 0 };

  if (G_UNLIKELY (features[0] == 0))
    {
      features[0] = TP_ACCOUNT_MANAGER_FEATURE_CORE;
    }

  return features;
}

static TpAccountManagerFeature *
_tp_account_manager_get_feature (TpAccountManager *self,
    GQuark feature)
{
  TpAccountManagerPrivate *priv = self->priv;
  GList *l;

  for (l = priv->features; l != NULL; l = l->next)
    {
      TpAccountManagerFeature *f = l->data;

      if (f->name == feature)
        return f;
    }

  return NULL;
}

static gboolean
_tp_account_manager_feature_in_array (GQuark feature,
    const GArray *array)
{
  const GQuark *c = (const GQuark *) array->data;

  for (; *c != 0; c++)
    {
      if (*c == feature)
        return TRUE;
    }

  return FALSE;
}

static gboolean
_tp_account_manager_check_features (TpAccountManager *self,
    const GArray *features)
{
  const GQuark *f;
  TpAccountManagerFeature *feat;

  for (f = (GQuark *) features->data; f != NULL && *f != 0; f++)
    {
      feat = _tp_account_manager_get_feature (self, *f);

      /* features which are NULL (ie. don't exist) are always considered as
       * being ready, except in _is_prepared when it doesn't make sense to
       * return TRUE. */
      if (feat != NULL && !feat->ready)
        return FALSE;
    }

  /* Special-case core: no other feature is ready unless core itself is
   * ready. */
  feat = _tp_account_manager_get_feature (self,
      TP_ACCOUNT_MANAGER_FEATURE_CORE);
  if (!feat->ready)
    return FALSE;

  return TRUE;
}

static void
_tp_account_manager_become_ready (TpAccountManager *self,
    GQuark feature)
{
  TpAccountManagerPrivate *priv = self->priv;
  TpAccountManagerFeature *f = NULL;
  GList *l, *remove = NULL;

  f = _tp_account_manager_get_feature (self, feature);

  g_assert (f != NULL);

  if (f->ready)
    return;

  f->ready = TRUE;

  if (!_tp_account_manager_feature_in_array (feature, priv->actual_features))
    g_array_append_val (priv->actual_features, feature);

  /* First, find which callbacks are satisfied and add those items
   * from the remove list. */
  l = priv->callbacks;
  while (l != NULL)
    {
      GList *c = l;
      TpAccountManagerFeatureCallback *cb = l->data;

      l = l->next;

      if (_tp_account_manager_check_features (self, cb->features))
        {
          priv->callbacks = g_list_remove_link (priv->callbacks, c);
          remove = g_list_concat (c, remove);
        }
    }

  /* Next, complete these callbacks */
  for (l = remove; l != NULL; l = l->next)
    {
      TpAccountManagerFeatureCallback *cb = l->data;

      g_simple_async_result_complete (cb->result);
      g_object_unref (cb->result);
      g_array_free (cb->features, TRUE);
      g_slice_free (TpAccountManagerFeatureCallback, cb);
    }

  g_list_free (remove);
}

static void
_tp_account_manager_invalidated_cb (TpAccountManager *self,
    guint domain,
    guint code,
    gchar *message)
{
  TpAccountManagerPrivate *priv = self->priv;
  GList *l;

  /* Make all currently pending callbacks fail. */
  for (l = priv->callbacks; l != NULL; l = l->next)
    {
      TpAccountManagerFeatureCallback *cb = l->data;

      g_simple_async_result_set_error (cb->result,
          domain, code, "%s", message);
      g_simple_async_result_complete (cb->result);
      g_object_unref (cb->result);
      g_array_free (cb->features, TRUE);
      g_slice_free (TpAccountManagerFeatureCallback, cb);
    }

  g_list_free (priv->callbacks);
  priv->callbacks = NULL;
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

  priv->create_results = g_hash_table_new (g_direct_hash, g_direct_equal);

  g_signal_connect (self, "invalidated",
      G_CALLBACK (_tp_account_manager_invalidated_cb), NULL);
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
_tp_account_manager_validity_changed_cb (TpAccountManager *proxy,
    const gchar *path,
    gboolean valid,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account;

  account = tp_account_manager_ensure_account (manager, path);

  g_object_ref (account);

  if (!valid)
    g_hash_table_remove (priv->accounts, path);

  g_signal_emit (manager, signals[ACCOUNT_VALIDITY_CHANGED], 0,
      account, valid);

  g_object_unref (account);
}

static void
_tp_account_manager_ensure_all_accounts (TpAccountManager *manager,
    GPtrArray *valid_accounts,
    GPtrArray *invalid_accounts)
{
  guint i, missing_accounts;
  GHashTableIter iter;
  TpAccountManagerPrivate *priv = manager->priv;
  gpointer value;
  TpAccount *account;
  gboolean found_in_valid = FALSE;
  gboolean found_in_invalid = FALSE;
  const gchar *name;

  /* ensure all accounts coming from MC5 first */
  for (i = 0; i < valid_accounts->len; i++)
    {
      name = g_ptr_array_index (valid_accounts, i);

      account = tp_account_manager_ensure_account (manager, name);
      _tp_account_refresh_properties (account);
    }

  missing_accounts = g_hash_table_size (priv->accounts) - valid_accounts->len;

  if (missing_accounts > 0)
    {
      /* look for accounts we have and the TpAccountManager doesn't,
       * and remove them from our cache. */

      DEBUG ("%d missing accounts", missing_accounts);

      g_hash_table_iter_init (&iter, priv->accounts);

      while (g_hash_table_iter_next (&iter, NULL, &value) && missing_accounts > 0)
        {
          account = value;

          /* look for this account in the valid accounts array */
          for (i = 0; i < valid_accounts->len; i++)
            {
              name = g_ptr_array_index (valid_accounts, i);

              if (!tp_strdiff (name, tp_proxy_get_object_path (account)))
                {
                  found_in_valid = TRUE;
                  break;
                }
            }

          if (!found_in_valid)
            {
              /* look for this account in the invalid accounts array */
              for (i = 0; i < invalid_accounts->len; i++)
                {
                  name = g_ptr_array_index (invalid_accounts, i);

                  if (!tp_strdiff (name, tp_proxy_get_object_path (account)))
                    {
                      found_in_invalid = TRUE;
                      break;
                    }
                }

              if (found_in_invalid)
                {
                  DEBUG ("Account %s's validity changed",
                      tp_proxy_get_object_path (account));

                  _tp_account_manager_validity_changed_cb (manager,
                      tp_proxy_get_object_path (account), FALSE, NULL,
                      G_OBJECT (manager));
                }
              else
                {
                  DEBUG ("Account %s was not found, remove it from the cache",
                      tp_proxy_get_object_path (account));

                  g_object_ref (account);
                  g_hash_table_iter_remove (&iter);
                  g_signal_emit (manager, signals[ACCOUNT_REMOVED], 0, account);
                  g_object_unref (account);
                }

              missing_accounts--;
            }

          found_in_valid = FALSE;
          found_in_invalid = FALSE;
        }
    }
}

static void
_tp_account_manager_update_most_available_presence (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv = manager->priv;
  TpConnectionPresenceType presence = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
  TpAccount *account = NULL;
  GHashTableIter iter;
  gpointer value;

  /* this presence is equal to the presence of the account with the
   * highest availability */

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *a = TP_ACCOUNT (value);
      TpConnectionPresenceType p;

      p = tp_account_get_current_presence (a, NULL, NULL);

      if (tp_connection_presence_type_cmp_availability (p, presence) > 0)
        {
          account = a;
          presence = p;
        }
    }

  priv->most_available_account = account;
  g_free (priv->most_available_status);
  g_free (priv->most_available_status_message);

  if (account == NULL)
    {
      priv->most_available_presence = presence;
      priv->most_available_status = NULL;
      priv->most_available_status_message = NULL;
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
  GHashTableIter iter;
  gpointer value;

  if (tp_account_manager_is_prepared (manager, TP_ACCOUNT_MANAGER_FEATURE_CORE))
    return;

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      TpAccount *account = TP_ACCOUNT (value);

      if (!tp_account_is_prepared (account, TP_ACCOUNT_FEATURE_CORE))
        return;
    }

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

  _tp_account_manager_become_ready (manager, TP_ACCOUNT_MANAGER_FEATURE_CORE);
}

static void
_tp_account_manager_got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (weak_object);
  GPtrArray *valid_accounts;
  GPtrArray *invalid_accounts;

  if (error != NULL)
    {
      DEBUG ("Failed to get account manager properties: %s", error->message);
      tp_proxy_invalidate (proxy, error);
      return;
    }

  valid_accounts = tp_asv_get_boxed (properties, "ValidAccounts",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  invalid_accounts = tp_asv_get_boxed (properties, "InvalidAccounts",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  if (valid_accounts != NULL && invalid_accounts != NULL)
    _tp_account_manager_ensure_all_accounts (manager, valid_accounts,
        invalid_accounts);

  _tp_account_manager_check_core_ready (manager);
}

static void
_tp_account_manager_constructed (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_account_manager_parent_class)->constructed;
  TpAccountManagerPrivate *priv = self->priv;
  guint i;
  const GQuark *known_features;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);

  priv->features = NULL;
  priv->callbacks = NULL;

  priv->requested_features = g_array_new (TRUE, FALSE, sizeof (GQuark));
  priv->actual_features = g_array_new (TRUE, FALSE, sizeof (GQuark));
  priv->missing_features = g_array_new (TRUE, FALSE, sizeof (GQuark));

  known_features = _tp_account_manager_get_known_features ();

  /* Fill features list. */
  for (i = 0; known_features[i] != 0; i++)
    {
      TpAccountManagerFeature *feature;
      feature = g_slice_new0 (TpAccountManagerFeature);
      feature->name = known_features[i];
      feature->ready = FALSE;
      priv->features = g_list_prepend (priv->features, feature);
    }

  tp_cli_account_manager_connect_to_account_validity_changed (self,
      _tp_account_manager_validity_changed_cb, NULL,
      NULL, G_OBJECT (self), NULL);

  tp_cli_dbus_properties_call_get_all (self, -1, TP_IFACE_ACCOUNT_MANAGER,
      _tp_account_manager_got_all_cb, NULL, NULL, G_OBJECT (self));
}

static void
_tp_account_manager_feature_free (gpointer data,
    gpointer user_data)
{
  g_slice_free (TpAccountManagerFeature, data);
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

  g_list_foreach (priv->features, _tp_account_manager_feature_free, NULL);
  g_list_free (priv->features);
  priv->features = NULL;

  /* GSimpleAsyncResult keeps a ref to the source GObject, so this list
   * should be empty. */
  g_assert_cmpuint (g_list_length (priv->callbacks), ==, 0);
  g_list_free (priv->callbacks);
  priv->callbacks = NULL;

  g_array_free (priv->requested_features, TRUE);
  g_array_free (priv->actual_features, TRUE);
  g_array_free (priv->missing_features, TRUE);

  G_OBJECT_CLASS (tp_account_manager_parent_class)->finalize (object);
}

static void
_tp_account_manager_dispose (GObject *object)
{
  TpAccountManager *self = TP_ACCOUNT_MANAGER (object);
  TpAccountManagerPrivate *priv = self->priv;

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  /* GSimpleAsyncResult keeps a ref to the source GObject, so this hash
   * table should be empty. */
  g_assert_cmpuint (g_hash_table_size (priv->create_results), ==, 0);
  g_hash_table_destroy (priv->create_results);
  priv->create_results = NULL;

  g_hash_table_destroy (priv->accounts);

  tp_dbus_daemon_cancel_name_owner_watch (tp_proxy_get_dbus_daemon (self),
      TP_ACCOUNT_MANAGER_BUS_NAME, _tp_account_manager_name_owner_cb, self);

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
  tp_account_manager_init_known_interfaces ();

  /**
   * TpAccountManager::account-validity-changed:
   * @manager: a #TpAccountManager
   * @account: a #TpAccount
   * @valid: %TRUE if the account is now valid
   *
   * Emitted when the validity on @account changes. @account is not guaranteed
   * to be ready when this signal is emitted.
   *
   * Since: 0.9.0
   */
  signals[ACCOUNT_VALIDITY_CHANGED] = g_signal_new ("account-validity-changed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      _tp_marshal_VOID__OBJECT_BOOLEAN,
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
   * Note that the returned #TpAccount @account is not guaranteed to have any
   * features pre-prepared, including %TP_ACCOUNT_FEATURE_CORE.
   *
   * Since: 0.9.0
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
   * Since: 0.9.0
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
        NULL, NULL,
        _tp_marshal_VOID__UINT_STRING_STRING,
        G_TYPE_NONE,
        3, G_TYPE_UINT, /* Presence type */
        G_TYPE_STRING,  /* status */
        G_TYPE_STRING); /* stauts message*/
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
 * Convenience function to create a new account manager proxy. The returned
 * #TpAccountManager is not guaranteed to be ready on return.
 *
 * Use tp_account_manager_dup() instead if you want an account manager proxy
 * on the starter or session bus (which is almost always the right thing for
 * Telepathy).
 *
 * Returns: a new reference to an account manager proxy
 */
TpAccountManager *
tp_account_manager_new (TpDBusDaemon *bus_daemon)
{
  TpAccountManager *self;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (bus_daemon), NULL);

  self = TP_ACCOUNT_MANAGER (g_object_new (TP_TYPE_ACCOUNT_MANAGER,
          "dbus-daemon", bus_daemon,
          "dbus-connection", ((TpProxy *) bus_daemon)->dbus_connection,
          "bus-name", TP_ACCOUNT_MANAGER_BUS_NAME,
          "object-path", TP_ACCOUNT_MANAGER_OBJECT_PATH,
          NULL));

  return self;
}

static gpointer starter_account_manager_proxy = NULL;

/**
 * tp_account_manager_dup:
 *
 * Returns an account manager proxy on the D-Bus daemon on which this
 * process was activated (if it was launched by D-Bus service activation), or
 * the session bus (otherwise).
 *
 * The returned #TpAccountManager is cached; the same #TpAccountManager object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists. Note that the returned #TpAccountManager is not guaranteed
 * to be ready on return.
 *
 * Returns: an account manager proxy on the starter or session bus, or %NULL
 *          if it wasn't possible to get a dbus daemon proxy for the
 *          appropriate bus
 *
 * Since: 0.9.0
 */
TpAccountManager *
tp_account_manager_dup (void)
{
  TpDBusDaemon *dbus;

  if (starter_account_manager_proxy != NULL)
    return g_object_ref (starter_account_manager_proxy);

  dbus = tp_dbus_daemon_dup (NULL);

  if (dbus == NULL)
    return NULL;

  starter_account_manager_proxy = tp_account_manager_new (dbus);
  g_assert (starter_account_manager_proxy != NULL);
  g_object_add_weak_pointer (starter_account_manager_proxy,
      &starter_account_manager_proxy);

  g_object_unref (dbus);

  return starter_account_manager_proxy;
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
  g_signal_emit (manager, signals[MOST_AVAILABLE_PRESENCE_CHANGED], 0,
      priv->most_available_presence, priv->most_available_status,
      priv->most_available_status_message);
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
_tp_account_manager_account_ready_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpAccountManager *manager = TP_ACCOUNT_MANAGER (user_data);
  TpAccountManagerPrivate *priv = manager->priv;
  TpAccount *account = TP_ACCOUNT (source_object);
  GSimpleAsyncResult *result;

  if (!tp_account_prepare_finish (account, res, NULL))
    {
      g_object_ref (account);
      g_hash_table_remove (priv->accounts,
          tp_proxy_get_object_path (account));

      g_signal_emit (manager, signals[ACCOUNT_REMOVED], 0, account);
      g_object_unref (account);

      _tp_account_manager_check_core_ready (manager);
      return;
    }

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

  tp_g_signal_connect_object (account, "notify::enabled",
      G_CALLBACK (_tp_account_manager_account_enabled_cb),
      G_OBJECT (manager), 0);

  tp_g_signal_connect_object (account, "presence-changed",
      G_CALLBACK (_tp_account_manager_account_presence_changed_cb),
      G_OBJECT (manager), 0);

  tp_g_signal_connect_object (account, "invalidated",
      G_CALLBACK (_tp_account_manager_account_invalidated_cb),
      G_OBJECT (manager), 0);

  _tp_account_manager_check_core_ready (manager);
}

/**
 * tp_account_manager_ensure_account:
 * @manager: a #TpAccountManager
 * @path: the object path for an account
 *
 * Lookup an account in the account manager @manager. If the desired account
 * has already been ensured then the same object will be returned, otherwise
 * it will create a new #TpAccount and add it to @manager. As a result, if
 * @manager thinks that the account doesn't exist, this will still add it to
 * @manager to avoid races. Note that the returned #TpAccount is not guaranteed
 * to be ready on return.
 *
 * The caller must keep a ref to the returned object using g_object_ref() if
 * it is to be kept.
 *
 * Returns: a new #TpAccount at @path
 *
 * Since: 0.9.0
 */
TpAccount *
tp_account_manager_ensure_account (TpAccountManager *manager,
    const gchar *path)
{
  TpAccountManagerPrivate *priv;
  TpAccount *account;
  GQuark fs[] = { TP_ACCOUNT_FEATURE_CORE, 0 };

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  priv = manager->priv;

  account = g_hash_table_lookup (priv->accounts, path);
  if (account != NULL)
    return account;

  account = tp_account_new (tp_proxy_get_dbus_daemon (manager), path, NULL);
  g_return_val_if_fail (account != NULL, NULL);
  g_hash_table_insert (priv->accounts, g_strdup (path), account);

  tp_account_prepare_async (account, fs, _tp_account_manager_account_ready_cb,
      manager);

  return account;
}

/**
 * tp_account_manager_get_valid_accounts:
 * @manager: a #TpAccountManager
 *
 * Returns a newly allocated #GList of valid accounts in @manager. The list
 * must be freed with g_list_free() after used. None of the accounts in the
 * returned list are guaranteed to be ready.
 *
 * Note that the #TpAccount<!-- -->s in the returned #GList are not reffed
 * before returning from this function. One could ref every item in the list
 * like the following example:
 * |[
 * GList *accounts;
 * account = tp_account_manager_get_valid_accounts (manager);
 * g_list_foreach (accounts, (GFunc) g_object_ref, NULL);
 * ]|
 *
 * The list of valid accounts returned is not guaranteed to have been retrieved
 * until %TP_ACCOUNT_MANAGER_FEATURE_CORE is prepared
 * (tp_account_manager_prepare_async() has returned). Until this feature has
 * been prepared, an empty list (%NULL) will be returned.
 *
 * Returns: a newly allocated #GList of valid accounts in @manager
 *
 * Since: 0.9.0
 */
GList *
tp_account_manager_get_valid_accounts (TpAccountManager *manager)
{
  TpAccountManagerPrivate *priv;
  GList *ret;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  priv = manager->priv;

  ret = g_hash_table_get_values (priv->accounts);

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
 * until tp_account_manager_prepare_async() has finished.
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

      if (tp_account_is_prepared (account, TP_ACCOUNT_FEATURE_CORE))
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
 * @status: a string to fill with the actual status
 * @message: a string to fill with the actual status message
 *
 * Gets the most available presence over all accounts in @manager. This
 * function does not average presences across all accounts, but it merely
 * finds the "most available" presence. As a result, there is a guarantee
 * that there exists at least one account in @manager with the returned
 * presence.
 *
 * If no accounts are enabled or valid the output will be
 * (%TP_CONNECTION_PRESENCE_TYPE_OFFLINE, "offline", "").
 *
 * The return value of this function is not guaranteed to have been retrieved
 * until tp_account_manager_prepare_async() has finished; until then, the
 * value will be the same as if no accounts are enabled or valid.
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

  if (status != NULL)
    *status = g_strdup (priv->most_available_status);

  if (message != NULL)
    *message = g_strdup (priv->most_available_status_message);

  return priv->most_available_presence;
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
      g_simple_async_result_set_from_error (my_res, (GError *) error);
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
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous create of an account on the account manager
 * @manager. When the operation is finished, @callback will be called. You can
 * then call tp_account_manager_create_account_finish() to get the result of
 * the operation.
 *
 * @callback will only be called when the newly created #TpAccount has the
 * %TP_ACCOUNT_FEATURE_CORE feature ready on it, so when calling
 * tp_account_manager_create_account_finish(), one can guarantee this feature
 * will be ready.
 *
 * Since: 0.9.0
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

  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));
  g_return_if_fail (connection_manager != NULL);
  g_return_if_fail (protocol != NULL);
  g_return_if_fail (display_name != NULL);
  g_return_if_fail (parameters != NULL);
  g_return_if_fail (properties != NULL);
  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));

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
 * Finishes an async create account operation, and returns a new #TpAccount
 * object, with the %TP_ACCOUNT_FEATURE_CORE feature ready on it.
 *
 * Returns: a new #TpAccount which was just created on success, otherwise
 *          %NULL
 *
 * Since: 0.9.0
 */
TpAccount *
tp_account_manager_create_account_finish (TpAccountManager *manager,
    GAsyncResult *result,
    GError **error)
{
  TpAccount *retval;
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return NULL;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (manager), tp_account_manager_create_account_finish), NULL);

  retval = TP_ACCOUNT (g_simple_async_result_get_op_res_gpointer (
          G_SIMPLE_ASYNC_RESULT (result)));

  return retval;
}

/**
 * tp_account_manager_is_prepared:
 * @manager: a #TpAccountManager
 * @feature: a feature which is required
 * @error: a #GError to fill
 *
 * <!-- -->
 *
 * Returns: %TRUE whether @feature is ready on @manager, otherwise %FALSE
 *
 * Since: 0.9.0
 */
gboolean
tp_account_manager_is_prepared (TpAccountManager *manager,
    GQuark feature)
{
  TpAccountManagerFeature *f;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), FALSE);

  if (tp_proxy_get_invalidated (manager) != NULL)
    return FALSE;

  f = _tp_account_manager_get_feature (manager, feature);

  if (f == NULL)
    return FALSE;

  return f->ready;
}

/**
 * tp_account_manager_prepare_async:
 * @manager: a #TpAccountManager
 * @features: a 0-terminated list of features, or %NULL
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous preparation of @manager with the features specified
 * by @features. When the operation is finished, @callback will be called. You
 * can then call tp_account_manager_prepare_finish() to get the result of the
 * operation.
 *
 * If @features is %NULL, then @callback will be called when the implied
 * %TP_ACCOUNT_MANAGER_FEATURE_CORE feature is ready.
 *
 * If %NULL is given to @callback, then no callback will be called when the
 * operation is finished. Instead, it will simply set @features on @manager.
 * Note that if @callback is %NULL, then @user_data must also be %NULL.
 *
 * Since: 0.9.0
 */
void
tp_account_manager_prepare_async (TpAccountManager *manager,
    const GQuark *features,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpAccountManagerPrivate *priv;
  GSimpleAsyncResult *result;
  const GQuark *f;
  const GError *error;
  GArray *feature_array;

  g_return_if_fail (TP_IS_ACCOUNT_MANAGER (manager));

  priv = manager->priv;

  /* In this object, there are no features which are activatable (core is
   * forced on you). They'd be activated here though. */

  for (f = features; f != NULL && *f != 0; f++)
    {
      /* Only add features to requested which exist on this object and are not
       * already in the list.
       */
      if (_tp_account_manager_get_feature (manager, *f) != NULL
          && !_tp_account_manager_feature_in_array (*f, priv->requested_features))
        g_array_append_val (priv->requested_features, *f);
    }

  if (callback == NULL)
    return;

  result = g_simple_async_result_new (G_OBJECT (manager),
      callback, user_data, tp_account_manager_prepare_finish);

  feature_array = _tp_quark_array_copy (features);

  error = tp_proxy_get_invalidated (manager);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      g_array_free (feature_array, TRUE);
    }
  else if (_tp_account_manager_check_features (manager, feature_array))
    {
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      g_array_free (feature_array, TRUE);
    }
  else
    {
      TpAccountManagerFeatureCallback *cb;

      cb = g_slice_new0 (TpAccountManagerFeatureCallback);
      cb->result = result;
      cb->features = feature_array;
      priv->callbacks = g_list_prepend (priv->callbacks, cb);
    }
}

/**
 * tp_account_manager_prepare_finish:
 * @manager: a #TpAccountManager
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async preparation of the account manager @manager.
 *
 * Returns: %TRUE if the preparation was successful, otherwise %FALSE
 *
 * Since: 0.9.0
 */
gboolean
tp_account_manager_prepare_finish (TpAccountManager *manager,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (manager),tp_account_manager_prepare_finish), FALSE);

  return TRUE;
}

/**
 * _tp_account_manager_get_requested_features:
 * @manager: a #TpAccountManager
 *
 * <!-- -->
 *
 * Returns: a 0-terminated list of requested features on @manager
 *
 * Since: 0.9.0
 */
const GQuark *
_tp_account_manager_get_requested_features (TpAccountManager *manager)
{
  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  return (const GQuark *) manager->priv->requested_features->data;
}

/**
 * _tp_account_manager_get_actual_features:
 * @manager: a #TpAccountManager
 *
 * <!-- -->
 *
 * Returns: a 0-terminated list of actual features on @manager
 *
 * Since: 0.9.0
 */
const GQuark *
_tp_account_manager_get_actual_features (TpAccountManager *manager)
{
  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  return (const GQuark *) manager->priv->actual_features->data;
}

/**
 * _tp_account_manager_get_missing_features:
 * @manager: a #TpAccountManager
 *
 * <!-- -->
 *
 * Returns: a 0-terminated list of missing features on @manager
 *
 * Since: 0.9.0
 */
const GQuark *
_tp_account_manager_get_missing_features (TpAccountManager *manager)
{
  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (manager), NULL);

  return (const GQuark *) manager->priv->missing_features->data;
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

  tp_dbus_daemon_watch_name_owner (tp_proxy_get_dbus_daemon (manager),
      TP_ACCOUNT_MANAGER_BUS_NAME, _tp_account_manager_name_owner_cb,
      manager, NULL);

  _tp_account_manager_start_mc5 (tp_proxy_get_dbus_daemon (manager));
}
