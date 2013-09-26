/* ContactList base class
 *
 * Copyright © 2010 Collabora Ltd.
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

#include <config.h>
#include <telepathy-glib/base-contact-list.h>
#include <telepathy-glib/base-contact-list-internal.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/contacts-mixin.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/handle-repo-static.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-connection.h>

#include <telepathy-glib/base-connection-internal.h>
#include <telepathy-glib/handle-repo-internal.h>

/**
 * SECTION:base-contact-list
 * @title: TpBaseContactList
 * @short_description: base class for ContactList Connection interface
 *
 * This class represents a connection's contact list (roster, buddy list etc.)
 * inside a connection manager. It can be used to implement the ContactList
 * D-Bus interface on the Connection.
 *
 * Connections that use #TpBaseContactList must also have the #TpContactsMixin.
 *
 * Connection managers should subclass #TpBaseContactList, implementing the
 * virtual methods for core functionality in #TpBaseContactListClass.
 * Then, in the connection manager's #TpBaseConnection subclass:
 *
 * <itemizedlist>
 *  <listitem>
 *   <para>in #G_DEFINE_TYPE_WITH_CODE, implement
 *   #TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_LIST using
 *   tp_base_contact_list_mixin_list_iface_init():</para>
 * |[
 * G_DEFINE_TYPE_WITH_CODE (MyConnection, my_connection,
 *     TP_TYPE_BASE_CONNECTION,
 *     // ...
 *     G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_LIST,
 *         tp_base_contact_list_mixin_list_iface_init);
 *     // ...
 *     )
 * ]|
 *  </listitem>
 *  <listitem>
 *   <para>in the <function>class_init</function> method, call
 *    tp_base_contact_list_mixin_class_init() after
 *    tp_contacts_mixin_class_init():</para>
 * |[
 * // ...
 * tp_contacts_mixin_class_init (object_class,
 *     G_STRUCT_OFFSET (MyConnectionClass, contacts_mixin));
 * tp_base_contact_list_mixin_class_init (base_connection_class);
 * // ...
 * ]|
 *   <para>and include %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST in
 *    the output of
 *    #TpBaseConnectionClass.get_interfaces_always_present;</para>
 *  </listitem>
 *  <listitem>
 *   <para>in the <function>constructed</function> method, call
 *    tp_base_contact_list_mixin_register_with_contacts_mixin() on the
 *    <emphasis>connection</emphasis>.</para>
 *  </listitem>
 * </itemizedlist>
 *
 * To support user-defined contact groups too, additionally implement
 * %TP_TYPE_CONTACT_GROUP_LIST in the #TpBaseContactList subclass, add the
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS interface to the output of
 * #TpBaseConnectionClass.get interfaces_always_present, and implement the
 * %TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_GROUPS in the #TpBaseConnection
 * subclass using tp_base_contact_list_mixin_groups_iface_init().
 *
 * Optionally, one or more of the #TP_TYPE_MUTABLE_CONTACT_LIST,
 * #TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, and #TP_TYPE_BLOCKABLE_CONTACT_LIST
 * GObject interfaces may also be implemented, as appropriate to the protocol.
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactList:
 *
 * A connection's contact list (roster, buddy list) inside a connection
 * manager. Each #TpBaseConnection may have at most one #TpBaseContactList.
 *
 * This abstract base class provides the Telepathy "view" of the contact list:
 * subclasses must provide access to the "model" by implementing its virtual
 * methods in terms of the protocol's real contact list (e.g. the XMPP roster
 * object in Wocky).
 *
 * The implementation must call tp_base_contact_list_set_list_received()
 * exactly once, when the initial set of contacts has been received (or
 * immediately, if that condition is not meaningful for the protocol).
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListClass:
 * @parent_class: the parent class
 * @dup_contacts: the implementation of tp_base_contact_list_dup_contacts();
 *  every subclass must implement this itself
 * @dup_states: the implementation of
 *  tp_base_contact_list_dup_states(); every subclass must implement
 *  this itself
 * @get_contact_list_persists: the implementation of
 *  tp_base_contact_list_get_contact_list_persists(); if a subclass does not
 *  implement this itself, the default implementation always returns %TRUE,
 *  which is correct for most protocols
 * @download_async: the implementation of
 *  tp_base_contact_list_download_async(); if a subclass does not implement
 *  this itself, the default implementation will raise
 *  TP_ERROR_NOT_IMPLEMENTED asynchronously. Since: 0.18.0
 * @download_finish: the implementation of
 *  tp_base_contact_list_download_finish(). Since: 0.18.0
 *
 * The class of a #TpBaseContactList.
 *
 * Additional functionality can be added by implementing #GInterface<!-- -->s.
 * Most subclasses should implement %TP_TYPE_MUTABLE_CONTACT_LIST, which allows
 * the contact list to be altered.
 *
 * Subclasses may implement %TP_TYPE_BLOCKABLE_CONTACT_LIST if contacts can
 * be blocked from communicating with the user.
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListDupContactsFunc:
 * @self: the contact list manager
 *
 * Signature of a virtual method to list contacts with a particular state;
 * the required state is defined by the particular virtual method being
 * implemented.
 *
 * The implementation is expected to have a cache of contacts on the contact
 * list, which is updated based on protocol events.
 *
 * Returns: (transfer full): a set of contacts with the desired state
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListDupStatesFunc:
 * @self: the contact list manager
 * @contact: the contact
 * @subscribe: (out) (allow-none): used to return the state of the user's
 *  subscription to @contact's presence
 * @publish: (out) (allow-none): used to return the state of @contact's
 *  subscription to the user's presence
 * @publish_request: (out) (allow-none) (transfer full): if @publish will be
 *  set to %TP_SUBSCRIPTION_STATE_ASK, used to return the message that
 *  @contact sent when they requested permission to see the user's presence;
 *  otherwise, used to return an empty string
 *
 * Signature of a virtual method to get contacts' presences. It should return
 * @subscribe = %TP_SUBSCRIPTION_STATE_NO, @publish = %TP_SUBSCRIPTION_STATE_NO
 * and @publish_request = "", without error, for any contact not on the
 * contact list.
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListAsyncFunc:
 * @self: the contact list manager
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a virtual method that needs no additional information.
 *
 * Since: 0.18.0
 */

/**
 * TpBaseContactListActOnContactsFunc:
 * @self: the contact list manager
 * @contacts: the contacts on which to act
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a virtual method that acts on a set of contacts and needs no
 * additional information, such as removing contacts, approving or cancelling
 * presence publication, cancelling presence subscription, or removing
 * contacts.
 *
 * The virtual method should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before returning.
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListRequestSubscriptionFunc:
 * @self: the contact list manager
 * @contacts: the contacts whose subscription is to be requested
 * @message: an optional human-readable message from the user
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a virtual method to request permission to see some contacts'
 * presence.
 *
 * The virtual method should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before it calls @callback.
 *
 * Since: 0.13.0
 */

/**
 * TpBaseContactListAsyncFinishFunc:
 * @self: the contact list manager
 * @result: the result of the asynchronous operation
 * @error: used to raise an error if %FALSE is returned
 *
 * Signature of a virtual method to finish an async operation.
 *
 * Returns: %TRUE on success, or %FALSE if @error is set
 *
 * Since: 0.13.0
 */

#include "config.h"

#include <telepathy-glib/base-connection.h>

#include <telepathy-glib/handle-repo.h>

#define DEBUG_FLAG TP_DEBUG_CONTACT_LISTS
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/util-internal.h"

#define BASE_CONTACT_LIST \
  g_quark_from_static_string ("tp-base-contact-list-conn")

struct _TpBaseContactListPrivate
{
  TpBaseConnection *conn;
  TpHandleRepoIface *contact_repo;

  TpContactListState state;
  /* NULL unless state = FAILURE */
  GError *failure /* initially NULL */;

  /* owned gchar* => owned TpHandleSet */
  GHashTable *groups;

  /* DBusGMethodInvocation *s for calls to RequestBlockedContacts which are
   * waiting for the contact list to (fail to) be downloaded.
   */
  GQueue blocked_contact_requests;

  gulong status_changed_id;

  /* TRUE if @conn implements TpSvcConnectionInterface$FOO - used to
   * decide whether to emit signals on these new interfaces. Initialized in
   * the constructor and cleared when we lose @conn. */
  gboolean svc_contact_list;
  gboolean svc_contact_groups;
  gboolean svc_contact_blocking;

  /* TRUE if the contact list must be downloaded at connection. Default is
   * TRUE. */
  gboolean download_at_connection;
};

struct _TpBaseContactListClassPrivate
{
  char dummy;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TpBaseContactList,
    tp_base_contact_list,
    G_TYPE_OBJECT,
    g_type_add_class_private (g_define_type_id, sizeof (
        TpBaseContactListClassPrivate)))

/**
 * TP_TYPE_MUTABLE_CONTACT_LIST:
 *
 * Interface representing a #TpBaseContactList on which the contact list can
 * potentially be changed.
 *
 * Since: 0.13.0
 */

/**
 * TpMutableContactListInterface:
 * @parent: the parent interface
 * @request_subscription_async: the implementation of
 *  tp_base_contact_list_request_subscription_async(); must always be provided
 * @request_subscription_finish: the implementation of
 *  tp_base_contact_list_request_subscription_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @authorize_publication_async: the implementation of
 *  tp_base_contact_list_authorize_publication_async(); must always be provided
 * @authorize_publication_finish: the implementation of
 *  tp_base_contact_list_authorize_publication_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @remove_contacts_async: the implementation of
 *  tp_base_contact_list_remove_contacts_async(); must always be provided
 * @remove_contacts_finish: the implementation of
 *  tp_base_contact_list_remove_contacts_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @unsubscribe_async: the implementation of
 *  tp_base_contact_list_unsubscribe_async(); must always be provided
 * @unsubscribe_finish: the implementation of
 *  tp_base_contact_list_unsubscribe_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @unpublish_async: the implementation of
 *  tp_base_contact_list_unpublish_async(); must always be provided
 * @unpublish_finish: the implementation of
 *  tp_base_contact_list_unpublish_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @store_contacts_async: the implementation of
 *  tp_base_contact_list_store_contacts_async(); if not reimplemented,
 *  the default implementation is %NULL, which is interpreted as "do nothing"
 * @store_contacts_finish: the implementation of
 *  tp_base_contact_list_store_contacts_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @can_change_contact_list: the implementation of
 *  tp_base_contact_list_can_change_contact_list(); if not reimplemented,
 *  the default implementation always returns %TRUE
 * @get_request_uses_message: the implementation of
 *  tp_base_contact_list_get_request_uses_message(); if not reimplemented,
 *  the default implementation always returns %TRUE
 *
 * The interface vtable for a %TP_TYPE_MUTABLE_CONTACT_LIST.
 *
 * Since: 0.13.0
 */

G_DEFINE_INTERFACE (TpMutableContactList, tp_mutable_contact_list,
    TP_TYPE_BASE_CONTACT_LIST)

/**
 * TP_TYPE_BLOCKABLE_CONTACT_LIST:
 *
 * Interface representing a #TpBaseContactList on which contacts can
 * be blocked from communicating with the user.
 *
 * Since: 0.13.0
 */

/**
 * TpBlockableContactListInterface:
 * @parent: the parent interface
 * @dup_blocked_contacts: the implementation of
 *  tp_base_contact_list_dup_blocked_contacts(); must always be provided
 * @block_contacts_async: the implementation of
 *  tp_base_contact_list_block_contacts_async(); either this or
 *  @block_contacts_with_abuse_async must always be provided
 * @block_contacts_finish: the implementation of
 *  tp_base_contact_list_block_contacts_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @unblock_contacts_async: the implementation of
 *  tp_base_contact_list_unblock_contacts_async(); must always be provided
 * @unblock_contacts_finish: the implementation of
 *  tp_base_contact_list_unblock_contacts_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @can_block: the implementation of
 *  tp_base_contact_list_can_block(); if not reimplemented,
 *  the default implementation always returns %TRUE
 * @block_contacts_with_abuse_async: the implementation of
 *  tp_base_contact_list_block_contacts_async(); either this or
 *  @block_contacts_async must always be provided. If the underlying protocol
 *  does not support reporting contacts as abusive, implement
 *  @block_contacts_async instead. Since: 0.15.1
 *
 * The interface vtable for a %TP_TYPE_BLOCKABLE_CONTACT_LIST.
 *
 * Since: 0.13.0
 */

G_DEFINE_INTERFACE (TpBlockableContactList, tp_blockable_contact_list,
    TP_TYPE_BASE_CONTACT_LIST)

/**
 * TP_TYPE_CONTACT_GROUP_LIST:
 *
 * Interface representing a #TpBaseContactList on which contacts can
 * be in user-defined groups, which cannot necessarily be edited
 * (%TP_TYPE_MUTABLE_CONTACT_GROUP_LIST represents a list where these
 * groups exist and can also be edited).
 *
 * Since: 0.13.0
 */

/**
 * TpContactGroupListInterface:
 * @parent: the parent interface
 * @dup_groups: the implementation of
 *  tp_base_contact_list_dup_groups(); must always be implemented
 * @dup_group_members: the implementation of
 *  tp_base_contact_list_dup_group_members(); must always be implemented
 * @dup_contact_groups: the implementation of
 *  tp_base_contact_list_dup_contact_groups(); must always be implemented
 * @has_disjoint_groups: the implementation of
 *  tp_base_contact_list_has_disjoint_groups(); if not reimplemented,
 *  the default implementation always returns %FALSE
 * @normalize_group: the implementation of
 *  tp_base_contact_list_normalize_group(); if not reimplemented,
 *  the default implementation is %NULL, which allows any UTF-8 string
 *  as a group name (including the empty string) and assumes that any distinct
 *  group names can coexist
 *
 * The interface vtable for a %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */

G_DEFINE_INTERFACE (TpContactGroupList, tp_contact_group_list,
    TP_TYPE_BASE_CONTACT_LIST)

/**
 * TP_TYPE_MUTABLE_CONTACT_GROUP_LIST:
 *
 * Interface representing a #TpBaseContactList on which user-defined contact
 * groups can potentially be changed. %TP_TYPE_CONTACT_GROUP_LIST is a
 * prerequisite for this interface.
 *
 * Since: 0.13.0
 */

/**
 * TpMutableContactGroupListInterface:
 * @parent: the parent interface
 * @get_group_storage: the implementation of
 *  tp_base_contact_list_get_group_storage(); the default implementation is
 *  %NULL, which results in %TP_CONTACT_METADATA_STORAGE_TYPE_ANYONE being
 *  advertised
 * @set_contact_groups_async: the implementation of
 *  tp_base_contact_list_set_contact_groups_async(); must always be implemented
 * @set_contact_groups_finish: the implementation of
 *  tp_base_contact_list_set_contact_groups_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @set_group_members_async: the implementation of
 *  tp_base_contact_list_set_group_members_async(); must always be implemented
 * @set_group_members_finish: the implementation of
 *  tp_base_contact_list_set_group_members_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @add_to_group_async: the implementation of
 *  tp_base_contact_list_add_to_group_async(); must always be implemented
 * @add_to_group_finish: the implementation of
 *  tp_base_contact_list_add_to_group_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @remove_from_group_async: the implementation of
 *  tp_base_contact_list_remove_from_group_async(); must always be implemented
 * @remove_from_group_finish: the implementation of
 *  tp_base_contact_list_remove_from_group_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @remove_group_async: the implementation of
 *  tp_base_contact_list_remove_group_async(); must always be implemented
 * @remove_group_finish: the implementation of
 *  tp_base_contact_list_remove_group_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 * @rename_group_async: the implementation of
 *  tp_base_contact_list_rename_group_async(); the default implementation
 *  results in group renaming being emulated via a call to
 *  @add_to_group_async and a call to @remove_group_async
 * @rename_group_finish: the implementation of
 *  tp_base_contact_list_rename_group_finish(); the default
 *  implementation may be used if @result is a #GSimpleAsyncResult
 *
 * The interface vtable for a %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */

