/*
 * base-connection.c - Source for TpBaseConnection
 *
 * Copyright © 2005-2010 Collabora Ltd.
 * Copyright © 2005-2009 Nokia Corporation
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
 * SECTION:base-connection
 * @title: TpBaseConnection
 * @short_description: base class for core Connection D-Bus interfaces
 *  implementation
 * @see_also: #TpBaseConnectionManager, #TpBaseContactList, #TpPresenceMixin
 *
 * This base class makes it easier to write Connection implementations
 * by managing connection status, channel managers and handle tracking.
 */

/**
 * TpBaseConnectionProc:
 * @self: The connection object
 *
 * Signature of a virtual method on #TpBaseConnection that takes no
 * additional parameters and returns nothing.
 */

/**
 * TpBaseConnectionStartConnectingImpl:
 * @self: The connection object
 * @error: Set to the error if %FALSE is returned
 *
 * Signature of an implementation of the start_connecting method
 * of #TpBaseConnection.
 *
 * On entry, the implementation may assume that it is in state NEW.
 *
 * If %TRUE is returned, the Connect D-Bus method succeeds; the
 * implementation must either have already set the status to CONNECTED by
 * calling tp_base_connection_change_status(), or have arranged for a
 * status change to either state DISCONNECTED or CONNECTED to be signalled by
 * calling tp_base_connection_change_status() at some later time.
 * If the status is still NEW after returning %TRUE, #TpBaseConnection will
 * automatically change it to CONNECTING for reason REQUESTED.
 *
 * If %FALSE is returned, the error will be raised from Connect as an
 * exception. If the status is not DISCONNECTED after %FALSE is returned,
 * #TpBaseConnection will automatically change it to DISCONNECTED
 * with a reason appropriate to the error; NetworkError results in
 * NETWORK_ERROR, PermissionDenied results in AUTHENTICATION_FAILED, and all
 * other errors currently result in NONE_SPECIFIED.
 *
 * All except the simplest connection managers are expected to implement this
 * asynchronously, returning %TRUE in most cases and changing the status
 * to CONNECTED or DISCONNECTED later.
 *
 * Subclasses may call g_dbus_object_skeleton_add_interface()
 * at any time before the status reaches CONNECTED. It is considered
 * to be an error to do so after CONNECTED status has been reached.
 *
 * Returns: %FALSE if failure has already occurred, else %TRUE.
 */

/**
 * TpBaseConnectionCreateHandleReposImpl: (skip)
 * @self: The connection object
 * @repos: An array of pointers to be filled in; the implementation
 *         may assume all are initially NULL.
 *
 * Signature of an implementation of the create_handle_repos method
 * of #TpBaseConnection.
 */

/**
 * TpBaseConnectionCreateChannelManagersImpl:
 * @self: The implementation, a subclass of TpBaseConnection
 *
 * Signature of an implementation of the create_channel_managers method
 * of #TpBaseConnection.
 *
 * Returns: (transfer full): a GPtrArray of objects implementing
 *  #TpChannelManager which, between them, implement all channel types this
 *  Connection supports.
 */

/**
 * TpBaseConnectionGetUniqueConnectionNameImpl:
 * @self: The implementation, a subclass of TpBaseConnection
 *
 * Signature of the @get_unique_connection_name virtual method
 * on #TpBaseConnection.
 *
 * Returns: (transfer full): a name for this connection which will be unique
 *  within this connection manager process, as a string which the caller must
 *  free with #g_free.
 */

/**
 * TpBaseConnectionClass:
 * @parent_class: The superclass' structure
 * @create_handle_repos: Fill in suitable handle repositories in the
 *  given array for all those entity types this Connection supports.
 *  Must be set by subclasses to a non-%NULL value; the function must create
 *  at least a CONTACT handle repository (failing to do so will cause a crash).
 * @get_unique_connection_name: Construct a unique name for this connection
 *  (for example using the protocol's format for usernames). If %NULL (the
 *  default), a unique name will be generated. Subclasses should usually
 *  override this to get more obvious names, to aid debugging and prevent
 *  multiple connections to the same account.
 * @connecting: If set by subclasses, will be called just after the state
 *  changes to CONNECTING. May be %NULL if nothing special needs to happen.
 * @connected: If set by subclasses, will be called just after the state
 *  changes to CONNECTED. May be %NULL if nothing special needs to happen.
 * @disconnected: If set by subclasses, will be called just after the state
 *  changes to DISCONNECTED. May be %NULL if nothing special needs to happen.
 * @shut_down: Called after disconnected() is called, to clean up the
 *  connection. Must start the shutdown process for the underlying
 *  network connection, and arrange for tp_base_connection_finish_shutdown()
 *  to be called after the underlying connection has been closed. May not
 *  be left as %NULL.
 * @start_connecting: Asynchronously start connecting - called to implement
 *  the Connect D-Bus method. See #TpBaseConnectionStartConnectingImpl for
 *  details. May not be left as %NULL.
 * @create_channel_managers: Create an array of channel managers for this
 *  Connection. This must be set by subclasses to a non-%NULL
 *  value. Since: 0.7.15
 * @fill_contact_attributes: If @dbus_interface is recognised by this
 *  object, fill in any contact attribute tokens for @contact in @attributes,
 *  and return. Otherwise, chain up to the superclass' implementation.
 *  Since: 0.UNRELEASED
 *
 * The class of a #TpBaseConnection. Many members are virtual methods etc.
 * to be filled in in the subclass' class_init function.
 */

/**
 * TP_INTERNAL_CONNECTION_STATUS_NEW: (skip)
 *
 * A special value for #TpConnectionStatus, used within GLib connection
 * managers to indicate that the connection is disconnected because
 * connection has never been attempted (as distinct from disconnected
 * after connection has started, either by user request or an error).
 *
 * Must never be visible on the D-Bus - %TP_CONNECTION_STATUS_DISCONNECTED
 * is sent instead.
 */

/**
 * TpBaseConnection:
 *
 * Data structure representing a generic connection implementation.
 *
 */

/**
 * TpChannelManagerIter: (skip)
 *
 * An iterator over the #TpChannelManager objects known to a #TpBaseConnection.
 * It has no public fields.
 *
 * Use tp_base_connection_channel_manager_iter_init() to start iteration and
 * tp_base_connection_channel_manager_iter_next() to continue.
 *
 * Since: 0.7.15
 */

/**
 * TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED: (skip)
 * @conn: A TpBaseConnection
 * @context: A GDBusMethodInvocation
 *
 * If @conn is not in state #TP_CONNECTION_STATUS_CONNECTED, complete the
 * D-Bus method invocation @context by raising the Telepathy error
 * #TP_ERROR_DISCONNECTED, and return from the current function (which
 * must be void). For use in D-Bus method implementations.
 */

#include "config.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/base-connection-internal.h>

#include <string.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/channel-manager-request-internal.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-internal.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-connection.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-interface.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/value-array.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/variant-util-internal.h"

G_DEFINE_ABSTRACT_TYPE (TpBaseConnection, tp_base_connection,
    G_TYPE_DBUS_OBJECT_SKELETON)

enum
{
    PROP_PROTOCOL = 1,
    PROP_SELF_HANDLE,
    PROP_SELF_ID,
    PROP_INTERFACES,
    PROP_DBUS_CONNECTION,
    PROP_ACCOUNT_PATH_SUFFIX,
    N_PROPS
};

/* signal enum */
enum
{
    INVALID_SIGNAL,
    SHUTDOWN_FINISHED,
    CLIENTS_INTERESTED,
    CLIENTS_UNINTERESTED,
    STATUS_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

#define _TP_GDBUS_ERROR_IF_NOT_CONNECTED(conn, context) \
  G_STMT_START { \
    TpBaseConnection *c_ = (conn); \
    GError *e_ = NULL; \
    \
    if (!tp_base_connection_check_connected (c_, &e_)) \
      { \
        g_dbus_method_invocation_take_error ((context), e_); \
        return TRUE; \
      } \
  } G_STMT_END

static void update_rcc_property (TpBaseConnection *self);
static void update_channels_property (TpBaseConnection *self);
static void conn_skeleton_init (TpBaseConnection *self);
static void requests_skeleton_init (TpBaseConnection *self);

static void
channel_request_cancel (gpointer data,
    gpointer user_data)
{
  TpChannelManagerRequest *request = (TpChannelManagerRequest *) data;

  _tp_channel_manager_request_cancel (request);
}

typedef struct
{
  /* GQuark iface => count */
  GHashTable *interests;
  guint watch_id;
} ClientData;

static void client_data_free (ClientData *client);

static gboolean
tp_base_connection_ensure_dbus (TpBaseConnection *self,
    GError **error)
{
  if (self->priv->dbus_connection == NULL)
    {
      self->priv->dbus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL,
          error);

      if (self->priv->dbus_connection == NULL)
        return FALSE;
    }

  return TRUE;
}

static GPtrArray * conn_requests_get_requestables (TpBaseConnection *self);

static void
tp_base_connection_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  TpBaseConnection *self = (TpBaseConnection *) object;
  TpBaseConnectionPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_PROTOCOL:
      g_value_set_string (value, priv->protocol);
      break;

    case PROP_SELF_HANDLE:
      g_value_set_uint (value, priv->self_handle);
      break;

    case PROP_SELF_ID:
      g_value_set_string (value, priv->self_id);
      break;

    case PROP_INTERFACES:
      g_object_get_property (G_OBJECT (self->priv->connection_skeleton),
          "interfaces", value);
      break;

    case PROP_DBUS_CONNECTION:
      g_value_set_object (value, self->priv->dbus_connection);
      break;

    case PROP_ACCOUNT_PATH_SUFFIX:
      g_value_set_string (value, self->priv->account_path_suffix);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_connection_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TpBaseConnection *self = (TpBaseConnection *) object;
  TpBaseConnectionPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_PROTOCOL:
      g_free (priv->protocol);
      priv->protocol = g_value_dup_string (value);
      g_assert (priv->protocol != NULL);
      break;

    case PROP_SELF_HANDLE:
      tp_base_connection_set_self_handle (self, g_value_get_uint (value));
      break;

    case PROP_DBUS_CONNECTION:
      g_assert (self->priv->dbus_connection == NULL);     /* construct-only */
      self->priv->dbus_connection = g_value_dup_object (value);
      break;

    case PROP_ACCOUNT_PATH_SUFFIX:
      g_assert (self->priv->account_path_suffix == NULL); /* construct-only */
      self->priv->account_path_suffix = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_connection_unregister (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = self->priv;

  if (priv->dbus_connection != NULL)
    {
      GHashTableIter iter;

      if (priv->been_registered)
        {
          tp_dbus_connection_unregister_object (priv->dbus_connection, self);

          if (priv->bus_name != NULL)
            tp_dbus_connection_release_name (priv->dbus_connection, priv->bus_name,
                NULL);
          else
            DEBUG ("not releasing bus name: nothing to release");

          priv->been_registered = FALSE;
        }

      g_hash_table_remove_all (self->priv->clients);

      g_hash_table_iter_init (&iter, self->priv->interests);
      while (g_hash_table_iter_next (&iter, NULL, NULL))
        g_hash_table_iter_replace (&iter, GUINT_TO_POINTER (0));
    }
}

