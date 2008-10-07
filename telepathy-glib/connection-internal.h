/*
 * TpConnection - proxy for a Telepathy connection (internals)
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef TP_CONNECTION_INTERNAL_H
#define TP_CONNECTION_INTERNAL_H

#include <telepathy-glib/connection.h>

G_BEGIN_DECLS

const GArray *_tp_connection_get_contact_attribute_interfaces (
    TpConnection *self);

void _tp_connection_init_handle_refs (TpConnection *self);
void _tp_connection_clean_up_handle_refs (TpConnection *self);

G_END_DECLS

#endif