G_DEFINE_INTERFACE (TpMutableContactGroupList, tp_mutable_contact_group_list,
    TP_TYPE_CONTACT_GROUP_LIST)

enum {
    PROP_CONNECTION = 1,
    PROP_DOWNLOAD_AT_CONNECTION,
    N_PROPS
};

static void
tp_base_contact_list_contacts_changed_internal (TpBaseContactList *self,
    TpHandleSet *changed, TpHandleSet *removed, gboolean is_initial_roster);

static void
tp_base_contact_list_init (TpBaseContactList *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_CONTACT_LIST,
      TpBaseContactListPrivate);
  g_queue_init (&self->priv->blocked_contact_requests);
  self->priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) tp_handle_set_destroy);
}

static void
tp_base_contact_list_fail_blocked_contact_requests (
    TpBaseContactList *self,
    const GError *error)
{
  DBusGMethodInvocation *context;

  while ((context = g_queue_pop_head (&self->priv->blocked_contact_requests))
          != NULL)
    dbus_g_method_return_error (context, error);
}

static void
tp_base_contact_list_free_contents (TpBaseContactList *self)
{
  GError error = { TP_ERROR, TP_ERROR_DISCONNECTED,
      "Disconnected before blocked contacts were retrieved" };

  tp_base_contact_list_fail_blocked_contact_requests (self, &error);

  g_clear_object (&self->priv->contact_repo);

  if (self->priv->conn != NULL)
    {
      if (self->priv->status_changed_id != 0)
        {
          g_signal_handler_disconnect (self->priv->conn,
              self->priv->status_changed_id);
          self->priv->status_changed_id = 0;
        }

      tp_clear_object (&self->priv->conn);
      self->priv->svc_contact_list = FALSE;
      self->priv->svc_contact_groups = FALSE;
      self->priv->svc_contact_blocking = FALSE;
    }
}

static void
tp_base_contact_list_dispose (GObject *object)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_base_contact_list_parent_class)->dispose;

  tp_base_contact_list_free_contents (self);
  g_assert (self->priv->contact_repo == NULL);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_base_contact_list_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->conn);
      break;

    case PROP_DOWNLOAD_AT_CONNECTION:
      g_value_set_boolean (value, self->priv->download_at_connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_contact_list_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_assert (self->priv->conn == NULL);    /* construct-only */
      self->priv->conn = g_value_dup_object (value);
      g_object_set_qdata ((GObject *) self->priv->conn, BASE_CONTACT_LIST, self);
      break;

    case PROP_DOWNLOAD_AT_CONNECTION:
      self->priv->download_at_connection = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
status_changed_cb (TpBaseConnection *conn,
    guint status,
    guint reason,
    TpBaseContactList *self)
{
  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    tp_base_contact_list_free_contents (self);
}

static void
tp_base_contact_list_constructed (GObject *object)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (object);
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);
  void (*chain_up) (GObject *) =
    G_OBJECT_CLASS (tp_base_contact_list_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->conn != NULL);

  g_return_if_fail (cls->dup_contacts != NULL);
  g_return_if_fail (cls->dup_states != NULL);
  g_return_if_fail (cls->get_contact_list_persists != NULL);
  g_return_if_fail (cls->download_async != NULL);

  self->priv->svc_contact_list =
    TP_IS_SVC_CONNECTION_INTERFACE_CONTACT_LIST (self->priv->conn);
  self->priv->svc_contact_groups =
    TP_IS_SVC_CONNECTION_INTERFACE_CONTACT_GROUPS (self->priv->conn);
  self->priv->svc_contact_blocking =
    TP_IS_SVC_CONNECTION_INTERFACE_CONTACT_BLOCKING (self->priv->conn);

  if (TP_IS_MUTABLE_CONTACT_LIST (self))
    {
      TpMutableContactListInterface *iface =
        TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);

      g_return_if_fail (iface->can_change_contact_list != NULL);
      g_return_if_fail (iface->get_request_uses_message != NULL);
      g_return_if_fail (iface->request_subscription_async != NULL);
      g_return_if_fail (iface->request_subscription_finish != NULL);
      g_return_if_fail (iface->authorize_publication_async != NULL);
      g_return_if_fail (iface->authorize_publication_finish != NULL);
      /* iface->store_contacts_async == NULL is OK */
      g_return_if_fail (iface->store_contacts_finish != NULL);
      g_return_if_fail (iface->remove_contacts_async != NULL);
      g_return_if_fail (iface->remove_contacts_finish != NULL);
      g_return_if_fail (iface->unsubscribe_async != NULL);
      g_return_if_fail (iface->unsubscribe_finish != NULL);
      g_return_if_fail (iface->unpublish_async != NULL);
      g_return_if_fail (iface->unpublish_finish != NULL);
    }

  if (TP_IS_BLOCKABLE_CONTACT_LIST (self))
    {
      TpBlockableContactListInterface *iface =
        TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);

      g_return_if_fail (iface->can_block != NULL);
      g_return_if_fail (iface->dup_blocked_contacts != NULL);
      g_return_if_fail ((iface->block_contacts_async != NULL) ^
          (iface->block_contacts_with_abuse_async != NULL));
      g_return_if_fail (iface->block_contacts_finish != NULL);
      g_return_if_fail (iface->unblock_contacts_async != NULL);
      g_return_if_fail (iface->unblock_contacts_finish != NULL);
    }

  self->priv->contact_repo = tp_base_connection_get_handles (self->priv->conn,
      TP_HANDLE_TYPE_CONTACT);
  g_object_ref (self->priv->contact_repo);

  if (TP_IS_CONTACT_GROUP_LIST (self))
    {
      TpContactGroupListInterface *iface =
        TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);

      g_return_if_fail (iface->has_disjoint_groups != NULL);
      g_return_if_fail (iface->dup_groups != NULL);
      g_return_if_fail (iface->dup_contact_groups != NULL);
      g_return_if_fail (iface->dup_group_members != NULL);
    }

  if (TP_IS_MUTABLE_CONTACT_GROUP_LIST (self))
    {
      TpMutableContactGroupListInterface *iface =
        TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);

      g_return_if_fail (iface->set_contact_groups_async != NULL);
      g_return_if_fail (iface->set_contact_groups_finish != NULL);
      g_return_if_fail (iface->set_group_members_async != NULL);
      g_return_if_fail (iface->set_group_members_finish != NULL);
      g_return_if_fail (iface->add_to_group_async != NULL);
      g_return_if_fail (iface->add_to_group_finish != NULL);
      g_return_if_fail (iface->remove_from_group_async != NULL);
      g_return_if_fail (iface->remove_from_group_finish != NULL);
      g_return_if_fail (iface->remove_group_async != NULL);
      g_return_if_fail (iface->remove_group_finish != NULL);
    }

  self->priv->status_changed_id = g_signal_connect (self->priv->conn,
      "status-changed", (GCallback) status_changed_cb, self);
}

static gboolean
tp_base_contact_list_simple_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple = (GSimpleAsyncResult *) result;

  g_return_val_if_fail (g_simple_async_result_is_valid (
      result, G_OBJECT (self), NULL), FALSE);

  return !g_simple_async_result_propagate_error (simple, error);
}

static void
tp_base_contact_list_download_async_default (TpBaseContactList *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_simple_async_report_error_in_idle (G_OBJECT (self), callback,
      user_data, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
      "This CM does not implement Download");
}

static void
tp_mutable_contact_list_default_init (TpMutableContactListInterface *iface)
{
  iface->request_subscription_finish = tp_base_contact_list_simple_finish;
  iface->authorize_publication_finish = tp_base_contact_list_simple_finish;
  iface->unsubscribe_finish = tp_base_contact_list_simple_finish;
  iface->unpublish_finish = tp_base_contact_list_simple_finish;
  iface->store_contacts_finish = tp_base_contact_list_simple_finish;
  iface->remove_contacts_finish = tp_base_contact_list_simple_finish;

  iface->can_change_contact_list = tp_base_contact_list_true_func;
  iface->get_request_uses_message = tp_base_contact_list_true_func;
  /* there's no default for the other virtual methods */
}

static void
tp_blockable_contact_list_default_init (TpBlockableContactListInterface *iface)
{
  iface->block_contacts_finish = tp_base_contact_list_simple_finish;
  iface->unblock_contacts_finish = tp_base_contact_list_simple_finish;

  iface->can_block = tp_base_contact_list_true_func;
  /* there's no default for the other virtual methods */
}

static void
tp_contact_group_list_default_init (TpContactGroupListInterface *iface)
{
  iface->has_disjoint_groups = tp_base_contact_list_false_func;
  /* there's no default for the other virtual methods */
}

static void tp_base_contact_list_emulate_rename_group (TpBaseContactList *,
    const gchar *, const gchar *, GAsyncReadyCallback, gpointer);

static void
tp_mutable_contact_group_list_default_init (
    TpMutableContactGroupListInterface *iface)
{
  iface->rename_group_async = tp_base_contact_list_emulate_rename_group;

  iface->add_to_group_finish = tp_base_contact_list_simple_finish;
  iface->remove_from_group_finish = tp_base_contact_list_simple_finish;
  iface->set_contact_groups_finish = tp_base_contact_list_simple_finish;
  iface->remove_group_finish = tp_base_contact_list_simple_finish;
  iface->rename_group_finish = tp_base_contact_list_simple_finish;
  iface->set_group_members_finish = tp_base_contact_list_simple_finish;
}

