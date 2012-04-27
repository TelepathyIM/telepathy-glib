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

#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/simple-client-factory.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/util-internal.h"

/**
 * SECTION:future-account
 * @title: TpFutureAccount
 * @short_description: object for a currently non-existent account in
 *   order to create easily without speaking fluent D-Bus
 * @see_also: #TpAccountManager
 *
 * This is a convenience object to aid in the creation of accounts on
 * a #TpAccountManager without having to construct #GHashTables with
 * well-known keys. For example:
 *
 * |[
 * static void created_cb (GObject *object, GAsyncResult *res, gpointer user_data);
 *
 * static void
 * create_acount (void)
 * {
 *   TpAccountManager *am = tp_account_manager_dup ();
 *   TpFutureAccount *future;
 *
 *   future = tp_future_account_new (am, "gabble", "jabber");
 *   tp_future_account_set_display_name (future, "Work Jabber account");
 *
 *   tp_future_account_set_parameter (future, "account", "walter.white@example.com");
 *
 *   // ...
 *
 *   tp_future_account_create_account_async (future, created_cb, NULL);
 *   g_object_unref (future);
 *   g_object_unref (am);
 * }
 *
 * static void
 * created_cb (GObject *object,
 *     GAsyncResult *result,
 *     gpointer user_data)
 * {
 *   TpFutureAccount *future = TP_FUTURE_ACCOUNT (object);
 *   TpAccount *account;
 *   GError *error = NULL;
 *
 *   account = tp_future_account_create_account_finish (future, result, &error);
 *
 *   if (account == NULL)
 *     {
 *       g_error ("Failed to create account: %s\n", error->message);
 *       g_clear_error (&error);
 *       return;
 *     }
 *
 *   // ...
 *
 *   g_object_unref (account);
 * }
 * ]|
 *
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpFutureAccount:
 *
 * An object for representing a currently non-existent account which
 * is to be created on a #TpAccountManager.
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

  GSimpleAsyncResult *result;

  gchar *cm_name;
  gchar *proto_name;
  gchar *display_name;

  GHashTable *parameters;
  GHashTable *properties;
};

G_DEFINE_TYPE (TpFutureAccount, tp_future_account, G_TYPE_OBJECT)

/* properties */
enum {
  PROP_ACCOUNT_MANAGER = 1,
  PROP_CONNECTION_MANAGER,
  PROP_PROTOCOL,
  PROP_DISPLAY_NAME,
  PROP_PARAMETERS,
  PROP_PROPERTIES,
  PROP_ICON_NAME,
  PROP_NICKNAME,
  PROP_REQUESTED_PRESENCE_TYPE,
  PROP_REQUESTED_STATUS,
  PROP_REQUESTED_STATUS_MESSAGE,
  PROP_AUTOMATIC_PRESENCE_TYPE,
  PROP_AUTOMATIC_STATUS,
  PROP_AUTOMATIC_STATUS_MESSAGE,
  PROP_ENABLED,
  PROP_CONNECT_AUTOMATICALLY,
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

#define GET_PRESENCE_VALUE(key, offset, type, default_value) \
  G_STMT_START { \
  GValueArray *_arr = tp_asv_get_boxed (self->priv->properties, \
      key, TP_STRUCT_TYPE_SIMPLE_PRESENCE); \
  if (_arr != NULL) \
    g_value_set_##type (value, g_value_get_##type (_arr->values + offset)); \
  else \
    g_value_set_##type (value, default_value); \
  } G_STMT_END

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
      g_value_set_string (value,
          tp_asv_get_string (self->priv->properties,
              TP_PROP_ACCOUNT_ICON));
      break;
    case PROP_NICKNAME:
      g_value_set_string (value,
          tp_asv_get_string (self->priv->properties,
              TP_PROP_ACCOUNT_NICKNAME));
      break;
    case PROP_REQUESTED_PRESENCE_TYPE:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_REQUESTED_PRESENCE, 0, uint, 0);
      break;
    case PROP_REQUESTED_STATUS:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_REQUESTED_PRESENCE, 1, string, "");
      break;
    case PROP_REQUESTED_STATUS_MESSAGE:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_REQUESTED_PRESENCE, 2, string, "");
      break;
    case PROP_AUTOMATIC_PRESENCE_TYPE:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_AUTOMATIC_PRESENCE, 0, uint, 0);
      break;
    case PROP_AUTOMATIC_STATUS:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_AUTOMATIC_PRESENCE, 1, string, "");
      break;
    case PROP_AUTOMATIC_STATUS_MESSAGE:
      GET_PRESENCE_VALUE (TP_PROP_ACCOUNT_AUTOMATIC_PRESENCE, 2, string, "");
      break;
    case PROP_ENABLED:
      g_value_set_boolean (value,
          tp_asv_get_boolean (self->priv->properties,
              TP_PROP_ACCOUNT_ENABLED, NULL));
      break;
    case PROP_CONNECT_AUTOMATICALLY:
      g_value_set_boolean (value,
          tp_asv_get_boolean (self->priv->properties,
              TP_PROP_ACCOUNT_CONNECT_AUTOMATICALLY,
              NULL));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#undef GET_PRESENCE_VALUE

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
   * The #TpAccountManager to create the account on.
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
   * The account's display name. To change this property use
   * tp_future_account_set_display_name().
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
   * The account's connection parameters. To add a parameter, use
   * tp_future_account_set_parameter() or another convience function.
   */
  g_object_class_install_property (object_class, PROP_PARAMETERS,
      g_param_spec_boxed ("parameters",
          "Parameters",
          "Connection parameters of the account",
          TP_HASH_TYPE_STRING_VARIANT_MAP,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:properties:
   *
   * The account's properties.
   */
  g_object_class_install_property (object_class, PROP_PROPERTIES,
      g_param_spec_boxed ("properties",
          "Properties",
          "Account properties",
          TP_HASH_TYPE_STRING_VARIANT_MAP,
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

  /**
   * TpFutureAccount:nickname:
   *
   * The account's nickname. To change this property use
   * tp_future_account_set_nickname().
   */
  g_object_class_install_property (object_class, PROP_NICKNAME,
      g_param_spec_string ("nickname",
          "Nickname",
          "The account's nickname",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:requested-presence-type:
   *
   * The account's requested presence type (a
   * #TpConnectionPresenceType). To change this property use
   * tp_future_account_set_requested_presence().
   */
  g_object_class_install_property (object_class, PROP_REQUESTED_PRESENCE_TYPE,
      g_param_spec_uint ("requested-presence-type",
          "RequestedPresence",
          "The account's requested presence type",
          0,
          TP_NUM_CONNECTION_PRESENCE_TYPES,
          TP_CONNECTION_PRESENCE_TYPE_UNSET,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:requested-status:
   *
   * The requested Status string of the account. To change this
   * property use tp_future_account_set_requested_presence().
   */
  g_object_class_install_property (object_class, PROP_REQUESTED_STATUS,
      g_param_spec_string ("requested-status",
          "RequestedStatus",
          "The account's requested status string",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:requested-status-message:
   *
   * The requested status message message of the account. To change
   * this property use tp_future_account_set_requested_presence().
   */
  g_object_class_install_property (object_class, PROP_REQUESTED_STATUS_MESSAGE,
      g_param_spec_string ("requested-status-message",
          "RequestedStatusMessage",
          "The requested Status message string of the account",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:automatic-presence-type:
   *
   * The account's automatic presence type (a
   * #TpConnectionPresenceType). To change this property use
   * tp_future_account_set_automatic_presence().
   *
   * When the account is put online automatically, for instance to
   * make a channel request or because network connectivity becomes
   * available, the automatic presence type, status and message will
   * be copied to their "requested" counterparts.
   */
  g_object_class_install_property (object_class, PROP_AUTOMATIC_PRESENCE_TYPE,
      g_param_spec_uint ("automatic-presence-type",
          "AutomaticPresence type",
          "Presence type used to put the account online automatically",
          0,
          TP_NUM_CONNECTION_PRESENCE_TYPES,
          TP_CONNECTION_PRESENCE_TYPE_UNSET,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:automatic-status:
   *
   * The string status name to use in conjunction with the
   * #TpFutureAccount:automatic-presence-type. To change this property
   * use tp_future_account_set_automatic_presence().
   */
  g_object_class_install_property (object_class, PROP_AUTOMATIC_STATUS,
      g_param_spec_string ("automatic-status",
          "AutomaticPresence status",
          "Presence status used to put the account online automatically",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:automatic-status-message:
   *
   * The user-defined message to use in conjunction with the
   * #TpAccount:automatic-presence-type. To change this property use
   * tp_future_account_set_automatic_presence().
   */
  g_object_class_install_property (object_class, PROP_AUTOMATIC_STATUS_MESSAGE,
      g_param_spec_string ("automatic-status-message",
          "AutomaticPresence message",
          "User-defined message used to put the account online automatically",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:enabled:
   *
   * Whether the account is enabled or not. To change this property
   * use tp_future_account_set_enabled().
   */
  g_object_class_install_property (object_class, PROP_ENABLED,
      g_param_spec_boolean ("enabled",
          "Enabled",
          "Whether this account is enabled or not",
          FALSE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpFutureAccount:connect-automatically:
   *
   * Whether the account should connect automatically or not. To change this
   * property, use tp_future_account_set_connect_automatically().
   */
  g_object_class_install_property (object_class, PROP_CONNECT_AUTOMATICALLY,
      g_param_spec_boolean ("connect-automatically",
          "ConnectAutomatically",
          "Whether this account should connect automatically or not",
          FALSE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));
}

/**
 * tp_future_account_new:
 * @account_manager: the #TpAccountManager to create the account on
 * @manager: the name of the connection manager
 * @protocol: the name of the protocol on @manager
 *
 * Convenience function to create a new future account object which
 * will assist in the creation of a new account on @account_manager,
 * using connection manager @manager, and protocol @protocol.
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
 * Set the display name for the new account, @self, to @name. Use the
 * #TpFutureAccount:display-name property to read the current display
 * name.
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
 * Set the icon name for the new account, @self, to @icon. Use the
 * #TpFutureAccount:icon-name property to read the current icon name.
 */
void
tp_future_account_set_icon_name (TpFutureAccount *self,
    const gchar *icon)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (icon != NULL);

  priv = self->priv;

  tp_asv_set_string (priv->properties, TP_PROP_ACCOUNT_ICON, icon);
}

/**
 * tp_future_account_set_nickname:
 * @self: a #TpFutureAccount
 * @nickname: a nickname for the account
 *
 * Set the nickname for the new account, @self, to @nickname. Use the
 * #TpFutureAccount:nickname property to read the current nickname.
 */
void
tp_future_account_set_nickname (TpFutureAccount *self,
    const gchar *nickname)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (nickname != NULL);

  priv = self->priv;

  tp_asv_set_string (priv->properties, TP_PROP_ACCOUNT_NICKNAME, nickname);
}

/**
 * tp_future_account_set_requested_presence:
 * @self: a #TpFutureAccount
 * @presence: the requested presence type
 * @status: the requested presence status
 * @message: the requested presence message
 *
 * Set the requested presence for the new account, @self, to the type
 * (@presence, @status), with message @message. Use the
 * #TpFutureAccount:requested-presence-type,
 * #TpFutureAccount:requested-status, and
 * #TpFutureAccount:requested-status-message properties to read the
 * current requested presence.
 */
void
tp_future_account_set_requested_presence (TpFutureAccount *self,
    TpConnectionPresenceType presence,
    const gchar *status,
    const gchar *message)
{
  TpFutureAccountPrivate *priv;
  GValue *value;
  GValueArray *arr;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));

  priv = self->priv;

  value = tp_g_value_slice_new_take_boxed (TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      dbus_g_type_specialized_construct (TP_STRUCT_TYPE_SIMPLE_PRESENCE));
  arr = (GValueArray *) g_value_get_boxed (value);

  g_value_set_uint (arr->values, presence);
  g_value_set_string (arr->values + 1, status);
  g_value_set_string (arr->values + 2, message);

  g_hash_table_insert (priv->properties,
      TP_PROP_ACCOUNT_REQUESTED_PRESENCE, value);
}

/**
 * tp_future_account_set_autmatic_presence:
 * @self: a #TpFutureAccount
 * @presence: the automatic presence type
 * @status: the automatic presence status
 * @message: the automatic presence message
 *
 * Set the automatic presence for the new account, @self, to the type
 * (@presence, @status), with message @message. Use the
 * #TpFutureAccount:automatic-presence-type,
 * #TpFutureAccount:automatic-status, and
 * #TpFutureAccount:automatic-status-message properties to read the
 * current automatic presence.
 */
void
tp_future_account_set_automatic_presence (TpFutureAccount *self,
    TpConnectionPresenceType presence,
    const gchar *status,
    const gchar *message)
{
  TpFutureAccountPrivate *priv;
  GValue *value;
  GValueArray *arr;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));

  priv = self->priv;

  value = tp_g_value_slice_new_take_boxed (TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      dbus_g_type_specialized_construct (TP_STRUCT_TYPE_SIMPLE_PRESENCE));
  arr = (GValueArray *) g_value_get_boxed (value);

  g_value_set_uint (arr->values, presence);
  g_value_set_string (arr->values + 1, status);
  g_value_set_string (arr->values + 2, message);

  g_hash_table_insert (priv->properties,
      TP_PROP_ACCOUNT_AUTOMATIC_PRESENCE, value);
}

/**
 * tp_future_account_set_enabled:
 * @self: a #TpFutureAccount
 * @enabled: %TRUE if the account is to be enabled
 *
 * Set the enabled property of the account on creation to
 * @enabled. Use the #TpFutureAccount:enabled property to read the
 * current enabled value.
 */
void
tp_future_account_set_enabled (TpFutureAccount *self,
    gboolean enabled)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));

  priv = self->priv;

  tp_asv_set_boolean (priv->properties, TP_PROP_ACCOUNT_ENABLED, enabled);
}

/**
 * tp_future_account_set_connect_automatically:
 * @self: a #TpFutureAccount
 * @connect_automatically: %TRUE if the account is to connect automatically
 *
 * Set the connect automatically property of the account on creation
 * to @connect_automatically so that the account is brought online to
 * the automatic presence. Use the
 * #TpFutureAccount:connect-automatically property to read the current
 * connect automatically value.
 */
void
tp_future_account_set_connect_automatically (TpFutureAccount *self,
    gboolean connect_automatically)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));

  priv = self->priv;

  tp_asv_set_boolean (priv->properties,
      TP_PROP_ACCOUNT_CONNECT_AUTOMATICALLY,
      connect_automatically);
}

/**
 * tp_future_account_set_parameter:
 * @self: a #TpFutureAccount
 * @key: the parameter key
 * @value: (transfer none): a variant containing the parameter value
 *
 * Set an account parameter, @key, to @value. Use the
 * #TpFutureAccount:parameters property to read the current list of
 * set parameters.
 *
 * Parameters can be unset using tp_future_account_unset_parameter().
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
 * tp_future_account_unset_parameter:
 * @self: a #TpFutureAccount
 * @key: the parameter key
 *
 * Unset the account parameter @key which has previously been set
 * using tp_future_account_set_parameter() or another convenience
 * function.
 */
void
tp_future_account_unset_parameter (TpFutureAccount *self,
    const gchar *key)
{
  TpFutureAccountPrivate *priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));
  g_return_if_fail (key != NULL);

  priv = self->priv;

  g_hash_table_remove (priv->parameters, key);
}

/**
 * tp_future_account_set_parameter_string: (skip)
 * @self: a #TpFutureAccount
 * @key: the parameter key
 * @value: the parameter value
 *
 * Convenience function to set an account parameter string value. See
 * tp_future_account_set_parameter() for more details.
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

static void
tp_future_account_account_prepared_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpFutureAccount *self = user_data;
  TpFutureAccountPrivate *priv = self->priv;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, result, &error))
    {
      DEBUG ("Error preparing account: %s", error->message);
      g_simple_async_result_take_error (priv->result, error);
    }

  g_simple_async_result_complete (priv->result);
  g_clear_object (&priv->result);
}

static void
tp_future_account_create_account_cb (TpAccountManager *proxy,
    const gchar *account_path,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (weak_object);
  TpFutureAccountPrivate *priv = self->priv;
  GError *e = NULL;
  TpAccount *account;
  GArray *features;

  if (error != NULL)
    {
      DEBUG ("failed to create account: %s", error->message);
      g_simple_async_result_set_from_error (priv->result, error);
      g_simple_async_result_complete (priv->result);
      g_clear_object (&priv->result);
      return;
    }

  account = tp_simple_client_factory_ensure_account (
      tp_proxy_get_factory (proxy), account_path, NULL, &e);

  if (account == NULL)
    {
      g_simple_async_result_take_error (priv->result, e);
      g_simple_async_result_complete (priv->result);
      g_clear_object (&priv->result);
      return;
    }

  /* Give account's ref to the result */
  g_simple_async_result_set_op_res_gpointer (priv->result, account,
      g_object_unref);

  features = tp_simple_client_factory_dup_account_features (
      tp_proxy_get_factory (proxy), account);

  tp_proxy_prepare_async (account, (GQuark *) features->data,
      tp_future_account_account_prepared_cb, self);

  g_array_unref (features);
}

/**
 * tp_future_account_create_account_async:
 * @self: a #TpFutureAccount
 * @callback: a function to call when the account has been created
 * @user_data: user data to @callback
 *
 * Start an asynchronous operation to create the account @self on the
 * account manager.
 *
 * @callback will only be called when the newly created #TpAccount has
 * the %TP_ACCOUNT_FEATURE_CORE feature ready on it, so when calling
 * tp_future_account_create_account_finish(), one can guarantee this
 * feature.
 */
void
tp_future_account_create_account_async (TpFutureAccount *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpFutureAccountPrivate *priv = self->priv;

  g_return_if_fail (TP_IS_FUTURE_ACCOUNT (self));

  priv = self->priv;

  if (priv->result != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self),
          callback, user_data,
          TP_ERROR, TP_ERROR_BUSY,
          "A account creation operation has already been started on this "
          "future account");
      return;
    }

  if (priv->account_manager == NULL
      || priv->cm_name == NULL
      || priv->proto_name == NULL
      || priv->display_name == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self),
          callback, user_data,
          TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Future account is missing one or more of: account manager, "
          "connection manager name, protocol name, or display name");
      return;
    }

  priv->result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_future_account_create_account_async);

  tp_cli_account_manager_call_create_account (priv->account_manager,
      -1, priv->cm_name, priv->proto_name, priv->display_name,
      priv->parameters, priv->properties,
      tp_future_account_create_account_cb, NULL, NULL, G_OBJECT (self));
}

/**
 * tp_future_account_create_account_finish:
 * @self: a #TpFutureAccount
 * @result: a #GAsyncResult
 * @error: something
 *
 * Finishes an asynchronous account creation operation and returns a
 * new #TpAccount object, with the %TP_ACCOUNT_FEATURE_CORE feature
 * prepared on it.
 *
 * Returns: (transfer full) a new #TpAccount which was just created on
 *   success, otherwise %NULL
 */

TpAccount *
tp_future_account_create_account_finish (TpFutureAccount *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_return_copy_pointer (self,
      tp_future_account_create_account_async, g_object_ref);
}
