/*
 * connection-manager.c - proxy for a Telepathy connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#include "telepathy-glib/connection-manager.h"

/**
 * SECTION:connection-manager
 * @title: TpConnectionManager
 * @short_description: proxy object for a running Telepathy connection manager
 * @see_also: #TpConnection
 */

/**
 * TpConnectionManagerClass:
 *
 * The class of a #TpConnectionManager.
 */
struct _TpConnectionManagerClass {
    TpProxyClass parent_class;
    /*<private>*/
};

/**
 * TpConnectionManager:
 *
 * A proxy object for a running Telepathy connection manager.
 */
struct _TpConnectionManager {
    TpProxy parent;
    /*<private>*/
};

G_DEFINE_TYPE (TpConnectionManager,
    tp_connection_manager,
    TP_TYPE_PROXY);

static void
tp_connection_manager_init (TpConnectionManager *self)
{
}

static void
tp_connection_manager_class_init (TpConnectionManagerClass *klass)
{
}
