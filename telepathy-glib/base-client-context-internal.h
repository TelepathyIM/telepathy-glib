/*
 * Context objects for TpBaseClient calls (internal)
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_BASE_CLIENT_CONTEXT_INTERNAL_H__
#define __TP_BASE_CLIENT_CONTEXT_INTERNAL_H__

#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-client-context.h>

G_BEGIN_DECLS

TpObserveChannelsContext * _tp_observe_channels_context_new (
    DBusGMethodInvocation *dbus_context,
    GHashTable *observer_info);

TpBaseClientContextState _tp_observe_channels_context_get_state (
    TpObserveChannelsContext *self);

G_END_DECLS

#endif