static void
tp_base_connection_dispose (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  TpBaseConnectionPrivate *priv = self->priv;
  guint i;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_assert ((priv->status == TP_CONNECTION_STATUS_DISCONNECTED) ||
            (priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW));

  tp_base_connection_unregister (self);

  tp_clear_object (&priv->dbus_connection);

  g_ptr_array_foreach (priv->channel_managers, (GFunc) g_object_unref, NULL);
  g_ptr_array_unref (priv->channel_managers);
  priv->channel_managers = NULL;

  if (priv->channel_requests)
    {
      g_assert (priv->channel_requests->len == 0);
      g_ptr_array_unref (priv->channel_requests);
      priv->channel_requests = NULL;
    }

  for (i = 0; i < TP_NUM_ENTITY_TYPES; i++)
    tp_clear_object (priv->handles + i);

  g_clear_object (&self->priv->connection_skeleton);
  g_clear_object (&self->priv->requests_skeleton);
  g_clear_object (&self->priv->presence_skeleton);

  if (G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose (object);
}

static void
tp_base_connection_finalize (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  TpBaseConnectionPrivate *priv = self->priv;

  g_free (priv->protocol);
  g_free (priv->bus_name);
  g_free (priv->object_path);
  g_hash_table_unref (priv->clients);
  g_hash_table_unref (priv->interests);
  g_free (priv->account_path_suffix);

  G_OBJECT_CLASS (tp_base_connection_parent_class)->finalize (object);
}

static void
satisfy_request (TpBaseConnection *conn,
    TpChannelManagerRequest *request,
    TpBaseChannel *channel)
{
  TpBaseConnectionPrivate *priv = conn->priv;

  _tp_channel_manager_request_satisfy (request, channel);
  g_ptr_array_remove (priv->channel_requests, request);
}

static void
fail_channel_request (TpBaseConnection *conn,
    TpChannelManagerRequest *request,
    GError *error)
{
  TpBaseConnectionPrivate *priv = conn->priv;

  _tp_channel_manager_request_fail (request, error);
  g_ptr_array_remove (priv->channel_requests, request);
}

/* Channel manager signal handlers */

static void
manager_new_channel (TpBaseConnection *self,
    TpBaseChannel *channel,
    GSList *request_tokens)
{
  gchar *object_path;
  GSList *iter;
  gboolean satisfies_create_channel = FALSE;
  TpChannelManagerRequest *first_ensure = NULL;

  g_object_get (channel,
      "object-path", &object_path,
      NULL);

  for (iter = request_tokens; iter != NULL; iter = iter->next)
    {
      TpChannelManagerRequest *request = iter->data;

      switch (request->method)
        {
          case TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL:
            satisfies_create_channel = TRUE;
            goto break_loop_early;
            break;

          case TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL:
            if (first_ensure == NULL)
              first_ensure = request;
            break;

          case TP_NUM_CHANNEL_MANAGER_REQUEST_METHODS:
            g_assert_not_reached ();
        }

    }

break_loop_early:
  /* If the only type of request satisfied by this new channel is
   * EnsureChannel, give exactly one request Yours=True.
   * If other kinds of requests are involved, don't give anyone Yours=True.
   */
  if (!satisfies_create_channel && first_ensure != NULL)
    {
      first_ensure->yours = TRUE;
    }

  for (iter = request_tokens; iter != NULL; iter = iter->next)
    {
      satisfy_request (self, iter->data, TP_BASE_CHANNEL (channel));
    }

  g_free (object_path);
}


static void
manager_new_channel_cb (TpChannelManager *manager,
    TpBaseChannel *channel,
    GSList *requests,
    TpBaseConnection *self)
{
  gchar *path;
  GVariant *variant;

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (TP_IS_BASE_CONNECTION (self));

  /* satisfy the RequestChannel/CreateChannel/EnsureChannel calls */
  manager_new_channel (self, channel, requests);

  g_object_get (channel,
      "object-path", &path,
      "channel-properties", &variant,
      NULL);

  update_channels_property (self);
  _tp_gdbus_connection_interface_requests_emit_new_channel (
      self->priv->requests_skeleton, path, variant);

  g_free (path);
  g_variant_unref (variant);
}


static void
manager_request_already_satisfied_cb (TpChannelManager *manager,
                                      gpointer request_token,
                                      TpBaseChannel *channel,
                                      TpBaseConnection *self)
{
  gchar *object_path;

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (TP_IS_BASE_CHANNEL (channel));
  g_assert (TP_IS_BASE_CONNECTION (self));

  g_object_get (channel,
      "object-path", &object_path,
      NULL);

  satisfy_request (self, request_token, TP_BASE_CHANNEL (channel));
  g_free (object_path);
}


static void
manager_request_failed_cb (TpChannelManager *manager,
                           gpointer request_token,
                           guint domain,
                           gint code,
                           gchar *message,
                           TpBaseConnection *self)
{
  GError error = { domain, code, message };

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (domain > 0);
  g_assert (message != NULL);
  g_assert (TP_IS_BASE_CONNECTION (self));

  fail_channel_request (self, request_token, &error);
}


static void
manager_channel_closed_cb (TpChannelManager *manager,
                           const gchar *path,
                           TpBaseConnection *self)
{
  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (path != NULL);
  g_assert (TP_IS_BASE_CONNECTION (self));

  update_channels_property (self);
  _tp_gdbus_connection_interface_requests_emit_channel_closed (
      self->priv->requests_skeleton, path);
}

/*
 * Set the @entity_type'th handle repository, which must be %NULL, to
 * @handle_repo. This method can only be called from code run during the
 * constructor(), after handle repository instantiation (in practice, this
 * means it can only be called from the @create_channel_managers callback).
 */
void
_tp_base_connection_set_handle_repo (TpBaseConnection *self,
    TpEntityType entity_type,
    TpHandleRepoIface *handle_repo)
{
  g_return_if_fail (TP_IS_BASE_CONNECTION (self));
  g_return_if_fail (!self->priv->been_constructed);
  g_return_if_fail (tp_entity_type_is_valid (entity_type, NULL));
  g_return_if_fail (self->priv->handles[TP_ENTITY_TYPE_CONTACT] != NULL);
  g_return_if_fail (self->priv->handles[entity_type] == NULL);
  g_return_if_fail (TP_IS_HANDLE_REPO_IFACE (handle_repo));

  self->priv->handles[entity_type] = g_object_ref (handle_repo);
}

static void
tp_base_connection_interface_changed_cb (TpBaseConnection *self,
    GDBusInterface *interface,
    gpointer user_data)
{
  GDBusInterfaceInfo *info = g_dbus_interface_get_info (interface);
  gint what_happened = GPOINTER_TO_INT (user_data);
  const gchar *verb = (what_happened == 1 ? "add" : "remove");

  g_assert (what_happened == 1 || what_happened == -1);

  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED)
    {
      WARNING ("Adding or removing Connection interfaces after CONNECTED "
          "status has been reached is not supported. "
          "(Tried to %s %s %p, \"%s\")",
          verb, G_OBJECT_TYPE_NAME (interface), interface, info->name);
    }
  else
    {
      GValue value = G_VALUE_INIT;

      /* We'd ideally like to detect attempts to remove interfaces
       * and diagnose those as invalid, too, but we can't do that because
       * replacement by a subclass is represented as remove + add. */
      DEBUG ("%s %s %p \"%s\"",
          verb, G_OBJECT_TYPE_NAME (interface), interface, info->name);

      /* Update the Interfaces property. For now we do this from scratch
       * every time, rather than doing anything intelligently diff-based. */
      g_value_init (&value, G_TYPE_STRV);
      g_value_take_boxed (&value,
          _tp_g_dbus_object_dup_interface_names_except (G_DBUS_OBJECT (self),
            TP_IFACE_CONNECTION, TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
            NULL));
      g_object_set_property (G_OBJECT (self->priv->connection_skeleton),
          "interfaces", &value);
      g_value_unset (&value);
    }
}

static GObject *
tp_base_connection_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_params)
{
  guint i;
  TpBaseConnection *self = TP_BASE_CONNECTION (
      G_OBJECT_CLASS (tp_base_connection_parent_class)->constructor (
        type, n_construct_properties, construct_params));
  TpBaseConnectionPrivate *priv = self->priv;
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);

  g_assert (cls->create_handle_repos != NULL);
  g_assert (cls->create_channel_managers  != NULL);
  g_assert (cls->shut_down != NULL);
  g_assert (cls->start_connecting != NULL);

  /* if we fail to connect to D-Bus here, we'll return an error from
   * register */
  tp_base_connection_ensure_dbus (self, NULL);

  (cls->create_handle_repos) (self, priv->handles);

  /* a connection that doesn't support contacts is no use to anyone */
  g_assert (priv->handles[TP_ENTITY_TYPE_CONTACT] != NULL);

  if (cls->create_channel_managers != NULL)
    priv->channel_managers = cls->create_channel_managers (self);
  else
    priv->channel_managers = g_ptr_array_sized_new (0);

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      g_signal_connect (manager, "new-channel",
          (GCallback) manager_new_channel_cb, self);
      g_signal_connect (manager, "request-already-satisfied",
          (GCallback) manager_request_already_satisfied_cb, self);
      g_signal_connect (manager, "request-failed",
          (GCallback) manager_request_failed_cb, self);
      g_signal_connect (manager, "channel-closed",
          (GCallback) manager_channel_closed_cb, self);
    }

  priv->been_constructed = TRUE;

  return (GObject *) self;
}

static void
tp_base_connection_constructed (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  GDBusObjectSkeleton *skel = G_DBUS_OBJECT_SKELETON (self);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_base_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  self->priv->connection_skeleton = _tp_gdbus_connection_skeleton_new ();
  g_dbus_object_skeleton_add_interface (skel,
      G_DBUS_INTERFACE_SKELETON (self->priv->connection_skeleton));
  _tp_gdbus_connection_set_status (self->priv->connection_skeleton,
      TP_CONNECTION_STATUS_DISCONNECTED);
  conn_skeleton_init (self);

  self->priv->requests_skeleton =
      _tp_gdbus_connection_interface_requests_skeleton_new ();
  g_dbus_object_skeleton_add_interface (skel,
      G_DBUS_INTERFACE_SKELETON (self->priv->requests_skeleton));
  requests_skeleton_init (self);

  /* Set the initial RCC, it won't be definitive until status is CONNECTED */
  update_rcc_property (self);

  g_signal_connect (self, "interface-added",
      G_CALLBACK (tp_base_connection_interface_changed_cb),
      GINT_TO_POINTER (+1));
  g_signal_connect (self, "interface-removed",
      G_CALLBACK (tp_base_connection_interface_changed_cb),
      GINT_TO_POINTER (-1));

  if (TP_IS_PRESENCE_MIXIN (self))
    {
      _tp_presence_mixin_init (self);
    }

  /* We don't have any interfaces yet (except for Connection and Requests)
   * so it's OK that the default for _TpGDBusConnection:interfaces is NULL. */
}

