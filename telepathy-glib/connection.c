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

#include <telepathy-glib/enums.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "debug-internal.h"

/**
 * SECTION:connection
 * @title: TpConnection
 * @short_description: proxy object for a Telepathy connection
 * @see_also: #TpConnectionManager, #TpChannel
 */

/**
 * TpConnectionClass:
 *
 * The class of a #TpConnection.
 */
struct _TpConnectionClass {
    TpProxyClass parent_class;
    /*<private>*/
};

/**
 * TpConnection:
 *
 * A proxy object for a Telepathy connection.
 */
struct _TpConnection {
    TpProxy parent;
    /*<private>*/
    TpConnectionStatus status;
    TpConnectionStatusReason status_reason;
};

enum
{
  PROP_STATUS = 1,
  PROP_STATUS_REASON,
  N_PROPS
};

G_DEFINE_TYPE (TpConnection,
    tp_connection,
    TP_TYPE_PROXY);

enum {
    SIGNAL_CONNECTION_READY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

static void
tp_connection_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  TpConnection *self = TP_CONNECTION (object);

  switch (property_id)
    {
    case PROP_STATUS:
      g_value_set_uint (value, self->status);
      break;
    case PROP_STATUS_REASON:
      g_value_set_uint (value, self->status_reason);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
tp_connection_destroy_idle_func (gpointer user_data)
{
  DEBUG ("%p", user_data);
  g_object_run_dispose (user_data);
  return FALSE;
}

static void
tp_connection_got_interfaces_cb (DBusGProxy *proxy,
                                 DBusGProxyCall *call,
                                 gpointer user_data)
{
  TpConnection *self = TP_CONNECTION (proxy);
  GError *error = NULL;
  gchar **interfaces;

  DEBUG ("%p", proxy);

  if (dbus_g_proxy_end_call (proxy, call, &error,
        G_TYPE_STRV, &interfaces,
        G_TYPE_INVALID))
    {
      DEBUG ("%p: Introspected interfaces", self);
      if (interfaces != NULL)
        {
          gchar **iter;

          for (iter = interfaces; *iter != NULL; iter++)
            {
              tp_proxy_add_interface_by_id ((TpProxy *) self,
                  g_quark_from_string (*iter));
            }

          g_strfreev (interfaces);
        }

      DEBUG ("%p: emitting connection-ready", self);
      g_signal_emit (self, signals[SIGNAL_CONNECTION_READY], 0);
    }
  else
    {
      DEBUG ("%p: GetInterfaces() failed", self);
      g_error_free (error);
      tp_connection_destroy_idle_func (self);
    }
}

static void
tp_connection_status_changed_cb (TpConnection *self,
                                 guint status,
                                 guint reason,
                                 gpointer user_data)
{
  DEBUG ("%p: %d -> %d because %d", self, self->status, status, reason);

  self->status = status;
  self->status_reason = reason;

  switch (status)
    {
    case TP_CONNECTION_STATUS_DISCONNECTED:
      g_idle_add (tp_connection_destroy_idle_func, self);
      break;
    case TP_CONNECTION_STATUS_CONNECTED:
      dbus_g_proxy_begin_call ((DBusGProxy *) self, "GetInterfaces",
          tp_connection_got_interfaces_cb, NULL, NULL, G_TYPE_INVALID);
      break;
    default:
      break;
    }
}

static void
tp_connection_got_status_cb (DBusGProxy *proxy,
                             DBusGProxyCall *call,
                             gpointer user_data)
{
  TpConnection *self = TP_CONNECTION (proxy);
  guint status;
  GError *error = NULL;

  DEBUG ("%p", self);

  if (dbus_g_proxy_end_call (proxy, call, &error,
        G_TYPE_UINT, &status,
        G_TYPE_INVALID))
    {
      DEBUG ("%p: Initial status is %d", self, status);
      tp_connection_status_changed_cb (self, status,
          TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED, NULL);
    }
  else
    {
      DEBUG ("%p: GetStatus() failed, will self-destruct", self);
      g_error_free (error);
      g_idle_add (tp_connection_destroy_idle_func, self);
      return;
    }
}

static GObject *
tp_connection_constructor (GType type,
                           guint n_params,
                           GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_connection_parent_class;
  TpConnection *self = TP_CONNECTION (object_class->constructor (type,
        n_params, params));
  DBusGProxy *proxy = (DBusGProxy *) self;

  /* connect to my own StatusChanged signal */
  DEBUG ("Connecting to StatusChanged");
  dbus_g_proxy_add_signal (proxy, "StatusChanged", G_TYPE_UINT, G_TYPE_UINT,
      G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "StatusChanged",
      G_CALLBACK (tp_connection_status_changed_cb), NULL, NULL);

  /* get my initial status */
  DEBUG ("Calling GetStatus");
  dbus_g_proxy_begin_call (proxy, "GetStatus",
      tp_connection_got_status_cb, NULL, NULL, G_TYPE_INVALID);

  DEBUG ("Returning %p", self);
  return (GObject *) self;
}

static void
tp_connection_init (TpConnection *self)
{
  DEBUG ("%p", self);

  self->status = TP_UNKNOWN_CONNECTION_STATUS;
  self->status_reason = TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
}

static void
tp_connection_dispose (GObject *object)
{
  DEBUG ("%p", object);

  ((GObjectClass *) tp_connection_parent_class)->dispose (object);
}

static void
tp_connection_class_init (TpConnectionClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructor = tp_connection_constructor;
  object_class->get_property = tp_connection_get_property;
  object_class->dispose = tp_connection_dispose;

  proxy_class->fixed_interface = TP_IFACE_QUARK_CONNECTION;
  proxy_class->must_have_unique_name = TRUE;

  /**
   * TpConnection:status:
   *
   * This connection's status, or TP_UNKNOWN_CONNECTION_STATUS if we don't
   * know yet.
   */
  param_spec = g_param_spec_uint ("status", "Status",
      "The status of this connection", 0, G_MAXUINT32,
      TP_UNKNOWN_CONNECTION_STATUS,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STATUS,
      param_spec);

  /**
   * TpConnection:status-reason:
   *
   * The reason why #TpConnection:status changed to its current value,
   * or TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED if unknown.
   * know yet.
   */
  param_spec = g_param_spec_uint ("status-reason", "Last status change reason",
      "The reason why TpConnection:status changed to its current value",
      0, G_MAXUINT32, TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STATUS_REASON,
      param_spec);

  /**
   * TpConnection::connection-ready:
   * @self: the connection proxy
   *
   * Emitted once, either shortly after the connection becomes connected (if
   * it was not initially connected), or shortly after we find out that it
   * is connected (if it was initially connected).
   *
   * Before this signal is emitted, the interfaces will have been set up
   * in the #TpProxy code and the TpConnection:status property will have been
   * set to TP_CONNECTION_STATUS_CONNECTED.
   */
  signals[SIGNAL_CONNECTION_READY] = g_signal_new ("connection-ready",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}
