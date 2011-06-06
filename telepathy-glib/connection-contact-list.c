/*
 * Proxy for a Telepathy connection - ContactList support
 *
 * Copyright © 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "telepathy-glib/connection-contact-list.h"
#include <telepathy-glib/interfaces.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/connection-internal.h"
#include "telepathy-glib/util-internal.h"

static void
contact_list_state_changed_cb (TpConnection *self,
    guint state,
    gpointer user_data,
    GObject *weak_object)
{
  DEBUG ("contact list state changed: %d", state);

  self->priv->contact_list_state = state;
  g_object_notify ((GObject *) self, "contact-list-state");
}

static void
prepare_contact_list_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpConnection *self = (TpConnection *) proxy;
  GSimpleAsyncResult *result = user_data;
  gboolean valid;

  if (error != NULL)
    {
      DEBUG ("Error preparing ContactList properties: %s", error->message);
      g_simple_async_result_set_from_error (result, error);
      goto OUT;
    }

  self->priv->contact_list_state = tp_asv_get_uint32 (properties,
      "ContactListState", &valid);
  if (!valid)
    {
      DEBUG ("Connection %s doesn't have ContactListState property",
          tp_proxy_get_object_path (self));
    }

  self->priv->contact_list_persists = tp_asv_get_boolean (properties,
      "ContactListPersists", &valid);
  if (!valid)
    {
      DEBUG ("Connection %s doesn't have ContactListPersists property",
          tp_proxy_get_object_path (self));
    }

  self->priv->can_change_contact_list = tp_asv_get_boolean (properties,
      "CanChangeContactList", &valid);
  if (!valid)
    {
      DEBUG ("Connection %s doesn't have CanChangeContactList property",
          tp_proxy_get_object_path (self));
    }

  self->priv->request_uses_message = tp_asv_get_boolean (properties,
      "RequestUsesMessage", &valid);
  if (!valid)
    {
      DEBUG ("Connection %s doesn't have RequestUsesMessage property",
          tp_proxy_get_object_path (self));
    }

  DEBUG ("Got contact list properties; state=%d",
      self->priv->contact_list_state);

OUT:
  g_simple_async_result_complete (result);
}

void _tp_connection_prepare_contact_list_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpConnection *self = (TpConnection *) proxy;
  GSimpleAsyncResult *result;

  tp_cli_connection_interface_contact_list_connect_to_contact_list_state_changed (
      self, contact_list_state_changed_cb, NULL, NULL, NULL, NULL);

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      _tp_connection_prepare_contact_list_async);

  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST,
      prepare_contact_list_cb, result, g_object_unref, NULL);
}

/**
 * TP_CONNECTION_FEATURE_CONTACT_LIST:
 *
 * Expands to a call to a function that returns a #GQuark representing the
 * "contact-list" feature.
 *
 * When this feature is prepared, the contact list properties of the Connection
 * has been retrieved.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.UNRELEASED
 */

GQuark
tp_connection_get_feature_quark_contact_list (void)
{
  return g_quark_from_static_string ("tp-connection-feature-contact-list");
}

/**
 * tp_connection_get_contact_list_state:
 * @self: a #TpConnection
 *
 * <!-- -->
 *
 * Returns: the value of #TpConnection:contact-list-state property
 *
 * Since: 0.UNRELEASED
 */
TpContactListState
tp_connection_get_contact_list_state (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), TP_CONTACT_LIST_STATE_NONE);

  return self->priv->contact_list_state;
}

/**
 * tp_connection_get_contact_list_persists:
 * @self: a #TpConnection
 *
 * <!-- -->
 *
 * Returns: the value of #TpConnection:contact-list-persists property
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_get_contact_list_persists (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  return self->priv->contact_list_persists;
}

/**
 * tp_connection_get_can_change_contact_list:
 * @self: a #TpConnection
 *
 * <!-- -->
 *
 * Returns: the value of #TpConnection:can-change-contact-list property
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_get_can_change_contact_list (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  return self->priv->can_change_contact_list;
}

/**
 * tp_connection_get_request_uses_message:
 * @self: a #TpConnection
 *
 * <!-- -->
 *
 * Returns: the value of #TpConnection:request-uses-message property
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_get_request_uses_message (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  return self->priv->request_uses_message;
}

static void
generic_callback (TpConnection *self,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Operation failed: %s", error->message);
      g_simple_async_result_set_from_error (result, error);
    }

  /* tp_cli callbacks can potentially be called in a re-entrant way,
   * so we can't necessarily complete @result without using an idle. */
  g_simple_async_result_complete_in_idle (result);
}

