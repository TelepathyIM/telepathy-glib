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

#include <string.h>

#include "telepathy-glib/account.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/signals-marshal.h"
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
 * Since: 0.7.32
 */

/**
 * TpAccountClass:
 *
 * The class of a #TpAccount.
 */

struct _TpAccountPrivate {
  gboolean dispose_has_run;

  TpConnection *connection;
  guint connection_invalidated_id;

  TpConnectionStatus connection_status;
  TpConnectionStatusReason reason;

  TpConnectionPresenceType presence;
  gchar *status;
  gchar *message;

  TpConnectionPresenceType requested_presence;
  gchar *requested_status;
  gchar *requested_message;

  gboolean connect_automatically;
  gboolean has_been_online;

  gboolean enabled;
  gboolean valid;
  gboolean ready;
  gboolean removed;
  /* Timestamp when the connection got connected in seconds since the epoch */
  glong connect_time;

  gchar *cm_name;
  gchar *proto_name;
  gchar *icon_name;

  gchar *unique_name;
  gchar *display_name;
  TpDBusDaemon *dbus;

  GHashTable *parameters;
};

G_DEFINE_TYPE (TpAccount, tp_account, TP_TYPE_PROXY);

/* signals */
enum {
  STATUS_CHANGED,
  PRESENCE_CHANGED,
  REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* properties */
enum {
  PROP_ENABLED = 1,
  PROP_PRESENCE,
  PROP_STATUS,
  PROP_STATUS_MESSAGE,
  PROP_READY,
  PROP_CONNECTION_STATUS,
  PROP_CONNECTION_STATUS_REASON,
  PROP_CONNECTION,
  PROP_UNIQUE_NAME,
  PROP_DBUS_DAEMON,
  PROP_DISPLAY_NAME
};

static void
tp_account_init (TpAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_ACCOUNT,
      TpAccountPrivate);

  self->priv->connection_status = TP_CONNECTION_STATUS_DISCONNECTED;
}

static void
_tp_account_removed_cb (TpAccount *self,
    gpointer unused G_GNUC_UNUSED,
    GObject *object G_GNUC_UNUSED)
{
  GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
               "Account removed" };

  if (self->priv->removed)
    return;

  self->priv->removed = TRUE;

  tp_proxy_invalidate ((TpProxy *) self, &e);

  g_signal_emit (self, signals[REMOVED], 0);
}

static gchar *
_tp_account_unescape_protocol (const gchar *protocol,
    gssize len)
{
  gchar *result, *escape;
  /* Bad implementation might accidentally use tp_escape_as_identifier,
   * which escapes - in the wrong way... */
  if ((escape = g_strstr_len (protocol, len, "_2d")) != NULL)
    {
      GString *str;
      const gchar *input;

      str = g_string_new ("");
      input = protocol;
      do {
        g_string_append_len (str, input, escape - input);
        g_string_append_c (str, '-');

        len -= escape - input + 3;
        input = escape + 3;
      } while ((escape = g_strstr_len (input, len, "_2d")) != NULL);

      g_string_append_len (str, input, len);

      result = g_string_free (str, FALSE);
    }
  else
    {
      result = g_strndup (protocol, len);
    }

  g_strdelimit (result, "_", '-');

  return result;
}

static gboolean
_tp_account_parse_unique_name (const gchar *bus_name,
    gchar **protocol,
    gchar **manager)
{
  const gchar *proto, *proto_end;
  const gchar *cm, *cm_end;

  g_return_val_if_fail (
      g_str_has_prefix (bus_name, TP_ACCOUNT_OBJECT_PATH_BASE), FALSE);

  cm = bus_name + strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  for (cm_end = cm; *cm_end != '/' && *cm_end != '\0'; cm_end++)
    /* pass */;

  if (*cm_end == '\0')
    return FALSE;

  if (cm_end == '\0')
    return FALSE;

  proto = cm_end + 1;

  for (proto_end = proto; *proto_end != '/' && *proto_end != '\0'; proto_end++)
    /* pass */;

  if (*proto_end == '\0')
    return FALSE;

  if (protocol != NULL)
    *protocol = _tp_account_unescape_protocol (proto, proto_end - proto);

  if (manager != NULL)
    *manager = g_strndup (cm, cm_end - cm);

  return TRUE;
}

static void
_tp_account_free_connection (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;
  TpConnection *conn;

  if (priv->connection == NULL)
    return;

  conn = priv->connection;
  priv->connection = NULL;

  if (priv->connection_invalidated_id != 0)
    g_signal_handler_disconnect (conn, priv->connection_invalidated_id);
  priv->connection_invalidated_id = 0;

  g_object_unref (conn);
}