/**
 * tp_base_connection_add_possible_client_interest:
 * @self: a connection
 * @token: a quark corresponding to a D-Bus interface, or a token
 *  representing part of a D-Bus interface, for which this connection wishes
 *  to be notified when clients register an interest
 *
 * Add @token to the set of tokens for which this connection will emit
 * #TpBaseConnection::clients-interested and
 * #TpBaseConnection::clients-uninterested.
 *
 * This method must be called from the #GObjectClass<!--
 * -->.constructed or #GObjectClass<!-- -->.constructor callback
 * (otherwise, it will run too late to be useful).
 */
void
tp_base_connection_add_possible_client_interest (TpBaseConnection *self,
    GQuark token)
{
  gpointer p = GUINT_TO_POINTER (token);

  g_return_if_fail (TP_IS_BASE_CONNECTION (self));
  g_return_if_fail (self->priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW);

  if (!g_hash_table_contains (self->priv->interests, p))
    g_hash_table_insert (self->priv->interests, p, GUINT_TO_POINTER (0));
}

/* D-Bus properties for the Requests interface */

static void
manager_get_channel_details_foreach (TpBaseChannel *chan,
    gpointer user_data)
{
  GVariantBuilder *builder = user_data;
  GVariant *properties;
  gchar *object_path;

  g_object_get (chan,
      "object-path", &object_path,
      "channel-properties", &properties,
      NULL);

  g_variant_builder_add (builder, "(o@a{sv})", object_path, properties);
  g_variant_unref (properties);
  g_free (object_path);
}

static void
update_channels_property (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = self->priv;
  GVariantBuilder builder;
  guint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(oa{sv})"));
  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      tp_channel_manager_foreach_channel (manager,
          manager_get_channel_details_foreach, &builder);
    }

  _tp_gdbus_connection_interface_requests_set_channels (
      self->priv->requests_skeleton,
      g_variant_builder_end (&builder));
}

static void
get_requestables_foreach (TpChannelManager *manager,
                          GHashTable *fixed_properties,
                          const gchar * const *allowed_properties,
                          gpointer user_data)
{
  GPtrArray *details = user_data;

  g_ptr_array_add (details, tp_value_array_build (2,
        TP_HASH_TYPE_CHANNEL_CLASS, fixed_properties,
        G_TYPE_STRV, allowed_properties,
        G_TYPE_INVALID));
}


static GPtrArray *
conn_requests_get_requestables (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = self->priv;
  /* generously guess that each ChannelManager has about 2 ChannelClasses */
  GPtrArray *details = g_ptr_array_sized_new (priv->channel_managers->len * 2);
  guint i;

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      tp_channel_manager_foreach_channel_class (manager,
          get_requestables_foreach, details);
    }

  return details;
}

static void
update_rcc_property (TpBaseConnection *self)
{
  GPtrArray *rcc;
  GValue value = G_VALUE_INIT;

  rcc = conn_requests_get_requestables (self);
  g_value_init (&value, TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST);
  g_value_take_boxed (&value, rcc);

  _tp_gdbus_connection_set_requestable_channel_classes (
      self->priv->connection_skeleton,
      dbus_g_value_build_g_variant (&value));

  g_value_unset (&value);
}

/* this is not really gtk-doc - it's for gobject-introspection */
/**
 * TpBaseConnectionClass::fill_contact_attributes:
 * @self: a connection
 * @dbus_interface: a D-Bus interface
 * @contact: a contact
 * @attributes: used to return @contact's attributes
 *
 * If @dbus_interface is recognised by this object, fill in any contact
 * attribute tokens for @contact in @attributes, and return. Otherwise,
 * chain up to the superclass' implementation.
 *
 * Since: 0.UNRELEASED
 */

static void
_tp_base_connection_fill_contact_attributes (TpBaseConnection *self,
    const gchar *dbus_interface,
    TpHandle contact,
    GVariantDict *attributes)
{
  const gchar *tmp;

  if (_tp_presence_mixin_fill_contact_attributes (self, dbus_interface, contact,
          attributes))
    return;

  if (tp_strdiff (dbus_interface, TP_IFACE_CONNECTION))
    {
      DEBUG ("contact #%u: interface '%s' unhandled", contact, dbus_interface);
      return;
    }

  tmp = tp_handle_inspect (self->priv->handles[TP_ENTITY_TYPE_CONTACT],
      contact);
  g_assert (tmp != NULL);

  /* this is always included */
  g_variant_dict_insert (attributes, TP_TOKEN_CONNECTION_CONTACT_ID, "s", tmp);
}

