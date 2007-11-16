/*
 * proxy.c - Base class for Telepathy client proxies
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

#include "telepathy-glib/proxy.h"

#include <telepathy-glib/errors.h>

#include "dbus-internal.h"
#define DEBUG_FLAG TP_DEBUG_PROXY
#include "debug-internal.h"
#include "proxy-internal.h"

/**
 * SECTION:proxy
 * @title: TpProxy
 * @short_description: base class for Telepathy client proxy objects
 * @see_also: #TpChannel, #TpConnection, #TpConnectionManager
 */

struct _TpProxySignalConnection
{
    TpProxy *tp_proxy;
    DBusGProxy *dg_proxy;
    char *signal_name;
    GCallback handler;
    gpointer data;
};

struct _TpProxyPendingCall
{
};

G_DEFINE_TYPE (TpProxy,
    tp_proxy,
    G_TYPE_OBJECT);

enum
{
  PROP_DBUS_CONNECTION = 1,
  PROP_BUS_NAME,
  PROP_OBJECT_PATH,
  N_PROPS
};

enum {
    SIGNAL_DESTROYED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

/**
 * tp_proxy_borrow_interface_by_id:
 * @self: the TpProxy
 * @interface: quark representing the interface required
 * @error: used to raise TP_ERROR_NOT_IMPLEMENTED if this object does not have
 *    the required interface
 *
 * <!-- -->
 *
 * Returns: a borrowed reference to a #DBusGProxy
 * for which the bus name and object path are the same as for @self, but the
 * interface is as given (or %NULL if this proxy does not implement it).
 * The reference is only valid as long as @self is.
 */
DBusGProxy *
tp_proxy_borrow_interface_by_id (TpProxy *self,
                                 GQuark interface,
                                 GError **error)
{
  DBusGProxy *proxy;

  if (!self->valid)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Object %s has become invalid", self->object_path);

      return NULL;
    }

  proxy = g_datalist_id_get_data (&(self->interfaces), interface);

  if (proxy != NULL)
    {
      return proxy;
    }

  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Object %s does not have interface %s",
      self->object_path, g_quark_to_string (interface));

  return NULL;
}

void
tp_proxy_invalidated (TpProxy *self)
{
  self->valid = FALSE;
  g_signal_emit (self, signals[SIGNAL_DESTROYED], 0);
}

static void
tp_proxy_iface_destroyed_cb (DBusGProxy *proxy,
                             TpProxy *self)
{
  tp_proxy_invalidated (self);
}

/**
 * tp_proxy_add_interface_by_id:
 * @self: the TpProxy
 * @interface: quark representing the interface to be added
 *
 * Declare that this proxy supports a given interface, and allocate a
 * #DBusGProxy to access it.
 *
 * To use methods and signals of that interface, either call
 * tp_proxy_borrow_interface_by_id() to get the #DBusGProxy, or use the
 * tp_cli_* wrapper functions.
 *
 * If the interface is the proxy's "main interface", or has already been
 * added, then do nothing.
 *
 * Returns: the borrowed DBusGProxy
 */
DBusGProxy *
tp_proxy_add_interface_by_id (TpProxy *self,
                              GQuark interface)
{
  DBusGProxy *iface_proxy = g_datalist_id_get_data (&(self->interfaces),
      interface);

  if (iface_proxy == NULL)
    {
      DEBUG ("%p: %s", self, g_quark_to_string (interface));
      iface_proxy = dbus_g_proxy_new_for_name (self->dbus_connection,
          self->bus_name, self->object_path, g_quark_to_string (interface));

      g_signal_connect (iface_proxy, "destroy",
          G_CALLBACK (tp_proxy_iface_destroyed_cb), self);

      g_datalist_id_set_data_full (&(self->interfaces), interface,
          iface_proxy, g_object_unref);
    }

  return iface_proxy;
}

TpProxyCallData *
tp_proxy_call_data_new (TpProxy *self,
                        GCallback callback,
                        gpointer user_data,
                        GDestroyNotify destroy)
{
  TpProxyCallData *ret = g_slice_new (TpProxyCallData);

  ret->proxy = g_object_ref (self);
  ret->callback = callback;
  ret->user_data = user_data;
  ret->destroy = destroy;

  return ret;
}