#define contact_list_generic_async(method, ...) \
  G_STMT_START { \
    GSimpleAsyncResult *result; \
    GArray *handles; \
    \
    g_return_if_fail (TP_IS_CONNECTION (self)); \
    g_return_if_fail (_tp_contacts_to_handles (self, n_contacts, contacts, \
        &handles)); \
    \
    result = g_simple_async_result_new ((GObject *) self, callback, user_data, \
        tp_connection_##method##_async); \
    \
    tp_cli_connection_interface_contact_list_call_##method (self, -1, \
    handles, ##__VA_ARGS__, generic_callback, result, g_object_unref, NULL); \
    g_array_unref (handles); \
  } G_STMT_END

#define generic_finish(method) \
    _tp_implement_finish_void (self, tp_connection_##method##_async);

/**
 * tp_connection_request_subscription_async:
 * @self: a #TpConnection
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to whom
 *  requests are to be sent.
 * @message: an optional plain-text message from the user, to send to those
 *  @contacts with the subscription request.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Request that the given @contacts allow the local user to subscribe to their
 * presence, i.e. that their #TpContact:subscribe-state property becomes
 * %TP_SUBSCRIPTION_STATE_YES.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_request_subscription_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_list_generic_async (request_subscription, message);
}

/**
 * tp_connection_request_subscription_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_request_subscription_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_request_subscription_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (request_subscription);
}

/**
 * tp_connection_authorize_publication_async:
 * @self: a #TpConnection
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  authorize
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * For each of the given @contacts, request that the local user's presence is
 * sent to that contact, i.e. that their #TpContact:publish-state property
 * becomes %TP_SUBSCRIPTION_STATE_YES.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_authorize_publication_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_list_generic_async (authorize_publication);
}

/**
 * tp_connection_authorize_publication_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_authorize_publication_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_authorize_publication_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (authorize_publication);
}

/**
 * tp_connection_remove_contacts_async:
 * @self: a #TpConnection
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  remove
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Remove the given @contacts from the contact list entirely. It is
 * protocol-dependent whether this works, and under which circumstances.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_remove_contacts_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_list_generic_async (remove_contacts);
}

/**
 * tp_connection_remove_contacts_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_remove_contacts_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_remove_contacts_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (remove_contacts);
}

/**
 * tp_connection_unsubscribe_async:
 * @self: a #TpConnection
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  remove
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Attempt to set the given @contacts' #TpContact:subscribe-state property to
 * %TP_SUBSCRIPTION_STATE_NO, i.e. stop receiving their presence.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_unsubscribe_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_list_generic_async (unsubscribe);
}

/**
 * tp_connection_unsubscribe_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_unsubscribe_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_unsubscribe_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (unsubscribe);
}

/**
 * tp_connection_unpublish_async:
 * @self: a #TpConnection
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  remove
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Attempt to set the given @contacts' #TpContact:publish-state property to
 * %TP_SUBSCRIPTION_STATE_NO, i.e. stop sending presence to them.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_unpublish_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_list_generic_async (unpublish);
}

/**
 * tp_connection_unpublish_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_unpublish_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_unpublish_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (unpublish);
}

#define contact_groups_generic_async(method) \
  G_STMT_START { \
    GSimpleAsyncResult *result; \
    GArray *handles; \
    \
    g_return_if_fail (TP_IS_CONNECTION (self)); \
    g_return_if_fail (group != NULL); \
    g_return_if_fail (_tp_contacts_to_handles (self, n_contacts, contacts, \
        &handles)); \
    \
    result = g_simple_async_result_new ((GObject *) self, callback, user_data, \
        tp_connection_##method##_async); \
    \
    tp_cli_connection_interface_contact_groups_call_##method (self, -1, \
    group, handles, generic_callback, result, g_object_unref, NULL); \
    g_array_unref (handles); \
  } G_STMT_END

/**
 * tp_connection_set_group_members_async:
 * @self: a #TpConnection
 * @group: the group to alter.
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects members
 *  for the group. If this set is empty, this method MAY remove the group.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Add the given @contacts to the given @group (creating it if necessary), and
 * remove all other members.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUP.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_set_group_members_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_groups_generic_async (set_group_members);
}

/**
 * tp_connection_set_group_members_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_set_group_members_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_set_group_members_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (set_group_members);
}

/**
 * tp_connection_add_to_group_async:
 * @self: a #TpConnection
 * @group: the group to alter.
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  include in the group.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Add the given @contacts to the given @group, creating it if necessary.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUP.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_add_to_group_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_groups_generic_async (add_to_group);
}

/**
 * tp_connection_add_to_group_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_add_to_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_add_to_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (add_to_group);
}

/**
 * tp_connection_remove_from_group_async:
 * @self: a #TpConnection
 * @group: the group to alter.
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: (array length=n_contacts): An array of #TpContact objects to
 *  remove from the group.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Remove the given @contacts from the given @group. If there are no members
 * left in the group afterwards, the group MAY itself be removed.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUP.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_remove_from_group_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  contact_groups_generic_async (remove_from_group);
}

/**
 * tp_connection_remove_from_group_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_remove_from_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_remove_from_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (remove_from_group);
}

/**
 * tp_connection_remove_group_async:
 * @self: a #TpConnection
 * @group: the group to remove.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Remove all members from the given group, then remove the group itself.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUP.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_remove_group_async (TpConnection *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail (TP_IS_CONNECTION (self));
    g_return_if_fail (group != NULL);

    result = g_simple_async_result_new ((GObject *) self, callback, user_data,
        tp_connection_remove_group_async);

    tp_cli_connection_interface_contact_groups_call_remove_group (self, -1,
        group, generic_callback, result, g_object_unref, NULL);
}

/**
 * tp_connection_remove_group_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_remove_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_remove_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (remove_group);
}

/**
 * tp_connection_rename_group_async:
 * @self: a #TpConnection
 * @old_name: the group to rename
 * @new_name: the new name for the group
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Rename the given @old_name.
 *
 * On protocols where groups behave like tags, this is an API short-cut for
 * adding all of the group's members to a group with the new name, then removing
 * the old group.
 *
 * For this to work properly @self must have interface
 * %TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUP.
 *
 * Since: 0.UNRELEASED
 */
void
tp_connection_rename_group_async (TpConnection *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail (TP_IS_CONNECTION (self));
    g_return_if_fail (old_name != NULL);
    g_return_if_fail (new_name != NULL);

    result = g_simple_async_result_new ((GObject *) self, callback, user_data,
        tp_connection_rename_group_async);

    tp_cli_connection_interface_contact_groups_call_rename_group (self, -1,
        old_name, new_name, generic_callback, result, g_object_unref, NULL);
}

/**
 * tp_connection_rename_group_finish:
 * @self: a #TpConnection
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_connection_rename_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.UNRELEASED
 */
gboolean
tp_connection_rename_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error)
{
  generic_finish (rename_group);
}