static void
_tp_account_connection_invalidated_cb (TpProxy *self,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TpAccount *account = TP_ACCOUNT (user_data);
  TpAccountPrivate *priv = account->priv;

  if (priv->connection == NULL)
    return;

  DEBUG ("(%s) Connection invalidated",
      tp_account_get_unique_name (account));

  g_assert (priv->connection == TP_CONNECTION (self));

  _tp_account_free_connection (account);

  g_object_notify (G_OBJECT (account), "connection");
}

static void
_tp_account_connection_ready_cb (TpConnection *connection,
    const GError *error,
    gpointer user_data)
{
  TpAccount *account = TP_ACCOUNT (user_data);

  if (error != NULL)
    {
      DEBUG ("(%s) Connection failed to become ready: %s",
          tp_account_get_unique_name (account), error->message);
      _tp_account_free_connection (account);
    }
  else
    {
      DEBUG ("(%s) Connection ready",
          tp_account_get_unique_name (account));
      g_object_notify (G_OBJECT (account), "connection");
    }
}

static void
_tp_account_set_connection (TpAccount *account,
    const gchar *path)
{
  TpAccountPrivate *priv = account->priv;

  if (priv->connection != NULL)
    {
      const gchar *current;

      current = tp_proxy_get_object_path (priv->connection);
      if (!tp_strdiff (current, path))
        return;
    }

  _tp_account_free_connection (account);

  if (tp_strdiff ("/", path))
    {
      GError *error = NULL;
      priv->connection = tp_connection_new (priv->dbus, NULL, path, &error);

      if (priv->connection == NULL)
        {
          DEBUG ("Failed to create a new TpConnection: %s",
              error->message);
          g_error_free (error);
        }
      else
        {
          priv->connection_invalidated_id = g_signal_connect (priv->connection,
              "invalidated",
              G_CALLBACK (_tp_account_connection_invalidated_cb), account);

          DEBUG ("Readying connection for %s", priv->unique_name);
          /* notify a change in the connection property when it's ready */
          tp_connection_call_when_ready (priv->connection,
              _tp_account_connection_ready_cb, account);
        }
    }

  g_object_notify (G_OBJECT (account), "connection");
}

