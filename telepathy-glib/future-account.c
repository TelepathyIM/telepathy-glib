/*
 * future-account.c - object for a currently non-existent account to create
 *
 * Copyright © 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "telepathy-glib/future-account.h"

#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

/**
 * SECTION:future-account
 * @title: TpFutureAccount
 * @short_description: object for a currently non-existent account in
 *   order to create easily without knowing speaking fluent D-Bus
 * @see_also: #TpAccountManager
 *
 * TODO
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpFutureAccount:
 *
 * TODO
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpFutureAccountClass:
 *
 * The class of a #TpFutureAccount.
 */

struct _TpFutureAccountPrivate {
  gboolean dispose_has_run;

  TpAccountManager *account_manager;

  gchar *cm_name;
  gchar *proto_name;
  gchar *display_name;

  GHashTable *parameters;
  GHashTable *properties;
};

G_DEFINE_TYPE (TpFutureAccount, tp_future_account, G_TYPE_OBJECT)

/* signals */
enum {
  LAST_SIGNAL
};

/*static guint signals[LAST_SIGNAL];*/

/* properties */
enum {
  PROP_ACCOUNT_MANAGER = 1,
  PROP_CONNECTION_MANAGER,
  PROP_PROTOCOL,
  PROP_DISPLAY_NAME,
  PROP_PARAMETERS,
  PROP_PROPERTIES,
  PROP_ICON_NAME,
  N_PROPS
};

static void
tp_future_account_init (TpFutureAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_FUTURE_ACCOUNT,
      TpFutureAccountPrivate);
}

static void
tp_future_account_constructed (GObject *object)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);
  TpFutureAccountPrivate *priv = self->priv;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_future_account_parent_class)->constructed;

  priv->parameters = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) tp_g_value_slice_free);

  priv->properties = tp_asv_new (NULL, NULL);

  if (chain_up != NULL)
    chain_up (object);
}

static void
tp_future_account_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);

  switch (prop_id)
    {
    case PROP_ACCOUNT_MANAGER:
      g_value_set_object (value, self->priv->account_manager);
      break;
    case PROP_CONNECTION_MANAGER:
      g_value_set_string (value, self->priv->cm_name);
      break;
    case PROP_PROTOCOL:
      g_value_set_string (value, self->priv->proto_name);
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, self->priv->display_name);
      break;
    case PROP_PARAMETERS:
      g_value_set_boxed (value, self->priv->parameters);
      break;
    case PROP_PROPERTIES:
      g_value_set_boxed (value, self->priv->properties);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, tp_asv_get_string (self->priv->properties, "Icon"));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tp_future_account_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);
  TpFutureAccountPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_ACCOUNT_MANAGER:
      g_assert (priv->account_manager == NULL);
      priv->account_manager = g_value_dup_object (value);
      break;
    case PROP_CONNECTION_MANAGER:
      g_assert (priv->cm_name == NULL);
      priv->cm_name = g_value_dup_string (value);
      break;
    case PROP_PROTOCOL:
      g_assert (priv->proto_name == NULL);
      priv->proto_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_future_account_dispose (GObject *object)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);
  TpFutureAccountPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  tp_clear_pointer (&priv->parameters, g_hash_table_unref);
  tp_clear_pointer (&priv->properties, g_hash_table_unref);

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_future_account_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (tp_future_account_parent_class)->dispose (object);
}

