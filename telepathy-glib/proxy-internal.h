/*
 * proxy-internal.h - Protected definitions for Telepathy client proxies
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

#ifndef __TP_PROXY_INTERNAL_H__
#define __TP_PROXY_INTERNAL_H__

G_BEGIN_DECLS

struct _TpProxyClass {
    DBusGProxyClass parent_class;

    /*<protected>*/
    GQuark fixed_interface;
    gboolean must_have_unique_name:1;

    /*<private>*/
    TpProxyClassPrivate *priv;
};

struct _TpProxy {
    DBusGProxy parent;
    /*<private>*/
    TpProxyPrivate *priv;
};

G_END_DECLS

#endif
