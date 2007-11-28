/*
 * proxy.h - Base class for Telepathy client proxies
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

#ifndef __TP_PROXY_H__
#define __TP_PROXY_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* Forward declaration of a subclass - from dbus.h */
typedef struct _TpDBusDaemon TpDBusDaemon;

typedef struct _TpProxyPrivate TpProxyPrivate;

typedef struct _TpProxy TpProxy;

struct _TpProxy {
    /*<public>*/
    GObject parent;

    TpDBusDaemon *dbus_daemon;
    DBusGConnection *dbus_connection;
    gchar *bus_name;
    gchar *object_path;

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

struct _TpProxyPendingCall {
    /*<public>*/
    TpProxy *proxy;
    GQuark interface;
    gchar *member;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    DBusGProxyCall *pending_call;
    gconstpointer priv;
};

TpProxyPendingCall *tp_proxy_pending_call_new (TpProxy *self,
    GQuark interface, const gchar *member, GCallback callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

void tp_proxy_pending_call_free (gpointer self);

void tp_proxy_pending_call_cancel (const TpProxyPendingCall *self);

typedef struct _TpProxySignalConnection TpProxySignalConnection;

struct _TpProxySignalConnection {
    /*<public>*/
    TpProxy *proxy;
    GQuark interface;
    gchar *member;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    gconstpointer priv;
};

TpProxySignalConnection *tp_proxy_signal_connection_new (TpProxy *self,
    GQuark interface, const gchar *member, GCallback callback,
    gpointer user_data, GDestroyNotify destroy, GObject *weak_object);

void tp_proxy_signal_connection_free_closure (gpointer self, GClosure *unused);

void tp_proxy_signal_connection_disconnect
    (const TpProxySignalConnection *self);

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

typedef void (*TpProxyInterfaceAddedCb) (TpProxy *self,
    guint quark, DBusGProxy *proxy, gpointer unused);

void tp_proxy_class_hook_on_interface_add (TpProxyClass *klass,
    TpProxyInterfaceAddedCb callback);

DBusGProxy *tp_proxy_borrow_interface_by_id (TpProxy *self, GQuark interface,
    GError **error);

DBusGProxy *tp_proxy_add_interface_by_id (TpProxy *self, GQuark interface);

void tp_proxy_invalidated (TpProxy *self, const GError *error);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-generic.h>

#endif /* #ifndef __TP_PROXY_H__*/