static void
tp_base_contact_list_class_init (TpBaseContactListClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpBaseContactListPrivate));

  cls->priv = G_TYPE_CLASS_GET_PRIVATE (cls, TP_TYPE_BASE_CONTACT_LIST,
      TpBaseContactListClassPrivate);

  /* defaults */
  cls->get_contact_list_persists = tp_base_contact_list_true_func;
  cls->download_async = tp_base_contact_list_download_async_default;
  cls->download_finish = tp_base_contact_list_simple_finish;

  object_class->get_property = tp_base_contact_list_get_property;
  object_class->set_property = tp_base_contact_list_set_property;
  object_class->constructed = tp_base_contact_list_constructed;
  object_class->dispose = tp_base_contact_list_dispose;

  /**
   * TpBaseContactList:connection:
   *
   * The connection that owns this contact list.
   * Read-only except during construction.
   *
   * Since: 0.13.0
   */
  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection", "Connection",
        "The connection that owns this contact list",
        TP_TYPE_BASE_CONNECTION,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * TpBaseContactList:download-at-connection:
   *
   * Whether the roster should be automatically downloaded at connection.
   *
   * This property doesn't change anything in TpBaseContactsList's behaviour.
   * Implementations should check this property when they become connected
   * and in their Download method, and behave accordingly.
   *
   * Since: 0.18.0
   */
  g_object_class_install_property (object_class, PROP_DOWNLOAD_AT_CONNECTION,
      g_param_spec_boolean ("download-at-connection", "Download at connection",
        "Whether the roster should be automatically downloaded at connection",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

/**
 * tp_base_contact_list_set_list_pending:
 * @self: the contact list manager
 *
 * Record that receiving the initial contact list is in progress.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_set_list_pending (TpBaseContactList *self)
{
  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->state == TP_CONTACT_LIST_STATE_NONE);

  if (self->priv->conn == NULL ||
      self->priv->state != TP_CONTACT_LIST_STATE_NONE)
    return;

  self->priv->state = TP_CONTACT_LIST_STATE_WAITING;
  tp_svc_connection_interface_contact_list_emit_contact_list_state_changed (
      self->priv->conn, self->priv->state);
}

/**
 * tp_base_contact_list_set_list_failed:
 * @self: the contact list manager
 * @domain: a #GError domain
 * @code: a #GError code
 * @message: a #GError message
 *
 * Record that receiving the initial contact list has failed.
 *
 * This method cannot be called after tp_base_contact_list_set_list_received()
 * is called.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_set_list_failed (TpBaseContactList *self,
    GQuark domain,
    gint code,
    const gchar *message)
{
  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS);

  if (self->priv->conn == NULL)
    return;

  self->priv->state = TP_CONTACT_LIST_STATE_FAILURE;
  g_clear_error (&self->priv->failure);
  self->priv->failure = g_error_new_literal (domain, code, message);
  tp_svc_connection_interface_contact_list_emit_contact_list_state_changed (
      self->priv->conn, self->priv->state);

  tp_base_contact_list_fail_blocked_contact_requests (self,
      self->priv->failure);
}

/**
 * tp_base_contact_list_set_list_received:
 * @self: the contact list manager
 *
 * Record that the initial contact list has been received. This allows the
 * contact list manager to reply to requests for the list of contacts that
 * were previously made, and reply to subsequent requests immediately.
 *
 * This method can be called at most once for a contact list manager.
 *
 * In protocols where there's no good definition of the point at which the
 * initial contact list has been received (such as link-local XMPP), this
 * method may be called immediately.
 *
 * The #TpBaseContactListDupContactsFunc and
 * #TpBaseContactListDupStatesFunc must already give correct
 * results when entering this method.
 *
 * If implemented, tp_base_contact_list_dup_blocked_contacts() must also
 * give correct results when entering this method.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_set_list_received (TpBaseContactList *self)
{
  TpHandleSet *contacts;
  guint i;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS);

  if (self->priv->conn == NULL)
    return;

  self->priv->state = TP_CONTACT_LIST_STATE_SUCCESS;
  /* we emit the signal for this later */

  contacts = tp_base_contact_list_dup_contacts (self);
  g_return_if_fail (contacts != NULL);

  /* A quick sanity check to make sure that faulty implementations crash
   * during development :-) */
  tp_base_contact_list_dup_states (self,
      tp_base_connection_get_self_handle (self->priv->conn),
      NULL, NULL, NULL);

  if (DEBUGGING)
    {
      gchar *tmp = tp_intset_dump (tp_handle_set_peek (contacts));

      DEBUG ("Initial contacts: %s", tmp);
      g_free (tmp);
    }

  tp_base_contact_list_contacts_changed_internal (self, contacts, NULL, TRUE);

  if (tp_base_contact_list_can_block (self))
    {
      TpHandleSet *blocked;

      blocked = tp_base_contact_list_dup_blocked_contacts (self);

      if (DEBUGGING)
        {
          gchar *tmp = tp_intset_dump (tp_handle_set_peek (contacts));

          DEBUG ("Initially blocked contacts: %s", tmp);
          g_free (tmp);
        }

      tp_base_contact_list_contact_blocking_changed (self, blocked);

      if (self->priv->svc_contact_blocking &&
          self->priv->blocked_contact_requests.length > 0)
        {
          GHashTable *map = tp_handle_set_to_identifier_map (blocked);
          DBusGMethodInvocation *context;

          while ((context = g_queue_pop_head (
                      &self->priv->blocked_contact_requests)) != NULL)
            tp_svc_connection_interface_contact_blocking_return_from_request_blocked_contacts (context, map);

          g_hash_table_unref (map);
        }

      tp_handle_set_destroy (blocked);
    }

  /* The natural thing to do here would be to iterate over all contacts, and
   * for each contact, emit a signal adding them to their own groups. However,
   * that emits a signal per contact. Here we turn the data model inside out,
   * to emit one signal per group - that's probably fewer. */
  if (TP_IS_CONTACT_GROUP_LIST (self))
    {
      GStrv groups = tp_base_contact_list_dup_groups (self);

      tp_base_contact_list_groups_created (self,
          (const gchar * const *) groups, -1);

      for (i = 0; groups != NULL && groups[i] != NULL; i++)
        {
          TpHandleSet *members = tp_base_contact_list_dup_group_members (self,
              groups[i]);

          tp_base_contact_list_groups_changed (self, members,
              (const gchar * const *) groups + i, 1, NULL, 0);
          tp_handle_set_destroy (members);
        }

      g_strfreev (groups);
    }

  tp_handle_set_destroy (contacts);

  /* emit this last, so people can distinguish between the initial state
   * and subsequent changes */
  tp_svc_connection_interface_contact_list_emit_contact_list_state_changed (
      self->priv->conn, self->priv->state);
}

char
_tp_base_contact_list_presence_state_to_letter (TpSubscriptionState ps)
{
  switch (ps)
    {
    case TP_SUBSCRIPTION_STATE_UNKNOWN:
      return '?';

    case TP_SUBSCRIPTION_STATE_YES:
      return 'Y';

    case TP_SUBSCRIPTION_STATE_NO:
      return 'N';

    case TP_SUBSCRIPTION_STATE_ASK:
      return 'A';

    case TP_SUBSCRIPTION_STATE_REMOVED_REMOTELY:
      return 'R';

    default:
      return '!';
    }
}

/**
 * tp_base_contact_list_contacts_changed:
 * @self: the contact list manager
 * @changed: (allow-none): a set of contacts added to the contact list or with
 *  a changed status
 * @removed: (allow-none): a set of contacts removed from the contact list
 *
 * Emit signals for a change to the contact list.
 *
 * The results of #TpBaseContactListDupContactsFunc and
 * #TpBaseContactListDupStatesFunc must already reflect
 * the contacts' new statuses when entering this method (in practice, this
 * means that implementations must update their own cache of contacts
 * before calling this method).
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_contacts_changed (TpBaseContactList *self,
    TpHandleSet *changed,
    TpHandleSet *removed)
{
  tp_base_contact_list_contacts_changed_internal (self, changed, removed,
      FALSE);
}

static void
tp_base_contact_list_contacts_changed_internal (TpBaseContactList *self,
    TpHandleSet *changed,
    TpHandleSet *removed,
    gboolean is_initial_roster)
{
  GHashTable *changes;
  GHashTable *change_ids;
  GHashTable *removal_ids;
  TpIntsetFastIter iter;
  TpHandle contact;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));

  /* don't do anything if we're disconnecting, or if we haven't had the
   * initial contact list yet */
  if (tp_base_contact_list_get_state (self, NULL) !=
      TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  changes = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) tp_value_array_free);
  change_ids = g_hash_table_new (NULL, NULL);

  if (changed != NULL)
    tp_intset_fast_iter_init (&iter, tp_handle_set_peek (changed));

  while (changed != NULL && tp_intset_fast_iter_next (&iter, &contact))
    {
      TpSubscriptionState subscribe = TP_SUBSCRIPTION_STATE_NO;
      TpSubscriptionState publish = TP_SUBSCRIPTION_STATE_NO;
      gchar *publish_request = NULL;

      tp_base_contact_list_dup_states (self, contact,
          &subscribe, &publish, &publish_request);

      if (publish_request == NULL)
        publish_request = g_strdup ("");

      DEBUG ("Contact %s: subscribe=%c publish=%c '%s'",
          tp_handle_inspect (self->priv->contact_repo, contact),
          _tp_base_contact_list_presence_state_to_letter (subscribe),
          _tp_base_contact_list_presence_state_to_letter (publish),
          publish_request);

      g_hash_table_insert (changes, GUINT_TO_POINTER (contact),
          tp_value_array_build (3,
            G_TYPE_UINT, subscribe,
            G_TYPE_UINT, publish,
            G_TYPE_STRING, publish_request,
            G_TYPE_INVALID));
      g_free (publish_request);

      g_hash_table_insert (change_ids, GUINT_TO_POINTER (contact),
          (gchar *) tp_handle_inspect (self->priv->contact_repo, contact));
    }

  removal_ids = g_hash_table_new (NULL, NULL);

  if (removed != NULL)
    {
      GArray *removals = tp_handle_set_to_array (removed);
      guint i;

      for (i = 0; i < removals->len; i++)
        {
          TpHandle handle = g_array_index (removals, TpHandle, i);

          g_hash_table_insert (removal_ids, GUINT_TO_POINTER (handle),
              (gchar *) tp_handle_inspect (self->priv->contact_repo, handle));
        }

      g_array_unref (removals);
    }

  if (g_hash_table_size (changes) > 0 || g_hash_table_size (removal_ids) > 0)
    {
      DEBUG ("ContactsChanged([%u changed], [%u removed])",
          g_hash_table_size (changes), g_hash_table_size (removal_ids));

      if (self->priv->svc_contact_list)
        {
          tp_svc_connection_interface_contact_list_emit_contacts_changed (
              self->priv->conn, changes, change_ids, removal_ids);
        }
    }

  g_hash_table_unref (changes);
  g_hash_table_unref (change_ids);
  g_hash_table_unref (removal_ids);
}

/**
 * tp_base_contact_list_one_contact_changed:
 * @self: the contact list manager
 * @changed: a contact handle
 *
 * Convenience wrapper around tp_base_contact_list_contacts_changed() for a
 * single handle in the 'changed' set and no 'removed' set.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_one_contact_changed (TpBaseContactList *self,
    TpHandle changed)
{
  TpHandleSet *set;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));

  /* if we're disconnecting, we might not have a handle repository any more:
   * tp_base_contact_list_contacts_changed does nothing in that situation */
  if (self->priv->contact_repo == NULL)
    return;

  set = tp_handle_set_new_containing (self->priv->contact_repo, changed);
  tp_base_contact_list_contacts_changed (self, set, NULL);
  tp_handle_set_destroy (set);
}

/**
 * tp_base_contact_list_one_contact_removed:
 * @self: the contact list manager
 * @removed: a contact handle
 *
 * Convenience wrapper around tp_base_contact_list_contacts_changed() for a
 * single handle in the 'removed' set and no 'changed' set.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_one_contact_removed (TpBaseContactList *self,
    TpHandle removed)
{
  TpHandleSet *set;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));

  /* if we're disconnecting, we might not have a handle repository any more:
   * tp_base_contact_list_contacts_changed does nothing in that situation */
  if (self->priv->contact_repo == NULL)
    return;

  set = tp_handle_set_new_containing (self->priv->contact_repo, removed);
  tp_base_contact_list_contacts_changed (self, NULL, set);
  tp_handle_set_destroy (set);
}

/**
 * tp_base_contact_list_contact_blocking_changed:
 * @self: the contact list manager
 * @changed: a set of contacts who were blocked or unblocked
 *
 * Emit signals for a change to the blocked contacts list.
 *
 * tp_base_contact_list_dup_blocked_contacts()
 * must already reflect the contacts' new statuses when entering this method
 * (in practice, this means that implementations must update their own cache
 * of contacts before calling this method).
 *
 * It is an error to call this method if tp_base_contact_list_can_block()
 * would return %FALSE.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_contact_blocking_changed (TpBaseContactList *self,
    TpHandleSet *changed)
{
  TpHandleSet *now_blocked;
  GHashTable *blocked_contacts, *unblocked_contacts;
  TpIntsetFastIter iter;
  TpHandle handle;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (changed != NULL);

  /* don't do anything if we're disconnecting, or if we haven't had the
   * initial contact list yet */
  if (tp_base_contact_list_get_state (self, NULL) !=
      TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  g_return_if_fail (tp_base_contact_list_can_block (self));

  now_blocked = tp_base_contact_list_dup_blocked_contacts (self);

  blocked_contacts = g_hash_table_new (NULL, NULL);
  unblocked_contacts = g_hash_table_new (NULL, NULL);

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (changed));

  while (tp_intset_fast_iter_next (&iter, &handle))
    {
      const char *id = tp_handle_inspect (self->priv->contact_repo, handle);

      if (tp_handle_set_is_member (now_blocked, handle))
        {
          g_hash_table_insert (blocked_contacts, GUINT_TO_POINTER (handle),
              (gpointer) id);
        }
      else
        {
          g_hash_table_insert (unblocked_contacts, GUINT_TO_POINTER (handle),
              (gpointer) id);
        }

      DEBUG ("Contact %s: blocked=%c", id,
          tp_handle_set_is_member (now_blocked, handle) ? 'Y' : 'N');
    }

  if (self->priv->svc_contact_blocking &&
      (g_hash_table_size (blocked_contacts) > 0 ||
       g_hash_table_size (unblocked_contacts) > 0))
    tp_svc_connection_interface_contact_blocking_emit_blocked_contacts_changed (
        self->priv->conn, blocked_contacts, unblocked_contacts);

  g_hash_table_unref (blocked_contacts);
  g_hash_table_unref (unblocked_contacts);
  tp_handle_set_destroy (now_blocked);
}

/**
 * tp_base_contact_list_dup_contacts:
 * @self: a contact list manager
 *
 * Return the contact list. It is incorrect to call this method before
 * tp_base_contact_list_set_list_received() has been called, or after the
 * connection has disconnected.
 *
 * This is a virtual method, implemented using
 * #TpBaseContactListClass.dup_contacts. Every subclass of #TpBaseContactList
 * must implement this method.
 *
 * If the contact list implements %TP_TYPE_BLOCKABLE_CONTACT_LIST, blocked
 * contacts should not appear in the result of this method unless they are
 * considered to be on the contact list for some other reason.
 *
 * Returns: (transfer full): a new #TpHandleSet of contact handles
 *
 * Since: 0.13.0
 */
TpHandleSet *
tp_base_contact_list_dup_contacts (TpBaseContactList *self)
{
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, NULL);
  g_return_val_if_fail (cls->dup_contacts != NULL, NULL);
  g_return_val_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS, NULL);

  return cls->dup_contacts (self);
}

