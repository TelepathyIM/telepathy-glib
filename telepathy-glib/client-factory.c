/*
 * A factory for TpContacts and plain subclasses of TpProxy
 *
 * Copyright © 2011 Collabora Ltd.
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

/**
 * SECTION:client-factory
 * @title: TpClientFactory
 * @short_description: a factory for #TpContact<!-- -->s and plain subclasses
 *  of #TpProxy
 * @see_also: #TpAutomaticClientFactory
 *
 * This factory constructs various #TpProxy subclasses as well as #TpContact,
 * which guarantees that at most one instance of those objects will exist for a
 * given remote object or contact. It also stores the desired features for
 * contacts and each type of proxy.
 *
 * Note that the factory will not prepare the desired features: it is the
 * caller's responsibility to do so. By default, only core features are
 * requested.
 *
 * Currently supported classes are #TpAccount, #TpConnection,
 * #TpChannel and #TpContact. Those objects should always be acquired through a
 * factory or a "larger" object (e.g. getting the #TpConnection from
 * a #TpAccount), rather than being constructed directly.
 *
 * One can subclass #TpClientFactory and override some of its virtual
 * methods to construct more specialized objects. See #TpAutomaticClientFactory
 * for a subclass which automatically constructs subclasses of #TpChannel for
 * common channel types.
 *
 * An application using its own factory subclass would look like this:
 * |[
 * int main(int argc, char *argv[])
 * {
 *   TpClientFactory *factory;
 *   TpAccountManager *manager;
 *
 *   factory = my_factory_new ();
 *   tp_client_factory_set_default (factory);
 *
 *   ...
 *   manager = tp_account_manager_dup ();
 *   tp_proxy_prepare_async (manager, am_features, callback, user_data);
 *   ...
 * }
 * ]|
 *
 * The call to tp_client_factory_set_default() near the beginning of main()
 * will ensure that any libraries or plugins which also use Telepathy (and call
 * tp_client_factory_dup()) will share your #TpClientFactory.
 *
 * Since: 0.99.1
 */

/**
 * TpClientFactory:
 *
 * Data structure representing a #TpClientFactory
 *
 * Since: 0.99.1
 */

/**
 * TpClientFactoryClass:
 * @parent_class: the parent
 * @create_account: create a #TpAccount;
 *  see tp_client_factory_ensure_account()
 * @dup_account_features: implementation of tp_client_factory_dup_account_features()
 * @create_connection: create a #TpConnection;
 *  see tp_client_factory_ensure_connection()
 * @dup_connection_features: implementation of
 *  tp_client_factory_dup_connection_features()
 * @create_channel: create a #TpChannel;
 *  see tp_client_factory_ensure_channel()
 * @dup_channel_features: implementation of tp_client_factory_dup_channel_features()
 * @create_contact: create a #TpContact;
 *  see tp_client_factory_ensure_contact()
 * @dup_contact_features: implementation of tp_client_factory_dup_contact_features()
 * @create_protocol: create a #TpProtocol;
 *  see tp_client_factory_ensure_protocol()
 * @dup_protocol_features: implementation of tp_client_factory_dup_protocol_features()
 * @create_tls_certificate: create a #TpTLSCertificate;
 *  see tp_client_factory_ensure_tls_certificate()
 * @dup_tls_certificate_features: implementation of
 *  tp_client_factory_dup_tls_certificate_features()
 *
 * The class structure for #TpClientFactory.
 *
 * #TpClientFactory maintains a cache of previously-constructed proxy
 * objects, so the implementations of @create_account,
 * @create_connection, @create_channel, @create_contact and @create_protocol
 * may assume that a
 * new object should be created when they are called. The default
 * implementations create unadorned instances of the relevant classes;
 * subclasses of the factory may choose to create more interesting proxy
 * subclasses.
 *
 * The default implementation of @dup_channel_features returns
 * #TP_CHANNEL_FEATURE_CORE, plus all features passed to
 * tp_client_factory_add_channel_features() by the application.
 * Subclasses may override this method to prepare more interesting features
 * from subclasses of #TpChannel, for instance. The default implementations of
 * the other <function>dup_x_features</function> methods behave similarly.
 *
 * Since: 0.99.1
 */

#include "config.h"

#include "telepathy-glib/client-factory.h"

#include <telepathy-glib/asv.h>
#include <telepathy-glib/automatic-client-factory.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/connection-internal.h"
#include "telepathy-glib/contact-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/client-factory-internal.h"
#include "telepathy-glib/protocol-internal.h"
#include "telepathy-glib/util-internal.h"
#include "telepathy-glib/variant-util.h"

struct _TpClientFactoryPrivate
{
  GDBusConnection *dbus_connection;
  /* Owned object-path -> weakref to TpProxy */
  GHashTable *proxy_cache;
  GArray *desired_account_features;
  GArray *desired_connection_features;
  GArray *desired_channel_features;
  GArray *desired_contact_features;
  GArray *desired_protocol_features;
  GArray *desired_tls_certificate_features;
};

enum
{
  PROP_DBUS_CONNECTION = 1,
  N_PROPS
};

G_DEFINE_TYPE (TpClientFactory, tp_client_factory, G_TYPE_OBJECT)

static void
proxy_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    TpClientFactory *self)
{
  g_hash_table_remove (self->priv->proxy_cache,
      tp_proxy_get_object_path (proxy));
}

static void
insert_proxy (TpClientFactory *self,
    gpointer proxy)
{
  if (proxy == NULL)
    return;

  g_hash_table_insert (self->priv->proxy_cache,
      (gpointer) tp_proxy_get_object_path (proxy), proxy);

  /* This assume that invalidated signal is emitted from TpProxy dispose. May
   * change in a future API break? */
  tp_g_signal_connect_object (proxy, "invalidated",
      G_CALLBACK (proxy_invalidated_cb), self, 0);
}