static void
tp_base_connection_class_init (TpBaseConnectionClass *klass)
{
  GParamSpec *param_spec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpBaseConnectionPrivate));
  object_class->constructed = tp_base_connection_constructed;
  object_class->dispose = tp_base_connection_dispose;
  object_class->finalize = tp_base_connection_finalize;
  object_class->constructor = tp_base_connection_constructor;
  object_class->get_property = tp_base_connection_get_property;
  object_class->set_property = tp_base_connection_set_property;

  klass->fill_contact_attributes = _tp_base_connection_fill_contact_attributes;

  /**
   * TpBaseConnection:protocol: (skip)
   *
   * Identifier used in the Telepathy protocol when this connection's protocol
   * name is required.
   */
  param_spec = g_param_spec_string ("protocol",
      "Telepathy identifier for protocol",
      "Identifier string used when the protocol name is required.",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PROTOCOL, param_spec);

  /**
   * TpBaseConnection:self-handle: (skip)
   *
   * The handle of type %TP_ENTITY_TYPE_CONTACT representing the local user.
   * Must be set nonzero by the subclass before moving to state CONNECTED.
   *
   * Since: 0.7.15
   */
  param_spec = g_param_spec_uint ("self-handle",
      "Connection.SelfHandle",
      "The handle of type %TP_ENTITY_TYPE_CONTACT representing the local user.",
      0, G_MAXUINT, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SELF_HANDLE, param_spec);

  /**
   * TpBaseConnection:self-id: (skip)
   *
   * The identifier representing the local user. This is the result of
   * inspecting #TpBaseConnection:self-handle.
   *
   * Since: 0.21.2
   */
  param_spec = g_param_spec_string ("self-id",
      "Connection.SelfID",
      "The identifier representing the local user.",
      "",
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SELF_ID, param_spec);

  /**
   * TpBaseConnection:interfaces: (skip)
   *
   * The set of D-Bus interfaces available on this Connection, other than
   * Connection itself.
   *
   * Since: 0.11.3
   */
  param_spec = g_param_spec_boxed ("interfaces",
      "Connection.Interfaces",
      "The set of D-Bus interfaces available on this Connection, other than "
      "Connection itself",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  /**
   * TpBaseConnection:dbus-connection:
   *
   * This object's connection to D-Bus. Read-only except during construction.
   *
   * If this property is %NULL or omitted during construction, the object will
   * automatically attempt to connect to the session bus with
   * g_bus_get_sync() just after it is ; if this fails, this
   * property will remain %NULL, and tp_base_connection_register() will fail.
   *
   * Since: 0.99.10
   */
  g_object_class_install_property (object_class, PROP_DBUS_CONNECTION,
      g_param_spec_object ("dbus-connection", "D-Bus connection",
        "The D-Bus connection used by this object", G_TYPE_DBUS_CONNECTION,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * TpBaseConnection:account-path-suffix:
   *
   * The suffix of the account object path such as
   * "gabble/jabber/chris_40example_2ecom0" for the account whose object path is
   * %TP_ACCOUNT_OBJECT_PATH_BASE + "gabble/jabber/chris_40example_2ecom0".
   * The same as returned by tp_account_get_path_suffix().
   *
   * It is given by the AccountManager in the connection parameters. Or %NULL if
   * the ConnectionManager or the AccountManager are too old.
   *
   * Since: 0.23.2
   */
  param_spec = g_param_spec_string ("account-path-suffix",
      "Account path suffix",
      "The suffix of the account path",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT_PATH_SUFFIX,
      param_spec);

  /* signal definitions */

  /**
   * TpBaseConnection::shutdown-finished: (skip)
   * @connection: the #TpBaseConnection
   *
   * Emitted by tp_base_connection_finish_shutdown() when the underlying
   * network connection has been closed; #TpBaseConnectionManager listens
   * for this signal and removes connections from its table of active
   * connections when it is received.
   */
  signals[SHUTDOWN_FINISHED] =
    g_signal_new ("shutdown-finished",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * TpBaseConnection::clients-interested:
   * @connection: the #TpBaseConnection
   * @token: the interface or part of an interface in which clients are newly
   *  interested
   *
   * Emitted when a client becomes interested in any token that was added with
   * tp_base_connection_add_possible_client_interest().
   *
   * The "signal detail" is a GQuark representing @token. Modules implementing
   * an interface (Location, say) should typically connect to a detailed signal
   * like
   * "clients-interested::im.telepathy.v1.Connection.Interface.Location"
   * rather than receiving all emissions of this signal.
   */
  signals[CLIENTS_INTERESTED] =
    g_signal_new ("clients-interested",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * TpBaseConnection::clients-uninterested:
   * @connection: the #TpBaseConnection
   * @token: the interface or part of an interface in which clients are no
   *  longer interested
   *
   * Emitted when no more clients are interested in an interface added with
   * tp_base_connection_add_possible_client_interest(), for which
   * #TpBaseConnection::clients-interested was previously emitted.
   *
   * As with #TpBaseConnection::clients-interested, the "signal detail" is a
   * GQuark representing @token. Modules implementing an interface (Location,
   * say) should typically connect to a detailed signal like
   * "clients-uninterested::im.telepathy.v1.Connection.Interface.Location"
   * rather than receiving all emissions of this signal.
   */
  signals[CLIENTS_UNINTERESTED] =
    g_signal_new ("clients-uninterested",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * TpBaseConnection::status-changed:
   * @connection: the #TpBaseConnection
   * @status: the new #TpConnectionStatus
   * @reason: the #TpConnectionStatusReason for this status change
   *
   * Emitted when the status of this connection changes. Mainly for
   * compatibility since #TpBaseConnection doesn't implement #TpSvcConnection
   * interface anymore.
   *
   * Since: 0.UNRELEASED
   */
  signals[STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}

static void
tp_base_connection_init (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  guint i;

  self->priv = priv;
  priv->status = TP_INTERNAL_CONNECTION_STATUS_NEW;

  for (i = 0; i < TP_NUM_ENTITY_TYPES; i++)
    {
      priv->handles[i] = NULL;
    }

  priv->channel_requests = g_ptr_array_new_with_free_func (g_object_unref);
  priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) client_data_free);
  priv->interests = g_hash_table_new (NULL, NULL);
}

static gchar *
squash_name (const gchar *name, guint length)
{
  GChecksum *checksum;
  gchar *squashed;

  g_assert (length >= 10);
  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) name, -1);
  squashed = g_strdup_printf (
      "%.*s_%.8s", length - 9, name, g_checksum_get_string (checksum));
  g_checksum_free (checksum);
  return squashed;
}

/**
 * tp_base_connection_register:
 * @self: A connection
 * @cm_name: The name of the connection manager in the Telepathy protocol
 * @bus_name: (out): Used to return the bus name corresponding to the connection
 *  if %TRUE is returned. To be freed by the caller.
 * @object_path: (out): Used to return the object path of the connection if
 *  %TRUE is returned. To be freed by the caller.
 * @error: Used to return an error if %FALSE is returned; may be %NULL
 *
 * Make the connection object appear on the bus, returning the bus
 * name and object path used. If %TRUE is returned, the connection owns the
 * bus name, and will release it when destroyed.
 *
 * Since 0.11.11, @bus_name and @object_path may be %NULL if the
 * strings are not needed.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
tp_base_connection_register (TpBaseConnection *self,
                             const gchar *cm_name,
                             gchar **bus_name,
                             gchar **object_path,
                             GError **error)
{
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  TpBaseConnectionPrivate *priv = self->priv;
  gchar *tmp;
  gchar *safe_proto;
  gchar *unique_name;
  guint prefix_length;
  const guint dbus_max_name_length = 255;

  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), FALSE);
  g_return_val_if_fail (cm_name != NULL, FALSE);
  g_return_val_if_fail (!self->priv->been_registered, FALSE);

  if (tp_connection_manager_check_valid_protocol_name (priv->protocol, NULL))
    {
      safe_proto = g_strdelimit (g_strdup (priv->protocol), "-", '_');
    }
  else
    {
      WARNING ("Protocol name %s is not valid - should match "
          "[A-Za-z][A-Za-z0-9-]+", priv->protocol);
      safe_proto = tp_escape_as_identifier (priv->protocol);
    }

  /* Plus two for the dots. */
  prefix_length = strlen (TP_CONN_BUS_NAME_BASE) +
      strlen (cm_name) + strlen (safe_proto) + 2;

  if (cls->get_unique_connection_name)
    {

      tmp = cls->get_unique_connection_name (self);
      g_assert (tmp != NULL);
      unique_name = tp_escape_as_identifier (tmp);
      g_free (tmp);

      if (prefix_length + strlen (unique_name) > dbus_max_name_length)
        {
          /* Is prefix is too long to make reasonable bus name? Ten = one
           * character of the original unique name plus underscore plus
           * 8-character hash.
           */
          if (prefix_length >= dbus_max_name_length - 10)
            {
              WARNING (
                  "Couldn't fit CM name + protocol name + unique name into "
                  "255 characters.");
              g_free (unique_name);
              return FALSE;
            }

          tmp = unique_name;
          unique_name = squash_name (
              tmp, dbus_max_name_length - prefix_length);
          g_free (tmp);
        }
    }
  else
    {
      unique_name = g_strdup_printf ("_%p", self);
    }

  if (!tp_base_connection_ensure_dbus (self, error))
    {
      g_free (safe_proto);
      g_free (unique_name);
      return FALSE;
    }

  priv->bus_name = g_strdup_printf (TP_CONN_BUS_NAME_BASE "%s.%s.%s",
      cm_name, safe_proto, unique_name);
  g_assert (strlen (priv->bus_name) <= 255);
  priv->object_path = g_strdup_printf (TP_CONN_OBJECT_PATH_BASE "%s/%s/%s",
      cm_name, safe_proto, unique_name);

  g_free (safe_proto);
  g_free (unique_name);

  if (!tp_dbus_connection_try_register_object (priv->dbus_connection, priv->object_path,
        self, error) ||
      !tp_dbus_connection_request_name (priv->dbus_connection, priv->bus_name, FALSE,
        error))
    {
      g_free (priv->bus_name);
      priv->bus_name = NULL;
      return FALSE;
    }

  DEBUG ("%p: bus name %s; object path %s", self, priv->bus_name,
      priv->object_path);
  self->priv->been_registered = TRUE;

  if (bus_name != NULL)
    *bus_name = g_strdup (priv->bus_name);

  if (object_path != NULL)
    *object_path = g_strdup (priv->object_path);

  return TRUE;
}

/* D-Bus methods on Connection interface ----------------------------*/

static inline TpConnectionStatusReason
conn_status_reason_from_g_error (GError *error)
{
  if (error->domain == TP_ERROR)
    {
      switch (error->code)
        {
#define OBVIOUS_MAPPING(x) \
        case TP_ERROR_ ## x: \
          return TP_CONNECTION_STATUS_REASON_ ## x

          OBVIOUS_MAPPING (NETWORK_ERROR);
          OBVIOUS_MAPPING (ENCRYPTION_ERROR);
          OBVIOUS_MAPPING (AUTHENTICATION_FAILED);
          OBVIOUS_MAPPING (CERT_NOT_PROVIDED);
          OBVIOUS_MAPPING (CERT_UNTRUSTED);
          OBVIOUS_MAPPING (CERT_EXPIRED);
          OBVIOUS_MAPPING (CERT_NOT_ACTIVATED);
          OBVIOUS_MAPPING (CERT_FINGERPRINT_MISMATCH);
          OBVIOUS_MAPPING (CERT_HOSTNAME_MISMATCH);
          OBVIOUS_MAPPING (CERT_SELF_SIGNED);
#undef OBVIOUS_MAPPING

        case TP_ERROR_PERMISSION_DENIED:
        case TP_ERROR_DOES_NOT_EXIST:
          return TP_CONNECTION_STATUS_REASON_AUTHENTICATION_FAILED;

        case TP_ERROR_CERT_INVALID:
          return TP_CONNECTION_STATUS_REASON_CERT_OTHER_ERROR;

        case TP_ERROR_CANCELLED:
          return TP_CONNECTION_STATUS_REASON_REQUESTED;

        case TP_ERROR_ENCRYPTION_NOT_AVAILABLE:
          return TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR;

        case TP_ERROR_REGISTRATION_EXISTS:
        case TP_ERROR_ALREADY_CONNECTED:
        case TP_ERROR_CONNECTION_REPLACED:
          return TP_CONNECTION_STATUS_REASON_NAME_IN_USE;

        case TP_ERROR_CONNECTION_REFUSED:
        case TP_ERROR_CONNECTION_FAILED:
        case TP_ERROR_CONNECTION_LOST:
        case TP_ERROR_SERVICE_BUSY:
          return TP_CONNECTION_STATUS_REASON_NETWORK_ERROR;

        /* current status: all TP_ERRORs up to and including
         * TP_ERROR_RESOURCE_UNAVAILABLE have been looked at */
        }
    }

  return TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
}

static gboolean
tp_base_connection_connect (_TpGDBusConnection *skeleton,
    GDBusMethodInvocation *context,
    TpBaseConnection *self)
{
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  GError *error = NULL;

  g_assert (TP_IS_BASE_CONNECTION (self));

  if (self->priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      if (cls->start_connecting (self, &error))
        {
          if (self->priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
            {
              tp_base_connection_change_status (self,
                TP_CONNECTION_STATUS_CONNECTING,
                TP_CONNECTION_STATUS_REASON_REQUESTED);
            }
        }
      else
        {
          if (self->priv->status != TP_CONNECTION_STATUS_DISCONNECTED)
            {
              tp_base_connection_change_status (self,
                TP_CONNECTION_STATUS_DISCONNECTED,
                conn_status_reason_from_g_error (error));
            }
          g_dbus_method_invocation_take_error (context, error);
          return TRUE;
        }
    }
  _tp_gdbus_connection_complete_connect (skeleton, context);
  return TRUE;
}

static gboolean
tp_base_connection_disconnect (_TpGDBusConnection *skeleton,
    GDBusMethodInvocation *context,
    TpBaseConnection *self)
{
  g_assert (TP_IS_BASE_CONNECTION (self));

  if (self->priv->disconnect_requests != NULL)
    {
      g_assert (self->priv->status == TP_CONNECTION_STATUS_DISCONNECTED);
      g_ptr_array_add (self->priv->disconnect_requests, context);
      return TRUE;
    }

  if (self->priv->status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      /* status DISCONNECTED and disconnect_requests NULL => already dead */
      _tp_gdbus_connection_complete_disconnect (skeleton, context);
      return TRUE;
    }

  self->priv->disconnect_requests = g_ptr_array_sized_new (1);
  g_ptr_array_add (self->priv->disconnect_requests, context);

  tp_base_connection_change_status (self,
      TP_CONNECTION_STATUS_DISCONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  return TRUE;
}

/**
 * tp_base_connection_get_status:
 * @self: the connection
 *
 * Return the status of this connection, as set by
 * tp_base_connection_change_status() or similar functions like
 * tp_base_connection_disconnect_with_dbus_error().
 *
 * Like the corresponding D-Bus property, this method returns
 * %TP_CONNECTION_STATUS_DISCONNECTED in two situations:
 * either the connection is newly-created (and has never emitted
 * #TpSvcConnection::status-changed), or D-Bus clients have already been
 * told that it has been destroyed (by the Disconnect D-Bus method,
 * a failed attempt to connect, or loss of an established connection).
 * Use tp_base_connection_is_destroyed() to distinguish between the two.
 *
 * Returns: the connection's status
 * Since: 0.19.1
 */
TpConnectionStatus
tp_base_connection_get_status (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self),
      TP_CONNECTION_STATUS_DISCONNECTED);

  if (self->priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      return TP_CONNECTION_STATUS_DISCONNECTED;
    }
  else
    {
      return self->priv->status;
    }
}

/**
 * tp_base_connection_is_destroyed:
 * @self: the connection
 *
 * Return whether this connection has already emitted the D-Bus signal
 * indicating that it has been destroyed.
 *
 * In particular, this can be used to distinguish between the two reasons
 * why tp_base_connection_get_status() would return
 * %TP_CONNECTION_STATUS_DISCONNECTED: it will return %FALSE if the
 * connection is newly-created, and %TRUE if the Disconnect D-Bus method
 * has been called, an attempt to connect has failed, or an established
 * connection has encountered an error.
 *
 * Returns: %TRUE if this connection is disappearing from D-Bus
 * Since: 0.19.1
 */
gboolean
tp_base_connection_is_destroyed (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), TRUE);

  /* in particular return FALSE if the status is NEW */
  return (self->priv->status == TP_CONNECTION_STATUS_DISCONNECTED);
}

/**
 * tp_base_connection_check_connected:
 * @self: the connection
 * @error: used to raise %TP_ERROR_DISCONNECTED if %FALSE is returned
 *
 * Return whether this connection is fully active and connected.
 * If it is not, raise %TP_ERROR_DISCONNECTED.
 *
 * This is equivalent to checking whether tp_base_connection_get_status()
 * returns %TP_CONNECTION_STATUS_CONNECTED; it is provided because methods
 * on the connection often need to make this check, and return a
 * #GError if it fails.
 *
 * Returns: %TRUE if this connection is connected
 * Since: 0.19.1
 */