static void
_tp_account_update (TpAccount *account,
    GHashTable *properties)
{
  TpAccountPrivate *priv = account->priv;
  GValueArray *arr;
  TpConnectionStatus old_s = priv->connection_status;
  gboolean presence_changed = FALSE;

  if (g_hash_table_lookup (properties, "ConnectionStatus") != NULL)
    priv->connection_status =
      tp_asv_get_int32 (properties, "ConnectionStatus", NULL);

  if (g_hash_table_lookup (properties, "ConnectionStatusReason") != NULL)
    priv->reason = tp_asv_get_int32 (properties,
        "ConnectionStatusReason", NULL);

  if (g_hash_table_lookup (properties, "CurrentPresence") != NULL)
    {
      presence_changed = TRUE;
      arr = tp_asv_get_boxed (properties, "CurrentPresence",
          TP_STRUCT_TYPE_SIMPLE_PRESENCE);
      priv->presence = g_value_get_uint (g_value_array_get_nth (arr, 0));

      g_free (priv->status);
      priv->status = g_value_dup_string (g_value_array_get_nth (arr, 1));

      g_free (priv->message);
      priv->message = g_value_dup_string (g_value_array_get_nth (arr, 2));
    }

  if (g_hash_table_lookup (properties, "RequestedPresence") != NULL)
    {
      arr = tp_asv_get_boxed (properties, "RequestedPresence",
          TP_STRUCT_TYPE_SIMPLE_PRESENCE);
      priv->requested_presence =
        g_value_get_uint (g_value_array_get_nth (arr, 0));

      g_free (priv->requested_status);
      priv->requested_status =
        g_value_dup_string (g_value_array_get_nth (arr, 1));

      g_free (priv->requested_message);
      priv->requested_message =
        g_value_dup_string (g_value_array_get_nth (arr, 2));
    }

  if (g_hash_table_lookup (properties, "DisplayName") != NULL)
    {
      g_free (priv->display_name);
      priv->display_name =
        g_strdup (tp_asv_get_string (properties, "DisplayName"));
      g_object_notify (G_OBJECT (account), "display-name");
    }

  if (g_hash_table_lookup (properties, "Icon") != NULL)
    {
      const gchar *icon_name;

      icon_name = tp_asv_get_string (properties, "Icon");

      g_free (priv->icon_name);

      if (icon_name == NULL || icon_name[0] == '\0')
        priv->icon_name = g_strdup_printf ("im-%s", priv->proto_name);
      else
        priv->icon_name = g_strdup (icon_name);
    }

  if (g_hash_table_lookup (properties, "Enabled") != NULL)
    {
      gboolean enabled = tp_asv_get_boolean (properties, "Enabled", NULL);
      if (priv->enabled != enabled)
        {
          priv->enabled = enabled;
          g_object_notify (G_OBJECT (account), "enabled");
        }
    }

  if (g_hash_table_lookup (properties, "Valid") != NULL)
    priv->valid = tp_asv_get_boolean (properties, "Valid", NULL);

  if (g_hash_table_lookup (properties, "Parameters") != NULL)
    {
      GHashTable *parameters;

      parameters = tp_asv_get_boxed (properties, "Parameters",
          TP_HASH_TYPE_STRING_VARIANT_MAP);

      if (priv->parameters != NULL)
        g_hash_table_unref (priv->parameters);

      priv->parameters = g_boxed_copy (TP_HASH_TYPE_STRING_VARIANT_MAP,
          parameters);
    }

  if (!priv->ready)
    {
      priv->ready = TRUE;
      g_object_notify (G_OBJECT (account), "ready");
    }

  if (priv->connection_status != old_s)
    {
      if (priv->connection_status == TP_CONNECTION_STATUS_CONNECTED)
        {
          GTimeVal val;
          g_get_current_time (&val);

          priv->connect_time = val.tv_sec;
        }

      g_signal_emit (account, signals[STATUS_CHANGED], 0,
          old_s, priv->connection_status, priv->reason);

      g_object_notify (G_OBJECT (account), "connection-status");
      g_object_notify (G_OBJECT (account), "connection-status-reason");
    }

  if (presence_changed)
    {
      g_signal_emit (account, signals[PRESENCE_CHANGED], 0,
          priv->presence, priv->status, priv->message);
      g_object_notify (G_OBJECT (account), "presence");
      g_object_notify (G_OBJECT (account), "status");
      g_object_notify (G_OBJECT (account), "status-message");
    }

  if (g_hash_table_lookup (properties, "Connection") != NULL)
    {
      const gchar *conn_path =
        tp_asv_get_object_path (properties, "Connection");

      _tp_account_set_connection (account, conn_path);
    }

  if (g_hash_table_lookup (properties, "ConnectAutomatically") != NULL)
    {
      priv->connect_automatically =
        tp_asv_get_boolean (properties, "ConnectAutomatically", NULL);
    }

  if (g_hash_table_lookup (properties, "HasBeenOnline") != NULL)
    {
      priv->has_been_online =
        tp_asv_get_boolean (properties, "HasBeenOnline", NULL);
    }
}

static void
_tp_account_properties_changed (TpAccount *proxy,
    GHashTable *properties,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccount *self = TP_ACCOUNT (weak_object);

  if (!self->priv->ready)
    return;

  _tp_account_update (self, properties);
}

static void
_tp_account_constructed (GObject *object)
{
  TpAccount *self = TP_ACCOUNT (object);
  TpAccountPrivate *priv = self->priv;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_account_parent_class)->constructed;
  GError *error = NULL;
  TpProxySignalConnection *sc;

  if (chain_up != NULL)
    chain_up (object);

  g_return_if_fail (tp_proxy_get_dbus_daemon (self) != NULL);

  sc = tp_cli_account_connect_to_removed (self, _tp_account_removed_cb,
      NULL, NULL, NULL, &error);

  if (sc == NULL)
    {
      g_critical ("Couldn't connect to Removed: %s", error->message);
      g_error_free (error);
    }

  _tp_account_parse_unique_name (priv->unique_name,
      &(priv->proto_name), &(priv->cm_name));

  priv->icon_name = g_strdup_printf ("im-%s", priv->proto_name);

  tp_cli_account_connect_to_account_property_changed (self,
      _tp_account_properties_changed, NULL, NULL, object, NULL);

  tp_account_refresh_properties (self);
}