static gpointer
lookup_proxy (TpClientFactory *self,
    const gchar *object_path)
{
  return g_hash_table_lookup (self->priv->proxy_cache, object_path);
}

void
_tp_client_factory_insert_proxy (TpClientFactory *self,
    gpointer proxy)
{
  g_return_if_fail (lookup_proxy (self,
      tp_proxy_get_object_path (proxy)) == NULL);

  insert_proxy (self, proxy);
}

static TpAccount *
create_account_impl (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties G_GNUC_UNUSED,
    GError **error)
{
  return _tp_account_new (self, object_path, error);
}

static GArray *
dup_account_features_impl (TpClientFactory *self,
    TpAccount *account)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_account_features->data);
}

static TpConnection *
create_connection_impl (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  return _tp_connection_new (self, NULL, object_path, error);
}

static GArray *
dup_connection_features_impl (TpClientFactory *self,
    TpConnection *connection)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_connection_features->data);
}

static TpChannel *
create_channel_impl (TpClientFactory *self,
    TpConnection *conn,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpChannel *channel;

  channel = _tp_channel_new (self, conn, object_path, immutable_properties,
      error);

  return channel;
}

static GArray *
dup_channel_features_impl (TpClientFactory *self,
    TpChannel *channel)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_channel_features->data);
}

static TpContact *
create_contact_impl (TpClientFactory *self,
    TpConnection *connection,
    TpHandle handle,
    const gchar *identifier)
{
  return _tp_contact_new (connection, handle, identifier);
}

static GArray *
dup_contact_features_impl (TpClientFactory *self,
    TpConnection *connection)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_contact_features->data);
}

static void
tp_client_factory_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpClientFactory *self = (TpClientFactory *) object;

  switch (property_id)
    {
    case PROP_DBUS_CONNECTION:
      g_value_set_object (value, self->priv->dbus_connection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_client_factory_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpClientFactory *self = (TpClientFactory *) object;

  switch (property_id)
    {
    case PROP_DBUS_CONNECTION:
      g_assert (self->priv->dbus_connection == NULL); /* construct only */
      self->priv->dbus_connection = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_client_factory_constructed (GObject *object)
{
  TpClientFactory *self = (TpClientFactory *) object;

  g_assert (self->priv->dbus_connection != NULL);

  G_OBJECT_CLASS (tp_client_factory_parent_class)->constructed (object);
}

static void
tp_client_factory_finalize (GObject *object)
{
  TpClientFactory *self = (TpClientFactory *) object;

  g_clear_object (&self->priv->dbus_connection);
  tp_clear_pointer (&self->priv->proxy_cache, g_hash_table_unref);
  tp_clear_pointer (&self->priv->desired_account_features, g_array_unref);
  tp_clear_pointer (&self->priv->desired_connection_features, g_array_unref);
  tp_clear_pointer (&self->priv->desired_channel_features, g_array_unref);
  tp_clear_pointer (&self->priv->desired_contact_features, g_array_unref);
  tp_clear_pointer (&self->priv->desired_protocol_features, g_array_unref);
  tp_clear_pointer (&self->priv->desired_tls_certificate_features,
      g_array_unref);

  G_OBJECT_CLASS (tp_client_factory_parent_class)->finalize (object);
}

static void
tp_client_factory_init (TpClientFactory *self)
{
  GQuark feature;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CLIENT_FACTORY,
      TpClientFactoryPrivate);

  self->priv->proxy_cache = g_hash_table_new (g_str_hash, g_str_equal);

  self->priv->desired_account_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));
  feature = TP_ACCOUNT_FEATURE_CORE;
  g_array_append_val (self->priv->desired_account_features, feature);

  self->priv->desired_connection_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));
  feature = TP_CONNECTION_FEATURE_CORE;
  g_array_append_val (self->priv->desired_connection_features, feature);

  self->priv->desired_channel_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));
  feature = TP_CHANNEL_FEATURE_CORE;
  g_array_append_val (self->priv->desired_channel_features, feature);

  self->priv->desired_contact_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));

  self->priv->desired_protocol_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));
  feature = TP_PROTOCOL_FEATURE_CORE;
  g_array_append_val (self->priv->desired_protocol_features, feature);

  self->priv->desired_tls_certificate_features = g_array_new (TRUE, FALSE,
      sizeof (GQuark));
  feature = TP_TLS_CERTIFICATE_FEATURE_CORE;
  g_array_append_val (self->priv->desired_tls_certificate_features, feature);
}

static TpProtocol *
create_protocol_impl (TpClientFactory *self,
    const gchar *cm_name,
    const gchar *protocol_name,
    GVariant *immutable_properties G_GNUC_UNUSED,
    GError **error)
{
  return _tp_protocol_new (self, cm_name, protocol_name, immutable_properties,
      error);
}

static GArray *
dup_protocol_features_impl (TpClientFactory *self,
    TpProtocol *protocol)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_protocol_features->data);
}

static TpTLSCertificate *
create_tls_certificate_impl (TpClientFactory *self,
    TpProxy *conn_or_chan,
    const gchar *object_path,
    GError **error)
{
  return _tp_tls_certificate_new (conn_or_chan, object_path, error);
}

static GArray *
dup_tls_certificate_features_impl (TpClientFactory *self,
    TpTLSCertificate *certificate)
{
  return _tp_quark_array_copy (
      (GQuark *) self->priv->desired_tls_certificate_features->data);
}