gboolean
tp_base_connection_check_connected (TpBaseConnection *self,
    GError **error)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), FALSE);

  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED)
    return TRUE;

  g_set_error_literal (error, TP_ERROR, TP_ERROR_DISCONNECTED,
      "Connection is disconnected");
  return FALSE;
}

/**
 * tp_base_connection_get_handles:
 * @self: A connection
 * @entity_type: The entity type
 *
 * <!---->
 *
 * Returns: (transfer none): the handle repository corresponding to the given
 * entity type, or #NULL if it's unsupported or invalid.
 */
TpHandleRepoIface *
tp_base_connection_get_handles (TpBaseConnection *self,
                                TpEntityType entity_type)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  if (entity_type >= TP_NUM_ENTITY_TYPES)
    return NULL;

  return self->priv->handles[entity_type];
}


/**
 * tp_base_connection_get_self_handle: (skip)
 * @self: A connection
 *
 * Returns the #TpBaseConnection:self-handle property, which is guaranteed not
 * to be 0 once the connection has moved to the CONNECTED state.
 *
 * Returns: the current self handle of the connection.
 *
 * Since: 0.7.15
 */
TpHandle
tp_base_connection_get_self_handle (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), 0);

  return self->priv->self_handle;
}

/**
 * tp_base_connection_set_self_handle:
 * @self: A connection
 * @self_handle: The new self handle for the connection.
 *
 * Sets the #TpBaseConnection:self-handle property.  self_handle may not be 0
 * once the connection has moved to the CONNECTED state.
 *
 * Since: 0.7.15
 */
void
tp_base_connection_set_self_handle (TpBaseConnection *self,
                                    TpHandle self_handle)
{
  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED)
    g_return_if_fail (self_handle != 0);

  if (self->priv->self_handle == self_handle)
    return;

  self->priv->self_handle = self_handle;
  self->priv->self_id = NULL;

  if (self_handle != 0)
    {
      self->priv->self_id = tp_handle_inspect (
          self->priv->handles[TP_ENTITY_TYPE_CONTACT], self_handle);
    }

  _tp_gdbus_connection_set_self_handle (self->priv->connection_skeleton,
      self->priv->self_handle);
  _tp_gdbus_connection_set_self_id (self->priv->connection_skeleton,
      self->priv->self_id);
  _tp_gdbus_connection_emit_self_contact_changed (
      self->priv->connection_skeleton, self->priv->self_handle,
      self->priv->self_id);

  g_object_notify ((GObject *) self, "self-handle");
  g_object_notify ((GObject *) self, "self-id");
}


/**
 * tp_base_connection_finish_shutdown: (skip)
 * @self: The connection
 *
 * Tell the connection manager that this Connection has been disconnected,
 * has emitted StatusChanged and is ready to be removed from D-Bus.
 */
void tp_base_connection_finish_shutdown (TpBaseConnection *self)
{
  GPtrArray *contexts;
  guint i;

  g_return_if_fail (TP_IS_BASE_CONNECTION (self));
  g_return_if_fail (self->priv->status == TP_CONNECTION_STATUS_DISCONNECTED);
  g_return_if_fail (self->priv->disconnect_requests != NULL);

  contexts = self->priv->disconnect_requests;
  self->priv->disconnect_requests = NULL;

  for (i = 0; i < contexts->len; i++)
    {
      _tp_gdbus_connection_complete_disconnect (self->priv->connection_skeleton,
          g_ptr_array_index (contexts, i));
    }

  g_ptr_array_unref (contexts);

  g_signal_emit (self, signals[SHUTDOWN_FINISHED], 0);
}

/**
 * tp_base_connection_disconnect_with_dbus_error: (skip)
 * @self: The connection
 * @error_name: The D-Bus error with which the connection changed status to
 *              Disconnected
 * @details: Further details of the error, as a variant of
 *           type %G_VARIANT_TYPE_VARDICT. The keys
 *           are strings as defined in the Telepathy specification, and the
 *           values are of type %G_VARIANT_TYPE_VARIANT.
 *           %NULL is allowed, and treated as an empty dictionary.
 * @reason: The reason code to use in the StatusChanged signal
 *          (a less specific, non-extensible version of @error_name)
 *
 * Changes the #TpBaseConnection<!-- -->.status of @self to
 * %TP_CONNECTION_STATUS_DISCONNECTED, as if by a call to
 * tp_base_connection_change_status(), but additionally emits the
 * <code>ConnectionError</code> D-Bus signal to provide more details about the
 * error.
 *
 * Well-known keys for @details are documented in the Telepathy specification's
 * <ulink url='http://telepathy.freedesktop.org/spec/Connection.html#Signal:ConnectionError'>definition
 * of the ConnectionError signal</ulink>, and include:
 *
 * <itemizedlist>
 * <listitem><code>"debug-message"</code>, whose value should have type
 *    #G_TYPE_STRING, for debugging information about the
 *    disconnection which should not be shown to the user</listitem>
 * <listitem><code>"server-message"</code>, whose value should also have type
 *    #G_TYPE_STRING, for a human-readable error message from the server (in an
 *    unspecified language) explaining why the user was
 *    disconnected.</listitem>
 * </itemizedlist>
 *
 * Since: 0.7.24
 */
void
tp_base_connection_disconnect_with_dbus_error (TpBaseConnection *self,
    const gchar *error_name,
    GVariant *details,
    TpConnectionStatusReason reason)
{
  g_return_if_fail (TP_IS_BASE_CONNECTION (self));
  g_return_if_fail (tp_dbus_check_valid_interface_name (error_name, NULL));

  if (details == NULL)
      details = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

  _tp_gdbus_connection_emit_connection_error (self->priv->connection_skeleton,
      error_name, details);
  tp_base_connection_change_status (self, TP_CONNECTION_STATUS_DISCONNECTED,
      reason);
}

/**
 * tp_base_connection_change_status:
 * @self: The connection
 * @status: The new status
 * @reason: The reason for the status change
 *
 * Change the status of the connection. The allowed state transitions are:
 *
 * <itemizedlist>
 * <listitem>#TP_INTERNAL_CONNECTION_STATUS_NEW →
 *    #TP_CONNECTION_STATUS_CONNECTING</listitem>
 * <listitem>#TP_CONNECTION_STATUS_CONNECTING →
 *    #TP_CONNECTION_STATUS_CONNECTED</listitem>
 * <listitem>#TP_INTERNAL_CONNECTION_STATUS_NEW →
 *    #TP_CONNECTION_STATUS_CONNECTED (exactly equivalent to both of the above
 *    one after the other; see below)</listitem>
 * <listitem>anything except #TP_CONNECTION_STATUS_DISCONNECTED →
 *    #TP_CONNECTION_STATUS_DISCONNECTED</listitem>
 * </itemizedlist>
 *
 * Before the transition to #TP_CONNECTION_STATUS_CONNECTED, the implementation
 * must have discovered the handle for the local user and passed it to
 * tp_base_connection_set_self_handle().
 *
 * Changing from NEW to CONNECTED is implemented by doing the transition from
 * NEW to CONNECTING, followed by the transition from CONNECTING to CONNECTED;
 * it's exactly equivalent to calling tp_base_connection_change_status for
 * those two transitions one after the other.
 *
 * Any other valid transition does the following, in this order:
 *
 * <itemizedlist>
 * <listitem>Update #TpBaseConnection<!-- -->.status;</listitem>
 * <listitem>Emit the D-Bus StatusChanged signal;</listitem>
 * <listitem>Call #TpBaseConnectionClass.connecting,
 *    #TpBaseConnectionClass.connected or #TpBaseConnectionClass.disconnected
 *    as appropriate;</listitem>
 * <listitem>If the new state is #TP_CONNECTION_STATUS_DISCONNECTED, call the
 *    subclass' #TpBaseConnectionClass.shut_down callback.</listitem>
 * </itemizedlist>
 *
 * To provide more details about what happened when moving to @status
 * #TP_CONNECTION_STATUS_DISCONNECTED due to an error, consider calling
 * tp_base_connection_disconnect_with_dbus_error() instead of this function.
 *
 * Changed in 0.7.35: the @self_handle member of #TpBaseConnection was
 * previously set to 0 at this stage. It now remains non-zero until the object
 * is disposed.
 */
void
tp_base_connection_change_status (TpBaseConnection *self,
                                  TpConnectionStatus status,
                                  TpConnectionStatusReason reason)
{
  TpBaseConnectionPrivate *priv;
  TpBaseConnectionClass *klass;
  TpConnectionStatus prev_status;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = self->priv;
  klass = TP_BASE_CONNECTION_GET_CLASS (self);

  if (priv->status == TP_INTERNAL_CONNECTION_STATUS_NEW
      && status == TP_CONNECTION_STATUS_CONNECTED)
    {
      /* going straight from NEW to CONNECTED would cause confusion, so before
       * we do anything else, go via CONNECTING */
      DEBUG("from NEW to CONNECTED: going via CONNECTING first");
      tp_base_connection_change_status (self, TP_CONNECTION_STATUS_CONNECTING,
          reason);
    }

  DEBUG("was %u, now %u, for reason %u", priv->status, status, reason);
  g_return_if_fail (status != TP_INTERNAL_CONNECTION_STATUS_NEW);

  if (priv->status == status)
    {
      WARNING ("attempted to re-emit the current status %u, reason %u",
          status, reason);
      return;
    }

  prev_status = priv->status;

  /* make appropriate assertions about our state */
  switch (status)
    {
    case TP_CONNECTION_STATUS_DISCONNECTED:
      /* you can go from any state to DISCONNECTED, except DISCONNECTED;
       * and we already warned and returned if that was the case, so
       * nothing to do here */
      break;
    case TP_CONNECTION_STATUS_CONNECTED:
      /* you can only go to CONNECTED if you're CONNECTING (or NEW, but we
       * covered that by forcing a transition to CONNECTING above) */
      g_return_if_fail (prev_status == TP_CONNECTION_STATUS_CONNECTING);
      /* by the time we go CONNECTED we must have the self handle */
      g_return_if_fail (priv->self_handle != 0);
      break;
    case TP_CONNECTION_STATUS_CONNECTING:
      /* you can't go CONNECTING if a connection attempt has been made
       * before */
      g_return_if_fail (prev_status == TP_INTERNAL_CONNECTION_STATUS_NEW);
      break;
    default:
      CRITICAL ("invalid connection status %d", status);
      return;
    }

  /* now that we've finished return_if_fail'ing, we can start to make
   * the actual changes */
  priv->status = status;

  /* ref self in case user callbacks unref us */
  g_object_ref (self);

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      /* the presence of this array indicates that we are shutting down */
      if (self->priv->disconnect_requests == NULL)
        self->priv->disconnect_requests = g_ptr_array_sized_new (0);
    }

  DEBUG("emitting status-changed to %u, for reason %u", status, reason);
  _tp_gdbus_connection_set_status (self->priv->connection_skeleton, status);
  /* Emit status-changed before sending the D-Bus signal, because in practice
   * that's what happened in telepathy-glib 0.x, as demonstrated by Gabble's
   * regression tests failing otherwise. */
  g_signal_emit (self, signals[STATUS_CHANGED], 0, status, reason);
  _tp_gdbus_connection_emit_status_changed (self->priv->connection_skeleton,
      status, reason);

  /* tell subclass about the state change. In the case of
   * disconnection, shut down afterwards */
  switch (status)
    {
    case TP_CONNECTION_STATUS_CONNECTING:
      if (klass->connecting)
        (klass->connecting) (self);
      break;

    case TP_CONNECTION_STATUS_CONNECTED:
      /* the implementation should have ensured we have a valid self_handle
       * before changing the state to CONNECTED */
      g_assert (priv->self_handle != 0);
      g_assert (tp_handle_is_valid (priv->handles[TP_ENTITY_TYPE_CONTACT],
                priv->self_handle, NULL));

      /* RCC property is immutable after CONNECTED, do a last update now */
      update_rcc_property (self);

      if (klass->connected)
        (klass->connected) (self);
      break;

    case TP_CONNECTION_STATUS_DISCONNECTED:
      /* cancel all queued channel requests that weren't already cancelled by
       * the channel managers.
       */
      if (priv->channel_requests->len > 0)
        {
          g_ptr_array_foreach (priv->channel_requests, (GFunc)
            channel_request_cancel, NULL);
          g_ptr_array_remove_range (priv->channel_requests, 0,
            priv->channel_requests->len);
        }

      if (prev_status != TP_INTERNAL_CONNECTION_STATUS_NEW)
        {
          if (klass->disconnected)
            (klass->disconnected) (self);
        }
      (klass->shut_down) (self);
      tp_base_connection_unregister (self);
      break;

    default:
      g_assert_not_reached ();
    }

  g_object_unref (self);
}