/**
 * tp_base_contact_list_request_subscription_async:
 * @self: a contact list manager
 * @contacts: the contacts whose subscription is to be requested
 * @message: an optional human-readable message from the user
 * @callback: a callback to call when the request for subscription succeeds
 *  or fails
 * @user_data: optional data to pass to @callback
 *
 * Request permission to see some contacts' presence.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpMutableContactListInterface.request_subscription_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before it calls @callback.
 *
 * If @message will be ignored,
 * #TpMutableContactListInterface.get_request_uses_message should also be
 * reimplemented to return %FALSE.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_request_subscription_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);
  g_return_if_fail (mutable_iface->request_subscription_async != NULL);

  mutable_iface->request_subscription_async (self, contacts, message, callback,
      user_data);
}

/**
 * tp_base_contact_list_request_subscription_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_request_subscription_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_request_subscription_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.request_subscription_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_request_subscription_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->request_subscription_finish != NULL,
      FALSE);

  return mutable_iface->request_subscription_finish (self, result, error);
}

/**
 * tp_base_contact_list_dup_states:
 * @self: a contact list manager
 * @contact: the contact
 * @subscribe: (out) (allow-none): used to return the state of the user's
 *  subscription to @contact's presence
 * @publish: (out) (allow-none): used to return the state of @contact's
 *  subscription to the user's presence
 * @publish_request: (out) (allow-none) (transfer full): if @publish will be
 *  set to %TP_SUBSCRIPTION_STATE_ASK, used to return the message that
 *  @contact sent when they requested permission to see the user's presence;
 *  otherwise, used to return an empty string
 *
 * Return the presence subscription state of @contact. It is incorrect to call
 * this method before tp_base_contact_list_set_list_received() has been
 * called, or after the connection has disconnected.
 *
 * This is a virtual method, implemented using
 * #TpBaseContactListClass.dup_states. Every subclass of #TpBaseContactList
 * must implement this method.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_dup_states (TpBaseContactList *self,
    TpHandle contact,
    TpSubscriptionState *subscribe,
    TpSubscriptionState *publish,
    gchar **publish_request)
{
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);

  g_return_if_fail (cls != NULL);
  g_return_if_fail (cls->dup_states != NULL);
  g_return_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS);

  cls->dup_states (self, contact, subscribe, publish, publish_request);

  if (publish_request != NULL && *publish_request == NULL)
    *publish_request = g_strdup ("");
}

/**
 * tp_base_contact_list_authorize_publication_async:
 * @self: a contact list manager
 * @contacts: the contacts to whom presence will be published
 * @callback: a callback to call when the authorization succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Give permission for some contacts to see the local user's presence.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpMutableContactListInterface.authorize_publication_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before it calls @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_authorize_publication_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);
  g_return_if_fail (mutable_iface->authorize_publication_async != NULL);

  mutable_iface->authorize_publication_async (self, contacts, callback,
      user_data);
}

/**
 * tp_base_contact_list_authorize_publication_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_authorize_publication_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_authorize_publication_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.authorize_publication_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_authorize_publication_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->authorize_publication_finish != NULL,
      FALSE);

  return mutable_iface->authorize_publication_finish (self, result, error);
}

/**
 * tp_base_contact_list_store_contacts_async:
 * @self: a contact list manager
 * @contacts: the contacts to be stored
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Store @contacts on the contact list, without attempting to subscribe to
 * them or send presence to them. If this is not possible, do nothing.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method, which may be implemented using
 * #TpMutableContactListInterface.store_contacts_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * If the implementation of
 * #TpMutableContactListInterface.store_contacts_async is %NULL (which is
 * the default), this method calls @callback to signal success, but does
 * nothing in the underlying protocol.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_store_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);

  if (mutable_iface->store_contacts_async == NULL)
    tp_simple_async_report_success_in_idle ((GObject *) self,
        callback, user_data, NULL);
  else
    mutable_iface->store_contacts_async (self, contacts, callback,
        user_data);
}

/**
 * tp_base_contact_list_store_contacts_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_store_contacts_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_store_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.store_contacts_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_store_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->store_contacts_finish != NULL, FALSE);

  return mutable_iface->store_contacts_finish (self, result, error);
}

/**
 * tp_base_contact_list_remove_contacts_async:
 * @self: a contact list manager
 * @contacts: the contacts to be removed
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Remove @contacts from the contact list entirely; this includes the
 * effect of both tp_base_contact_list_unsubscribe_async() and
 * tp_base_contact_list_unpublish_async(), and also reverses the effect of
 * tp_base_contact_list_store_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, this method does nothing.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpMutableContactListInterface.remove_contacts_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_remove_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);
  g_return_if_fail (mutable_iface->remove_contacts_async != NULL);

  mutable_iface->remove_contacts_async (self, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_remove_contacts_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_remove_contacts_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_remove_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.remove_contacts_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_remove_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->remove_contacts_finish != NULL, FALSE);

  return mutable_iface->remove_contacts_finish (self, result, error);
}

/**
 * tp_base_contact_list_unsubscribe_async:
 * @self: a contact list manager
 * @contacts: the contacts whose presence will no longer be received
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Cancel a pending subscription request to @contacts, or attempt to stop
 * receiving their presence.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpMutableContactListInterface.unsubscribe_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_unsubscribe_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);
  g_return_if_fail (mutable_iface->unsubscribe_async != NULL);

  mutable_iface->unsubscribe_async (self, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_unsubscribe_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_unsubscribe_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_unsubscribe_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.unsubscribe_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_unsubscribe_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->unsubscribe_finish != NULL, FALSE);

  return mutable_iface->unsubscribe_finish (self, result, error);
}

/**
 * tp_base_contact_list_unpublish_async:
 * @self: a contact list manager
 * @contacts: the contacts to whom presence will no longer be published
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Reject a pending subscription request from @contacts, or attempt to stop
 * sending presence to them.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpMutableContactListInterface.unpublish_async.
 * The implementation should call tp_base_contact_list_contacts_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_unpublish_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_iface != NULL);
  g_return_if_fail (mutable_iface->unpublish_async != NULL);

  mutable_iface->unpublish_async (self, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_unpublish_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_unpublish_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_unpublish_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactListInterface.unpublish_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_unpublish_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactListInterface *mutable_iface;

  mutable_iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_iface->unpublish_finish != NULL, FALSE);

  return mutable_iface->unpublish_finish (self, result, error);
}

/**
 * TpBaseContactListBooleanFunc:
 * @self: a contact list manager
 *
 * Signature of a virtual method that returns a boolean result. These are used
 * for feature-discovery.
 *
 * For the simple cases of a constant result, use
 * tp_base_contact_list_true_func() or tp_base_contact_list_false_func().
 *
 * Returns: a boolean result
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_true_func:
 * @self: ignored
 *
 * An implementation of #TpBaseContactListBooleanFunc that returns %TRUE,
 * for use in simple cases.
 *
 * Returns: %TRUE
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_true_func (TpBaseContactList *self G_GNUC_UNUSED)
{
  return TRUE;
}

/**
 * tp_base_contact_list_false_func:
 * @self: ignored
 *
 * An implementation of #TpBaseContactListBooleanFunc that returns %FALSE,
 * for use in simple cases.
 *
 * Returns: %FALSE
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_false_func (TpBaseContactList *self G_GNUC_UNUSED)
{
  return FALSE;
}

/**
 * tp_base_contact_list_can_change_contact_list:
 * @self: a contact list manager
 *
 * Return whether the contact list can be changed.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, this method always returns %FALSE.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST this is a virtual
 * method, implemented using
 * #TpMutableContactListInterface.can_change_contact_list.
 * The default implementation always returns %TRUE.
 *
 * In the rare case of a protocol where subscriptions can only sometimes be
 * changed and this is detected while connecting, the #TpBaseContactList
 * subclass should implement %TP_TYPE_MUTABLE_CONTACT_LIST.
 * #TpMutableContactListInterface.can_change_contact_list to its own
 * implementation, whose result must remain constant after the
 * #TpBaseConnection has moved to state %TP_CONNECTION_STATUS_CONNECTED.
 *
 * (For instance, this could be useful for XMPP, where subscriptions can
 * normally be altered, but on connections to Facebook Chat servers this is
 * not actually supported.)
 *
 * Returns: %TRUE if the contact list can be changed
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_can_change_contact_list (TpBaseContactList *self)
{
  TpMutableContactListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), FALSE);

  if (!TP_IS_MUTABLE_CONTACT_LIST (self))
    return FALSE;

  iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->can_change_contact_list != NULL, FALSE);

  return iface->can_change_contact_list (self);
}

/**
 * tp_base_contact_list_get_contact_list_persists:
 * @self: a contact list manager
 *
 * Return whether subscriptions on this protocol persist between sessions
 * (i.e. are stored on the server).
 *
 * This is a virtual method, implemented using
 * #TpBaseContactListClass.get_contact_list_persists.
 *
 * The default implementation is tp_base_contact_list_true_func(), which is
 * correct for most protocols. Protocols where the contact list isn't stored
 * should use tp_base_contact_list_false_func() as their implementation.
 *
 * In the rare case of a protocol where subscriptions sometimes persist
 * and this is detected while connecting, the subclass can implement another
 * #TpBaseContactListBooleanFunc (whose result must remain constant
 * after the #TpBaseConnection has moved to state
 * %TP_CONNECTION_STATUS_CONNECTED), and use that as the implementation.
 *
 * Returns: %TRUE if subscriptions persist
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_get_contact_list_persists (TpBaseContactList *self)
{
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, TRUE);
  g_return_val_if_fail (cls->get_contact_list_persists != NULL, TRUE);

  return cls->get_contact_list_persists (self);
}

/**
 * tp_base_contact_list_get_download_at_connection:
 * @self: a contact list manager
 *
 * This function returns the
 * #TpBaseContactList:download-at-connection property.
 *
 * Returns: the #TpBaseContactList:download-at-connection property
 *
 * Since: 0.18.0
 */
gboolean
tp_base_contact_list_get_download_at_connection (TpBaseContactList *self)
{
  return self->priv->download_at_connection;
}

/**
 * tp_base_contact_list_download_async:
 * @self: a contact list manager
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Download the contact list when it is not done automatically at
 * connection.
 *
 * If the #TpBaseContactList subclass does not override
 * download_async, the default implementation will raise
 * TP_ERROR_NOT_IMPLEMENTED asynchronously.
 *
 * Since: 0.18.0
 */
void
tp_base_contact_list_download_async (TpBaseContactList *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);

  g_return_if_fail (cls != NULL);
  g_return_if_fail (cls->download_async != NULL);

  return cls->download_async (self, callback, user_data);
}

/**
 * tp_base_contact_list_download_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_download_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_download_async().
 *
 * This is a virtual method which may be implemented using
 * #TpBaseContactListClass.download_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.18.0
 */
gboolean
tp_base_contact_list_download_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpBaseContactListClass *cls = TP_BASE_CONTACT_LIST_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, FALSE);
  g_return_val_if_fail (cls->download_finish != NULL, FALSE);

  return cls->download_finish (self, result, error);
}

/**
 * tp_base_contact_list_get_request_uses_message:
 * @self: a contact list manager
 *
 * Return whether the tp_base_contact_list_request_subscription_async()
 * method's @message argument is actually used.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_LIST, this method is meaningless, and always
 * returns %FALSE.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_LIST, this is a virtual
 * method, implemented using
 * #TpMutableContactListInterface.get_request_uses_message.
 * The default implementation always returns %TRUE, which is correct for most
 * protocols; subclasses may reimplement this method with
 * tp_base_contact_list_false_func() or a custom implementation if desired.
 *
 * Returns: %TRUE if tp_base_contact_list_request_subscription_async() will not
 *  ignore its @message argument
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_get_request_uses_message (TpBaseContactList *self)
{
  TpMutableContactListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), FALSE);

  if (!TP_IS_MUTABLE_CONTACT_LIST (self))
    return FALSE;

  iface = TP_MUTABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_request_uses_message != NULL, FALSE);

  return iface->get_request_uses_message (self);
}

/**
 * TpBaseContactListBlockContactsWithAbuseFunc:
 * @self: the contact list manager
 * @contacts: the contacts to block
 * @report_abusive: whether to report the contacts as abusive to the server
 *  operator
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a virtual method that blocks a set of contacts, optionally
 * reporting them to the server operator as abusive.
 *
 * Since: 0.15.1
 */

/**
 * tp_base_contact_list_can_block:
 * @self: a contact list manager
 *
 * Return whether this contact list has a list of blocked contacts. If it
 * does, that list is assumed to be modifiable.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, this method always returns %FALSE.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method, implemented using #TpBlockableContactListInterface.can_block.
 * The default implementation always returns %TRUE.
 *
 * In the case of a protocol where blocking may or may not work
 * and this is detected while connecting, the subclass can implement another
 * #TpBaseContactListBooleanFunc (whose result must remain constant
 * after the #TpBaseConnection has moved to state
 * %TP_CONNECTION_STATUS_CONNECTED), and use that as the implementation.
 *
 * (For instance, this could be useful for XMPP, where support for contact
 * blocking is server-dependent: telepathy-gabble 0.8.x implements it for
 * connections to Google Talk servers, but not for any other server.)
 *
 * Returns: %TRUE if communication from contacts can be blocked
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_can_block (TpBaseContactList *self)
{
  TpBlockableContactListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), FALSE);

  if (!TP_IS_BLOCKABLE_CONTACT_LIST (self))
    return FALSE;

  iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->can_block != NULL, FALSE);

  return iface->can_block (self);
}

/**
 * tp_base_contact_list_dup_blocked_contacts:
 * @self: a contact list manager
 *
 * Return the list of blocked contacts. It is incorrect to call this method
 * before tp_base_contact_list_set_list_received() has been called, after
 * the connection has disconnected, or on a #TpBaseContactList that does
 * not implement %TP_TYPE_BLOCKABLE_CONTACT_LIST.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method, implemented using
 * #TpBlockableContactListInterface.dup_blocked_contacts.
 * It must always be implemented.
 *
 * Returns: (transfer full): a new #TpHandleSet of contact handles
 *
 * Since: 0.13.0
 */
TpHandleSet *
tp_base_contact_list_dup_blocked_contacts (TpBaseContactList *self)
{
  TpBlockableContactListInterface *iface =
    TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->dup_blocked_contacts != NULL, NULL);
  g_return_val_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS, NULL);

  return iface->dup_blocked_contacts (self);
}

/**
 * tp_base_contact_list_block_contacts_async:
 * @self: a contact list manager
 * @contacts: contacts whose communications should be blocked
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Request that the given contacts are prevented from communicating with the
 * user, and that presence is not sent to them even if they have a valid
 * presence subscription, if possible. This is equivalent to calling
 * tp_base_contact_list_block_contacts_with_abuse_async(), passing #FALSE as
 * the report_abusive argument.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpBlockableContactListInterface.block_contacts_async or
 * #TpBlockableContactListInterface.block_contacts_with_abuse_async.
 * The implementation should call
 * tp_base_contact_list_contact_blocking_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_block_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_base_contact_list_block_contacts_with_abuse_async (self, contacts, FALSE,
      callback, user_data);
}

/**
 * tp_base_contact_list_block_contacts_with_abuse_async:
 * @self: a contact list manager
 * @contacts: contacts whose communications should be blocked
 * @report_abusive: whether to report the contacts as abusive to the server
 *  operator
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Request that the given contacts are prevented from communicating with the
 * user, and that presence is not sent to them even if they have a valid
 * presence subscription, if possible. If the #TpBaseContactList subclass
 * implements #TpBlockableContactListInterface.block_contacts_with_abuse_async
 * and @report_abusive is #TRUE, also report the given contacts as abusive to
 * the server operator.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpBlockableContactListInterface.block_contacts_async or
 * #TpBlockableContactListInterface.block_contacts_with_abuse_async.
 * The implementation should call
 * tp_base_contact_list_contact_blocking_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.15.1
 */
void
tp_base_contact_list_block_contacts_with_abuse_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    gboolean report_abusive,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpBlockableContactListInterface *blockable_iface;

  blockable_iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (blockable_iface != NULL);

  if (blockable_iface->block_contacts_async != NULL)
    blockable_iface->block_contacts_async (self, contacts, callback, user_data);
  else if (blockable_iface->block_contacts_with_abuse_async != NULL)
    blockable_iface->block_contacts_with_abuse_async (self, contacts,
        report_abusive, callback, user_data);
  else
    g_critical ("neither block_contacts_async nor "
        "block_contacts_with_abuse_async is implemented");
}

/**
 * tp_base_contact_list_block_contacts_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_block_contacts_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_block_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpBlockableContactListInterface.block_contacts_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_block_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpBlockableContactListInterface *blockable_iface;

  blockable_iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (blockable_iface != NULL, FALSE);
  g_return_val_if_fail (blockable_iface->block_contacts_finish != NULL, FALSE);

  return blockable_iface->block_contacts_finish (self, result, error);
}

/**
 * tp_base_contact_list_block_contacts_with_abuse_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_block_contacts_with_abuse_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_block_contacts_with_abuse_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpBlockableContactListInterface.block_contacts_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.15.1
 */
gboolean
tp_base_contact_list_block_contacts_with_abuse_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpBlockableContactListInterface *blockable_iface;

  blockable_iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (blockable_iface != NULL, FALSE);
  g_return_val_if_fail (blockable_iface->block_contacts_finish != NULL, FALSE);

  return blockable_iface->block_contacts_finish (self, result, error);
}

/**
 * tp_base_contact_list_unblock_contacts_async:
 * @self: a contact list manager
 * @contacts: contacts whose communications should no longer be blocked
 * @callback: a callback to call when the operation succeeds or fails
 * @user_data: optional data to pass to @callback
 *
 * Reverse the effects of tp_base_contact_list_block_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, this method does nothing.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which must be implemented, using
 * #TpBlockableContactListInterface.unblock_contacts_async.
 * The implementation should call
 * tp_base_contact_list_contact_blocking_changed()
 * for any contacts it has changed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_unblock_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpBlockableContactListInterface *blockable_iface;

  blockable_iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_if_fail (blockable_iface != NULL);
  g_return_if_fail (blockable_iface->unblock_contacts_async != NULL);

  blockable_iface->unblock_contacts_async (self, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_unblock_contacts_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_unblock_contacts_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_unblock_contacts_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_BLOCKABLE_CONTACT_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_BLOCKABLE_CONTACT_LIST, this is a virtual
 * method which may be implemented using
 * #TpBlockableContactListInterface.unblock_contacts_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_unblock_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpBlockableContactListInterface *blockable_iface;

  blockable_iface = TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (blockable_iface != NULL, FALSE);
  g_return_val_if_fail (blockable_iface->unblock_contacts_finish != NULL,
      FALSE);

  return blockable_iface->unblock_contacts_finish (self, result, error);
}

/**
 * TpBaseContactListNormalizeFunc:
 * @self: a contact list manager
 * @s: a non-%NULL name to normalize
 *
 * Signature of a virtual method to normalize strings in a contact list
 * manager.
 *
 * Returns: a normalized form of @s, or %NULL on error
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_normalize_group:
 * @self: a contact list manager
 * @s: a non-%NULL group name to normalize
 *
 * Return a normalized form of the group name @s, or %NULL if a group of a
 * sufficiently similar name cannot be created.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_CONTACT_GROUP_LIST, this method is meaningless, and always
 * returns %NULL.
 *
 * For implementations of %TP_TYPE_CONTACT_GROUP_LIST, this is a virtual
 * method, implemented using #TpContactGroupListInterface.normalize_group.
 * If unimplemented, the default behaviour is to use the group's name as-is.
 *
 * Protocols where this default is not suitable (for instance, if group names
 * cannot be the empty string, or can only contain XML character data, or can
 * only contain a particular Unicode normal form like NFKC) should reimplement
 * this virtual method.
 *
 * Returns: a normalized form of @s, or %NULL on error
 *
 * Since: 0.13.0
 */
gchar *
tp_base_contact_list_normalize_group (TpBaseContactList *self,
    const gchar *s)
{
  TpContactGroupListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), NULL);
  g_return_val_if_fail (s != NULL, NULL);

  if (!TP_IS_CONTACT_GROUP_LIST (self))
    return NULL;

  iface = TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, FALSE);

  if (iface->normalize_group == NULL)
    return g_strdup (s);

  return iface->normalize_group (self, s);
}

