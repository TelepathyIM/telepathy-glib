/* Async operations for TpContact
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

#include <telepathy-glib/contact-operations.h>

#include <telepathy-glib/connection-contact-list.h>

#define DEBUG_FLAG TP_DEBUG_CONTACTS
#include "telepathy-glib/debug-internal.h"

/**
 * tp_contact_request_subscription_async:
 * @self: a #TpContact
 * @message: an optional message
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_request_subscription_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_request_subscription_async (TpContact *self,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_request_subscription_async (tp_contact_get_connection (self),
      1, &self, message, callback, user_data);
}

/**
 * tp_contact_request_subscription_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_request_subscription_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_request_subscription_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_request_subscription_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_authorize_publication_async:
 * @self: a #TpContact
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_authorize_publication_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_authorize_publication_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_authorize_publication_async (tp_contact_get_connection (self),
      1, &self, callback, user_data);
}

/**
 * tp_contact_authorize_publication_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_authorize_publication_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_authorize_publication_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_authorize_publication_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_remove_async:
 * @self: a #TpContact
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_remove_contacts_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_remove_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_remove_contacts_async (tp_contact_get_connection (self),
      1, &self, callback, user_data);
}

/**
 * tp_contact_remove_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_remove_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_remove_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_remove_contacts_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_unsubscribe_async:
 * @self: a #TpContact
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_unsubscribe_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_unsubscribe_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_unsubscribe_async (tp_contact_get_connection (self),
      1, &self, callback, user_data);
}

/**
 * tp_contact_unsubscribe_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_unsubscribe_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_unsubscribe_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_unsubscribe_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_unpublish_async:
 * @self: a #TpContact
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_unpublish_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_unpublish_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_unpublish_async (tp_contact_get_connection (self),
      1, &self, callback, user_data);
}

/**
 * tp_contact_unpublish_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_unpublish_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_unpublish_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_unpublish_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_add_to_group_async:
 * @self: a #TpContact
 * @group: the group to alter.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_add_to_group_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_add_to_group_async (TpContact *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_add_to_group_async (tp_contact_get_connection (self),
      group, 1, &self, callback, user_data);
}

/**
 * tp_contact_add_to_group_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_add_to_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_add_to_group_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_add_to_group_finish (
      tp_contact_get_connection (self), result, error);
}

/**
 * tp_contact_remove_from_group_async:
 * @self: a #TpContact
 * @group: the group to alter.
 * @callback: a callback to call when the operation finishes
 * @user_data: data to pass to @callback
 *
 * Convenience wrapper for tp_connection_remove_from_group_async()
 * on a single contact.
 *
 * Since: 0.15.5
 */
void
tp_contact_remove_from_group_async (TpContact *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  tp_connection_remove_from_group_async (tp_contact_get_connection (self),
      group, 1, &self, callback, user_data);
}

/**
 * tp_contact_remove_from_group_finish:
 * @self: a #TpContact
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes tp_contact_remove_from_group_async()
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 0.15.5
 */
gboolean
tp_contact_remove_from_group_finish (TpContact *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_connection_remove_from_group_finish (
      tp_contact_get_connection (self), result, error);
}
