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

/**
 * SECTION:proxy
 * @title: TpProxy
 * @short_description: base class for Telepathy client proxy objects
 * @see_also: #TpChannel, #TpConnection, #TpConnectionManager
 */

G_DEFINE_TYPE (TpProxy,
    tp_proxy,
    DBUS_TYPE_G_PROXY);

struct _TpProxyPrivate
{
  /* The interface represented by this proxy */
  GQuark main_interface;
  /* GQuark for interface => ref'd DBusGProxy * */
  GData *interfaces;
  /* TRUE if disposed of */
  gboolean dispose_has_run:1;
};

/**
 * tp_proxy_borrow_interface_by_id:
 * @self: the TpProxy
 * @interface: quark representing the interface required
 * @error: used to raise TP_ERROR_NOT_IMPLEMENTED if this object does not have
 *    the required interface
 *
 * <!-- -->
 *
 * Returns: a borrowed reference to a #DBusGProxy (possibly equal to @self)
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

  if (interface == self->priv->main_interface)
    {
      return (DBusGProxy *) self;
    }

  proxy = g_datalist_id_get_data (&(self->priv->interfaces), interface);

  if (proxy != NULL)
    {
      return proxy;
    }

  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Object %s does not have interface %s",
      dbus_g_proxy_get_path ((DBusGProxy *) self),
      g_quark_to_string (interface));

  return NULL;
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
 */
void
tp_proxy_add_interface_by_id (TpProxy *self,
                              GQuark interface)
{
  DBusGProxy *self_gproxy = (DBusGProxy *) self;
  DBusGProxy *iface_proxy;

  /* Silently do nothing if we're "adding" the main interface */
  if (interface == self->priv->main_interface)
    return;

  /* Silently do nothing if we're adding an already-added interface */
  if (g_datalist_id_get_data (&(self->priv->interfaces), interface) != NULL)
    return;

  DEBUG ("%p: %s", self, g_quark_to_string (interface));
  iface_proxy = dbus_g_proxy_new_from_proxy (self_gproxy,
      g_quark_to_string (interface), dbus_g_proxy_get_path (self_gproxy));

  g_datalist_id_set_data_full (&(self->priv->interfaces), interface,
      iface_proxy, g_object_unref);
}

static void
tp_proxy_init (TpProxy *self)
{
  DEBUG ("%p", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_PROXY,
      TpProxyPrivate);
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
  gchar *main_interface, *bus_name;

  _tp_register_dbus_glib_marshallers ();

  g_object_get (self,
      "interface", &main_interface,
      "name", &bus_name,
      NULL);

  DEBUG ("%p: bus name %s, interface %s", self, bus_name, main_interface);

  /* We require the interface property; it makes no sense to not have the
   * main interface. This is e.g. Channel or Connection. */
  g_return_val_if_fail (main_interface != NULL, NULL);
  if (klass->fixed_interface != 0)
    {
      g_return_val_if_fail (klass->fixed_interface ==
          g_quark_try_string (main_interface), NULL);
    }

  /* Some interfaces are stateful, so we only allow binding to a unique
   * name, like in dbus_g_proxy_new_for_name_owner() */
  if (klass->must_have_unique_name)
    {
      g_return_val_if_fail (bus_name != NULL && bus_name[0] != ':', NULL);
    }

  self->priv->main_interface = g_quark_from_string (main_interface);

  g_free (main_interface);
  g_free (bus_name);

  return (GObject *) self;
}

static void
tp_proxy_dispose (GObject *object)
{
  TpProxy *self = TP_PROXY (object);

  if (self->priv->dispose_has_run)
    return;
  self->priv->dispose_has_run = TRUE;

  /* Discard interface proxies */
  g_datalist_clear (&(self->priv->interfaces));

  G_OBJECT_CLASS (tp_proxy_parent_class)->dispose (object);
}

static void
tp_proxy_class_init (TpProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpProxyPrivate));

  object_class->constructor = tp_proxy_constructor;
  object_class->dispose = tp_proxy_dispose;
}
