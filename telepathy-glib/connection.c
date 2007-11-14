/*
 * connection.c - proxy for a Telepathy connection
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

#include "telepathy-glib/connection.h"

struct _TpConnectionClass {
    TpProxyClass parent_class;
    /*<private>*/
};

struct _TpConnection {
    TpProxy parent;
    /*<private>*/
};

G_DEFINE_TYPE (TpConnection,
    tp_connection,
    TP_TYPE_PROXY);

static void
tp_connection_init (TpConnection *self)
{
}

static void
tp_connection_class_init (TpConnectionClass *klass)
{
}
