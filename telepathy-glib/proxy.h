/*
 * proxy.h - Base class for Telepathy client proxies
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
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

#ifndef __TP_PROXY_H__
#define __TP_PROXY_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* Forward declaration of a subclass - from dbus.h */
typedef struct _TpDBusDaemon TpDBusDaemon;

typedef struct _TpProxyPrivate TpProxyPrivate;

typedef struct _TpProxy TpProxy;

#define TP_DBUS_ERRORS (tp_dbus_errors_quark ())
GQuark tp_dbus_errors_quark (void);

typedef enum {
    TP_DBUS_ERROR_UNKNOWN_REMOTE_ERROR = 0,
    TP_DBUS_ERROR_PROXY_UNREFERENCED = 1,
    TP_DBUS_ERROR_NO_INTERFACE = 2,
    TP_DBUS_ERROR_NAME_OWNER_LOST = 3,
    TP_DBUS_ERROR_INVALID_BUS_NAME = 4,
    TP_DBUS_ERROR_INVALID_INTERFACE_NAME = 5,
    TP_DBUS_ERROR_INVALID_OBJECT_PATH = 6,
    TP_DBUS_ERROR_INVALID_MEMBER_NAME = 7,
    TP_DBUS_ERROR_OBJECT_REMOVED = 8,
    TP_DBUS_ERROR_CANCELLED = 9,
    NUM_TP_DBUS_ERRORS
} TpDBusError;

struct _TpProxy {
    /*<public>*/
    GObject parent;

    TpDBusDaemon *dbus_daemon;
    DBusGConnection *dbus_connection;
    gchar *bus_name;
    gchar *object_path;

    GError *invalidated /* initialized to NULL by g_object_new */;

    TpProxyPrivate *priv;
};

typedef struct _TpProxyClass TpProxyClass;

struct _TpProxyClass {
    /*<public>*/
    GObjectClass parent_class;

    GQuark interface;

    gboolean must_have_unique_name:1;
    guint _reserved_flags:31;

    GCallback _reserved[4];
    gpointer priv;
};

typedef struct _TpProxyPendingCall TpProxyPendingCall;

void tp_proxy_pending_call_cancel (TpProxyPendingCall *pc);

typedef struct _TpProxySignalConnection TpProxySignalConnection;

void tp_proxy_signal_connection_disconnect (TpProxySignalConnection *sc);

GType tp_proxy_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_PROXY \
  (tp_proxy_get_type ())
#define TP_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_PROXY, \
                              TpProxy))
#define TP_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_PROXY, \
                           TpProxyClass))
#define TP_IS_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_PROXY))
#define TP_IS_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_PROXY))
#define TP_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_PROXY, \
                              TpProxyClass))

gboolean tp_proxy_has_interface_by_id (gpointer self, GQuark interface);

static inline gboolean
_tp_proxy_inline_has_interface (gpointer self, const gchar *interface)
{
  GQuark q = g_quark_try_string (interface);

  return q != 0 && tp_proxy_has_interface_by_id (self, q);
}

#define tp_proxy_has_interface(self, interface) \
    (_tp_proxy_inline_has_interface (self, interface))

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-generic.h>

#endif /* #ifndef __TP_PROXY_H__*/
