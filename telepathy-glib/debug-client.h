/*
 * debug-client.h - proxy for Telepathy debug objects
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef TP_DEBUG_CLIENT_H
#define TP_DEBUG_CLIENT_H

#include <telepathy-glib/defs.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpDebugClient TpDebugClient;
typedef struct _TpDebugClientPrivate TpDebugClientPrivate;
typedef struct _TpDebugClientClass TpDebugClientClass;

TpDebugClient *tp_debug_client_new (
    TpDBusDaemon *dbus,
    const gchar *unique_name,
    GError **error);

/* Tedious GObject boilerplate */

GType tp_debug_client_get_type (void);

#define TP_TYPE_DEBUG_CLIENT \
  (tp_debug_client_get_type ())
#define TP_DEBUG_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_DEBUG_CLIENT, \
                              TpDebugClient))
#define TP_DEBUG_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_DEBUG_CLIENT, \
                           TpDebugClientClass))
#define TP_IS_DEBUG_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_DEBUG_CLIENT))
#define TP_IS_DEBUG_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_DEBUG_CLIENT))
#define TP_DEBUG_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_DEBUG_CLIENT, \
                              TpDebugClientClass))

void tp_debug_client_init_known_interfaces (void);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-debug.h>

#endif