/**
 * tp_base_contact_list_groups_created:
 * @self: a contact list manager
 * @created: (array length=n_created) (element-type utf8) (allow-none): zero
 *  or more groups that were created
 * @n_created: the number of groups created, or -1 if @created is
 *  %NULL-terminated
 *
 * Called by subclasses when new groups have been created. This will typically
 * be followed by a call to tp_base_contact_list_groups_changed() to add
 * some members to those groups.
 *
 * It is an error to call this method on a contact list that
 * does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_groups_created (TpBaseContactList *self,
    const gchar * const *created,
    gssize n_created)
{
  GPtrArray *actually_created;
  gssize i;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));
  g_return_if_fail (n_created >= -1);
  g_return_if_fail (n_created <= 0 || created != NULL);

  if (n_created == 0 || created == NULL)
    return;

  if (n_created < 0)
    {
      n_created = (gssize) g_strv_length ((GStrv) created);

      g_return_if_fail (n_created >= 0);
    }
  else
    {
      for (i = 0; i < n_created; i++)
        g_return_if_fail (created[i] != NULL);
    }

  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  actually_created = g_ptr_array_sized_new (n_created + 1);

  for (i = 0; i < n_created; i++)
    {
      gchar *normalized_group = tp_base_contact_list_normalize_group (
          self, created[i]);

      if (g_hash_table_lookup (self->priv->groups, normalized_group) == NULL)
        {
          g_ptr_array_add (actually_created, (gchar *) created[i]);

          g_hash_table_insert (self->priv->groups,
              g_strdup (normalized_group),
              tp_handle_set_new (self->priv->contact_repo));
        }

      g_free (normalized_group);
    }

  if (actually_created->len > 0)
    {
      DEBUG ("GroupsCreated([%u including '%s'])", actually_created->len,
          (gchar *) g_ptr_array_index (actually_created, 0));

      if (self->priv->svc_contact_groups)
      {
        g_ptr_array_add (actually_created, NULL);
        tp_svc_connection_interface_contact_groups_emit_groups_created (
            self->priv->conn, (const gchar **) actually_created->pdata);
      }
    }

  g_ptr_array_unref (actually_created);
}

/**
 * tp_base_contact_list_groups_removed:
 * @self: a contact list manager
 * @removed: (array length=n_removed) (element-type utf8) (allow-none): zero
 *  or more groups that were removed
 * @n_removed: the number of groups removed, or -1 if @removed is
 *  %NULL-terminated
 *
 * Called by subclasses when groups have been removed.
 *
 * Calling tp_base_contact_list_dup_group_members() during this method should
 * return the groups' old members. If this is done correctly by a subclass,
 * then tp_base_contact_list_groups_changed() will automatically be emitted
 * for the old members, and the subclass does not need to do so.
 *
 * It is an error to call this method on a contact list that
 * does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_groups_removed (TpBaseContactList *self,
    const gchar * const *removed,
    gssize n_removed)
{
  GPtrArray *actually_removed;
  gssize i;
  TpHandleSet *old_members;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));
  g_return_if_fail (removed != NULL);
  g_return_if_fail (n_removed >= -1);
  g_return_if_fail (n_removed <= 0 || removed != NULL);

  if (n_removed == 0 || removed == NULL)
    return;

  if (n_removed < 0)
    {
      n_removed = (gssize) g_strv_length ((GStrv) removed);

      g_return_if_fail (n_removed >= 0);
    }
  else
    {
      for (i = 0; i < n_removed; i++)
        g_return_if_fail (removed[i] != NULL);
    }

  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  old_members = tp_handle_set_new (self->priv->contact_repo);
  actually_removed = g_ptr_array_new_full (n_removed + 1, g_free);

  for (i = 0; i < n_removed; i++)
    {
      gchar *normalized_group = tp_base_contact_list_normalize_group (
          self, removed[i]);
      TpHandleSet *group_members = g_hash_table_lookup (self->priv->groups,
          normalized_group);
      TpHandle contact;
      TpIntsetFastIter iter;

      if (group_members != NULL)
        {
          g_ptr_array_add (actually_removed, g_strdup (removed[i]));

          tp_intset_fast_iter_init (&iter,
              tp_handle_set_peek (group_members));

          while (tp_intset_fast_iter_next (&iter, &contact))
            tp_handle_set_add (old_members, contact);

          g_hash_table_remove (self->priv->groups, normalized_group);
        }

      g_free (normalized_group);
    }

  if (actually_removed->len > 0)
    {
      GArray *members_arr = tp_handle_set_to_array (old_members);

      DEBUG ("GroupsRemoved([%u including '%s'])",
          actually_removed->len,
          (gchar *) g_ptr_array_index (actually_removed, 0));

      g_ptr_array_add (actually_removed, NULL);

      if (self->priv->svc_contact_groups)
        tp_svc_connection_interface_contact_groups_emit_groups_removed (
            self->priv->conn, (const gchar **) actually_removed->pdata);

      if (members_arr->len > 0)
        {
          /* we already added NULL to actually_removed, so subtract 1 from its
           * length */
          DEBUG ("GroupsChanged([%u contacts], [], [%u groups])",
              members_arr->len, actually_removed->len - 1);

          if (self->priv->svc_contact_groups)
            tp_svc_connection_interface_contact_groups_emit_groups_changed (
                self->priv->conn, members_arr, NULL,
                (const gchar **) actually_removed->pdata);
        }

      g_array_unref (members_arr);
    }

  tp_handle_set_destroy (old_members);
  g_ptr_array_unref (actually_removed);
}

/**
 * tp_base_contact_list_group_renamed:
 * @self: a contact list manager
 * @old_name: the group's old name
 * @new_name: the group's new name
 *
 * Called by subclasses when a group has been renamed.
 *
 * Calling tp_base_contact_list_dup_group_members() for @old_name during this
 * method should return the group's old members. If this is done correctly by
 * a subclass, then tp_base_contact_list_groups_changed() will automatically
 * be emitted for the members, and the subclass does not need to do so.
 *
 * It is an error to call this method on a contact list that
 * does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_group_renamed (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name)
{
  const gchar *old_names[] = { old_name, NULL };
  const gchar *new_names[] = { new_name, NULL };
  const TpIntset *set;
  TpHandleSet *old_members;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));

  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  DEBUG ("GroupRenamed('%s', '%s')", old_names[0], new_names[0]);

  if (self->priv->svc_contact_groups)
    {
      tp_svc_connection_interface_contact_groups_emit_group_renamed (
          self->priv->conn, old_names[0], new_names[0]);

      tp_svc_connection_interface_contact_groups_emit_groups_created (
          self->priv->conn, new_names);

      tp_svc_connection_interface_contact_groups_emit_groups_removed (
          self->priv->conn, old_names);
    }

  old_members = tp_base_contact_list_dup_group_members (self, old_name);
  set = tp_handle_set_peek (old_members);

  if (tp_intset_size (set) > 0)
    {
      DEBUG ("GroupsChanged([%u contacts], ['%s'], ['%s'])",
          tp_intset_size (set), new_names[0], old_names[0]);

      if (self->priv->svc_contact_groups)
        {
          GArray *arr = tp_intset_to_array (set);

          tp_svc_connection_interface_contact_groups_emit_groups_changed (
              self->priv->conn, arr, new_names, old_names);
          g_array_unref (arr);
        }
    }

  tp_handle_set_destroy (old_members);
}

static gboolean
add_contacts_to_handle_set (TpHandleSet *set,
    TpIntset *contacts)
{
  TpIntset *subset;
  gboolean changed;

  subset = tp_handle_set_update (set, contacts);
  changed = tp_intset_size (subset) > 0;
  tp_intset_destroy (subset);

  return changed;
}

static gboolean
remove_contacts_from_handle_set (TpHandleSet *set,
    TpIntset *contacts)
{
  TpIntset *subset;
  gboolean changed;

  subset = tp_handle_set_difference_update (set, contacts);
  changed = tp_intset_size (subset) > 0;
  tp_intset_destroy (subset);

  return changed;
}

/**
 * tp_base_contact_list_groups_changed:
 * @self: a contact list manager
 * @contacts: a set containing one or more contacts
 * @added: (array length=n_added) (element-type utf8) (allow-none): zero or
 *  more groups to which the @contacts were added, or %NULL (which has the
 *  same meaning as an empty list)
 * @n_added: the number of groups added, or -1 if @added is %NULL-terminated
 * @removed: (array zero-terminated=1) (element-type utf8) (allow-none): zero
 *  or more groups from which the @contacts were removed, or %NULL (which has
 *  the same meaning as an empty list)
 * @n_removed: the number of groups removed, or -1 if @removed is
 *  %NULL-terminated
 *
 * Called by subclasses when groups' membership has been changed.
 *
 * If any of the groups in @added are not already known to exist,
 * this method also signals that they were created, as if
 * tp_base_contact_list_groups_created() had been called first.
 *
 * It is an error to call this method on a contact list that
 * does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_groups_changed (TpBaseContactList *self,
    TpHandleSet *contacts,
    const gchar * const *added,
    gssize n_added,
    const gchar * const *removed,
    gssize n_removed)
{
  gssize i;
  GPtrArray *really_added, *really_removed;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));
  g_return_if_fail (contacts != NULL);
  g_return_if_fail (n_added >= -1);
  g_return_if_fail (n_removed >= -1);
  g_return_if_fail (n_added <= 0 || added != NULL);
  g_return_if_fail (n_removed <= 0 || removed != NULL);

  if (tp_handle_set_is_empty (contacts))
    {
      DEBUG ("No contacts, doing nothing");
      return;
    }

  if (n_added < 0)
    {
      if (added == NULL)
        n_added = 0;
      else
        n_added = (gssize) g_strv_length ((GStrv) added);

      g_return_if_fail (n_added >= 0);
    }
  else
    {
      for (i = 0; i < n_added; i++)
        g_return_if_fail (added[i] != NULL);
    }

  if (n_removed < 0)
    {
      if (added == NULL)
        n_removed = 0;
      else
        n_removed = (gssize) g_strv_length ((GStrv) added);

      g_return_if_fail (n_removed >= 0);
    }
  else
    {
      for (i = 0; i < n_removed; i++)
        g_return_if_fail (removed[i] != NULL);
    }

  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  DEBUG ("Changing up to %u contacts, adding %" G_GSSIZE_FORMAT
      " groups, removing %" G_GSSIZE_FORMAT,
      tp_handle_set_size (contacts), n_added, n_removed);

  tp_base_contact_list_groups_created (self, added, n_added);

  /* These two arrays are lists of the groups whose members really changed;
   * groups where the change was a no-op are skipped. */
  really_added = g_ptr_array_sized_new (n_added);
  really_removed = g_ptr_array_sized_new (n_removed);

  for (i = 0; i < n_added; i++)
    {
      gchar *normalized_group = tp_base_contact_list_normalize_group (
          self, added[i]);
      TpHandleSet *contacts_in_group = g_hash_table_lookup (self->priv->groups,
          normalized_group);

      if (contacts_in_group == NULL)
        {
          DEBUG ("No record of group '%s', it must be invalid?",
              normalized_group);
        }
      else
        {
          DEBUG ("Adding %u contacts to group '%s'",
              tp_handle_set_size (contacts), added[i]);

          if (add_contacts_to_handle_set (contacts_in_group,
                  tp_handle_set_peek (contacts)))
            {
              g_ptr_array_add (really_added, (gchar *) added[i]);
            }
        }

      g_free (normalized_group);
    }

  for (i = 0; i < n_removed; i++)
    {
      gchar *normalized_group = tp_base_contact_list_normalize_group (
          self, removed[i]);
      TpHandleSet *contacts_in_group = g_hash_table_lookup (self->priv->groups,
          normalized_group);

      if (contacts_in_group == NULL)
        {
          DEBUG ("No record of group '%s', it must be invalid?",
              normalized_group);
        }
      else
        {
          DEBUG ("Removing %u contacts from group '%s'",
              tp_handle_set_size (contacts), removed[i]);

          if (remove_contacts_from_handle_set (contacts_in_group,
                  tp_handle_set_peek (contacts)))
            {
              g_ptr_array_add (really_removed, (gchar *) removed[i]);
            }
        }

      g_free (normalized_group);
    }

  if (really_added->len > 0 || really_removed->len > 0)
    {
      DEBUG ("GroupsChanged([%u contacts], [%u groups], [%u groups])",
          tp_handle_set_size (contacts), really_added->len,
          really_removed->len);

      g_ptr_array_add (really_added, NULL);
      g_ptr_array_add (really_removed, NULL);

      if (self->priv->svc_contact_groups)
        {
          GArray *members_arr = tp_handle_set_to_array (contacts);

          tp_svc_connection_interface_contact_groups_emit_groups_changed (
              self->priv->conn, members_arr,
              (const gchar **) really_added->pdata,
              (const gchar **) really_removed->pdata);
          g_array_unref (members_arr);
        }
    }

  g_ptr_array_unref (really_added);
  g_ptr_array_unref (really_removed);
}

/**
 * tp_base_contact_list_one_contact_groups_changed:
 * @self: the contact list manager
 * @contact: a contact handle
 * @added: (array length=n_added) (element-type utf8) (allow-none): zero or
 *  more groups to which @contact was added, or %NULL
 * @n_added: the number of groups added, or -1 if @added is %NULL-terminated
 * @removed: (array zero-terminated=1) (element-type utf8) (allow-none): zero
 *  or more groups from which the @contact was removed, or %NULL
 * @n_removed: the number of groups removed, or -1 if @removed is
 *  %NULL-terminated
 *
 * Convenience wrapper around tp_base_contact_list_groups_changed() for a
 * single handle in the 'contacts' set.
 *
 * (There is no equivalent function for @added and @removed having trivial
 * contents, because you can already use <code>NULL, 0</code> for an empty
 * list or <code>&group_name, 1</code> for a single group.)
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_one_contact_groups_changed (TpBaseContactList *self,
    TpHandle contact,
    const gchar * const *added,
    gssize n_added,
    const gchar * const *removed,
    gssize n_removed)
{
  TpHandleSet *set;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));

  /* if we're disconnecting, we might not have a handle repository any more:
   * tp_base_contact_list_groups_changed does nothing in that situation */
  if (self->priv->contact_repo == NULL)
    return;

  set = tp_handle_set_new_containing (self->priv->contact_repo, contact);
  tp_base_contact_list_groups_changed (self, set, added, n_added, removed,
      n_removed);
  tp_handle_set_destroy (set);
}