static void
tp_client_factory_class_init (TpClientFactoryClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpClientFactoryPrivate));

  object_class->get_property = tp_client_factory_get_property;
  object_class->set_property = tp_client_factory_set_property;
  object_class->constructed = tp_client_factory_constructed;
  object_class->finalize = tp_client_factory_finalize;

  klass->create_account = create_account_impl;
  klass->dup_account_features = dup_account_features_impl;
  klass->create_connection = create_connection_impl;
  klass->dup_connection_features = dup_connection_features_impl;
  klass->create_channel = create_channel_impl;
  klass->dup_channel_features = dup_channel_features_impl;
  klass->create_contact = create_contact_impl;
  klass->dup_contact_features = dup_contact_features_impl;
  klass->create_protocol = create_protocol_impl;
  klass->dup_protocol_features = dup_protocol_features_impl;
  klass->create_tls_certificate = create_tls_certificate_impl;
  klass->dup_tls_certificate_features = dup_tls_certificate_features_impl;

  /**
   * TpClientFactory:dbus-connection:
   *
   * The D-Bus connection for this object.
   */
  param_spec = g_param_spec_object ("dbus-connection", "D-Bus connection",
      "The D-Bus connection used by this object",
      G_TYPE_DBUS_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_CONNECTION,
      param_spec);
}

/**
 * tp_client_factory_new:
 * @dbus_connection: a #GDBusConnection
 *
 * Creates a new #TpClientFactory instance.
 *
 * Returns: a new #TpClientFactory
 *
 * Since: 0.99.1
 */
TpClientFactory *
tp_client_factory_new (GDBusConnection *dbus_connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), NULL);

  return g_object_new (TP_TYPE_CLIENT_FACTORY,
      "dbus-connection", dbus_connection,
      NULL);
}

static GWeakRef singleton;

/**
 * tp_client_factory_dup:
 * @error: Used to raise an error if getting the session #GDBusConnection fails
 *
 * Get a reference to a #TpClientFactory singleton. It can fail and block only
 * if the session #GDBusConnection singleton doesn't exist yet. It is thus
 * recommended to call g_bus_get() before using a #TpClientFactory if the
 * application must not block.
 *
 * By default it will create a #TpAutomaticClientFactory.
 *
 * Returns: (transfer full): a reference to a #TpClientFactory singleton.
 * Since: 0.99.10
 */
TpClientFactory *
tp_client_factory_dup (GError **error)
{
  TpClientFactory *self;

  self = g_weak_ref_get (&singleton);
  if (self == NULL)
    {
      GDBusConnection *dbus_connection;

      dbus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
      if (dbus_connection == NULL)
        return NULL;

      self = tp_automatic_client_factory_new (dbus_connection);
      g_weak_ref_set (&singleton, self);
      g_object_unref (dbus_connection);
    }

  return self;
}

/**
 * tp_client_factory_set_default:
 * @self: a #TpClientFactory
 *
 * Define the #TpClientFactory singleton that will be returned by
 * tp_client_factory_dup().
 *
 * This function may only be called before the first call to
 * tp_client_factory_dup(), and may not be called more than once. Applications
 * which use a custom #TpClientFactory and want it to be the default factory
 * should call this.
 *
 * Only a weak reference is taken on @self. It is the caller's responsibility
 * to keep it alive. If @self is disposed after calling this function, the
 * next call to tp_client_factory_dup() will return a newly created
 * #TpClientFactory.
 *
 * Since: 0.99.10
 */
void
tp_client_factory_set_default (TpClientFactory *self)
{
  TpClientFactory *tmp;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  tmp = g_weak_ref_get (&singleton);
  if (tmp != NULL)
    {
      CRITICAL ("tp_client_factory_set_default() may only be called once and"
          "before first call of tp_client_factory_dup()");
      g_object_unref (tmp);
      g_return_if_reached ();
    }

  g_weak_ref_set (&singleton, self);
}

/**
 * tp_client_factory_can_set_default:
 *
 * Check if tp_client_factory_set_default() has already successfully been
 * called.
 *
 * Returns: %TRUE if tp_client_factory_set_default() has already successfully
 * been called in this process, %FALSE otherwise.
 *
 * Since: 0.99.10
 */
gboolean
tp_client_factory_can_set_default (void)
{
  TpClientFactory *tmp;
  gboolean ret;

  tmp = g_weak_ref_get (&singleton);
  ret = (tmp == NULL);
  g_clear_object (&tmp);

  return ret;
}

/**
 * tp_client_factory_get_dbus_connection:
 * @self: a #TpClientFactory object
 *
 * <!-- -->
 *
 * Returns: (transfer none): the #TpClientFactory:dbus-connection property.
 *
 * Since: 0.99.10
 */
GDBusConnection *
tp_client_factory_get_dbus_connection (TpClientFactory *self)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  return self->priv->dbus_connection;
}

/**
 * tp_client_factory_ensure_account_manager:
 * @self: a #TpClientFactory object
 *
 * <!-- -->
 *
 * Returns: (transfer full): a reference to a #TpAccountManager singleton.
 *
 * Since: 0.99.10
 */
TpAccountManager *
tp_client_factory_ensure_account_manager (TpClientFactory *self)
{
  TpAccountManager *account_manager;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  account_manager = lookup_proxy (self, TP_ACCOUNT_MANAGER_OBJECT_PATH);
  if (account_manager != NULL)
    return g_object_ref (account_manager);

  account_manager = _tp_account_manager_new (self);
  insert_proxy (self, account_manager);

  return account_manager;
}

/**
 * tp_client_factory_ensure_channel_dispatcher:
 * @self: a #TpClientFactory object
 *
 * <!-- -->
 *
 * Returns: (transfer full): a reference to a #TpChannelDispatcher singleton.
 *
 * Since: 0.99.10
 */
TpChannelDispatcher *
tp_client_factory_ensure_channel_dispatcher (TpClientFactory *self)
{
  TpChannelDispatcher *channel_dispatcher;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  channel_dispatcher = lookup_proxy (self, TP_CHANNEL_DISPATCHER_OBJECT_PATH);
  if (channel_dispatcher != NULL)
    return g_object_ref (channel_dispatcher);

  channel_dispatcher = _tp_channel_dispatcher_new (self);
  insert_proxy (self, channel_dispatcher);

  return channel_dispatcher;
}

