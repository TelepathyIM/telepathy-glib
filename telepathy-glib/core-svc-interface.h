/*
 * Copyright © 2014 Collabora Ltd.
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_CORE_SVC_INTERFACE_H__
#define __TP_CORE_SVC_INTERFACE_H__

#include <gio/gio.h>

#include <telepathy-glib/defs.h>

G_BEGIN_DECLS

typedef struct _TpSvcInterfaceInfo TpSvcInterfaceInfo;

struct _TpSvcInterfaceInfo {
    volatile gint ref_count;
    GDBusInterfaceInfo *interface_info;
    GDBusInterfaceVTable *vtable;
    gchar **signals;
    /*<private>*/
    gpointer _reserved[8];
};

void tp_svc_interface_set_dbus_interface_info (GType g_interface,
    const TpSvcInterfaceInfo *info);

const TpSvcInterfaceInfo *tp_svc_interface_peek_dbus_interface_info (
    GType g_interface);

G_END_DECLS

#endif