void
tp_proxy_call_data_free (gpointer call_data)
{
  TpProxyCallData *data = call_data;

  g_object_unref (TP_PROXY (data->proxy));

  if (data->destroy != NULL)
    data->destroy (data->user_data);

  g_slice_free (TpProxyCallData, data);
}

static void
tp_proxy_get_property (GObject *object,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
  TpProxy *self = TP_PROXY (object);

  switch (property_id)
    {
    case PROP_DBUS_CONNECTION:
      g_value_set_object (value, self->dbus_connection);
      break;
    case PROP_BUS_NAME:
      g_value_set_string (value, self->bus_name);
      break;
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_proxy_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
  TpProxy *self = TP_PROXY (object);

  switch (property_id)
    {
    case PROP_DBUS_CONNECTION:
      g_assert (self->dbus_connection == NULL);
      self->dbus_connection = g_value_dup_boxed (value);
      break;
    case PROP_BUS_NAME:
      g_assert (self->bus_name == NULL);
      self->bus_name = g_value_dup_string (value);
      break;
    case PROP_OBJECT_PATH:
      g_assert (self->object_path == NULL);
      self->object_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_proxy_init (TpProxy *self)
{
  DEBUG ("%p", self);
}

static GObject *
tp_proxy_constructor (GType type,
                      guint n_params,
                      GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_proxy_parent_class;
  TpProxy *self = TP_PROXY (object_class->constructor (type,
        n_params, params));
  TpProxyClass *klass = TP_PROXY_GET_CLASS (self);

  _tp_register_dbus_glib_marshallers ();

  g_return_val_if_fail (self->dbus_connection != NULL, NULL);
  g_return_val_if_fail (self->object_path != NULL, NULL);
  g_return_val_if_fail (self->bus_name != NULL, NULL);

  if (klass->interface != 0)
    {
      tp_proxy_add_interface_by_id (self, klass->interface);
    }

  /* Some interfaces are stateful, so we only allow binding to a unique
   * name, like in dbus_g_proxy_new_for_name_owner() */
  if (klass->must_have_unique_name)
    {
      g_return_val_if_fail (self->bus_name[0] != ':', NULL);
    }

  self->valid = TRUE;

  return (GObject *) self;
}

static void
tp_proxy_dispose (GObject *object)
{
  TpProxy *self = TP_PROXY (object);

  if (self->dispose_has_run)
    return;
  self->dispose_has_run = TRUE;

  G_OBJECT_CLASS (tp_proxy_parent_class)->dispose (object);
}

static void
tp_proxy_class_init (TpProxyClass *klass)
{
  GParamSpec *param_spec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = tp_proxy_constructor;
  object_class->get_property = tp_proxy_get_property;
  object_class->set_property = tp_proxy_set_property;
  object_class->dispose = tp_proxy_dispose;

  /**
   * TpProxy:bus-name:
   *
   * The D-Bus bus name for this object. Read-only except during construction.
   */
  param_spec = g_param_spec_boxed ("dbus-connection", "D-Bus connection",
      "The D-Bus connection used by this object", DBUS_TYPE_G_CONNECTION,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_DBUS_CONNECTION,
      param_spec);

  /**
   * TpProxy:bus-name:
   *
   * The D-Bus bus name for this object. Read-only except during construction.
   */
  param_spec = g_param_spec_string ("bus-name", "D-Bus bus name",
      "The D-Bus bus name for this object", NULL,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_BUS_NAME,
      param_spec);

  /**
   * TpProxy:object-path:
   *
   * The D-Bus object path for this object. Read-only except during
   * construction.
   */
  param_spec = g_param_spec_string ("object-path", "D-Bus object path",
      "The D-Bus object path for this object", NULL,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      param_spec);

  /**
   * TpProxy::destroyed:
   * @self: the proxy object
   *
   * Emitted when this proxy has been destroyed and become invalid for
   * whatever reason. Any more specific signal should be emitted first.
   */
  signals[SIGNAL_DESTROYED] = g_signal_new ("destroyed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}