/**
 * tp_client_factory_ensure_logger:
 * @self: a #TpClientFactory object
 *
 * <!-- -->
 *
 * Returns: (transfer full): a reference to a #TpLogger singleton.
 *
 * Since: 0.99.10
 */
TpLogger *
tp_client_factory_ensure_logger (TpClientFactory *self)
{
  TpLogger *logger;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  logger = lookup_proxy (self, TP_LOGGER_OBJECT_PATH);
  if (logger != NULL)
    return g_object_ref (logger);

  logger = _tp_logger_new (self);
  insert_proxy (self, logger);

  return logger;
}

/**
 * tp_client_factory_ensure_account:
 * @self: a #TpClientFactory object
 * @object_path: the object path of an account
 * @immutable_properties: (allow-none): a #G_VARIANT_TYPE_VARDICT containing
 * the immutable properties of the account, or %NULL.
 * @error: Used to raise an error if @object_path is not valid
 *
 * Returns a #TpAccount proxy for the account at @object_path. The returned
 * #TpAccount is cached; the same #TpAccount object will be returned by this
 * function repeatedly, as long as at least one reference exists.
 *
 * Note that the returned #TpAccount is not guaranteed to be ready; the caller
 * is responsible for calling tp_proxy_prepare_async() with the desired
 * features (as given by tp_client_factory_dup_account_features()).
 *
 * This function is rather low-level. tp_account_manager_dup_usable_accounts()
 * and #TpAccountManager::usability-changed are more appropriate for most
 * applications.
 *
 * @immutable_properties is consumed if it is floating.
 *
 * Returns: (transfer full): a reference to a #TpAccount;
 *  see tp_account_new().
 *
 * Since: 0.99.1
 */
TpAccount *
tp_client_factory_ensure_account (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpAccount *account;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  if (immutable_properties == NULL)
    immutable_properties = g_variant_new ("a{sv}", NULL);

  g_variant_ref_sink (immutable_properties);

  account = lookup_proxy (self, object_path);
  if (account != NULL)
    {
      g_object_ref (account);
    }
  else
    {
      account = TP_CLIENT_FACTORY_GET_CLASS (self)->create_account (self,
          object_path, immutable_properties, error);
      insert_proxy (self, account);
    }

  g_variant_unref (immutable_properties);

  return account;
}

/**
 * tp_client_factory_dup_account_features:
 * @self: a #TpClientFactory object
 * @account: a #TpAccount
 *
 * Return a zero-terminated #GArray containing the #TpAccount features that
 * should be prepared on @account.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly allocated
 *  #GArray
 *
 * Since: 0.99.1
 */
GArray *
tp_client_factory_dup_account_features (TpClientFactory *self,
    TpAccount *account)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (account) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_account_features (self,
      account);
}

/**
 * tp_client_factory_add_account_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none): an array
 *  of desired features, ending with 0; %NULL is equivalent to an array
 *  containing only 0
 *
 * Add @features to the desired features to be prepared on #TpAccount
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_account_features().
 *
 * It is not necessary to add %TP_ACCOUNT_FEATURE_CORE as it is already
 * included by default.
 *
 * Note that these features will not be added to existing #TpAccount
 * objects; the user must call tp_proxy_prepare_async() themself.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_account_features (
    TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_account_features, features, -1);
}

/**
 * tp_client_factory_add_account_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_account_features(), but with a more
 * convenient calling convention from C.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_account_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_account_features, feature,
      var_args);
  va_end (var_args);
}

/**
 * tp_client_factory_ensure_connection:
 * @self: a #TpClientFactory object
 * @object_path: the object path of a connection
 * @immutable_properties: (allow-none): a #G_VARIANT_TYPE_VARDICT containing
 * the immutable properties of the connection, or %NULL.
 * @error: Used to raise an error if @object_path is not valid
 *
 * Returns a #TpConnection proxy for the connection at @object_path.
 * The returned #TpConnection is cached; the same #TpConnection object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpConnection is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async() with the desired
 * features (as given by tp_client_factory_dup_connection_features()).
 *
 * This function is rather low-level. #TpAccount:connection is more
 * appropriate for most applications.
 *
 * @immutable_properties is consumed if it is floating.
 *
 * Returns: (transfer full): a reference to a #TpConnection;
 *  see tp_connection_new().
 *
 * Since: 0.99.1
 */
TpConnection *
tp_client_factory_ensure_connection (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpConnection *connection;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  if (immutable_properties == NULL)
    immutable_properties = g_variant_new ("a{sv}", NULL);

  g_variant_ref_sink (immutable_properties);

  connection = lookup_proxy (self, object_path);
  if (connection != NULL)
    {
      g_object_ref (connection);
    }
  else
    {
      connection = TP_CLIENT_FACTORY_GET_CLASS (self)->create_connection (
          self, object_path, immutable_properties, error);
      insert_proxy (self, connection);
    }

  g_variant_unref (immutable_properties);

  return connection;
}

/**
 * tp_client_factory_dup_connection_features:
 * @self: a #TpClientFactory object
 * @connection: a #TpConnection
 *
 * Return a zero-terminated #GArray containing the #TpConnection features that
 * should be prepared on @connection.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly allocated
 *  #GArray
 *
 * Since: 0.99.1
 */
GArray *
tp_client_factory_dup_connection_features (TpClientFactory *self,
    TpConnection *connection)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (connection) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_connection_features (
      self, connection);
}