static void
_tp_account_set_property (GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpAccount *self = TP_ACCOUNT (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      tp_account_set_enabled_async (self,
          g_value_get_boolean (value), NULL, NULL);
      break;
    case PROP_UNIQUE_NAME:
      self->priv->unique_name = g_value_dup_string (value);
      break;
    case PROP_DBUS_DAEMON:
      self->priv->dbus = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_tp_account_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpAccount *self = TP_ACCOUNT (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->priv->enabled);
      break;
    case PROP_READY:
      g_value_set_boolean (value, self->priv->ready);
      break;
    case PROP_PRESENCE:
      g_value_set_uint (value, self->priv->presence);
      break;
    case PROP_STATUS:
      g_value_set_string (value, self->priv->status);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, self->priv->message);
      break;
    case PROP_CONNECTION_STATUS:
      g_value_set_uint (value, self->priv->connection_status);
      break;
    case PROP_CONNECTION_STATUS_REASON:
      g_value_set_uint (value, self->priv->reason);
      break;
    case PROP_CONNECTION:
      g_value_set_object (value,
          tp_account_get_connection (self));
      break;
    case PROP_UNIQUE_NAME:
      g_value_set_string (value,
          tp_account_get_unique_name (self));
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value,
          tp_account_get_display_name (self));
      break;
    case PROP_DBUS_DAEMON:
      g_value_set_object (value, self->priv->dbus);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_tp_account_dispose (GObject *object)
{
  TpAccount *self = TP_ACCOUNT (object);
  TpAccountPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  _tp_account_free_connection (self);

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (tp_account_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (tp_account_parent_class)->dispose (object);
}

static void
_tp_account_finalize (GObject *object)
{
  TpAccount *self = TP_ACCOUNT (object);
  TpAccountPrivate *priv = self->priv;

  g_free (priv->status);
  g_free (priv->message);
  g_free (priv->requested_status);
  g_free (priv->requested_message);

  g_free (priv->cm_name);
  g_free (priv->proto_name);
  g_free (priv->icon_name);
  g_free (priv->display_name);

  /* free any data held directly by the object here */
  if (G_OBJECT_CLASS (tp_account_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (tp_account_parent_class)->finalize (object);
}

static void
tp_account_class_init (TpAccountClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpAccountPrivate));

  object_class->constructed = _tp_account_constructed;
  object_class->get_property = _tp_account_get_property;
  object_class->set_property = _tp_account_set_property;
  object_class->dispose = _tp_account_dispose;
  object_class->finalize = _tp_account_finalize;

  /**
   * TpAccount:enabled:
   *
   * Whether this account is enabled or not.
   */
  g_object_class_install_property (object_class, PROP_ENABLED,
      g_param_spec_boolean ("enabled",
          "Enabled",
          "Whether this account is enabled or not",
          FALSE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  /**
   * TpAccount:ready:
   *
   * Whether this account is ready to be used or not.
   */
  g_object_class_install_property (object_class, PROP_READY,
      g_param_spec_boolean ("ready",
          "Ready",
          "Whether this account is ready to be used",
          FALSE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:presence:
   *
   * The account connection's presence type.
   */
  g_object_class_install_property (object_class, PROP_PRESENCE,
      g_param_spec_uint ("presence",
          "Presence",
          "The account connections presence type",
          0,
          NUM_TP_CONNECTION_PRESENCE_TYPES,
          TP_CONNECTION_PRESENCE_TYPE_UNSET,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:status:
   *
   * The Status string of the account.
   */
  g_object_class_install_property (object_class, PROP_STATUS,
      g_param_spec_string ("status",
          "Status",
          "The Status string of the account",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount: status-message:
   *
   * The status message message of the account.
   */
  g_object_class_install_property (object_class, PROP_STATUS_MESSAGE,
      g_param_spec_string ("status-message",
          "status-message",
          "The Status message string of the account",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:connection-status:
   *
   * The account's connection status type.
   */
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS,
      g_param_spec_uint ("connection-status",
          "ConnectionStatus",
          "The accounts connections status type",
          0,
          NUM_TP_CONNECTION_STATUSES,
          TP_CONNECTION_STATUS_DISCONNECTED,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:connection-status-reason:
   *
   * The account's connection status reason.
   */
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS_REASON,
      g_param_spec_uint ("connection-status-reason",
          "ConnectionStatusReason",
          "The account connections status reason",
          0,
          NUM_TP_CONNECTION_STATUS_REASONS,
          TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:connection:
   *
   * The account's connection.
   */
  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection",
          "Connection",
          "The accounts connection",
          TP_TYPE_CONNECTION,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount:unique-name:
   *
   * The account's unique name.
   */
  g_object_class_install_property (object_class, PROP_UNIQUE_NAME,
      g_param_spec_string ("unique-name",
          "UniqueName",
          "The accounts unique name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * TpAccount:dbus-daemon:
   *
   * The #TpDBusDaemon on which this account exists.
   */
  g_object_class_install_property (object_class, PROP_DBUS_DAEMON,
      g_param_spec_object ("dbus-daemon",
          "dbus-daemon",
          "The Tp Dbus daemon on which this account exists",
          TP_TYPE_DBUS_DAEMON,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * TpAccount:display-name:
   *
   * The account's display name.
   */
  g_object_class_install_property (object_class, PROP_DISPLAY_NAME,
      g_param_spec_string ("display-name",
          "DisplayName",
          "The accounts display name",
          NULL,
          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  /**
   * TpAccount::status-changed:
   * @account: the #TpAccount
   * @old_status: old connection status
   * @new_status: new connection status
   * @reason: the reason for the status change
   *
   * Emitted when the connection status on the account changes.
   */
  signals[STATUS_CHANGED] = g_signal_new ("status-changed",
      G_TYPE_FROM_CLASS (object_class),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tp_marshal_VOID__UINT_UINT_UINT,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * TpAccount::presence-changed:
   * @account: the #TpAccount
   * @presence: the new presence
   * @status: the new presence status
   * @status_message: the new presence status message
   *
   * Emitted when the presence of the account changes.
   */
  signals[PRESENCE_CHANGED] = g_signal_new ("presence-changed",
      G_TYPE_FROM_CLASS (object_class),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tp_marshal_VOID__UINT_STRING_STRING,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * TpAccount::removed:
   * @account: the #TpAccount
   *
   * Emitted when the account is removed.
   */
  signals[REMOVED] = g_signal_new ("removed",
      G_TYPE_FROM_CLASS (object_class),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

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

static void
_tp_account_got_all_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpAccount *self = TP_ACCOUNT (weak_object);

  DEBUG ("Got whole set of properties for %s",
      tp_account_get_unique_name (self));

  if (error != NULL)
    {
      DEBUG ("Failed to get the initial set of account properties: %s",
          error->message);
      return;
    }

  _tp_account_update (self, properties);
}

/**
 * tp_account_is_just_connected:
 * @account: a #TpAccount
 *
 * Returns whether @account has connected in the last ten seconds. This
 * is useful for determining whether the account has only just come online, or
 * whether its status has simply changed.
 *
 * Returns: whether @account has only just connected
 */
gboolean
tp_account_is_just_connected (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;
  GTimeVal val;

  if (priv->connection_status != TP_CONNECTION_STATUS_CONNECTED)
    return FALSE;

  g_get_current_time (&val);

  return (val.tv_sec - priv->connect_time) < 10;
}

/**
 * tp_account_get_connection:
 * @account: a #TpAccount
 *
 * Get the connection of the account, or NULL if account is offline or the
 * connection is not yet ready. This function does not return a new ref.
 *
 * Returns: the connection of the account.
 **/
TpConnection *
tp_account_get_connection (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  if (priv->connection != NULL &&
      tp_connection_is_ready (priv->connection))
    return priv->connection;

  return NULL;
}

/**
 * tp_account_get_connection_for_path:
 * @account: a #TpAccount
 * @path: the path to connection object for #TpAccount
 *
 * Get the connection of the account on path. This function does not return a
 * new ref. It is not guaranteed that the returned connection object is ready
 *
 * Returns: the connection of the account.
 **/
TpConnection *
tp_account_get_connection_for_path (TpAccount *account,
    const gchar *path)
{
  TpAccountPrivate *priv = account->priv;

  /* double-check that the object path is valid */
  if (!tp_dbus_check_valid_object_path (path, NULL))
    return NULL;

  /* Should be a full object path, not the special "/" value */
  if (strlen (path) == 1)
    return NULL;

  _tp_account_set_connection (account, path);

  return priv->connection;
}

/**
 * tp_account_get_unique_name:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the unique name of @account
 **/
const gchar *
tp_account_get_unique_name (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->unique_name;
}

/**
 * tp_account_get_display_name:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the display name of @account
 **/
const gchar *
tp_account_get_display_name (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->display_name;
}

/**
 * tp_account_is_valid:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: whether @account is valid
 */
gboolean
tp_account_is_valid (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->valid;
}

/**
 * tp_account_get_connection_manager:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the name of the connection manager @account uses
 */
const gchar *
tp_account_get_connection_manager (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->cm_name;
}

/**
 * tp_account_get_protocol:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the protocol name @account uses
 */
const gchar *
tp_account_get_protocol (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->proto_name;
}

/**
 * tp_account_get_icon_name:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the Icon property on @account
 */
const gchar *
tp_account_get_icon_name (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->icon_name;
}

/**
 * tp_account_get_parameters:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the hash table of parameters on @account
 */
const GHashTable *
tp_account_get_parameters (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->parameters;
}

/**
 * tp_account_is_enabled:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the Enabled property on @account
 */
gboolean
tp_account_is_enabled (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->enabled;
}

/**
 * tp_account_is_ready:
 * @account: a #TpAccount
 *
 * <!-- -->
 *
 * Returns: the same thing as the #TpAccount:ready property
 */
gboolean
tp_account_is_ready (TpAccount *account)
{
  TpAccountPrivate *priv = account->priv;

  return priv->ready;
}

static void
_tp_account_property_set_cb (TpProxy *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to set property: %s", error->message);
      g_simple_async_result_set_from_error (result, (GError *) error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_account_set_enabled_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async set of the Enabled property.
 *
 * Returns: %TRUE if the set was successful, otherwise %FALSE
 */
gboolean
tp_account_set_enabled_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_set_enabled_finish))
    return FALSE;

  return TRUE;
}

/**
 * tp_account_set_enabled_async:
 * @account: a #TpAccount
 * @enabled: the new enabled value of @account
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous set of the Enabled property of @account. When the
 * operation is finished, @callback will be called. You can then call
 * tp_account_set_enabled_finish() to get the result of the operation.
 */
void
tp_account_set_enabled_async (TpAccount *account,
    gboolean enabled,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  /* Disabled for now due to lack of account manager */

#if 0
  TpAccountPrivate *priv = account->priv;
  TpAccountManager *acc_manager;
  GValue value = {0, };
  GSimpleAsyncResult *result;
  char *status = NULL;
  char *status_message = NULL;
  TpConnectionPresenceType presence;

  result = g_simple_async_result_new (G_OBJECT (account),
      callback, user_data, tp_account_set_enabled_finish);

  if (priv->enabled == enabled)
    {
      g_simple_async_result_complete_in_idle (result);
      return;
    }

  if (enabled)
    {
      acc_manager = tp_account_manager_dup_singleton ();
      presence = tp_account_manager_get_requested_global_presence (
          acc_manager, &status, &status_message);

      if (presence != TP_CONNECTION_PRESENCE_TYPE_UNSET)
        tp_account_request_presence (account, presence, status,
            status_message);

      g_object_unref (acc_manager);
      g_free (status);
      g_free (status_message);
    }

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, enabled);

  tp_cli_dbus_properties_call_set (TP_PROXY (account),
      -1, TP_IFACE_ACCOUNT, "Enabled", &value,
      _tp_account_property_set_cb, result, NULL, G_OBJECT (account));
#endif
}

static void
_tp_account_reconnected_cb (TpAccount *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    g_simple_async_result_set_from_error (result, (GError *) error);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_account_reconnect_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to be filled
 *
 * Finishes an async reconnect of @account.
 *
 * Returns: %TRUE if the reconnect call was successful, otherwise %FALSE
 */
gboolean
tp_account_reconnect_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_reconnect_finish))
    return FALSE;

  return TRUE;
}

/**
 * tp_account_reconnect_async:
 * @account: a #TpAccount
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous reconnect of @account. When the operation is
 * finished, @callback will be called. You can then call
 * tp_account_reconnect_finish() to get the result of the operation.
 */

void
tp_account_reconnect_async (TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (account),
      callback, user_data, tp_account_reconnect_finish);

  tp_cli_account_call_reconnect (account, -1, _tp_account_reconnected_cb,
      result, NULL, G_OBJECT (account));
}

/**
 * tp_account_request_presence_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async presence change request on @account.
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE
 */
gboolean
tp_account_request_presence_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_request_presence_finish))
    return FALSE;

  return TRUE;
}

/**
 * tp_account_request_presence_async:
 * @account: a #TpAccount
 * @type: the requested presence
 * @status: a status message to set, or %NULL
 * @message: a message for the change, or %NULL
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous change of presence on @account. When the
 * operation is finished, @callback will be called. You can then call
 * tp_account_request_presence_finish() to get the result of the operation.
 */
void
tp_account_request_presence_async (TpAccount *account,
    TpConnectionPresenceType type,
    const gchar *status,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GValue value = {0, };
  GValueArray *arr;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (account),
      callback, user_data, tp_account_request_presence_finish);

  g_value_init (&value, TP_STRUCT_TYPE_SIMPLE_PRESENCE);
  g_value_take_boxed (&value, dbus_g_type_specialized_construct (
          TP_STRUCT_TYPE_SIMPLE_PRESENCE));
  arr = (GValueArray *) g_value_get_boxed (&value);

  g_value_set_uint (arr->values, type);
  g_value_set_static_string (arr->values + 1, status);
  g_value_set_static_string (arr->values + 2, message);

  tp_cli_dbus_properties_call_set (TP_PROXY (account), -1,
      TP_IFACE_ACCOUNT, "RequestedPresence", &value,
      _tp_account_property_set_cb, result, NULL, G_OBJECT (account));

  g_value_unset (&value);
}

static void
_tp_account_updated_cb (TpAccount *proxy,
    const gchar **reconnect_required,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);

  if (error != NULL)
    g_simple_async_result_set_from_error (result, (GError *) error);

  g_simple_async_result_complete (result);
  g_object_unref (G_OBJECT (result));
}

/**
 * tp_account_update_parameters_async:
 * @account: a #TpAccount
 * @parameters: new parameters to set on @account
 * @unset_parameters: list of parameters to unset on @account
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous update of parameters of @account. When the
 * operation is finished, @callback will be called. You can then call
 * tp_account_update_parameters_finish() to get the result of the operation.
 */
void
tp_account_update_parameters_async (TpAccount *account,
    GHashTable *parameters,
    const gchar **unset_parameters,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (account),
      callback, user_data, tp_account_update_parameters_finish);

  tp_cli_account_call_update_parameters (account, -1, parameters,
      unset_parameters, _tp_account_updated_cb, result,
      NULL, G_OBJECT (account));
}

/**
 * tp_account_update_parameters_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async update of the parameters on @account.
 *
 * Returns: %TRUE if the request succeeded, otherwise %FALSE
 */
gboolean
tp_account_update_parameters_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
      error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
    G_OBJECT (account), tp_account_update_parameters_finish), FALSE);

  return TRUE;
}

/**
 * tp_account_set_display_name_async:
 * @account: a #TpAccount
 * @display_name: a new display name to set on @account
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous set of the DisplayName property of @account. When
 * the operation is finished, @callback will be called. You can then call
 * tp_account_set_display_name_finish() to get the result of the operation.
 */
void
tp_account_set_display_name_async (TpAccount *account,
    const char *display_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GValue value = {0, };

  if (display_name == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (account),
          callback, user_data, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
          "Can't set an empty display name");
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, tp_account_set_display_name_finish);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, display_name);

  tp_cli_dbus_properties_call_set (account, -1, TP_IFACE_ACCOUNT,
      "DisplayName", &value, _tp_account_property_set_cb, result, NULL,
      G_OBJECT (account));
}

/**
 * tp_account_set_display_name_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async set of the DisplayName property.
 *
 * Returns: %TRUE if the call was successful, otherwise %FALSE
 */
gboolean
tp_account_set_display_name_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_set_display_name_finish))
    return FALSE;

  return TRUE;
}

/**
 * tp_account_set_icon_name_async:
 * @account: a #TpAccount
 * @icon_name: a new icon name
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous set of the Icon property of @account. When
 * the operation is finished, @callback will be called. You can then call
 * tp_account_set_icon_name_finish() to get the result of the operation.
 */
void
tp_account_set_icon_name_async (TpAccount *account,
    const char *icon_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GValue value = {0, };
  const char *icon_name_set;

  if (icon_name == NULL)
    /* settings an empty icon name is allowed */
    icon_name_set = "";
  else
    icon_name_set = icon_name;

  result = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, tp_account_set_icon_name_finish);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, icon_name_set);

  tp_cli_dbus_properties_call_set (account, -1, TP_IFACE_ACCOUNT,
      "Icon", &value, _tp_account_property_set_cb, result, NULL,
      G_OBJECT (account));
}

/**
 * tp_account_set_icon_name_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async set of the Icon parameter.
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE
 */
gboolean
tp_account_set_icon_name_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_set_icon_name_finish))
    return FALSE;

  return TRUE;
}

