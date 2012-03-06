/*
 * cli-connection.h - auto-generated client API for a Telepathy connection
 *
 * Copyright © 2007-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007 Nokia Corporation
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

#ifndef TELEPATHY_GLIB_CLI_CONNECTION_H
#define TELEPATHY_GLIB_CLI_CONNECTION_H

#include <telepathy-glib/connection.h>

#include <telepathy-glib/_gen/tp-cli-connection.h>

G_BEGIN_DECLS

/* connection-handles.c - this has to come after the auto-generated
 * stuff because it uses an auto-generated typedef */

void tp_connection_get_contact_attributes (TpConnection *self,
    gint timeout_ms, guint n_handles, const TpHandle *handles,
    const gchar * const *interfaces,
    tp_cli_connection_interface_contacts_callback_for_get_contact_attributes callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

void tp_connection_get_contact_list_attributes (TpConnection *self,
    gint timeout_ms, const gchar * const *interfaces,
    tp_cli_connection_interface_contacts_callback_for_get_contact_attributes callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

G_END_DECLS

#endif