/**
 * tp_client_factory_add_connection_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none): an array
 *  of desired features, ending with 0; %NULL is equivalent to an array
 *  containing only 0
 *
 * Add @features to the desired features to be prepared on #TpConnection
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_connection_features().
 *
 * It is not necessary to add %TP_CONNECTION_FEATURE_CORE as it is already
 * included by default.
 *
 * Note that these features will not be added to existing #TpConnection
 * objects; the user must call tp_proxy_prepare_async() themself.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_connection_features (
    TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_connection_features, features, -1);
}

/**
 * tp_client_factory_add_connection_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_connection_features(), but with a
 * more convenient calling convention from C.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_connection_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_connection_features,
      feature, var_args);
  va_end (var_args);
}

/**
 * tp_client_factory_ensure_channel:
 * @self: a #TpClientFactory
 * @connection: a #TpConnection whose #TpProxy:factory is this object
 * @object_path: the object path of a channel on @connection
 * @immutable_properties: (allow-none): a #G_VARIANT_TYPE_VARDICT containing
 * the immutable properties of the account, or %NULL.
 * @error: Used to raise an error if @object_path is not valid
 *
 * Returns a #TpChannel proxy for the channel at @object_path on @connection.
 * The returned #TpChannel is cached; the same #TpChannel object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpChannel is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async() with the desired
 * features (as given by tp_client_factory_dup_channel_features()).
 *
 * This function is rather low-level.
 * #TpAccountChannelRequest and #TpBaseClient are more appropriate ways
 * to obtain channels for most applications.
 *
 * @immutable_properties is consumed if it is floating.
 *
 * Returns: (transfer full): a reference to a #TpChannel;
 *  see tp_channel_new_from_properties().
 *
 * Since: 0.99.1
 */
TpChannel *
tp_client_factory_ensure_channel (TpClientFactory *self,
    TpConnection *connection,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpChannel *channel;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (connection) == self, NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  if (immutable_properties == NULL)
    immutable_properties = g_variant_new ("a{sv}", NULL);

  g_variant_ref_sink (immutable_properties);

  channel = lookup_proxy (self, object_path);
  if (channel != NULL)
    {
      g_object_ref (channel);
    }
  else
    {
      channel = TP_CLIENT_FACTORY_GET_CLASS (self)->create_channel (self,
          connection, object_path, immutable_properties, error);
      insert_proxy (self, channel);
    }

  g_variant_unref (immutable_properties);

  return channel;
}

/**
 * tp_client_factory_dup_channel_features:
 * @self: a #TpClientFactory object
 * @channel: a #TpChannel
 *
 * Return a zero-terminated #GArray containing the #TpChannel features that
 * should be prepared on @channel.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly allocated
 *  #GArray
 *
 * Since: 0.99.1
 */
GArray *
tp_client_factory_dup_channel_features (TpClientFactory *self,
    TpChannel *channel)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (channel) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_channel_features (
      self, channel);
}

/**
 * tp_client_factory_add_channel_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none): an array
 *  of desired features, ending with 0; %NULL is equivalent to an array
 *  containing only 0
 *
 * Add @features to the desired features to be prepared on #TpChannel
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_channel_features().
 *
 * It is not necessary to add %TP_CHANNEL_FEATURE_CORE as it is already
 * included by default.
 *
 * Note that these features will not be added to existing #TpChannel
 * objects; the user must call tp_proxy_prepare_async() themself.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_channel_features (
    TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_channel_features, features, -1);
}

/**
 * tp_client_factory_add_channel_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_channel_features(), but with a
 * more convenient calling convention from C.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_channel_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_channel_features,
      feature, var_args);
  va_end (var_args);
}

/**
 * tp_client_factory_ensure_contact:
 * @self: a #TpClientFactory object
 * @connection: a #TpConnection whose #TpProxy:factory is this object
 * @handle: a #TpHandle
 * @identifier: a string representing the contact's identifier
 *
 * Returns a #TpContact representing @identifier (and @handle) on @connection.
 * The returned #TpContact is cached; the same #TpContact object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpContact is not guaranteed to be ready; the caller
 * is responsible for calling tp_connection_upgrade_contacts() with the desired
 * features (as given by tp_client_factory_dup_contact_features()).
 *
 * Returns: (transfer full): a reference to a #TpContact.
 *
 * Since: 0.99.1
 */
TpContact *
tp_client_factory_ensure_contact (TpClientFactory *self,
    TpConnection *connection,
    TpHandle handle,
    const gchar *identifier)
{
  TpContact *contact;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (connection) == self, NULL);
  g_return_val_if_fail (handle != 0, NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  contact = _tp_connection_lookup_contact (connection, handle);
  if (contact != NULL)
    {
      g_return_val_if_fail (!tp_strdiff (tp_contact_get_identifier (contact),
          identifier), NULL);
      return g_object_ref (contact);
    }

  contact = TP_CLIENT_FACTORY_GET_CLASS (self)->create_contact (self,
      connection, handle, identifier);
  _tp_connection_add_contact (connection, handle, contact);

  return contact;
}

static void
upgrade_contacts_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  GSimpleAsyncResult *my_result = user_data;
  GPtrArray *contacts;
  GError *error = NULL;

  if (!tp_connection_upgrade_contacts_finish (connection, result,
          &contacts, &error))
    {
      g_simple_async_result_take_error (my_result, error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (my_result, contacts,
          (GDestroyNotify) g_ptr_array_unref);
    }

  g_simple_async_result_complete (my_result);
  g_object_unref (my_result);
}


/**
 * tp_client_factory_upgrade_contacts_async:
 * @self: a #TpClientFactory object
 * @connection: a #TpConnection whose #TpProxy:factory is this object
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects
 *  associated with @self
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Same as tp_connection_upgrade_contacts_async(), but prepare contacts with all
 * features previously passed to
 * tp_client_factory_add_contact_features().
 *
 * Since: 0.19.1
 */