static guint
get_interest_count (GHashTable *table,
    GQuark q)
{
  return GPOINTER_TO_UINT (g_hash_table_lookup (table, GUINT_TO_POINTER (q)));
}

static guint
change_interest_count (GHashTable *table,
    GQuark q,
    gint delta)
{
  guint count;

  count = get_interest_count (table, q);
  g_assert (delta >= 0 || count >= (guint) -delta);
  count += delta;
  g_hash_table_replace (table, GUINT_TO_POINTER (q), GUINT_TO_POINTER (count));

  return count;
}

static void
client_vanished_cb (GDBusConnection *connection,
    const gchar *unique_name,
    gpointer user_data)
{
  TpBaseConnection *self = user_data;
  ClientData *client;
  GHashTableIter iter;
  gpointer key;

  client = g_hash_table_lookup (self->priv->clients, unique_name);
  g_assert (client != NULL);

  /* For each iface this client was interested in, decrease the count of clients
   * interested in it. Emit "clients-uninterested" if count drops to 0. */
  g_hash_table_iter_init (&iter, client->interests);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GQuark q = GPOINTER_TO_UINT (key);
      guint count;

      count = change_interest_count (self->priv->interests, q, -1);
      if (count == 0)
        {
          const gchar *s = g_quark_to_string (q);

          DEBUG ("%s was the last client interested in %s", unique_name, s);
          g_signal_emit (self, signals[CLIENTS_UNINTERESTED], q, s);
        }
    }

  g_hash_table_remove (self->priv->clients, unique_name);
}

static ClientData *
ensure_client_data (TpBaseConnection *self,
    const gchar *unique_name)
{
  ClientData *client;

  client = g_hash_table_lookup (self->priv->clients, unique_name);
  if (client == NULL)
    {
      client = g_slice_new0 (ClientData);
      client->interests = g_hash_table_new (NULL, NULL);
      client->watch_id = g_bus_watch_name_on_connection (
          self->priv->dbus_connection,
          unique_name,
          G_BUS_NAME_WATCHER_FLAGS_NONE,
          NULL, client_vanished_cb,
          self, NULL);

      g_hash_table_insert (self->priv->clients, g_strdup (unique_name), client);
    }

  return client;
}

static void
client_data_free (ClientData *client)
{
  g_hash_table_unref (client->interests);
  g_bus_unwatch_name (client->watch_id);
  g_slice_free (ClientData, client);
}

static void
tp_base_connection_add_client_interest_impl (TpBaseConnection *self,
    const gchar *unique_name,
    const gchar * const *interests,
    gboolean only_if_uninterested)
{
  ClientData *client = NULL;
  const gchar * const *interest;

  for (interest = interests; *interest != NULL; interest++)
    {
      GQuark q = g_quark_try_string (*interest);
      guint count;

      if (q == 0)
        {
          /* we can only declare an interest in known quarks, so clearly this
           * one is not useful */
          continue;
        }

      if (!g_hash_table_contains (self->priv->interests, GUINT_TO_POINTER (q)))
        {
          /* declaring an interest in this token has no effect */
          continue;
        }

      if (client == NULL)
        client = ensure_client_data (self, unique_name);

      count = get_interest_count (client->interests, q);
      if (count > 0 && only_if_uninterested)
        {
          /* that client is already interested - nothing to do */
          continue;
        }

      count = change_interest_count (client->interests, q, +1);
      if (count == 1)
        {
          /* First time this client is interested */
          count = change_interest_count (self->priv->interests, q, +1);
          if (count == 1)
            {
              /* First client to be interested */
              DEBUG ("%s is the first to be interested in %s", unique_name,
                  *interest);
              g_signal_emit (self, signals[CLIENTS_INTERESTED], q, *interest);
            }
        }
    }
}

/**
 * tp_base_connection_add_client_interest:
 * @self: a #TpBaseConnection
 * @unique_name: the unique bus name of a D-Bus client
 * @token: a D-Bus interface or a token representing part of an interface,
 *  added with tp_base_connection_add_possible_client_interest()
 * @only_if_uninterested: only add to the interest count if the client is not
 *  already interested (appropriate for APIs that implicitly subscribe on first
 *  use if this has not been done already, like Location)
 *
 * Add a "client interest" for @token on behalf of the given client.
 *
 * This emits #TpBaseConnection::clients-interested if this was the first
 * time a client expressed an interest in this token.
 */
void
tp_base_connection_add_client_interest (TpBaseConnection *self,
    const gchar *unique_name,
    const gchar *token,
    gboolean only_if_uninterested)
{
  const gchar * tokens[2] = { NULL, NULL };

  tokens[0] = token;
  tp_base_connection_add_client_interest_impl (self, unique_name, tokens,
      only_if_uninterested);
}

static gboolean
tp_base_connection_dbus_add_client_interest (_TpGDBusConnection *skeleton,
    GDBusMethodInvocation *context,
    const gchar * const *interests,
    TpBaseConnection *self)
{
  const gchar *unique_name = NULL;

  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), FALSE);
  g_return_val_if_fail (self->priv->dbus_connection != NULL, FALSE);

  if (interests == NULL || interests[0] == NULL)
    goto finally;

  unique_name = g_dbus_method_invocation_get_sender (context);

  tp_base_connection_add_client_interest_impl (self, unique_name, interests,
      FALSE);

finally:
  _tp_gdbus_connection_complete_add_client_interest (skeleton, context);
  return TRUE;
}

static gboolean
tp_base_connection_dbus_remove_client_interest (_TpGDBusConnection *skeleton,
    GDBusMethodInvocation *context,
    const gchar * const *interests,
    TpBaseConnection *self)
{
  const gchar *unique_name;
  const gchar * const *interest;
  ClientData *client;

  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), FALSE);
  g_return_val_if_fail (self->priv->dbus_connection != NULL, FALSE);

  if (interests == NULL || interests[0] == NULL)
    goto finally;

  unique_name = g_dbus_method_invocation_get_sender (context);

  client = g_hash_table_lookup (self->priv->clients, unique_name);
  if (client == NULL)
    {
      /* unique_name doesn't own any client interests. Strictly speaking this
       * is an error, but it's probably ignoring the reply anyway, so we
       * won't tell it. */
      goto finally;
    }

  for (interest = interests; *interest != NULL; interest++)
    {
      GQuark q = g_quark_try_string (*interest);
      guint count;

      if (q == 0)
        {
          /* we can only declare an interest in known quarks, so clearly this
           * one is not useful */
          continue;
        }

      count = get_interest_count (client->interests, q);
      if (count == 0)
        {
          /* strictly speaking, this is an error, but nobody will be waiting
           * for a reply anyway */
          DEBUG ("unable to decrement %s interest in %s past zero",
              unique_name, *interest);
        }
      else if (count == 1)
        {
          /* This client is not interested anymore */
          g_hash_table_remove (client->interests, GUINT_TO_POINTER (q));
          if (g_hash_table_size (client->interests) == 0)
            g_hash_table_remove (self->priv->clients, client);

          count = change_interest_count (self->priv->interests, q, -1);
          if (count == 0)
            {
              /* This was the last client interested */
              DEBUG ("%s was the last client interested in %s", unique_name,
                  *interest);
              g_signal_emit (self, signals[CLIENTS_UNINTERESTED], q,
                  *interest);
            }
        }
      else
        {
          change_interest_count (client->interests, q, -1);
        }
    }

finally:
  _tp_gdbus_connection_complete_remove_client_interest (skeleton, context);
  return TRUE;
}

/* The handling of calls to Connection.Interface.Requests.CreateChannel is
 * split into three chained functions, which each call the next function in
 * the chain unless an error has occured.
 */
static void conn_requests_check_basic_properties (TpBaseConnection *self,
    GHashTable *requested_properties, TpChannelManagerRequestMethod method,
    GDBusMethodInvocation *context);

static void
conn_requests_requestotron_validate_handle (TpBaseConnection *self,
    GHashTable *requested_properties, TpChannelManagerRequestMethod method,
    const gchar *type, TpEntityType target_entity_type,
    TpHandle target_handle, const gchar *target_id,
    GDBusMethodInvocation *context);

static void conn_requests_offer_request (TpBaseConnection *self,
    GHashTable *requested_properties, TpChannelManagerRequestMethod method,
    const gchar *type, TpEntityType target_entity_type,
    TpHandle target_handle, GDBusMethodInvocation *context);


#define RETURN_INVALID_ARGUMENT(message) \
  G_STMT_START { \
    g_dbus_method_invocation_return_error_literal (context, TP_ERROR, \
        TP_ERROR_INVALID_ARGUMENT, message); \
    return; \
  } G_STMT_END


static void
conn_requests_requestotron (TpBaseConnection *self,
                            GVariant *requested_properties,
                            TpChannelManagerRequestMethod method,
                            GDBusMethodInvocation *context)
{
  GHashTable *asv;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  /* Call the first function in the chain handling incoming requests; it will
   * call the next steps.
   */
  asv = tp_asv_from_vardict (requested_properties);
  conn_requests_check_basic_properties (self, asv, method, context);
  g_hash_table_unref (asv);
}