static void
_tp_account_remove_cb (TpAccount *proxy,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (user_data);

  if (error != NULL)
    g_simple_async_result_set_from_error (result, (GError *) error);

  g_simple_async_result_complete (result);
  g_object_unref (G_OBJECT (result));
}

/**
 * tp_account_remove_async:
 * @account: a #TpAccount
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous removal of @account. When the operation is
 * finished, @callback will be called. You can then call
 * tp_account_remove_finish() to get the result of the operation.
 */
void
tp_account_remove_async (TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result = g_simple_async_result_new (G_OBJECT (account),
      callback, user_data, tp_account_remove_finish);

  tp_cli_account_call_remove (account, -1, _tp_account_remove_cb, result, NULL,
      G_OBJECT (account));
}

/**
 * tp_account_remove_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async removal of @account.
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE
 */
gboolean
tp_account_remove_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (account), tp_account_remove_finish), FALSE);

  return TRUE;
}

/**
 * tp_account_refresh_properties:
 * @account: a #TpAccount
 *
 * Refreshes @account's hashtable of properties with what actually exists on
 * the account manager.
 */
void
tp_account_refresh_properties (TpAccount *account)
{
  tp_cli_dbus_properties_call_get_all (account, -1, TP_IFACE_ACCOUNT,
      _tp_account_got_all_cb, NULL, NULL, G_OBJECT (account));
}

