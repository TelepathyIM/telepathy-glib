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

#ifndef __TP_CONTACT_OPERATIONS_H__
#define __TP_CONTACT_OPERATIONS_H__

#include <telepathy-glib/contact.h>

G_BEGIN_DECLS

void tp_contact_request_subscription_async (TpContact *self,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_request_subscription_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_authorize_publication_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_authorize_publication_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_remove_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_remove_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_unsubscribe_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_unsubscribe_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_unpublish_async (TpContact *self,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_unpublish_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_add_to_group_async (TpContact *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_add_to_group_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

void tp_contact_remove_from_group_async (TpContact *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_contact_remove_from_group_finish (TpContact *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