void
tp_client_factory_upgrade_contacts_async (
    TpClientFactory *self,
    TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GArray *features;

  /* no real reason this shouldn't work, but it's really confusing
   * and probably indicates an error */
  g_warn_if_fail (tp_proxy_get_factory (connection) == self);

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      tp_client_factory_upgrade_contacts_async);

  features = tp_client_factory_dup_contact_features (self, connection);
  tp_connection_upgrade_contacts_async (connection, n_contacts, contacts,
      (GQuark *) features->data,
      upgrade_contacts_cb, result);
  g_array_unref (features);
}

/**
 * tp_client_factory_upgrade_contacts_finish:
 * @self: a #TpClientFactory
 * @result: a #GAsyncResult
 * @contacts: (element-type TelepathyGLib.Contact) (transfer container) (out) (allow-none):
 *  a location to set a #GPtrArray of upgraded #TpContact, or %NULL.
 * @error: a #GError to fill
 *
 * Finishes tp_client_factory_upgrade_contacts_async()
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 * Since: 0.19.1
 */
gboolean
tp_client_factory_upgrade_contacts_finish (
    TpClientFactory *self,
    GAsyncResult *result,
    GPtrArray **contacts,
    GError **error)
{
  _tp_implement_finish_copy_pointer (self,
      tp_client_factory_upgrade_contacts_async,
      g_ptr_array_ref, contacts);
}

static void
dup_contact_by_id_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  GSimpleAsyncResult *my_result = user_data;
  TpContact *contact;
  GError *error = NULL;

  contact = tp_connection_dup_contact_by_id_finish (connection, result, &error);
  if (contact == NULL)
    {
      g_simple_async_result_take_error (my_result, error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (my_result, contact,
          g_object_unref);
    }

  g_simple_async_result_complete (my_result);
  g_object_unref (my_result);
}


/**
 * tp_client_factory_ensure_contact_by_id_async:
 * @self: a #TpClientFactory object
 * @connection: a #TpConnection
 * @identifier: a string representing the contact's identifier
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Same as tp_connection_dup_contact_by_id_async(), but prepare the
 * contact with all features previously passed to
 * tp_client_factory_add_contact_features().
 *
 * Since: 0.19.1
 */
void
tp_client_factory_ensure_contact_by_id_async (
    TpClientFactory *self,
    TpConnection *connection,
    const gchar *identifier,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GArray *features;

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      tp_client_factory_ensure_contact_by_id_async);

  features = tp_client_factory_dup_contact_features (self, connection);
  tp_connection_dup_contact_by_id_async (connection, identifier,
      (GQuark *) features->data,
      dup_contact_by_id_cb, result);
  g_array_unref (features);
}

/**
 * tp_client_factory_ensure_contact_by_id_finish:
 * @self: a #TpClientFactory
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_client_factory_ensure_contact_by_id_async()
 *
 * Returns: (transfer full): a #TpContact or %NULL on error.
 * Since: 0.19.1
 */
TpContact *
tp_client_factory_ensure_contact_by_id_finish (
    TpClientFactory *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_return_copy_pointer (self,
      tp_client_factory_ensure_contact_by_id_async, g_object_ref);
}

/**
 * tp_client_factory_dup_contact_features:
 * @self: a #TpClientFactory object
 * @connection: a #TpConnection
 *
 * Return a #GArray containing the contact feature #GQuark<!-- -->s
 * that should be prepared on all contacts of @connection.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly
 *  allocated #GArray
 *
 * Since: 0.99.1
 */
GArray *
tp_client_factory_dup_contact_features (TpClientFactory *self,
    TpConnection *connection)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (connection) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_contact_features (
      self, connection);
}

/**
 * tp_client_factory_add_contact_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none):
 *  an array of desired features
 *
 * Add @features to the desired features to be prepared on #TpContact
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_contact_features().
 *
 * Note that these features will not be added to existing #TpContact
 * objects; the user must call tp_connection_upgrade_contacts() themself.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_contact_features (TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_contact_features, features, -1);
}

/**
 * tp_client_factory_add_contact_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_contact_features(), but with a
 * more convenient calling convention from C.
 *
 * Since: 0.99.1
 */
void
tp_client_factory_add_contact_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_contact_features,
      feature, var_args);
  va_end (var_args);
}

/*
 * _tp_client_factory_ensure_channel_request:
 * @self: a #TpClientFactory object
 * @object_path: the object path of a channel request
 * @immutable_properties: (allow-none): the immutable properties of the channel
 *  request as %G_VARIANT_TYPE_VARDICT; ownership is taken
 *  if floating
 * @error: Used to raise an error if @object_path is not valid
 *
 * Returns a #TpChannelRequest for @object_path. The returned
 * #TpChannelRequest is cached; the same #TpChannelRequest object will be
 * returned by this function repeatedly, as long as at least one reference
 * exists.
 *
 * Note that the returned #TpChannelRequest is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async().
 *
 * Returns: (transfer full): a reference to a #TpChannelRequest;
 *  see tp_channel_request_new().
 *
 * Since: 0.99.1
 */
TpChannelRequest *
_tp_client_factory_ensure_channel_request (TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpChannelRequest *request;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  request = lookup_proxy (self, object_path);
  if (request != NULL)
    return g_object_ref (request);

  g_variant_ref_sink (immutable_properties);
  request = _tp_channel_request_new (self, object_path, immutable_properties,
      error);
  g_variant_unref (immutable_properties);
  insert_proxy (self, request);

  return request;
}