static void
conn_requests_check_basic_properties (TpBaseConnection *self,
                                      GHashTable *requested_properties,
                                      TpChannelManagerRequestMethod method,
                                      GDBusMethodInvocation *context)
{
  /* Step 1:
   *  Check that ChannelType, TargetEntityType, TargetHandle, TargetID have
   *  the correct types, and that ChannelType is not omitted.
   */
  const gchar *type;
  TpEntityType target_entity_type;
  TpHandle target_handle;
  const gchar *target_id;
  gboolean valid;

  type = tp_asv_get_string (requested_properties,
        TP_PROP_CHANNEL_CHANNEL_TYPE);

  if (type == NULL)
    RETURN_INVALID_ARGUMENT ("ChannelType is required");

  target_entity_type = tp_asv_get_uint32 (requested_properties,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);

  /* Allow TargetEntityType to be missing, but not to be otherwise broken */
  if (!valid && tp_asv_lookup (requested_properties,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE) != NULL)
    RETURN_INVALID_ARGUMENT (
        "TargetEntityType must be an integer in range 0 to 2**32-1");

  target_handle = tp_asv_get_uint32 (requested_properties,
      TP_PROP_CHANNEL_TARGET_HANDLE, &valid);

  /* Allow TargetHandle to be missing, but not to be otherwise broken */
  if (!valid && tp_asv_lookup (requested_properties,
          TP_PROP_CHANNEL_TARGET_HANDLE) != NULL)
    RETURN_INVALID_ARGUMENT (
      "TargetHandle must be an integer in range 1 to 2**32-1");

  /* TargetHandle may not be 0 */
  if (valid && target_handle == 0)
    RETURN_INVALID_ARGUMENT ("TargetHandle may not be 0");

  target_id = tp_asv_get_string (requested_properties,
      TP_PROP_CHANNEL_TARGET_ID);

  /* Allow TargetID to be missing, but not to be otherwise broken */
  if (target_id == NULL && tp_asv_lookup (requested_properties,
          TP_PROP_CHANNEL_TARGET_ID) != NULL)
    RETURN_INVALID_ARGUMENT ("TargetID must be a string");

  if (tp_asv_lookup (requested_properties, TP_PROP_CHANNEL_INITIATOR_HANDLE)
      != NULL)
    RETURN_INVALID_ARGUMENT ("InitiatorHandle may not be requested");

  if (tp_asv_lookup (requested_properties, TP_PROP_CHANNEL_INITIATOR_ID)
      != NULL)
    RETURN_INVALID_ARGUMENT ("InitiatorID may not be requested");

  if (tp_asv_lookup (requested_properties, TP_PROP_CHANNEL_REQUESTED)
      != NULL)
    RETURN_INVALID_ARGUMENT ("Requested may not be requested");

  conn_requests_requestotron_validate_handle (self,
      requested_properties, method,
      type, target_entity_type, target_handle, target_id,
      context);
}


/*
 * @target_handle: non-zero if a TargetHandle property was in the request;
 *                 zero if TargetHandle was not in the request.
 */
static void
conn_requests_requestotron_validate_handle (TpBaseConnection *self,
                                            GHashTable *requested_properties,
                                            TpChannelManagerRequestMethod method,
                                            const gchar *type,
                                            TpEntityType target_entity_type,
                                            TpHandle target_handle,
                                            const gchar *target_id,
                                            GDBusMethodInvocation *context)
{
  /* Step 2: Validate the supplied set of Handle properties */
  TpHandleRepoIface *handles = NULL;
  GHashTable *altered_properties = NULL;
  GValue *target_handle_value = NULL;
  GValue *target_id_value = NULL;

  /* entity type 0 cannot have a handle */
  if (target_entity_type == TP_ENTITY_TYPE_NONE && target_handle != 0)
    RETURN_INVALID_ARGUMENT (
        "When TargetEntityType is NONE, TargetHandle must be omitted");

  /* entity type 0 cannot have a target id */
  if (target_entity_type == TP_ENTITY_TYPE_NONE && target_id != NULL)
    RETURN_INVALID_ARGUMENT (
      "When TargetEntityType is NONE, TargetID must be omitted");

  if (target_entity_type != TP_ENTITY_TYPE_NONE)
    {
      GError *error = NULL;

      if (target_handle == 0 && target_id == NULL)
        RETURN_INVALID_ARGUMENT ("When TargetEntityType is not None, either "
            "TargetHandle or TargetID must also be given");

      if (target_handle != 0 && target_id != NULL)
        RETURN_INVALID_ARGUMENT (
            "TargetHandle and TargetID must not both be given");

      handles = tp_base_connection_get_handles (self, target_entity_type);

      if (handles == NULL)
        {
          g_dbus_method_invocation_return_error_literal (context,
              TP_ERROR, TP_ERROR_NOT_AVAILABLE,
              "entity type not supported by this connection manager");
          return;
        }

      if (target_handle == 0)
        {
          /* Turn TargetID into TargetHandle */
          target_handle = tp_handle_ensure (handles, target_id, NULL, &error);

          if (target_handle == 0)
            {
              /* tp_handle_ensure can return any error in any domain; force
               * the domain and code to be as documented for CreateChannel.
               */
              error->domain = TP_ERROR;
              error->code = TP_ERROR_INVALID_HANDLE;
              g_dbus_method_invocation_take_error (context, error);
              return;
            }

          altered_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
              NULL, NULL);
          tp_g_hash_table_update (altered_properties, requested_properties,
              NULL, NULL);

          target_handle_value = tp_g_value_slice_new_uint (target_handle);
          g_hash_table_insert (altered_properties,
              TP_PROP_CHANNEL_TARGET_HANDLE, target_handle_value);

          requested_properties = altered_properties;
        }
      else
        {
          /* Check the supplied TargetHandle is valid */
          if (!tp_handle_is_valid (handles, target_handle, &error))
            {
              error->domain = TP_ERROR;
              error->code = TP_ERROR_INVALID_HANDLE;
              g_dbus_method_invocation_take_error (context, error);
              return;
            }

          altered_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
              NULL, NULL);
          tp_g_hash_table_update (altered_properties, requested_properties,
              NULL, NULL);

          target_id_value = tp_g_value_slice_new_string (
              tp_handle_inspect (handles, target_handle));
          g_hash_table_insert (altered_properties,
              TP_PROP_CHANNEL_TARGET_ID,
              target_id_value);

          requested_properties = altered_properties;
        }
    }

  conn_requests_offer_request (self, requested_properties, method, type,
      target_entity_type, target_handle, context);

  /* If we made a new table, we should destroy it, and whichever of the GValues
   * holding TargetHandle or TargetID we filled in.  The other GValues are
   * borrowed from the supplied requested_properties table.
   */
  if (altered_properties != NULL)
    {
      g_hash_table_unref (altered_properties);

      if (target_handle_value != NULL)
        tp_g_value_slice_free (target_handle_value);

      if (target_id_value != NULL)
        tp_g_value_slice_free (target_id_value);
    }
}


static void
conn_requests_offer_request (TpBaseConnection *self,
                             GHashTable *requested_properties,
                             TpChannelManagerRequestMethod method,
                             const gchar *type,
                             TpEntityType target_entity_type,
                             TpHandle target_handle,
                             GDBusMethodInvocation *context)
{
  /* Step 3: offer the incoming, vaguely sanitized request to the channel
   * managers.
   */
  TpBaseConnectionPrivate *priv = self->priv;
  TpChannelManagerRequestFunc func;
  TpChannelManagerRequest *request;
  guint i;

  switch (method)
    {
    case TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL:
      func = tp_channel_manager_create_channel;
      break;

    case TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL:
      func = tp_channel_manager_ensure_channel;
      break;

    default:
      g_assert_not_reached ();
    }

  request = _tp_channel_manager_request_new (context,
      self->priv->requests_skeleton, method,
      type, target_entity_type, target_handle);
  g_ptr_array_add (priv->channel_requests, request);

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      if (func (manager, request, requested_properties))
        return;
    }

  /* Nobody accepted the request */
  tp_dbus_g_method_return_not_implemented (context);
  request->context = NULL;

  g_ptr_array_remove (priv->channel_requests, request);
}


static gboolean
conn_requests_create_channel (_TpGDBusConnectionInterfaceRequests *skeleton,
    GDBusMethodInvocation *context,
    GVariant *requested_properties,
    TpBaseConnection *self)
{
  conn_requests_requestotron (self, requested_properties,
      TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL, context);
  return TRUE;
}


static gboolean
conn_requests_ensure_channel (_TpGDBusConnectionInterfaceRequests *skeleton,
    GDBusMethodInvocation *context,
    GVariant *requested_properties,
    TpBaseConnection *self)
{
  conn_requests_requestotron (self, requested_properties,
      TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL, context);
  return TRUE;
}


static void
requests_skeleton_init (TpBaseConnection *self)
{
  g_signal_connect_object (self->priv->requests_skeleton,
      "handle-create-channel",
      G_CALLBACK (conn_requests_create_channel),
      self, 0);
  g_signal_connect_object (self->priv->requests_skeleton,
      "handle-ensure-channel",
      G_CALLBACK (conn_requests_ensure_channel),
      self, 0);
}


/**
 * tp_base_connection_channel_manager_iter_init: (skip)
 * @iter: an uninitialized #TpChannelManagerIter
 * @self: a connection
 *
 * Initializes an iterator over the #TpChannelManager objects known to
 * @self.  It is intended to be used as followed:
 *
 * <informalexample><programlisting>
 * TpChannelManagerIter iter;
 * TpChannelManager *manager;
 *
 * tp_base_connection_channel_manager_iter_init (&amp;iter, base_conn);
 * while (tp_base_connection_channel_manager_iter_next (&amp;iter, &amp;manager))
 *   {
 *     ...do something with manager...
 *   }
 * </programlisting></informalexample>
 *
 * Since: 0.7.15
 */
void
tp_base_connection_channel_manager_iter_init (TpChannelManagerIter *iter,
                                              TpBaseConnection *self)
{
  g_return_if_fail (TP_IS_BASE_CONNECTION (self));

  iter->self = self;
  iter->index = 0;
}


/**
 * tp_base_connection_channel_manager_iter_next: (skip)
 * @iter: an initialized #TpChannelManagerIter
 * @manager_out: a location to store the channel manager, or %NULL.
 *
 * Advances @iter, and retrieves the #TpChannelManager it now points to.  If
 * there are no more channel managers, @manager_out is not set and %FALSE is
 * returned.
 *
 * Returns: %FALSE if there are no more channel managers; else %TRUE.
 *
 * Since: 0.7.15
 */