/**
 * tp_base_contact_list_has_disjoint_groups:
 * @self: a contact list manager
 *
 * Return whether groups in this protocol are disjoint
 * (i.e. each contact can be in at most one group).
 * This is merely informational: subclasses are responsible for making
 * appropriate calls to tp_base_contact_list_groups_changed(), etc.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_CONTACT_GROUP_LIST, this method is meaningless, and always
 * returns %FALSE.
 *
 * For implementations of %TP_TYPE_CONTACT_GROUP_LIST, this is a virtual
 * method, implemented using #TpContactGroupListInterface.has_disjoint_groups.
 *
 * The default implementation is tp_base_contact_list_false_func();
 * subclasses where groups are disjoint should use
 * tp_base_contact_list_true_func() instead.
 * In the unlikely event that a protocol can have disjoint groups, or not,
 * determined at runtime, it can use a custom implementation.
 *
 * Returns: %TRUE if groups are disjoint
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_has_disjoint_groups (TpBaseContactList *self)
{
  TpContactGroupListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), FALSE);

  if (!TP_IS_CONTACT_GROUP_LIST (self))
    return FALSE;

  iface = TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->has_disjoint_groups != NULL, FALSE);

  return iface->has_disjoint_groups (self);
}

/**
 * TpBaseContactListDupGroupsFunc:
 * @self: a contact list manager
 *
 * Signature of a virtual method that lists every group that exists on a
 * connection.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full): an
 *  array of groups
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_dup_groups:
 * @self: a contact list manager
 *
 * Return a list of all groups on this connection. It is incorrect to call
 * this method before tp_base_contact_list_set_list_received() has been
 * called, after the connection has disconnected, or on a #TpBaseContactList
 * that does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * For implementations of %TP_TYPE_CONTACT_GROUP_LIST, this is a virtual
 * method, implemented using #TpContactGroupListInterface.dup_groups.
 * It must always be implemented.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full): an
 *  array of groups
 *
 * Since: 0.13.0
 */
GStrv
tp_base_contact_list_dup_groups (TpBaseContactList *self)
{
  TpContactGroupListInterface *iface =
    TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->dup_groups != NULL, NULL);
  g_return_val_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS, NULL);

  return iface->dup_groups (self);
}

/**
 * TpBaseContactListDupContactGroupsFunc:
 * @self: a contact list manager
 * @contact: a non-zero contact handle
 *
 * Signature of a virtual method that lists the groups to which @contact
 * belongs.
 *
 * If @contact is not on the contact list, this method must return either
 * %NULL or an empty array, without error.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full): an
 *  array of groups
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_dup_contact_groups:
 * @self: a contact list manager
 * @contact: a contact handle
 *
 * Return a list of groups of which @contact is a member. It is incorrect to
 * call this method before tp_base_contact_list_set_list_received() has been
 * called, after the connection has disconnected, or on a #TpBaseContactList
 * that does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * If @contact is not on the contact list, this method must return either
 * %NULL or an empty array.
 *
 * For implementations of %TP_TYPE_CONTACT_GROUP_LIST, this is a virtual
 * method, implemented using #TpContactGroupListInterface.dup_contact_groups.
 * It must always be implemented.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full): an
 *  array of groups
 *
 * Since: 0.13.0
 */
GStrv
tp_base_contact_list_dup_contact_groups (TpBaseContactList *self,
    TpHandle contact)
{
  TpContactGroupListInterface *iface =
    TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->dup_contact_groups != NULL, NULL);
  g_return_val_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS, NULL);

  return iface->dup_contact_groups (self, contact);
}

/**
 * TpBaseContactListDupGroupMembersFunc:
 * @self: a contact list manager
 * @group: a normalized group name
 *
 * Signature of a virtual method that lists the members of a group.
 *
 * Returns: (transfer full): a set of contact (%TP_HANDLE_TYPE_CONTACT) handles
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_dup_group_members:
 * @self: a contact list manager
 * @group: a normalized group name
 *
 * Return the set of members of @group. It is incorrect to
 * call this method before tp_base_contact_list_set_list_received() has been
 * called, after the connection has disconnected, or on a #TpBaseContactList
 * that does not implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * If @group does not exist, this method must return either %NULL or an empty
 * set, without error.
 *
 * For implementations of %TP_TYPE_CONTACT_GROUP_LIST, this is a virtual
 * method, implemented using #TpContactGroupListInterface.dup_group_members.
 * It must always be implemented.
 *
 * Returns: a set of contact (%TP_HANDLE_TYPE_CONTACT) handles
 *
 * Since: 0.13.0
 */
TpHandleSet *
tp_base_contact_list_dup_group_members (TpBaseContactList *self,
    const gchar *group)
{
  TpContactGroupListInterface *iface =
    TP_CONTACT_GROUP_LIST_GET_INTERFACE (self);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->dup_group_members != NULL, NULL);
  g_return_val_if_fail (tp_base_contact_list_get_state (self, NULL) ==
      TP_CONTACT_LIST_STATE_SUCCESS, NULL);

  return iface->dup_group_members (self, group);
}

/**
 * TpBaseContactListGroupContactsFunc:
 * @self: a contact list manager
 * @group: a group
 * @contacts: a set of contact handles
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a virtual method that alters a group's members.
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_add_to_group_async:
 * @self: a contact list manager
 * @group: the normalized name of a group
 * @contacts: some contacts (may be an empty set)
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Add @contacts to @group, creating it if necessary.
 *
 * If @group does not exist, the implementation should create it, even if
 * @contacts is empty.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which must be implemented, using
 * #TpMutableContactGroupListInterface.add_to_group_async.
 * The implementation should call tp_base_contact_list_groups_changed()
 * for any changes it successfully made, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_add_to_group_async (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *iface;

  iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->add_to_group_async != NULL);

  iface->add_to_group_async (self, group, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_add_to_group_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_add_to_group_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_add_to_group_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented using
 * #TpMutableContactGroupListInterface.add_to_group_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_add_to_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->add_to_group_finish != NULL,
      FALSE);

  return mutable_groups_iface->add_to_group_finish (self, result, error);
}

/**
 * TpBaseContactListRenameGroupFunc:
 * @self: a contact list manager
 * @old_name: a group
 * @new_name: a new name for the group
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a method that renames groups.
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_rename_group_async:
 * @self: a contact list manager
 * @old_name: the normalized name of a group, which must exist
 * @new_name: a new normalized name for the group name
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Rename a group; if possible, do so as an atomic operation. If this
 * protocol can't do that, emulate renaming in terms of other operations.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented, using
 * #TpMutableContactGroupListInterface.rename_group_async.
 *
 * If this virtual method is not implemented, the default is to implement
 * renaming a group as creating the new group, adding all the old group's
 * members to it, and removing the old group: this is appropriate for protocols
 * like XMPP, in which groups behave more like tags.
 *
 * The implementation should call tp_base_contact_list_group_renamed() before
 * calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_rename_group_async (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *iface;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->rename_group_async != NULL);

  iface->rename_group_async (self, old_name, new_name, callback,
      user_data);
}

static void
emulate_rename_group_remove_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GSimpleAsyncResult *rename_result = user_data;
  GError *error = NULL;

  if (!tp_base_contact_list_remove_group_finish (self, result, &error))
    {
      g_simple_async_result_set_from_error (rename_result, error);
      g_clear_error (&error);
    }

  g_simple_async_result_complete (rename_result);
  g_object_unref (rename_result);
}

static void
emulate_rename_group_add_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GSimpleAsyncResult *rename_result = user_data;
  GError *error = NULL;

  if (!tp_base_contact_list_add_to_group_finish (self, result, &error))
    {
      g_simple_async_result_set_from_error (rename_result, error);
      g_clear_error (&error);
      g_simple_async_result_complete (rename_result);
      goto out;
    }

  tp_base_contact_list_remove_group_async (self,
      g_simple_async_result_get_op_res_gpointer (rename_result),
      emulate_rename_group_remove_cb, g_object_ref (rename_result));

out:
  g_object_unref (rename_result);
}

static void
tp_base_contact_list_emulate_rename_group (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpHandleSet *old_members;

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      tp_base_contact_list_emulate_rename_group);

  /* not really the operation result, just some extra data */
  g_simple_async_result_set_op_res_gpointer (result, g_strdup (old_name),
      g_free);

  old_members = tp_base_contact_list_dup_group_members (self, old_name);
  tp_base_contact_list_add_to_group_async (self, new_name, old_members,
      emulate_rename_group_add_cb, g_object_ref (result));
  g_object_unref (result);
  tp_handle_set_destroy (old_members);
}

/**
 * tp_base_contact_list_rename_group_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_rename_group_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_rename_group_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented using
 * #TpMutableContactGroupListInterface.rename_group_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_rename_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->rename_group_finish != NULL,
      FALSE);

  return mutable_groups_iface->rename_group_finish (self, result, error);
}

/**
 * tp_base_contact_list_remove_from_group_async:
 * @self: a contact list manager
 * @group: the normalized name of a group
 * @contacts: some contacts
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Remove @contacts from @group.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which must be implemented, using
 * #TpMutableContactGroupListInterface.remove_from_group_async.
 * The implementation should call tp_base_contact_list_groups_changed()
 * for any changes it successfully made, before calling @callback.
 *
 * Since: 0.13.0
 */
void tp_base_contact_list_remove_from_group_async (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *iface;

  iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->remove_from_group_async != NULL);

  iface->remove_from_group_async (self, group, contacts, callback, user_data);
}

/**
 * tp_base_contact_list_remove_from_group_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_remove_from_group_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_remove_from_group_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented using
 * #TpMutableContactGroupListInterface.remove_from_group_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_remove_from_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->remove_from_group_finish != NULL,
      FALSE);

  return mutable_groups_iface->remove_from_group_finish (self, result, error);
}

/**
 * TpBaseContactListRemoveGroupFunc:
 * @self: a contact list manager
 * @group: the normalized name of a group
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of a method that deletes groups.
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_remove_group_async:
 * @self: a contact list manager
 * @group: the normalized name of a group
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Remove a group entirely, removing any members in the process.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which must be implemented, using
 * #TpMutableContactGroupListInterface.remove_group_async.
 * The implementation should call tp_base_contact_list_groups_removed()
 * for any groups it successfully removed, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_remove_group_async (TpBaseContactList *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *mutable_group_iface;

  mutable_group_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_group_iface != NULL);
  g_return_if_fail (mutable_group_iface->remove_group_async != NULL);

  mutable_group_iface->remove_group_async (self, group, callback, user_data);
}

/**
 * tp_base_contact_list_remove_group_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_remove_group_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_remove_group_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented using
 * #TpMutableContactGroupListInterface.remove_group_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_remove_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->remove_group_finish != NULL,
      FALSE);

  return mutable_groups_iface->remove_group_finish (self, result, error);
}

static void
tp_base_contact_list_mixin_get_contact_list_attributes (
    TpSvcConnectionInterfaceContactList *svc,
    const gchar **interfaces,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  TpContactsMixin *contacts_mixin = TP_CONTACTS_MIXIN (svc);
  GError *error = NULL;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (contacts_mixin != NULL);

  if (tp_base_contact_list_get_state (self, &error)
      != TP_CONTACT_LIST_STATE_SUCCESS)
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
    }
  else
    {
      TpHandleSet *set;
      GArray *contacts;
      const gchar *assumed[] = { TP_IFACE_CONNECTION,
          TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST, NULL };
      GHashTable *result;

      set = tp_base_contact_list_dup_contacts (self);
      contacts = tp_handle_set_to_array (set);
      result = tp_contacts_mixin_get_contact_attributes (
          (GObject *) self->priv->conn, contacts, interfaces, assumed);
      tp_svc_connection_interface_contact_list_return_from_get_contact_list_attributes (
          context, result);

      g_array_unref (contacts);
      tp_handle_set_destroy (set);
      g_hash_table_unref (result);
    }
}

/**
 * TpBaseContactListSetContactGroupsFunc:
 * @self: a contact list manager
 * @contact: a contact handle
 * @normalized_names: (array length=n_names): the normalized names of some
 *  groups
 * @n_names: the number of groups
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature of an implementation of
 * tp_base_contact_list_set_contact_groups_async().
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_set_contact_groups_async:
 * @self: a contact list manager
 * @contact: a contact handle
 * @normalized_names: (array length=n_names): the normalized names of some
 *  groups
 * @n_names: the number of groups
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Add @contact to each group in @normalized_names, creating them if necessary,
 * and remove @contact from any other groups of which they are a member.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which must be implemented, using
 * #TpMutableContactGroupListInterface.set_contact_groups_async.
 * The implementation should call tp_base_contact_list_groups_changed()
 * for any changes it successfully made, before returning.
 *
 * Since: 0.13.0
 */
void tp_base_contact_list_set_contact_groups_async (TpBaseContactList *self,
    TpHandle contact,
    const gchar * const *normalized_names,
    gsize n_names,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_groups_iface != NULL);
  g_return_if_fail (mutable_groups_iface->set_contact_groups_async != NULL);

  mutable_groups_iface->set_contact_groups_async (self, contact,
      normalized_names, n_names, callback, user_data);
}

/**
 * tp_base_contact_list_set_contact_groups_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_set_contact_groups_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_set_contact_groups_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which may be implemented using
 * #TpMutableContactGroupListInterface.set_contact_groups_finish. If the
 * @result will be a #GSimpleAsyncResult, the default implementation may be
 * used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_set_contact_groups_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->set_contact_groups_finish !=
      NULL, FALSE);

  return mutable_groups_iface->set_contact_groups_finish (self, result, error);
}

/**
 * tp_base_contact_list_set_group_members_async:
 * @self: a contact list manager
 * @normalized_group: the normalized name of a group
 * @contacts: the contacts who should be in the group
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Set the members of @normalized_group to be exactly @contacts (i.e.
 * add @contacts, and simultaneously remove all members not in @contacts).
 *
 * If @normalized_group does not exist, the implementation should create it,
 * even if @contacts is empty.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method which must be implemented, using
 * #TpMutableContactGroupListInterface.set_group_members_async.
 * The implementation should call tp_base_contact_list_groups_changed()
 * for any changes it successfully made, before calling @callback.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_set_group_members_async (TpBaseContactList *self,
    const gchar *normalized_group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_if_fail (mutable_groups_iface != NULL);
  g_return_if_fail (mutable_groups_iface->set_group_members_async != NULL);

  mutable_groups_iface->set_group_members_async (self, normalized_group,
      contacts, callback, user_data);
}

/**
 * tp_base_contact_list_set_group_members_finish:
 * @self: a contact list manager
 * @result: the result passed to @callback by an implementation of
 *  tp_base_contact_list_set_group_members_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Interpret the result of an asynchronous call to
 * tp_base_contact_list_set_group_members_async().
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, it is an error to call this method.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a virtual
 * method which may be implemented using
 * #TpMutableContactGroupListInterface.set_group_members_finish. If the @result
 * will be a #GSimpleAsyncResult, the default implementation may be used.
 *
 * Returns: %TRUE on success or %FALSE on error
 *
 * Since: 0.13.0
 */
gboolean
tp_base_contact_list_set_group_members_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error)
{
  TpMutableContactGroupListInterface *mutable_groups_iface;

  mutable_groups_iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (mutable_groups_iface != NULL, FALSE);
  g_return_val_if_fail (mutable_groups_iface->set_group_members_finish !=
      NULL, FALSE);

  return mutable_groups_iface->set_group_members_finish (self, result, error);
}

static gboolean
tp_base_contact_list_check_change (TpBaseContactList *self,
    const GArray *contacts_or_null,
    GError **error)
{
  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), FALSE);

  if (tp_base_contact_list_get_state (self, error) !=
      TP_CONTACT_LIST_STATE_SUCCESS)
    return FALSE;

  if (contacts_or_null != NULL &&
      !tp_handles_are_valid (self->priv->contact_repo, contacts_or_null, FALSE,
        error))
    return FALSE;

  return TRUE;
}