/*
 * _tp_client_factory_ensure_channel_dispatch_operation:
 * @self: a #TpClientFactory object
 * @object_path: the object path of a channel dispatch operation
 * @immutable_properties: (allow-none): the immutable properties of the channel
 *  dispatch operation as %G_VARIANT_TYPE_VARDICT; ownership is taken
 *  if floating
 * @error: Used to raise an error if @object_path is not valid
 *
 * Returns a #TpChannelDispatchOperation for @object_path.
 * The returned #TpChannelDispatchOperation is cached; the same
 * #TpChannelDispatchOperation object will be returned by this function
 * repeatedly, as long as at least one reference exists.
 *
 * Note that the returned #TpChannelDispatchOperation is not guaranteed to be
 * ready; the caller is responsible for calling tp_proxy_prepare_async().
 *
 * Returns: (transfer full): a reference to a
 *  #TpChannelDispatchOperation; see tp_channel_dispatch_operation_new().
 *
 * Since: 0.99.1
 */
TpChannelDispatchOperation *
_tp_client_factory_ensure_channel_dispatch_operation (
    TpClientFactory *self,
    const gchar *object_path,
    GVariant *immutable_properties,
    GError **error)
{
  TpChannelDispatchOperation *dispatch = NULL;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (immutable_properties == NULL ||
      g_variant_is_of_type (immutable_properties,
        G_VARIANT_TYPE_VARDICT), NULL);

  if (immutable_properties != NULL)
    g_variant_ref_sink (immutable_properties);

  dispatch = lookup_proxy (self, object_path);
  if (dispatch != NULL)
    {
      g_object_ref (dispatch);
      goto finally;
    }

  dispatch = _tp_channel_dispatch_operation_new (self, object_path,
      immutable_properties, error);
  insert_proxy (self, dispatch);

finally:
  if (immutable_properties != NULL)
    g_variant_unref (immutable_properties);

  return dispatch;
}

/**
 * tp_client_factory_ensure_protocol:
 * @self: a #TpClientFactory
 * @cm_name: the connection manager name (such as "gabble")
 * @protocol_name: the protocol name (such as "jabber")
 * @immutable_properties: (allow-none): a #G_VARIANT_TYPE_VARDICT containing
 * the immutable properties of the protocol, or %NULL.
 * @error: Used to raise an error if @cm_name or @protocol_name is invalid
 *
 * Returns a #TpProtocol proxy for the protocol @protocol_name on connection
 * manager @cm_name.
 * The returned #TpProtocol is cached; the same #TpProtocol object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpProtocol is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async() with the desired
 * features (as given by tp_client_factory_dup_protocol_features()).
 *
 * @immutable_properties is consumed if it is floating.
 *
 * Returns: (transfer full): a reference to a #TpProtocol,
 * or %NULL on invalid arguments
 *
 * Since: 0.99.8
 */
TpProtocol *
tp_client_factory_ensure_protocol (TpClientFactory *self,
    const gchar *cm_name,
    const gchar *protocol_name,
    GVariant *immutable_properties,
    GError **error)
{
  TpProtocol *protocol;
  gchar *object_path;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  if (immutable_properties == NULL)
    immutable_properties = g_variant_new ("a{sv}", NULL);

  g_variant_ref_sink (immutable_properties);

  object_path = _tp_protocol_build_object_path (cm_name, protocol_name);
  protocol = lookup_proxy (self, object_path);
  if (protocol != NULL)
    {
      g_object_ref (protocol);
    }
  else
    {
      protocol = TP_CLIENT_FACTORY_GET_CLASS (self)->create_protocol (self,
          cm_name, protocol_name, immutable_properties, error);

      if (protocol != NULL)
        {
          g_assert (g_str_equal (tp_proxy_get_object_path (protocol),
              object_path));
          insert_proxy (self, protocol);
        }
    }

  g_variant_unref (immutable_properties);
  g_free (object_path);

  return protocol;
}

/**
 * tp_client_factory_dup_protocol_features:
 * @self: a #TpClientFactory object
 * @protocol: a #TpProtocol
 *
 * Return a zero-terminated #GArray containing the #TpProtocol features that
 * should be prepared on @protocol.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly allocated
 *  #GArray
 *
 * Since: 0.99.8
 */
GArray *
tp_client_factory_dup_protocol_features (TpClientFactory *self,
    TpProtocol *protocol)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_PROTOCOL (protocol), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (protocol) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_protocol_features (
      self, protocol);
}

/**
 * tp_client_factory_add_protocol_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none): an array
 *  of desired features, ending with 0; %NULL is equivalent to an array
 *  containing only 0
 *
 * Add @features to the desired features to be prepared on #TpProtocol
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_protocol_features().
 *
 * It is not necessary to add %TP_PROTOCOL_FEATURE_CORE as it is already
 * included by default.
 *
 * Note that these features will not be added to existing #TpProtocol
 * objects; the user must call tp_proxy_prepare_async() themself.
 *
 * Since: 0.99.8
 */
void
tp_client_factory_add_protocol_features (
    TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_protocol_features, features, -1);
}

/**
 * tp_client_factory_add_protocol_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_protocol_features(), but with a
 * more convenient calling convention from C.
 *
 * Since: 0.99.8
 */
void
tp_client_factory_add_protocol_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_protocol_features,
      feature, var_args);
  va_end (var_args);
}

/**
 * tp_client_factory_ensure_tls_certificate:
 * @self: a #TpClientFactory
 * @conn_or_chan: a #TpConnection or #TpChannel parent for this object, whose
 *  invalidation will also result in invalidation of the returned object
 * @object_path: the object path of this TLS certificate
 * @error: Used to raise an error
 *
 * Returns a #TpTLSCertificate proxy for the channel or connection
 * @conn_or_chan.
 * The returned #TpTLSCertificate is cached; the same #TpTLSCertificate object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpTLSCertificate is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async() with the desired
 * features (as given by tp_client_factory_dup_tls_certificate_features()).
 *
 * Returns: (transfer full): a reference to a #TpTLSCertificate,
 * or %NULL on invalid arguments
 *
 * Since: 0.99.8
 */