/**
 * tp_account_get_connect_automatically:
 * @account: a #TpAccount
 *
 * Gets the ConnectAutomatically parameter on @account.
 *
 * Returns: the value of the ConnectAutomatically parameter on @account
 */
gboolean
tp_account_get_connect_automatically (TpAccount *account)
{
  return account->priv->connect_automatically;
}

/**
 * tp_account_set_connect_automatically_async:
 * @account: a #TpAccount
 * @connect_automatically: new value for the parameter
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Requests an asynchronous set of the ConnectAutomatically property of
 * @account. When the operation is finished, @callback will be called. You can
 * then call tp_account_set_display_name_finish() to get the result of the
 * operation.
 */
void
tp_account_set_connect_automatically_async (TpAccount *account,
    gboolean connect_automatically,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GValue value = {0, };

  result = g_simple_async_result_new (G_OBJECT (account), callback,
      user_data, tp_account_set_connect_automatically_finish);

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, connect_automatically);

  tp_cli_dbus_properties_call_set (account, -1, TP_IFACE_ACCOUNT,
      "ConnectAutomatically", &value, _tp_account_property_set_cb, result,
      NULL, G_OBJECT (account));
}

/**
 * tp_account_set_connect_automatically_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes an async set of the ConnectAutomatically property.
 *
 * Returns: %TRUE if the call was successful, otherwise %FALSE
 */