static void
tp_future_account_finalize (GObject *object)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);
  TpFutureAccountPrivate *priv = self->priv;

  tp_clear_pointer (&priv->cm_name, g_free);
  tp_clear_pointer (&priv->proto_name, g_free);
  tp_clear_pointer (&priv->display_name, g_free);

  /* free any data held directly by the object here */

  if (G_OBJECT_CLASS (tp_future_account_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (tp_future_account_parent_class)->finalize (object);
}

static void
tp_future_account_class_init (TpFutureAccountClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpFutureAccountPrivate));

  object_class->constructed = tp_future_account_constructed;
  object_class->get_property = tp_future_account_get_property;
  object_class->set_property = tp_future_account_set_property;
  object_class->dispose = tp_future_account_dispose;
  object_class->finalize = tp_future_account_finalize;

  /**
   * TpFutureAccount:account-manager:
   *
   * TODO
   */
  g_object_class_install_property (object_class, PROP_ACCOUNT_MANAGER,
      g_param_spec_object ("account-manager",
          "Account manager",
          "The future account's account manager",
          TP_TYPE_ACCOUNT_MANAGER,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * TpFutureAccount:connection-manager:
   *
   * The account's connection manager name.
   */
  g_object_class_install_property (object_class, PROP_CONNECTION_MANAGER,
      g_param_spec_string ("connection-manager",
          "Connection manager",
          "The account's connection manager name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * TpFutureAccount:protocol:
   *
   * The account's machine-readable protocol name, such as "jabber", "msn" or
   * "local-xmpp". Recommended names for most protocols can be found in the
   * Telepathy D-Bus Interface Specification.
   */
  g_object_class_install_property (object_class, PROP_PROTOCOL,
      g_param_spec_string ("protocol",
          "Protocol",
          "The account's protocol name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * TpFutureAccount:display-name:
   *
   * The account's display name.
   */
  g_object_class_install_property (object_class, PROP_DISPLAY_NAME,
      g_param_spec_string ("display-name",
          "DisplayName",
          "The account's display name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:parameters:
   *
   * TODO
   */
  g_object_class_install_property (object_class, PROP_PARAMETERS,
      g_param_spec_boxed ("parameters",
          "Parameters",
          "Connection parameters of the account",
          G_TYPE_HASH_TABLE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:properties:
   *
   * TODO
   */
  g_object_class_install_property (object_class, PROP_PROPERTIES,
      g_param_spec_boxed ("properties",
          "Properties",
          "Account properties",
          G_TYPE_HASH_TABLE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:icon-name:
   *
   * The account's icon name. To change this propery, use
   * tp_future_account_set_icon_name().
   */
  g_object_class_install_property (object_class, PROP_ICON_NAME,
      g_param_spec_string ("icon-name",
          "Icon",
          "The account's icon name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));
}

/**
 * tp_future_account_new:
 * @account_manager: TODO
 * @manager: TODO
 * @protocol: TODO
 *
 * Convenience function to create a new future account object.
 *
 * Returns: a new reference to a future account object, or %NULL if
 *   any argument is incorrect
 */
TpFutureAccount *
tp_future_account_new (TpAccountManager *account_manager,
    const gchar *manager,
    const gchar *protocol)
{
  g_return_val_if_fail (TP_IS_ACCOUNT_MANAGER (account_manager), NULL);
  g_return_val_if_fail (manager != NULL, NULL);
  g_return_val_if_fail (protocol != NULL, NULL);

  return g_object_new (TP_TYPE_FUTURE_ACCOUNT,
      "account-manager", account_manager,
      "connection-manager", manager,
      "protocol", protocol,
      NULL);
}

/**
 * tp_future_account_set_display_name:
 * @self: a #TpFutureAccount
 * @name: a display name for the account
 *
 * Set the display name for the new account, @self, to @name.
 */
void
tp_future_account_set_display_name (TpFutureAccount *self,
    const gchar *name)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (name != NULL);

  priv = self->priv;

  g_free (priv->display_name);
  priv->display_name = g_strdup (name);
}

/**
 * tp_future_account_set_icon_name:
 * @self: a #TpFutureAccount
 * @icon: an icon name for the account
 *
 * Set the icon name for the new account, @self, to @icon.
 */
void
tp_future_account_set_icon_name (TpFutureAccount *self,
    const gchar *icon)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (icon != NULL);

  priv = self->priv;

  tp_asv_set_string (priv->properties, "Icon", icon);
}

/**
 * tp_future_account_set_parameter:
 * @self: a #TpFutureAccount
 * @key: the parameter key
 * @value: (transfer none): a variant containing the parameter value
 *
 * Set an account parameter, @key, to @value.
 */
void
tp_future_account_set_parameter (TpFutureAccount *self,
    const gchar *key,
    GVariant *value)
{
  TpFutureAccountPrivate *priv;
  GValue one = G_VALUE_INIT, *two;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  priv = self->priv;

  dbus_g_value_parse_g_variant (value, &one);
  two = tp_g_value_slice_dup (&one);

  g_hash_table_insert (priv->parameters, g_strdup (key), two);

  g_value_unset (&one);
}

/**
 * tp_future_account_set_parameter_string: (skip)
 * @self: a #TpFutureAccount
 * @key: the parameter key
 * @value: the parameter value
 *
 * Set an account parameter, @key, to @value.
 */
void
tp_future_account_set_parameter_string (TpFutureAccount *self,
    const gchar *key,
    const gchar *value)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  priv = self->priv;

  g_hash_table_insert (priv->parameters, g_strdup (key),
      tp_g_value_slice_new_string (value));
}