TpTLSCertificate *
tp_client_factory_ensure_tls_certificate (TpClientFactory *self,
    TpProxy *conn_or_chan,
    const gchar *object_path,
    GError **error)
{
  TpTLSCertificate *cert;

  g_return_val_if_fail (tp_proxy_get_factory (conn_or_chan) == self, NULL);

  cert = lookup_proxy (self, object_path);
  if (cert != NULL)
    {
      g_object_ref (cert);
    }
  else
    {
      cert = TP_CLIENT_FACTORY_GET_CLASS (self)->create_tls_certificate (self,
        conn_or_chan, object_path, error);
      insert_proxy (self, cert);
    }

  return cert;
}

/**
 * tp_client_factory_dup_tls_certificate_features:
 * @self: a #TpClientFactory object
 * @certificate: a #TpTLSCertificate
 *
 * Return a zero-terminated #GArray containing the #TpTLSCertificate features
 * that should be prepared on @protocol.
 *
 * Returns: (transfer full) (element-type GLib.Quark): a newly allocated
 *  #GArray
 *
 * Since: 0.99.8
 */
GArray *
tp_client_factory_dup_tls_certificate_features (TpClientFactory *self,
    TpTLSCertificate *certificate)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);
  g_return_val_if_fail (TP_IS_TLS_CERTIFICATE (certificate), NULL);
  g_return_val_if_fail (tp_proxy_get_factory (certificate) == self, NULL);

  return TP_CLIENT_FACTORY_GET_CLASS (self)->dup_tls_certificate_features (
      self, certificate);
}

/**
 * tp_client_factory_add_tls_certificate_features:
 * @self: a #TpClientFactory object
 * @features: (array zero-terminated=1) (allow-none): an array
 *  of desired features, ending with 0; %NULL is equivalent to an array
 *  containing only 0
 *
 * Add @features to the desired features to be prepared on #TpTLSCertificate
 * objects. Those features will be added to the features already returned be
 * tp_client_factory_dup_tls_certificate_features().
 *
 * It is not necessary to add %TP_TLS_CERTIFICATE_FEATURE_CORE as it is already
 * included by default.
 *
 * Note that these features will not be added to existing #TpTLSCertificate
 * objects; the user must call tp_proxy_prepare_async() themself.
 *
 * Since: 0.99.8
 */
void
tp_client_factory_add_tls_certificate_features (TpClientFactory *self,
    const GQuark *features)
{
  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  _tp_quark_array_merge (self->priv->desired_tls_certificate_features, features,
      -1);
}

/**
 * tp_client_factory_add_tls_certificate_features_varargs: (skip)
 * @self: a #TpClientFactory
 * @feature: the first feature
 * @...: the second and subsequent features, if any, ending with 0
 *
 * The same as tp_client_factory_add_tls_certificate_features(), but with a
 * more convenient calling convention from C.
 *
 * Since: 0.99.8
 */
void
tp_client_factory_add_tls_certificate_features_varargs (
    TpClientFactory *self,
    GQuark feature,
    ...)
{
  va_list var_args;

  g_return_if_fail (TP_IS_CLIENT_FACTORY (self));

  va_start (var_args, feature);
  _tp_quark_array_merge_valist (self->priv->desired_tls_certificate_features,
      feature, var_args);
  va_end (var_args);
}

/**
 * tp_client_factory_ensure_debug_client:
 * @self: a #TpClientFactory
 * @unique_name: the unique name of the process to be debugged; may not be
 *  %NULL or a well-known name
 * @error: Used to raise an error
 *
 * The returned #TpDebugClient is cached; the same #TpDebugClient object
 * will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpDebugClient is not guaranteed to be ready; the
 * caller is responsible for calling tp_proxy_prepare_async() with the desired
 * features.
 *
 * Returns: (transfer full): a reference to a #TpDebugClient,
 *  or %NULL on invalid arguments.
 *
 * Since: 0.99.10
 */
TpDebugClient *
tp_client_factory_ensure_debug_client (TpClientFactory *self,
    const gchar *unique_name,
    GError **error)
{
  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  /* FIXME: make it unique per @unique_name, can't use self->priv->proxy_cache
   * in this case. */
  return _tp_debug_client_new (self, unique_name, error);
}

/**
 * tp_client_factory_ensure_connection_manager:
 * @self: a #TpClientFactory
 * @name: The connection manager name (such as "gabble")
 * @manager_filename: (allow-none): The #TpConnectionManager:manager-file
 *  property, which may (and generally should) be %NULL.
 * @error: Used to raise an error
 *
 * The returned #TpConnectionManager is cached; the same #TpConnectionManager
 * object will be returned by this function repeatedly, as long as at least one
 * reference exists.
 *
 * Note that the returned #TpConnectionManager is not guaranteed to be ready;
 * the caller is responsible for calling tp_proxy_prepare_async() with the
 * desired features.
 *
 * Returns: (transfer full): a reference to a #TpConnectionManager,
 *  or %NULL on invalid arguments.
 *
 * Since: 0.99.10
 */
TpConnectionManager *
tp_client_factory_ensure_connection_manager (TpClientFactory *self,
    const gchar *name,
    const gchar *manager_filename,
    GError **error)
{
  TpConnectionManager *cm;
  gchar *object_path;

  g_return_val_if_fail (TP_IS_CLIENT_FACTORY (self), NULL);

  object_path = _tp_connection_manager_build_object_path (name);
  cm = lookup_proxy (self, object_path);
  if (cm != NULL)
    {
      g_object_ref (cm);
    }
  else
    {
      cm = _tp_connection_manager_new (self, name, manager_filename, error);

      if (cm != NULL)
        {
          g_assert (g_str_equal (tp_proxy_get_object_path (cm),
              object_path));
          insert_proxy (self, cm);
        }
    }

  g_free (object_path);

  return cm;
}