gboolean
tp_account_set_connect_automatically_finish (TpAccount *account,
    GAsyncResult *result,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
          error) ||
      !g_simple_async_result_is_valid (result, G_OBJECT (account),
          tp_account_set_connect_automatically_finish))
    return FALSE;

  return TRUE;
}

/**
 * tp_account_get_has_been_online:
 * @account: a #TpAccount
 *
 * Gets the HasBeenOnline parameter on @account.
 *
 * Returns: the value of the HasBeenOnline parameter on @account
 */
gboolean
tp_account_get_has_been_online (TpAccount *account)
{
  return account->priv->has_been_online;
}

/**
 * tp_account_get_connection_status:
 * @account: a #TpAccount
 *
 * Gets the ConnectionStatus parameter on @account.
 *
 * Returns: the value of the ConnectionStatus parameter on @account
 */
TpConnectionStatus
tp_account_get_connection_status (TpAccount *account)
{
  return account->priv->connection_status;
}

/**
 * tp_account_get_connection_status_reason:
 * @account: a #TpAccount
 *
 * Gets the ConnectionStatusReason parameter on @account.
 *
 * Returns: the value of the ConnectionStatusReason parameter on @account
 */
TpConnectionStatusReason
tp_account_get_connection_status_reason (TpAccount *account)
{
  return account->priv->reason;
}