static gboolean
tp_base_contact_list_check_list_change (TpBaseContactList *self,
    const GArray *contacts_or_null,
    GError **error)
{
  if (!tp_base_contact_list_check_change (self, contacts_or_null, error))
    return FALSE;

  if (!tp_base_contact_list_can_change_contact_list (self))
    {
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "Cannot change subscriptions");
      return FALSE;
    }

  return TRUE;
}

static gboolean
tp_base_contact_list_check_group_change (TpBaseContactList *self,
    const GArray *contacts_or_null,
    GError **error)
{
  if (!tp_base_contact_list_check_change (self, contacts_or_null, error))
    return FALSE;

  if (tp_base_contact_list_get_group_storage (self) ==
      TP_CONTACT_METADATA_STORAGE_TYPE_NONE)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "Cannot change group memberships");
      return FALSE;
    }

  return TRUE;
}

/* Normally we'd use the return_from functions, but these methods all return
 * void, and life's too short. */
static void
tp_base_contact_list_mixin_return_void (DBusGMethodInvocation *context,
    const GError *error)
{
  if (error == NULL)
    dbus_g_method_return (context);
  else
    dbus_g_method_return_error (context, error);
}

static void
tp_base_contact_list_mixin_request_subscription_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_request_subscription_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_request_subscription (
    TpSvcConnectionInterfaceContactList *svc,
    const GArray *contacts,
    const gchar *message,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_list_change (self, contacts, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_request_subscription_async (self, contacts_set, message,
      tp_base_contact_list_mixin_request_subscription_cb, context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_authorize_publication_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_authorize_publication_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_authorize_publication (
    TpSvcConnectionInterfaceContactList *svc,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_list_change (self, contacts, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_authorize_publication_async (self, contacts_set,
      tp_base_contact_list_mixin_authorize_publication_cb, context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_remove_contacts_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_remove_contacts_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_remove_contacts (
    TpSvcConnectionInterfaceContactList *svc,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_list_change (self, contacts, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_remove_contacts_async (self, contacts_set,
      tp_base_contact_list_mixin_remove_contacts_cb, context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_unsubscribe_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_unsubscribe_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_unsubscribe (
    TpSvcConnectionInterfaceContactList *svc,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_list_change (self, contacts, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_unsubscribe_async (self, contacts_set,
      tp_base_contact_list_mixin_unsubscribe_cb, context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_unpublish_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_unpublish_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_unpublish (
    TpSvcConnectionInterfaceContactList *svc,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_list_change (self, contacts, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_unpublish_async (self, contacts_set,
      tp_base_contact_list_mixin_unpublish_cb, context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

typedef enum {
    LP_CONTACT_LIST_STATE,
    LP_CONTACT_LIST_PERSISTS,
    LP_CAN_CHANGE_CONTACT_LIST,
    LP_REQUEST_USES_MESSAGE,
    LP_DOWNLOAD_AT_CONNECTION,
    NUM_LIST_PROPERTIES
} ListProp;

static TpDBusPropertiesMixinPropImpl known_list_props[] = {
    { "ContactListState", GINT_TO_POINTER (LP_CONTACT_LIST_STATE), },
    { "ContactListPersists", GINT_TO_POINTER (LP_CONTACT_LIST_PERSISTS), },
    { "CanChangeContactList", GINT_TO_POINTER (LP_CAN_CHANGE_CONTACT_LIST) },
    { "RequestUsesMessage", GINT_TO_POINTER (LP_REQUEST_USES_MESSAGE) },
    { "DownloadAtConnection", GINT_TO_POINTER (LP_DOWNLOAD_AT_CONNECTION) },
    { NULL }
};

static void
tp_base_contact_list_get_list_dbus_property (GObject *conn,
    GQuark interface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer data)
{
  TpBaseContactList *self = g_object_get_qdata (conn, BASE_CONTACT_LIST);
  ListProp p = GPOINTER_TO_INT (data);

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  switch (p)
    {
    case LP_CONTACT_LIST_STATE:
      g_return_if_fail (G_VALUE_HOLDS_UINT (value));
      g_value_set_uint (value, self->priv->state);
      break;

    case LP_CONTACT_LIST_PERSISTS:
      g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
      g_value_set_boolean (value,
          tp_base_contact_list_get_contact_list_persists (self));
      break;

    case LP_CAN_CHANGE_CONTACT_LIST:
      g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
      g_value_set_boolean (value,
          tp_base_contact_list_can_change_contact_list (self));
      break;

    case LP_REQUEST_USES_MESSAGE:
      g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
      g_value_set_boolean (value,
          tp_base_contact_list_get_request_uses_message (self));
      break;

    case LP_DOWNLOAD_AT_CONNECTION:
      g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
      g_value_set_boolean (value, self->priv->download_at_connection);
      break;

    default:
      g_return_if_reached ();
    }
}

static void
tp_base_contact_list_fill_list_contact_attributes (GObject *obj,
  const GArray *contacts,
  GHashTable *attributes_hash)
{
  TpBaseContactList *self = g_object_get_qdata (obj, BASE_CONTACT_LIST);
  guint i;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  /* just omit the attributes if the contact list hasn't come in yet */
  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  for (i = 0; i < contacts->len; i++)
    {
      TpSubscriptionState subscribe = TP_SUBSCRIPTION_STATE_NO;
      TpSubscriptionState publish = TP_SUBSCRIPTION_STATE_NO;
      gchar *publish_request = NULL;
      TpHandle handle;

      handle = g_array_index (contacts, TpHandle, i);

      tp_base_contact_list_dup_states (self, handle,
          &subscribe, &publish, &publish_request);

      tp_contacts_mixin_set_contact_attribute (attributes_hash,
          handle, TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_PUBLISH,
          tp_g_value_slice_new_uint (publish));

      tp_contacts_mixin_set_contact_attribute (attributes_hash,
          handle, TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_SUBSCRIBE,
          tp_g_value_slice_new_uint (subscribe));

      if (tp_str_empty (publish_request) ||
          publish != TP_SUBSCRIPTION_STATE_ASK)
        {
          g_free (publish_request);
        }
      else
        {
          tp_contacts_mixin_set_contact_attribute (attributes_hash,
              handle, TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_PUBLISH_REQUEST,
              tp_g_value_slice_new_take_string (publish_request));
        }
    }
}

static void
tp_base_contact_list_mixin_download_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_download_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_download (
    TpSvcConnectionInterfaceContactList *svc,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);

  tp_base_contact_list_download_async (self,
      tp_base_contact_list_mixin_download_cb, context);
}

/**
 * tp_base_contact_list_mixin_list_iface_init:
 * @klass: the service-side D-Bus interface,
 *  a #TpSvcConnectionInterfaceContactListClass
 *
 * Use the #TpBaseContactList like a mixin, to implement the ContactList
 * D-Bus interface.
 *
 * This function should be passed to G_IMPLEMENT_INTERFACE() for
 * #TpSvcConnectionInterfaceContactList.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_mixin_list_iface_init (gpointer klass)
{
#define IMPLEMENT(x) tp_svc_connection_interface_contact_list_implement_##x (\
  klass, tp_base_contact_list_mixin_##x)
  IMPLEMENT (get_contact_list_attributes);
  IMPLEMENT (request_subscription);
  IMPLEMENT (authorize_publication);
  IMPLEMENT (remove_contacts);
  IMPLEMENT (unsubscribe);
  IMPLEMENT (unpublish);
  IMPLEMENT (download);
#undef IMPLEMENT
}

/**
 * TpBaseContactListUIntFunc:
 * @self: a contact list manager
 *
 * Signature of a virtual method that returns an unsigned integer result.
 * These are used for feature-discovery.
 *
 * Returns: an unsigned integer result
 *
 * Since: 0.13.0
 */

/**
 * tp_base_contact_list_get_group_storage:
 * @self: a contact list manager
 *
 * Return the extent to which user-defined groups can be set in this protocol.
 * If this is %TP_CONTACT_METADATA_STORAGE_TYPE_NONE, methods that would alter
 * the group list will not be called.
 *
 * If the #TpBaseContactList subclass does not implement
 * %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this method is meaningless, and always
 * returns %TP_CONTACT_METADATA_STORAGE_TYPE_NONE.
 *
 * For implementations of %TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, this is a
 * virtual method, implemented using
 * #TpMutableContactGroupListInterface.get_group_storage.
 *
 * The default implementation is %NULL, which is treated as equivalent to an
 * implementation that always returns %TP_CONTACT_METADATA_STORAGE_TYPE_ANYONE.
 * A custom implementation can also be used.
 *
 * Returns: a #TpContactMetadataStorageType
 *
 * Since: 0.13.0
 */
TpContactMetadataStorageType
tp_base_contact_list_get_group_storage (TpBaseContactList *self)
{
  TpMutableContactGroupListInterface *iface;

  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self),
      TP_CONTACT_METADATA_STORAGE_TYPE_NONE);

  if (!TP_IS_MUTABLE_CONTACT_GROUP_LIST (self))
    return TP_CONTACT_METADATA_STORAGE_TYPE_NONE;

  iface = TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE (self);
  g_return_val_if_fail (iface != NULL, TP_CONTACT_METADATA_STORAGE_TYPE_NONE);

  if (iface->get_group_storage == NULL)
    return TP_CONTACT_METADATA_STORAGE_TYPE_ANYONE;

  return iface->get_group_storage (self);
}

static void
tp_base_contact_list_mixin_set_contact_groups_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_set_contact_groups_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_set_contact_groups (
    TpSvcConnectionInterfaceContactGroups *svc,
    guint contact,
    const gchar **groups,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  const gchar *empty_strv[] = { NULL };
  GError *error = NULL;
  GPtrArray *normalized_groups = NULL;

  if (!tp_base_contact_list_check_group_change (self, NULL, &error))
    goto finally;

  if (groups == NULL)
    groups = empty_strv;

  normalized_groups = g_ptr_array_new_full (g_strv_length ((GStrv) groups),
      (GDestroyNotify) g_free);

  for (; groups != NULL && *groups != NULL; groups++)
    {
      gchar *normalized = tp_base_contact_list_normalize_group (self, *groups);

      if (normalized != NULL)
        {
          g_ptr_array_add (normalized_groups, normalized);
        }
      else
        {
          DEBUG ("group '%s' not valid, ignoring it", *groups);
        }
    }

  tp_base_contact_list_set_contact_groups_async (self, contact,
      (const gchar * const *) normalized_groups->pdata,
      normalized_groups->len,
      tp_base_contact_list_mixin_set_contact_groups_cb, context);
  context = NULL;     /* ownership transferred to callback */

finally:
  tp_clear_pointer (&normalized_groups, g_ptr_array_unref);

  if (context != NULL)
    tp_base_contact_list_mixin_return_void (context, error);

  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_set_group_members_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_set_group_members_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_set_group_members (
    TpSvcConnectionInterfaceContactGroups *svc,
    const gchar *group,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  TpHandleSet *contacts_set = NULL;
  GError *error = NULL;

  if (!tp_base_contact_list_check_group_change (self, NULL, &error))
    goto error;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_set_group_members_async (self,
      group, contacts_set, tp_base_contact_list_mixin_set_group_members_cb,
      context);
  tp_handle_set_destroy (contacts_set);
  return;

error:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_add_to_group_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_add_to_group_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_add_to_group (
    TpSvcConnectionInterfaceContactGroups *svc,
    const gchar *group,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  gchar *normalized_group = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_group_change (self, contacts, &error))
    goto sync_exit;

  normalized_group = tp_base_contact_list_normalize_group (self, group);

  if (normalized_group == NULL)
    goto sync_exit;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_add_to_group_async (self, normalized_group,
      contacts_set, tp_base_contact_list_mixin_add_to_group_cb, context);
  tp_handle_set_destroy (contacts_set);
  g_free (normalized_group);
  return;

sync_exit:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_remove_from_group_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_remove_from_group_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_remove_from_group (
    TpSvcConnectionInterfaceContactGroups *svc,
    const gchar *group,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  gchar *normalized_group = NULL;
  TpHandleSet *contacts_set;

  if (!tp_base_contact_list_check_group_change (self, contacts, &error))
    goto sync_exit;

  normalized_group = tp_base_contact_list_normalize_group (self, group);

  if (normalized_group == NULL
      || g_hash_table_lookup (self->priv->groups, normalized_group) == NULL)
    goto sync_exit;

  contacts_set = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts);
  tp_base_contact_list_remove_from_group_async (self, normalized_group,
      contacts_set, tp_base_contact_list_mixin_remove_from_group_cb, context);
  tp_handle_set_destroy (contacts_set);
  g_free (normalized_group);

  return;

sync_exit:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
  g_free (normalized_group);
}

static void
tp_base_contact_list_mixin_remove_group_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_remove_group_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_remove_group (
    TpSvcConnectionInterfaceContactGroups *svc,
    const gchar *group,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  gchar *normalized_group = NULL;

  if (!tp_base_contact_list_check_group_change (self, NULL, &error))
    goto sync_exit;

  normalized_group = tp_base_contact_list_normalize_group (self, group);

  if (normalized_group == NULL
      || g_hash_table_lookup (self->priv->groups, normalized_group) == NULL)
    goto sync_exit;

  g_free (normalized_group);

  tp_base_contact_list_remove_group_async (self, group,
      tp_base_contact_list_mixin_remove_group_cb, context);
  return;

sync_exit:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
  g_free (normalized_group);
}

static void
tp_base_contact_list_mixin_rename_group_cb (GObject *source,
    GAsyncResult *result,
    gpointer context)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  GError *error = NULL;

  tp_base_contact_list_rename_group_finish (self, result, &error);
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

static void
tp_base_contact_list_mixin_rename_group (
    TpSvcConnectionInterfaceContactGroups *svc,
    const gchar *before,
    const gchar *after,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  GError *error = NULL;
  gchar *old_normalized;
  gchar *new_normalized;

  if (!tp_base_contact_list_check_group_change (self, NULL, &error))
    goto sync_exit;

  /* jtodo: just use the normalize func directly */

  old_normalized = tp_base_contact_list_normalize_group (self, before);

  if (g_hash_table_lookup (self->priv->groups, old_normalized) == NULL)
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_DOES_NOT_EXIST,
          "Group '%s' does not exist", before);
      g_free (old_normalized);
      goto sync_exit;
    }

  new_normalized = tp_base_contact_list_normalize_group (self, after);

  if (g_hash_table_lookup (self->priv->groups, new_normalized) != NULL)
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "Group '%s' already exists", new_normalized);
      g_free (new_normalized);
      goto sync_exit;
    }

  tp_base_contact_list_rename_group_async (self,
      old_normalized, new_normalized,
      tp_base_contact_list_mixin_rename_group_cb, context);
  g_free (old_normalized);
  g_free (new_normalized);
  return;

sync_exit:
  tp_base_contact_list_mixin_return_void (context, error);
  g_clear_error (&error);
}

typedef enum {
    GP_DISJOINT_GROUPS,
    GP_GROUP_STORAGE,
    GP_GROUPS,
    NUM_GROUP_PROPERTIES
} GroupProp;

static TpDBusPropertiesMixinPropImpl known_group_props[] = {
    { "DisjointGroups", GINT_TO_POINTER (GP_DISJOINT_GROUPS), },
    { "GroupStorage", GINT_TO_POINTER (GP_GROUP_STORAGE) },
    { "Groups", GINT_TO_POINTER (GP_GROUPS) },
    { NULL }
};

static void
tp_base_contact_list_get_group_dbus_property (GObject *conn,
    GQuark interface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer data)
{
  TpBaseContactList *self = g_object_get_qdata (conn, BASE_CONTACT_LIST);
  GroupProp p = GPOINTER_TO_INT (data);

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  switch (p)
    {
    case GP_DISJOINT_GROUPS:
      g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (value));
      g_value_set_boolean (value,
          tp_base_contact_list_has_disjoint_groups (self));
      break;

    case GP_GROUP_STORAGE:
      g_return_if_fail (G_VALUE_HOLDS_UINT (value));
      g_value_set_uint (value, tp_base_contact_list_get_group_storage (self));
      break;

    case GP_GROUPS:
      g_return_if_fail (G_VALUE_HOLDS (value, G_TYPE_STRV));

      if (self->priv->state == TP_CONTACT_LIST_STATE_SUCCESS)
        g_value_take_boxed (value, tp_base_contact_list_dup_groups (self));

      break;

    default:
      g_return_if_reached ();
    }
}

static void
tp_base_contact_list_fill_groups_contact_attributes (GObject *obj,
  const GArray *contacts,
  GHashTable *attributes_hash)
{
  TpBaseContactList *self = g_object_get_qdata (obj, BASE_CONTACT_LIST);
  guint i;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_CONTACT_GROUP_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  /* just omit the attributes if the contact list hasn't come in yet */
  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  for (i = 0; i < contacts->len; i++)
    {
      TpHandle handle;

      handle = g_array_index (contacts, TpHandle, i);

      tp_contacts_mixin_set_contact_attribute (attributes_hash,
          handle, TP_TOKEN_CONNECTION_INTERFACE_CONTACT_GROUPS_GROUPS,
          tp_g_value_slice_new_take_boxed (G_TYPE_STRV,
            tp_base_contact_list_dup_contact_groups (self, handle)));
    }
}

static void
tp_base_contact_list_fill_blocking_contact_attributes (GObject *obj,
  const GArray *contacts,
  GHashTable *attributes_hash)
{
  TpBaseContactList *self = g_object_get_qdata (obj, BASE_CONTACT_LIST);
  guint i;
  TpHandleSet *blocked;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_BLOCKABLE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  /* just omit the attributes if the contact list hasn't come in yet */
  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  blocked = tp_base_contact_list_dup_blocked_contacts (self);

  for (i = 0; i < contacts->len; i++)
    {
      TpHandle handle;
      gboolean is_blocked;

      handle = g_array_index (contacts, TpHandle, i);

      is_blocked = tp_handle_set_is_member (blocked, handle);

      tp_contacts_mixin_set_contact_attribute (attributes_hash,
          handle, TP_TOKEN_CONNECTION_INTERFACE_CONTACT_BLOCKING_BLOCKED,
          tp_g_value_slice_new_boolean (is_blocked));
    }

  tp_handle_set_destroy (blocked);
}

/**
 * tp_base_contact_list_mixin_groups_iface_init:
 * @klass: the service-side D-Bus interface,
 *  a #TpSvcConnectionInterfaceContactGroupsClass
 *
 * Use the #TpBaseContactList like a mixin, to implement the ContactGroups
 * D-Bus interface.
 *
 * This function should be passed to G_IMPLEMENT_INTERFACE() for
 * #TpSvcConnectionInterfaceContactGroups.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_mixin_groups_iface_init (gpointer klass)
{
#define IMPLEMENT(x) tp_svc_connection_interface_contact_groups_implement_##x (\
  klass, tp_base_contact_list_mixin_##x)
  IMPLEMENT (set_contact_groups);
  IMPLEMENT (set_group_members);
  IMPLEMENT (add_to_group);
  IMPLEMENT (remove_from_group);
  IMPLEMENT (remove_group);
  IMPLEMENT (rename_group);
#undef IMPLEMENT
}

#define ERROR_IF_BLOCKING_NOT_SUPPORTED(self, context) \
  if (!self->priv->svc_contact_blocking) \
    { \
      GError e = { TP_ERROR, TP_ERROR_NOT_IMPLEMENTED, \
          "ContactBlocking is not supported on this connection" }; \
      dbus_g_method_return_error (context, &e); \
      return; \
    }

static void
tp_base_contact_list_mixin_request_blocked_contacts (
    TpSvcConnectionInterfaceContactBlocking *svc,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);

  ERROR_IF_BLOCKING_NOT_SUPPORTED (self, context);

  switch (self->priv->state)
    {
    case TP_CONTACT_LIST_STATE_NONE:
    case TP_CONTACT_LIST_STATE_WAITING:
      g_queue_push_tail (&self->priv->blocked_contact_requests, context);
      break;

    case TP_CONTACT_LIST_STATE_FAILURE:
      g_warn_if_fail (self->priv->failure != NULL);
      dbus_g_method_return_error (context, self->priv->failure);
      break;

    case TP_CONTACT_LIST_STATE_SUCCESS:
      {
        TpHandleSet *blocked = tp_base_contact_list_dup_blocked_contacts (self);
        GHashTable *map = tp_handle_set_to_identifier_map (blocked);

        tp_svc_connection_interface_contact_blocking_return_from_request_blocked_contacts (context, map);

        g_hash_table_unref (map);
        tp_handle_set_destroy (blocked);
        break;
      }

    default:
      {
        GError broken = { TP_ERROR, TP_ERROR_CONFUSED,
            "My internal list of blocked contacts is inconsistent! "
            "I apologise for any inconvenience caused." };
        dbus_g_method_return_error (context, &broken);
        g_return_if_reached ();
      }
    }
}

static void
blocked_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  DBusGMethodInvocation *context = user_data;
  GError *error = NULL;

  if (tp_base_contact_list_block_contacts_with_abuse_finish (self, result,
          &error))
    {
      tp_svc_connection_interface_contact_blocking_return_from_block_contacts (
          context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
    }
}

static void
tp_base_contact_list_mixin_block_contacts (
    TpSvcConnectionInterfaceContactBlocking *svc,
    const GArray *contacts_arr,
    gboolean report_abusive,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  TpHandleSet *contacts;

  ERROR_IF_BLOCKING_NOT_SUPPORTED (self, context);

  contacts = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts_arr);
  tp_base_contact_list_block_contacts_with_abuse_async (self, contacts,
      report_abusive, blocked_cb, context);
  tp_handle_set_destroy (contacts);
}

static void
unblocked_cb (
    GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseContactList *self = TP_BASE_CONTACT_LIST (source);
  DBusGMethodInvocation *context = user_data;
  GError *error = NULL;

  if (tp_base_contact_list_unblock_contacts_finish (self, result, &error))
    {
      tp_svc_connection_interface_contact_blocking_return_from_unblock_contacts (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
    }
}

static void
tp_base_contact_list_mixin_unblock_contacts (
    TpSvcConnectionInterfaceContactBlocking *svc,
    const GArray *contacts_arr,
    DBusGMethodInvocation *context)
{
  TpBaseContactList *self = g_object_get_qdata ((GObject *) svc,
      BASE_CONTACT_LIST);
  TpHandleSet *contacts;

  ERROR_IF_BLOCKING_NOT_SUPPORTED (self, context);

  contacts = tp_handle_set_new_from_array (self->priv->contact_repo,
      contacts_arr);
  tp_base_contact_list_unblock_contacts_async (self, contacts, unblocked_cb,
      context);
  tp_handle_set_destroy (contacts);
}

/**
 * tp_base_contact_list_mixin_blocking_iface_init:
 * @klass: the service-side D-Bus interface,
 *  a #TpSvcConnectionInterfaceContactBlockingClass
 *
 * Use the #TpBaseContactList like a mixin, to implement the ContactBlocking
 * D-Bus interface.
 *
 * This function should be passed to G_IMPLEMENT_INTERFACE() for
 * #TpSvcConnectionInterfaceContactBlocking
 *
 * Since: 0.15.1
 */
void
tp_base_contact_list_mixin_blocking_iface_init (gpointer klass)
{
#define IMPLEMENT(x) tp_svc_connection_interface_contact_blocking_implement_##x (\
  klass, tp_base_contact_list_mixin_##x)
  IMPLEMENT (block_contacts);
  IMPLEMENT (unblock_contacts);
  IMPLEMENT (request_blocked_contacts);
#undef IMPLEMENT
}

static TpDBusPropertiesMixinPropImpl known_blocking_props[] = {
    { "ContactBlockingCapabilities" },
    { NULL }
};

static void
tp_base_contact_list_get_blocking_dbus_property (GObject *conn,
    GQuark interface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer data)
{
  TpBaseContactList *self = g_object_get_qdata (conn, BASE_CONTACT_LIST);
  TpBlockableContactListInterface *iface =
      TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE (self);
  static GQuark contact_blocking_capabilities_q = 0;
  guint flags = 0;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_BLOCKABLE_CONTACT_LIST (self));
  g_return_if_fail (self->priv->conn != NULL);

  if (G_UNLIKELY (contact_blocking_capabilities_q == 0))
    contact_blocking_capabilities_q =
        g_quark_from_static_string ("ContactBlockingCapabilities");

  g_return_if_fail (name == contact_blocking_capabilities_q);

  if (iface->block_contacts_with_abuse_async != NULL)
    flags |= TP_CONTACT_BLOCKING_CAPABILITY_CAN_REPORT_ABUSIVE;

  g_value_set_uint (value, flags);
}

/**
 * tp_base_contact_list_mixin_class_init:
 * @cls: A subclass of #TpBaseConnection that has a #TpContactsMixinClass,
 *  and implements #TpSvcConnectionInterfaceContactList using
 *  #TpBaseContactList
 *
 * Register the #TpBaseContactList to be used like a mixin in @cls.
 * Before this function is called, the #TpContactsMixin must be initialized
 * with tp_contacts_mixin_class_init().
 *
 * If the connection implements #TpSvcConnectionInterfaceContactGroups, this
 * function automatically sets up that interface as well as ContactList.
 * In this case, when the #TpBaseContactList is created later, it must
 * implement %TP_TYPE_CONTACT_GROUP_LIST.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_mixin_class_init (TpBaseConnectionClass *cls)
{
  GType type = G_OBJECT_CLASS_TYPE (cls);
  GObjectClass *obj_cls = (GObjectClass *) cls;

  g_return_if_fail (TP_IS_BASE_CONNECTION_CLASS (cls));
  g_return_if_fail (TP_CONTACTS_MIXIN_CLASS (cls) != NULL);
  g_return_if_fail (g_type_is_a (type,
        TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_LIST));

  tp_dbus_properties_mixin_implement_interface (obj_cls,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACT_LIST,
      tp_base_contact_list_get_list_dbus_property,
      NULL, known_list_props);

  if (g_type_is_a (type, TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_GROUPS))
    {
      tp_dbus_properties_mixin_implement_interface (obj_cls,
          TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACT_GROUPS,
          tp_base_contact_list_get_group_dbus_property,
          NULL, known_group_props);
    }

  if (g_type_is_a (type, TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_BLOCKING))
    {
      tp_dbus_properties_mixin_implement_interface (obj_cls,
          TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACT_BLOCKING,
          tp_base_contact_list_get_blocking_dbus_property,
          NULL, known_blocking_props);
    }
}

/**
 * tp_base_contact_list_mixin_register_with_contacts_mixin:
 * @self: a contact list
 * @conn: An instance of #TpBaseConnection that uses a #TpContactsMixin,
 *  and implements #TpSvcConnectionInterfaceContactList using
 *  #TpBaseContactList
 *
 * Register the ContactList interface with the Contacts interface to make it
 * inspectable. Before this function is called, the #TpContactsMixin must be
 * initialized with tp_contacts_mixin_init().
 *
 * If the connection implements #TpSvcConnectionInterfaceContactGroups
 * the #TpBaseContactList implements %TP_TYPE_CONTACT_GROUP_LIST,
 * this function automatically also registers the ContactGroups interface
 * with the contacts mixin.
 *
 * Since: 0.13.0
 */
void
tp_base_contact_list_mixin_register_with_contacts_mixin (
    TpBaseContactList *self,
    TpBaseConnection *conn)
{
  GType type = G_OBJECT_TYPE (conn);
  GObject *object = (GObject *) conn;

  g_return_if_fail (TP_IS_BASE_CONTACT_LIST (self));
  g_return_if_fail (TP_IS_BASE_CONNECTION (conn));
  g_return_if_fail (self != NULL);
  g_return_if_fail (g_type_is_a (type,
        TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_LIST));

  tp_contacts_mixin_add_contact_attributes_iface (object,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST,
      tp_base_contact_list_fill_list_contact_attributes);

  if (g_type_is_a (type, TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_GROUPS)
      && TP_IS_CONTACT_GROUP_LIST (self))
    {
      tp_contacts_mixin_add_contact_attributes_iface (object,
          TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS,
          tp_base_contact_list_fill_groups_contact_attributes);
    }

  if (g_type_is_a (type, TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACT_BLOCKING)
      && TP_IS_BLOCKABLE_CONTACT_LIST (self))
    {
      tp_contacts_mixin_add_contact_attributes_iface (object,
          TP_IFACE_CONNECTION_INTERFACE_CONTACT_BLOCKING,
          tp_base_contact_list_fill_blocking_contact_attributes);
    }
}

/**
 * tp_base_contact_list_get_state:
 * @self: a contact list
 * @error: used to raise an error if something other than
 * %TP_CONTACT_LIST_STATE_SUCCESS is returned
 *
 * Return how much progress this object has made towards retrieving the
 * contact list.
 *
 * If this contact list's connection has disconnected, or retrieving the
 * contact list has failed, return %TP_CONTACT_LIST_STATE_FAILURE.
 *
 * Returns: the state of the contact list
 *
 * Since: 0.13.0
 */
TpContactListState
tp_base_contact_list_get_state (TpBaseContactList *self,
    GError **error)
{
  /* this checks TP_IS_BASE_CONTACT_LIST */
  if (tp_base_contact_list_get_connection (self, error) == NULL)
    return TP_CONTACT_LIST_STATE_FAILURE;

  if (self->priv->failure != NULL)
    {
      g_set_error_literal (error, self->priv->failure->domain,
          self->priv->failure->code, self->priv->failure->message);
      return TP_CONTACT_LIST_STATE_FAILURE;
    }

  /* on failure, self->priv->failure was meant to be set */
  g_return_val_if_fail (self->priv->state != TP_CONTACT_LIST_STATE_FAILURE,
      TP_CONTACT_LIST_STATE_FAILURE);

  if (self->priv->state != TP_CONTACT_LIST_STATE_SUCCESS)
    g_set_error (error, TP_ERROR, TP_ERROR_NOT_YET,
        "Contact list not downloaded yet");

  return self->priv->state;
}

/**
 * tp_base_contact_list_get_connection:
 * @self: a contact list
 * @error: used to raise an error if %NULL is returned
 *
 * Return the Connection this contact list uses. If this contact list's
 * connection has already disconnected, return %NULL instead.
 *
 * Returns: (transfer none): the connection, or %NULL
 *
 * Since: 0.13.0
 */
TpBaseConnection *
tp_base_contact_list_get_connection (TpBaseContactList *self,
    GError **error)
{
  g_return_val_if_fail (TP_IS_BASE_CONTACT_LIST (self), NULL);

  if (self->priv->conn == NULL)
    {
      g_set_error_literal (error, TP_ERROR, TP_ERROR_DISCONNECTED,
          "Connection is no longer connected");
      return NULL;
    }

  return self->priv->conn;
}
