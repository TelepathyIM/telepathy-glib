/*
 * connection-contact-list.h - ContactList and ContactGroup support
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

#ifndef __TP_CONNECTION_CONTACT_LIST_H__
#define __TP_CONNECTION_CONTACT_LIST_H__

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>

G_BEGIN_DECLS

void tp_connection_request_subscription_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_request_subscription_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_authorize_publication_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_authorize_publication_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_remove_contacts_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_remove_contacts_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_unsubscribe_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_unsubscribe_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_unpublish_async (TpConnection *self,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_unpublish_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_set_group_members_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_set_group_members_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_add_to_group_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_add_to_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_remove_from_group_async (TpConnection *self,
    const gchar *group,
    guint n_contacts,
    TpContact * const *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_remove_from_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_remove_group_async (TpConnection *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_remove_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

void tp_connection_rename_group_async (TpConnection *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean tp_connection_rename_group_finish (TpConnection *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