/**
 * tp_account_get_presence_type:
 * @account: a #TpAccount
 *
 * Gets the type from the CurrentPresence parameter on @account.
 *
 * Returns: the type from the CurrentPresence parameter on @account
 */
TpConnectionPresenceType
tp_account_get_presence_type (TpAccount *account)
{
  return account->priv->presence;
}

/**
 * tp_account_get_presence_status:
 * @account: a #TpAccount
 *
 * Gets the status from the CurrentPresence parameter on @account.
 *
 * Returns: the status from the CurrentPresence parameter on @account
 */
const gchar *
tp_account_get_presence_status (TpAccount *account)
{
  return account->priv->status;
}

/**
 * tp_account_get_presence_message:
 * @account: a #TpAccount
 *
 * Gets the message from the CurrentPresence parameter on @account.
 *
 * Returns: the message from the CurrentPresence parameter on @account
 */
const gchar *
tp_account_get_presence_message (TpAccount *account)
{
  return account->priv->message;
}

/**
 * tp_account_get_requested_presence_type:
 * @account: a #TpAccount
 *
 * Gets the presence from the RequestedPresence parameter on @account.
 *
 * Returns: the presence from the RequestedPresence parameter on @account
 */
TpConnectionPresenceType
tp_account_get_requested_presence_type (TpAccount *account)
{
  return account->priv->requested_presence;
}

/**
 * tp_account_get_requested_presence_status:
 * @account: a #TpAccount
 *
 * Gets the status from the RequestedPresence parameter on @account.
 *
 * Returns: the status from the RequestedPresence parameter on @account
 */
const gchar *
tp_account_get_requested_presence_status (TpAccount *account)
{
  return account->priv->requested_status;
}

/**
 * tp_account_get_requested_presence_message:
 * @account: a #TpAccount
 *
 * Gets the message from the RequestedPresence parameter on @account.
 *
 * Returns: the message from the RequestedPresence parameter on @account
 */
const gchar *
tp_account_get_requested_presence_message (TpAccount *account)
{
  return account->priv->requested_message;
}