gboolean
tp_base_connection_channel_manager_iter_next (TpChannelManagerIter *iter,
                                              TpChannelManager **manager_out)
{
  TpBaseConnectionPrivate *priv;

  /* Check the caller initialized the iterator properly. */
  g_assert (TP_IS_BASE_CONNECTION (iter->self));

  priv = iter->self->priv;

  /* Be noisy if something's gone really wrong */
  g_return_val_if_fail (iter->index <= priv->channel_managers->len, FALSE);

  if (iter->index == priv->channel_managers->len)
    return FALSE;

  if (manager_out != NULL)
    *manager_out = TP_CHANNEL_MANAGER (
        g_ptr_array_index (priv->channel_managers, iter->index));

  iter->index++;
  return TRUE;
}

/**
 * tp_base_connection_get_dbus_connection:
 * @self: the connection manager
 *
 * <!-- -->
 *
 * Returns: (transfer none): the value of the
 *  #TpBaseConnectionManager:dbus-connection property. The caller must reference
 *  the returned object with g_object_ref() if it will be kept.
 *
 * Since: 0.11.3
 */
GDBusConnection *
tp_base_connection_get_dbus_connection (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  return self->priv->dbus_connection;
}

gpointer
_tp_base_connection_find_channel_manager (TpBaseConnection *self,
    GType type)
{
  guint i;

  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  for (i = 0; i < self->priv->channel_managers->len; i++)
    {
      gpointer manager = g_ptr_array_index (self->priv->channel_managers, i);

      if (g_type_is_a (G_OBJECT_TYPE (manager), type))
        {
          return manager;
        }
    }

  return NULL;
}

/**
 * tp_base_connection_get_bus_name:
 * @self: the connection
 *
 * Return the bus name starting with %TP_CONN_BUS_NAME_BASE that represents
 * this connection on D-Bus.
 *
 * The returned string belongs to the #TpBaseConnection and must be copied
 * by the caller if it will be kept.
 *
 * If this connection has never been present on D-Bus
 * (tp_base_connection_register() has never been called), return %NULL
 * instead.
 *
 * Returns: (allow-none) (transfer none): the bus name of this connection,
 *  or %NULL
 * Since: 0.19.1
 */
const gchar *
tp_base_connection_get_bus_name (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  return self->priv->bus_name;
}

/**
 * tp_base_connection_get_object_path:
 * @self: the connection
 *
 * Return the object path starting with %TP_CONN_OBJECT_PATH_BASE that
 * represents this connection on D-Bus.
 *
 * The returned string belongs to the #TpBaseConnection and must be copied
 * by the caller if it will be kept.
 *
 * If this connection has never been present on D-Bus
 * (tp_base_connection_register() has never been called), return %NULL
 * instead.
 *
 * Returns: (allow-none) (transfer none): the object path of this connection,
 *  or %NULL
 * Since: 0.19.1
 */
const gchar *
tp_base_connection_get_object_path (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  return self->priv->object_path;
}

static const gchar * const contacts_always_included_interfaces[] = {
    TP_IFACE_CONNECTION,
    NULL
};

/*
 * tp_base_connection_dup_contact_attributes_hash: (skip)
 * @self: A connection instance that uses this mixin. The connection must
 *  be connected.
 * @handles: List of handles to retrieve contacts for. Any invalid handles
 *  will be dropped from the returned mapping.
 * @interfaces: (allow-none) (array zero-terminated=1) (element-type utf8): an
 *  array of user-requested interfaces
 * @assumed_interfaces: (allow-none) (array zero-terminated=1) (element-type utf8):
 *  A list of additional interfaces to retrieve attributes
 *  from. This can be used for interfaces documented as automatically included,
 *  like %TP_IFACE_CONNECTION for GetContactAttributes,
 *  or %TP_IFACE_CONNECTION and %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST for
 *  GetContactListAttributes.
 *
 * Get contact attributes for the given contacts. Provide attributes for
 * all requested interfaces. If contact attributes are not immediately known,
 * the behaviour is defined by the interface; the attribute should either
 * be omitted from the result or replaced with a default value.
 *
 * Returns: (transfer full): a #GVariant of type "a{ua{sv}}"
 */
GVariant *
_tp_base_connection_dup_contact_attributes (TpBaseConnection *self,
    const GArray *handles,
    const gchar * const *interfaces,
    const gchar * const *assumed_interfaces)
{
  TpBaseConnectionClass *klass = TP_BASE_CONNECTION_GET_CLASS (self);
  TpHandleRepoIface *contact_repo;
  GVariantBuilder builder;
  guint i;

  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);
  g_return_val_if_fail (tp_base_connection_check_connected (self, NULL), NULL);
  g_return_val_if_fail (klass->fill_contact_attributes != NULL, NULL);

  DEBUG ("%u contact(s)", handles->len);

  for (i = 0; assumed_interfaces != NULL && assumed_interfaces[i] != NULL; i++)
    {
      DEBUG ("\tassumed interface : '%s'", assumed_interfaces[i]);
    }

  for (i = 0; interfaces != NULL && interfaces[i] != NULL; i++)
    {
      DEBUG ("\tselected interface: '%s'", interfaces[i]);
    }

  contact_repo = tp_base_connection_get_handles (self, TP_ENTITY_TYPE_CONTACT);
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ua{sv}}"));

  for (i = 0; i < handles->len; i++)
    {
      TpHandle h = g_array_index (handles, TpHandle, i);
      GVariantDict dict;
      guint j;

      DEBUG ("\tcontact #%u", h);

      if (!tp_handle_is_valid (contact_repo, h, NULL))
        {
          DEBUG ("\t\tinvalid");
          continue;
        }

      g_variant_dict_init (&dict, NULL);

      for (j = 0; assumed_interfaces != NULL && assumed_interfaces[j] != NULL; j++)
        {
          klass->fill_contact_attributes (self, assumed_interfaces[j], h,
              &dict);
        }

      for (j = 0; interfaces != NULL && interfaces[j] != NULL; j++)
        {
          klass->fill_contact_attributes (self, interfaces[j], h, &dict);
        }

      g_variant_builder_add (&builder, "{u@a{sv}}", h,
          g_variant_dict_end (&dict));
    }

  return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static gboolean
contacts_get_contact_attributes_impl (_TpGDBusConnection *skeleton,
  GDBusMethodInvocation *context,
  GVariant *handles,
  const gchar * const *interfaces,
  TpBaseConnection *conn)
{
  const TpHandle *c_array;
  GArray *array;
  gsize n;
  GVariant *result;
  /* In principle C does not guarantee that TpHandle (which is an
   * unsigned int) is exactly 32 bits. In practice, int is 32-bit on
   * every relevant platform.
   *
   * If this assertion fails, the g_variant_get_fixed_array() call
   * will fail at runtime too, and we'll have to change the code to copy
   * handles into @array one at a time. */
  G_STATIC_ASSERT (sizeof (TpHandle) == sizeof (guint32));

  _TP_GDBUS_ERROR_IF_NOT_CONNECTED (conn, context);

  c_array = g_variant_get_fixed_array (handles, &n, sizeof (TpHandle));
  array = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), n);
  g_array_append_vals (array, c_array, n);

  result = _tp_base_connection_dup_contact_attributes (conn,
      array, interfaces, contacts_always_included_interfaces);

  _tp_gdbus_connection_complete_get_contact_attributes (skeleton, context,
      result);

  g_array_unref (array);
  g_variant_unref (result);

  return TRUE;
}

typedef struct
{
  TpBaseConnection *conn;
  GStrv interfaces;
  GDBusMethodInvocation *context;
} GetContactByIdData;

static void
ensure_handle_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpHandleRepoIface *contact_repo = (TpHandleRepoIface *) source;
  GetContactByIdData *data = user_data;
  TpBaseConnection *self = data->conn;
  TpHandle handle;
  GArray *handles;
  GVariant *attributes;
  GVariant *ret;
  TpHandle ret_handle;
  GError *error = NULL;

  handle = tp_handle_ensure_finish (contact_repo, result, &error);

  if (handle == 0)
    {
      g_dbus_method_invocation_take_error (data->context, error);
      goto out;
    }

  handles = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  g_array_append_val (handles, handle);

  attributes = _tp_base_connection_dup_contact_attributes (self,
      handles, (const gchar * const *) data->interfaces,
      contacts_always_included_interfaces);
  g_variant_get_child (attributes, 0, "{u@a{sv}}", &ret_handle, &ret);
  g_assert (ret_handle == handle);

  _tp_gdbus_connection_complete_get_contact_by_id (
      self->priv->connection_skeleton, data->context, handle, ret);

  g_array_unref (handles);
  g_variant_unref (attributes);

out:
  g_object_unref (data->conn);
  g_strfreev (data->interfaces);
  g_slice_free (GetContactByIdData, data);
}

static gboolean
contacts_get_contact_by_id_impl (_TpGDBusConnection *skeleton,
  GDBusMethodInvocation *context,
  const gchar *id,
  const gchar * const *interfaces,
  TpBaseConnection *conn)
{
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_ENTITY_TYPE_CONTACT);
  GetContactByIdData *data;

  _TP_GDBUS_ERROR_IF_NOT_CONNECTED (conn, context);

  DEBUG ("%s: '%s', %u interfaces", conn->priv->object_path, id,
      (interfaces == NULL ? 0 : g_strv_length ((GStrv) interfaces)));

  data = g_slice_new0 (GetContactByIdData);
  data->conn = g_object_ref (conn);
  data->interfaces = g_strdupv ((gchar **) interfaces);
  data->context = context;

  tp_handle_ensure_async (contact_repo, conn, id, NULL,
      ensure_handle_cb, data);

  return TRUE;
}

static void
conn_skeleton_init (TpBaseConnection *self)
{
  g_signal_connect_object (self->priv->connection_skeleton, "handle-connect",
      G_CALLBACK (tp_base_connection_connect), self, 0);
  g_signal_connect_object (self->priv->connection_skeleton, "handle-disconnect",
      G_CALLBACK (tp_base_connection_disconnect), self, 0);
  g_signal_connect_object (self->priv->connection_skeleton, "handle-add-client-interest",
      G_CALLBACK (tp_base_connection_dbus_add_client_interest), self, 0);
  g_signal_connect_object (self->priv->connection_skeleton, "handle-remove-client-interest",
      G_CALLBACK (tp_base_connection_dbus_remove_client_interest), self, 0);
  g_signal_connect_object (self->priv->connection_skeleton, "handle-get-contact-attributes",
      G_CALLBACK (contacts_get_contact_attributes_impl), self, 0);
  g_signal_connect_object (self->priv->connection_skeleton, "handle-get-contact-by-id",
      G_CALLBACK (contacts_get_contact_by_id_impl), self, 0);
}

/**
 * tp_base_connection_get_account_path_suffix:
 * @self: the connection
 *
 * <!-- -->
 *
 * Returns: the same value has the #TpBaseConnection:account-path-suffix
 *  property.
 * Since: 0.23.2
 */
const gchar *
tp_base_connection_get_account_path_suffix (TpBaseConnection *self)
{
  g_return_val_if_fail (TP_IS_BASE_CONNECTION (self), NULL);

  return self->priv->account_path_suffix;
}
