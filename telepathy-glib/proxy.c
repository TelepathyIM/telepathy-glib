/*
 * proxy.c - Base class for Telepathy client proxies
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

#include "telepathy-glib/proxy-subclass.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>

#include "dbus-internal.h"
#define DEBUG_FLAG TP_DEBUG_PROXY
#include "debug-internal.h"

#include "_gen/signals-marshal.h"

#include "_gen/tp-cli-generic-body.h"

/**
 * SECTION:proxy
 * @title: TpProxy
 * @short_description: base class for Telepathy client proxy objects
 * @see_also: #TpChannel, #TpConnection, #TpConnectionManager
 *
 * #TpProxy is a base class for Telepathy client-side proxies, which represent
 * an object accessed via D-Bus and provide access to its methods and signals.
 *
 * The header proxy.h also includes auto-generated client wrappers for the
 * Properties interface, which can be implemented by any type of object.
 *
 * Since: 0.7.1
 */

/**
 * SECTION:proxy-subclass
 * @title: TpProxy subclasses and mixins
 * @short_description: Providing extra functionality for a #TpProxy or
 *  subclass, or subclassing it
 * @see_also: #TpProxy
 *
 * The implementations of #TpProxy subclasses and "mixin" functions need
 * access to the underlying dbus-glib objects used to implement the
 * #TpProxy API.
 *
 * Mixin functions to implement particular D-Bus interfaces should usually
 * be auto-generated, by copying tools/glib-client-gen.py from telepathy-glib.
 *
 * Since: 0.7.1
 */

/**
 * TpProxy:
 * @parent: parent object
 * @dbus_daemon: the #TpDBusDaemon for this object, if any; always %NULL
 *  if this object is a #TpDBusDaemon (read-only)
 * @dbus_connection: the D-Bus connection used by this object (read-only)
 * @bus_name: the bus name of the application exporting the object (read-only)
 * @object_path: the object path of the remote object (read-only)
 * @invalidated: if not %NULL, the reason this proxy was invalidated
 *  (read-only)
 * @priv: private internal data
 *
 * Structure representing a Telepathy client-side proxy.
 */

/**
 * TpProxyInterfaceAddedCb:
 * @self: the proxy
 * @quark: a quark whose string value is the interface being added
 * @proxy: the #DBusGProxy for the added interface
 * @unused: unused
 *
 * The signature of a #TpProxy::interface-added signal callback.
 */

/**
 * TpProxyClass:
 * @parent_class: The parent class structure
 * @interface: If set non-zero by a subclass, #TpProxy will
 *    automatically add this interface in its constructor
 * @must_have_unique_name: If set %TRUE by a subclass, the #TpProxy
 *    constructor will fail if a well-known bus name is given
 * @_reserved_flags: Reserved for future expansion
 * @_reserved: Reserved for future expansion
 * @priv: Opaque pointer for private data
 *
 * The class of a #TpProxy.
 */
/* priv is actually a GSList of callbacks */

/**
 * TpProxyInvokeFunc:
 * @self: the #TpProxy on which the D-Bus method was invoked
 * @error: %NULL if the method call succeeded, or a non-%NULL error if the
 *  method call failed
 * @args: array of "out" arguments (return values) for the D-Bus method,
 *  or %NULL if an error occurred or if there were no "out" arguments
 * @callback: the callback that should be invoked, as passed to
 *  tp_proxy_pending_call_v0_new()
 * @user_data: user-supplied data to pass to the callback, as passed to
 *  tp_proxy_pending_call_v0_new()
 * @weak_object: user-supplied object to pass to the callback, as passed to
 *  tp_proxy_pending_call_v0_new()
 *
 * Signature of a callback invoked by the #TpProxy machinery after a D-Bus
 * method call has succeeded or failed. It is responsible for calling the
 * user-supplied callback.
 *
 * Because parts of dbus-glib aren't reentrant, this callback may be called
 * from an idle handler shortly after the method call reply is received,
 * rather than from the callback for the reply.
 *
 * At most one of @args and @error can be non-%NULL (implementations may
 * assert this). @args and @error may both be %NULL if a method with no
 * "out" arguments (i.e. a method that returns nothing) was called
 * successfully.
 *
 * The #TpProxyInvokeFunc must call callback with @user_data, @weak_object,
 * and appropriate arguments derived from @error and @args. It is responsible
 * for freeing @error and @args, if their ownership has not been transferred.
 */

/**
 * TpProxyPendingCall:
 *
 * Opaque structure representing a pending D-Bus call.
 */

struct _TpProxyPendingCall {
    TpProxy *proxy;
    GQuark interface;
    gchar *member;
    TpProxyInvokeFunc invoke_callback;
    GError *error;
    GValueArray *args;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    DBusGProxyCall *pending_call;
    guint idle_source;
    gconstpointer priv;
};

/**
 * TpProxySignalConnection:
 *
 * Opaque structure representing a D-Bus signal connection.
 */

struct _TpProxySignalConnection {
    TpProxy *proxy;
    GQuark interface;
    gchar *member;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    GCallback impl_callback;
    gconstpointer priv;
};

struct _TpProxyPrivate {
    /* GQuark for interface => ref'd DBusGProxy * */
    GData *interfaces;

    gboolean valid:1;
    gboolean dispose_has_run:1;
};

G_DEFINE_TYPE (TpProxy,
    tp_proxy,
    G_TYPE_OBJECT);

enum
{
  PROP_DBUS_DAEMON = 1,
  PROP_DBUS_CONNECTION,
  PROP_BUS_NAME,
  PROP_OBJECT_PATH,
  N_PROPS
};

enum {
    SIGNAL_INTERFACE_ADDED,
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

  if (!tp_dbus_check_valid_interface_name (g_quark_to_string (interface),
        error))
      return NULL;

  if (!self->priv->valid)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Object %s has become invalid", self->object_path);

      return NULL;
    }

  proxy = g_datalist_id_get_data (&self->priv->interfaces, interface);

  if (proxy != NULL)
    {
      return proxy;
    }

  g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Object %s does not have interface %s",
      self->object_path, g_quark_to_string (interface));

  return NULL;
}

/**
 * tp_proxy_has_interface_by_id:
 * @self: the #TpProxy (or subclass)
 * @interface: quark representing the interface required
 *
 * <!-- -->
 *
 * Returns: %TRUE if this proxy implements the given interface.
 */
gboolean
tp_proxy_has_interface_by_id (gpointer self,
                              GQuark interface)
{
  return tp_proxy_borrow_interface_by_id (self, interface, NULL) != NULL;
}

/**
 * tp_proxy_has_interface:
 * @self: the #TpProxy (or subclass)
 * @interface: the interface required, as a string
 *
 * A macro wrapping tp_proxy_has_interface_by_id(). Returns %TRUE if this
 * proxy implements the given interface.
 */

/**
 * tp_proxy_invalidated:
 * @self: a proxy
 * @error: an error causing the invalidation
 *
 * Mark @self as having been invalidated - no further calls will work, and
 * the #TpProxy:destroyed signal will be emitted with the given error.
 */
void
tp_proxy_invalidated (TpProxy *self, const GError *error)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (error != NULL);

  if (self->invalidated == NULL)
    {
      DEBUG ("%p: %s", self, error->message);
      self->invalidated = g_error_copy (error);

      g_signal_emit (self, signals[SIGNAL_DESTROYED], 0, self->invalidated);
    }

  /* Don't clear the datalist until after we've emitted the signal, so
   * the pending call and signal connection friend classes can still get
   * to the proxies */
  g_datalist_clear (&self->priv->interfaces);

  if (self->dbus_daemon != NULL)
    {
      g_object_unref (self->dbus_daemon);
      self->dbus_daemon = NULL;
    }

  if (self->dbus_connection != NULL)
    {
      dbus_g_connection_unref (self->dbus_connection);
      self->dbus_connection = NULL;
    }
}

/* this error is raised in multiple places - it's initialized by class_init */
static GError tp_proxy_dgproxy_destroyed = { 0 };

static void
tp_proxy_iface_destroyed_cb (DBusGProxy *proxy,
                             TpProxy *self)
{
  /* We can't call any API on the proxy now. Because the proxies are all
   * for the same bus name, we can assume that all of them are equally
   * useless now */
  g_datalist_clear (&self->priv->interfaces);

  tp_proxy_invalidated (self, &tp_proxy_dgproxy_destroyed);
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
  DBusGProxy *iface_proxy = g_datalist_id_get_data (&self->priv->interfaces,
      interface);

  g_return_val_if_fail
      (tp_dbus_check_valid_interface_name (g_quark_to_string (interface),
          NULL),
       NULL);

  if (iface_proxy == NULL)
    {
      DEBUG ("%p: %s", self, g_quark_to_string (interface));
      iface_proxy = dbus_g_proxy_new_for_name (self->dbus_connection,
          self->bus_name, self->object_path, g_quark_to_string (interface));

      g_signal_connect (iface_proxy, "destroy",
          G_CALLBACK (tp_proxy_iface_destroyed_cb), self);

      g_datalist_id_set_data_full (&self->priv->interfaces, interface,
          iface_proxy, g_object_unref);

      g_signal_emit (self, signals[SIGNAL_INTERFACE_ADDED], 0,
          (guint) interface, iface_proxy);
    }

  return iface_proxy;
}

static const gchar * const pending_call_magic = "TpProxyPendingCall";
static const gchar * const signal_conn_magic = "TpProxySignalConnection";

static void
tp_proxy_pending_call_lost_weak_ref (gpointer data,
                                     GObject *dead)
{
  TpProxyPendingCall *self = data;

  DEBUG ("%p lost weak ref to %p", self, dead);

  g_assert (self->priv == pending_call_magic);
  g_assert (dead == self->weak_object);

  self->weak_object = NULL;
  tp_proxy_pending_call_cancel (self);
}

static void
tp_proxy_pending_call_proxy_invalidated (TpProxy *proxy,
                                         const GError *why,
                                         TpProxyPendingCall *self)
{
  GCallback callback = self->callback;

  g_assert (self != NULL);
  g_assert (why != NULL);
  DEBUG ("%p: proxy %p invalidated (I have %p): %s", self, proxy, self->proxy,
      why->message);
  g_assert (proxy == self->proxy);

  if (self->idle_source != 0)
    {
      g_source_remove (self->idle_source);
    }

  if (callback != NULL)
    {
      self->callback = NULL;
      DEBUG ("Invoking %p", self->invoke_callback);
      g_return_if_fail (self->invoke_callback != NULL);
      self->invoke_callback (proxy, g_error_copy (why), NULL, callback,
          self->user_data, self->weak_object);
    }
}

/**
 * tp_proxy_pending_call_v0_new:
 * @self: a proxy
 * @interface: a quark whose string value is the D-Bus interface
 * @member: the name of the method being called
 * @invoke_callback: an implementation of #TpProxyInvokeFunc which will
 *  invoke @callback with appropriate arguments
 * @callback: a callback to be called when the call completes
 * @user_data: user-supplied data for the callback
 * @destroy: user-supplied destructor for the data
 * @weak_object: if not %NULL, a #GObject which will be weakly referenced by
 *   the signal connection - if it is destroyed, the signal connection will
 *   automatically be disconnected
 *
 * Allocate a new pending call structure. After calling this function, the
 * caller must start an asynchronous D-Bus call and give the resulting
 * DBusGProxyCall to the pending call object using
 * tp_proxy_pending_call_v0_take_pending_call().
 *
 * If dbus-glib gets a reply to the call before it's cancelled, the caller
 * must arrange for tp_proxy_pending_call_v0_take_results() to be called
 * with the results (the intention is for this to be done immediately
 * after dbus_g_proxy_end_call in the callback supplied to dbus-glib).
 *
 * When dbus-glib discards its reference to the user_data supplied in the
 * asynchronous D-Bus call (i.e. after the call is cancelled or a reply
 * arrives), tp_proxy_pending_call_v0_completed must be called (the intention
 * is for the #TpProxyPendingCall to be the @user_data in the async call,
 * and for tp_proxy_pending_call_v0_completed to be the #GDestroyNotify
 * passed to the same async call).
 *
 * This function is for use by #TpProxy subclass implementations only, and
 * should usually only be called from code generated by
 * tools/glib-client-gen.py.
 *
 * Returns: a new pending call structure
 */
TpProxyPendingCall *
tp_proxy_pending_call_v0_new (TpProxy *self,
                              GQuark interface,
                              const gchar *member,
                              TpProxyInvokeFunc invoke_callback,
                              GCallback callback,
                              gpointer user_data,
                              GDestroyNotify destroy,
                              GObject *weak_object)
{
  TpProxyPendingCall *ret = g_slice_new0 (TpProxyPendingCall);

  DEBUG ("(proxy=%p, if=%s, meth=%s, ic=%p; cb=%p, ud=%p, dn=%p, wo=%p) -> %p",
      self, g_quark_to_string (interface), member, invoke_callback,
      callback, user_data, destroy, weak_object, ret);

  ret->proxy = g_object_ref (self);
  ret->interface = interface;
  ret->member = g_strdup (member);
  ret->invoke_callback = invoke_callback;
  ret->callback = callback;
  ret->user_data = user_data;
  ret->destroy = destroy;
  ret->weak_object = weak_object;
  ret->pending_call = NULL;
  ret->priv = pending_call_magic;

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_pending_call_lost_weak_ref, ret);

  g_signal_connect (self, "destroyed",
      G_CALLBACK (tp_proxy_pending_call_proxy_invalidated), ret);

  return ret;
}

/**
 * tp_proxy_pending_call_cancel:
 * @self: a pending call
 *
 * Cancel the given pending call. After this function returns, you
 * must not assume that the pending call remains valid, but you must not
 * explicitly free it either.
 */
void
tp_proxy_pending_call_cancel (TpProxyPendingCall *self)
{
  DBusGProxy *iface;

  DEBUG ("%p", self);

  g_return_if_fail (self->priv == pending_call_magic);

  /* Mark the pending call as expired */
  self->callback = NULL;

  if (self->idle_source != 0)
    {
      /* we aren't actually doing dbus-glib things any more anyway */
      g_source_remove (self->idle_source);
      return;
    }

  iface = g_datalist_id_get_data (&self->proxy->priv->interfaces,
      self->interface);

  if (iface == NULL || self->pending_call == NULL)
    return;

  dbus_g_proxy_cancel_call (iface, self->pending_call);
}

static void
tp_proxy_pending_call_free (gpointer p)
{
  TpProxyPendingCall *self = p;
  DEBUG ("%p", self);

  g_return_if_fail (self->priv == pending_call_magic);

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (TP_PROXY (self->proxy),
          tp_proxy_pending_call_proxy_invalidated, self);
      g_object_unref (self->proxy);
      self->proxy = NULL;
    }

  if (self->error != NULL)
    g_error_free (self->error);

  if (self->args != NULL)
    g_value_array_free (self->args);

  g_free (self->member);

  if (self->destroy != NULL)
    self->destroy (self->user_data);

  if (self->weak_object != NULL)
    g_object_weak_unref (self->weak_object,
        tp_proxy_pending_call_lost_weak_ref, self);

  g_slice_free (TpProxyPendingCall, self);
}

/**
 * tp_proxy_pending_call_v0_completed:
 * @p: a #TpProxyPendingCall allocated with tp_proxy_pending_call_new()
 *
 * Indicate that dbus-glib has finished with this pending call, and therefore
 * either tp_proxy_pending_call_v0_take_results() has already been called,
 * or it will never be called. See tp_proxy_pending_call_v0_new().
 *
 * The signature is chosen to match #GDestroyNotify.
 *
 * This function is for use by #TpProxy subclass implementations only.
 */
void
tp_proxy_pending_call_v0_completed (gpointer p)
{
  TpProxyPendingCall *self = p;

  DEBUG ("%p", self);

  g_return_if_fail (self->priv == pending_call_magic);

  if (self->idle_source != 0)
    {
      /* we've kicked off an idle function, so we don't want to die until
       * that function runs */
      return;
    }

  if (self->proxy != NULL)
    {
      /* Annoyingly, dbus-glib frees us *before* it emits destroy. If we
       * haven't already finished or been cancelled, then that must be what's
       * happening to us. So, we have to force the TpProxy to invalidate
       * itself slightly early, in order to get our error callback.
       * tp_proxy_invalidate is safe against repeated calls, so this is fine -
       * the 'destroy' signal will call it again, which will basically be
       * ignored. */
      if (self->callback != NULL)
        {
          DEBUG ("Looks like this pending call hasn't finished, assuming "
              "the DBusGProxy is about to die");
          tp_proxy_invalidated (self->proxy, &tp_proxy_dgproxy_destroyed);
          g_assert (self->callback == NULL);
        }
    }

  tp_proxy_pending_call_free (self);
}

/**
 * tp_proxy_pending_call_v0_take_pending_call:
 * @self: A pending call on which this function has not yet been called
 * @pending_call: The underlying dbus-glib pending call
 *
 * Set the underlying pending call to be used by this object.
 * See also tp_proxy_pending_call_v0_new().
 *
 * This method should only be called from #TpProxy subclass implementations.
 */
void
tp_proxy_pending_call_v0_take_pending_call (TpProxyPendingCall *self,
                                            DBusGProxyCall *pending_call)
{
  DEBUG ("%p: %p", self, pending_call);

  g_return_if_fail (self->priv == pending_call_magic);
  self->pending_call = pending_call;
}

static gboolean
tp_proxy_pending_call_idle_invoke (gpointer p)
{
  TpProxyPendingCall *self = p;
  GCallback callback = self->callback;

  DEBUG ("%p", self);

  g_return_val_if_fail (self->invoke_callback != NULL, FALSE);
  g_return_val_if_fail (self->callback != NULL, FALSE);

  self->callback = NULL;
  self->invoke_callback (self->proxy, self->error, self->args, callback,
      self->user_data, self->weak_object);
  self->error = NULL;
  self->args = NULL;

  /* don't clear self->idle_source here! tp_proxy_pending_call_v0_completed
   * compares it to 0 to determine whether to free the object */

  return FALSE;
}

/**
 * tp_proxy_pending_call_v0_take_results:
 * @self: A pending call on which this function has not yet been called
 * @error: %NULL if the call was successful, or an error (whose ownership
 *  is taken over by the pending call object)
 * @args: %NULL if the call failed or had no "out" arguments, or an array
 *  of "out" arguments
 *
 * Set the "out" arguments (return values) from this pending call.
 * See also tp_proxy_pending_call_v0_new().
 *
 * This method should only be called from #TpProxy subclass implementations.
 */
void
tp_proxy_pending_call_v0_take_results (TpProxyPendingCall *self,
                                       GError *error,
                                       GValueArray *args)
{
  if (error == NULL)
    DEBUG ("%p: success, %d args", self, args == NULL ? 0 : args->n_values);
  else
    DEBUG ("%p: error, %s", self, error->message);

  g_return_if_fail (self->priv == pending_call_magic);
  g_return_if_fail (self->args == NULL);
  g_return_if_fail (self->error == NULL);
  g_return_if_fail (self->idle_source == 0);

  self->error = error;
  /* the ordering here means that error + args => assert or raise error */
  g_return_if_fail (error == NULL || args == NULL);
  self->args = args;

  /* queue up the actual callback to run after we go back to the event loop */
  self->idle_source = g_idle_add_full (G_PRIORITY_HIGH,
      tp_proxy_pending_call_idle_invoke, self, tp_proxy_pending_call_free);
}

static void
tp_proxy_signal_connection_lost_weak_ref (gpointer data,
                                          GObject *dead)
{
  TpProxySignalConnection *self = data;

  DEBUG ("%p: lost weak ref to %p", self, dead);

  g_assert (self->priv == signal_conn_magic);
  g_assert (dead == self->weak_object);

  self->weak_object = NULL;
  tp_proxy_signal_connection_disconnect (self);
}

static void
tp_proxy_signal_connection_proxy_invalidated (TpProxy *proxy,
                                              const GError *why,
                                              TpProxySignalConnection *self)
{
  g_assert (self != NULL);
  g_assert (why != NULL);
  DEBUG ("%p: proxy %p invalidated (I have %p): %s", self, proxy, self->proxy,
      why->message);
  g_assert (proxy == self->proxy);

  tp_proxy_signal_connection_disconnect (self);
}

static void
tp_proxy_signal_connection_lost_proxy (gpointer data,
                                       GObject *dead)
{
  TpProxySignalConnection *self = data;
  TpProxy *proxy = TP_PROXY (dead);

  g_assert (self != NULL);
  DEBUG ("%p: lost proxy %p (I have %p)", self, proxy, self->proxy);
  g_assert (proxy == self->proxy);

  self->proxy = NULL;
  tp_proxy_signal_connection_disconnect (self);
}

/**
 * tp_proxy_signal_connection_new:
 * @self: a proxy
 * @interface: a quark whose string value is the D-Bus interface
 * @member: the name of the signal to which we're connecting
 * @callback: a callback to be called when the signal is received
 * @user_data: user-supplied data for the callback
 * @destroy: user-supplied destructor for the data, which will be called
 *   when the signal connection is disconnected for any reason
 * @weak_object: if not %NULL, a #GObject which will be weakly referenced by
 *   the signal connection - if it is destroyed, the signal connection will
 *   automatically be disconnected
 * @impl_callback: the internal callback from a #TpProxy subclass given to
 *   dbus-glib, used to cancel the signal connection
 *
 * Allocate a new structure representing a signal connection. The
 * public members are set from the arguments.
 *
 * This function is for use by #TpProxy subclass implementations only.
 *
 * Returns: a signal connection structure to be freed with
 *  tp_proxy_signal_connection_free_closure().
 */
TpProxySignalConnection *
tp_proxy_signal_connection_new (TpProxy *self,
                                GQuark interface,
                                const gchar *member,
                                GCallback callback,
                                gpointer user_data,
                                GDestroyNotify destroy,
                                GObject *weak_object,
                                GCallback impl_callback)
{
  TpProxySignalConnection *ret = g_slice_new (TpProxySignalConnection);

  DEBUG ("(proxy=%p, if=%s, sig=%s, cb=%p, ud=%p, dn=%p, wo=%p) -> %p",
      self, g_quark_to_string (interface), member, callback, user_data,
      destroy, weak_object, ret);

  ret->proxy = self;
  ret->interface = interface;
  ret->member = g_strdup (member);
  ret->callback = callback;
  ret->user_data = user_data;
  ret->destroy = destroy;
  ret->weak_object = weak_object;
  ret->impl_callback = impl_callback;
  ret->priv = signal_conn_magic;

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_signal_connection_lost_weak_ref,
        ret);

  g_signal_connect (self, "destroyed",
      G_CALLBACK (tp_proxy_signal_connection_proxy_invalidated), ret);

  g_object_weak_ref ((GObject *) self,
      tp_proxy_signal_connection_lost_proxy, ret);

  return ret;
}

/**
 * tp_proxy_signal_connection_disconnect:
 * @self: a signal connection
 *
 * Disconnect the given signal connection. After this function returns, you
 * must not assume that the signal connection remains valid, but you must not
 * explicitly free it either.
 */
void
tp_proxy_signal_connection_disconnect (TpProxySignalConnection *self)
{
  DBusGProxy *iface;

  DEBUG ("%p", self);

  g_return_if_fail (self->priv == signal_conn_magic);

  if (self->proxy == NULL)
    return;

  iface = g_datalist_id_get_data (&self->proxy->priv->interfaces,
      self->interface);

  if (iface == NULL)
    return;

  dbus_g_proxy_disconnect_signal (iface, self->member, self->impl_callback,
      (gpointer) self);
}

/**
 * tp_proxy_signal_connection_free_closure:
 * @self: a #TpProxySignalConnection
 * @unused: not used
 *
 * Free a signal connection allocated with tp_proxy_signal_connection_new().
 * The signature of this function is chosen to make it match #GClosureNotify.
 *
 * This function is for use by #TpProxy subclass implementations only.
 */
void
tp_proxy_signal_connection_free_closure (gpointer self,
                                         GClosure *unused)
{
  TpProxySignalConnection *data = self;

  DEBUG ("%p", self);

  g_return_if_fail (data->priv == signal_conn_magic);

  if (data->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (data->proxy,
          tp_proxy_signal_connection_proxy_invalidated, self);
      g_object_weak_unref ((GObject *) data->proxy,
          tp_proxy_signal_connection_lost_proxy, data);
      data->proxy = NULL;
    }

  if (data->destroy != NULL)
    data->destroy (data->user_data);

  data->destroy = NULL;
  data->user_data = NULL;

  if (data->weak_object != NULL)
    {
      g_object_weak_unref (data->weak_object,
          tp_proxy_signal_connection_lost_weak_ref, data);
      data->weak_object = NULL;
    }

  g_free (data->member);

  g_slice_free (TpProxySignalConnection, data);
}

/**
 * tp_proxy_signal_connection_get_callback:
 * @self: The pending call
 * @proxy_out: Used to return the proxy on which the call was made
 * @user_data_out: Used to return the user-supplied data
 * @weak_object_out: Used to return the user-supplied object
 *
 * Return the callback to be called when this signal is received.
 * This method should only be called from #TpProxy subclass implementations.
 *
 * The other arguments are used to retrieve things that must be passed to the
 * callback. They output "borrowed" references.
 *
 * Returns: the callback for this pending call
 */
GCallback
tp_proxy_signal_connection_get_callback (TpProxySignalConnection *self,
                                         TpProxy **proxy_out,
                                         gpointer *user_data_out,
                                         GObject **weak_object_out)
{
  if (proxy_out != NULL)
    *proxy_out = self->proxy;

  if (user_data_out != NULL)
    *user_data_out = self->user_data;

  if (weak_object_out != NULL)
    *weak_object_out = self->weak_object;

  return self->callback;
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
    case PROP_DBUS_DAEMON:
      if (TP_IS_DBUS_DAEMON (self))
        {
          g_value_set_object (value, self);
        }
      else
        {
          g_value_set_object (value, self->dbus_daemon);
        }
      break;
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
    case PROP_DBUS_DAEMON:
      if (TP_IS_DBUS_DAEMON (self))
        {
          g_assert (g_value_get_object (value) == NULL);
        }
      else
        {
          TpProxy *daemon_as_proxy = g_value_get_object (value);

          g_assert (self->dbus_daemon == NULL);

          self->dbus_daemon = g_value_dup_object (value);

          if (daemon_as_proxy != NULL)
            {
              g_assert (self->dbus_connection == NULL ||
                  self->dbus_connection == daemon_as_proxy->dbus_connection);

              if (self->dbus_connection == NULL)
                self->dbus_connection =
                    dbus_g_connection_ref (daemon_as_proxy->dbus_connection);
            }
        }
      break;
    case PROP_DBUS_CONNECTION:
        {
          DBusGConnection *conn = g_value_get_boxed (value);

          /* if we're given a NULL dbus-connection, but we've got a
           * DBusGConnection from the dbus-daemon, we want to keep it */
          if (conn == NULL)
            return;

          if (self->dbus_connection == NULL)
            self->dbus_connection = g_value_dup_boxed (value);

          g_assert (self->dbus_connection == g_value_get_boxed (value));
        }
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

  _tp_register_dbus_glib_marshallers ();

  if (klass->priv != NULL)
    {
      GSList *iter;

      for (iter = klass->priv;
           iter != NULL;
           iter = iter->next)
        g_signal_connect (self, "interface-added", G_CALLBACK (iter->data),
            NULL);
    }

  g_return_val_if_fail (self->dbus_connection != NULL, NULL);
  g_return_val_if_fail (self->object_path != NULL, NULL);
  g_return_val_if_fail (self->bus_name != NULL, NULL);

  g_return_val_if_fail (tp_dbus_check_valid_object_path (self->object_path,
        NULL), NULL);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (self->bus_name,
        TP_DBUS_NAME_TYPE_ANY, NULL), NULL);

  if (klass->interface != 0)
    {
      tp_proxy_add_interface_by_id (self, klass->interface);
    }

  /* Some interfaces are stateful, so we only allow binding to a unique
   * name, like in dbus_g_proxy_new_for_name_owner() */
  if (klass->must_have_unique_name)
    {
      g_return_val_if_fail (self->bus_name[0] == ':', NULL);
    }

  self->priv->valid = TRUE;

  return (GObject *) self;
}

static void
tp_proxy_dispose (GObject *object)
{
  TpProxy *self = TP_PROXY (object);
  GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "Proxy unreferenced" };

  if (self->priv->dispose_has_run)
    return;
  self->priv->dispose_has_run = TRUE;

  tp_proxy_invalidated (self, &e);

  G_OBJECT_CLASS (tp_proxy_parent_class)->dispose (object);
}

static void
tp_proxy_finalize (GObject *object)
{
  TpProxy *self = TP_PROXY (object);

  g_free (self->bus_name);
  g_free (self->object_path);

  G_OBJECT_CLASS (tp_proxy_parent_class)->finalize (object);
}

/**
 * tp_proxy_class_hook_on_interface_add:
 * @klass: A subclass of TpProxyClass
 * @callback: A signal handler for #TpProxy::interface-added
 *
 * Arrange for @callback to be connected to #TpProxy::interface-added
 * during the #TpProxy constructor. This is done sufficiently early that
 * it will see the signal for the default interface (@interface member of
 * #TpProxyClass), if any, being added.
 */
void
tp_proxy_class_hook_on_interface_add (TpProxyClass *klass,
                                      TpProxyInterfaceAddedCb callback)
{
  klass->priv = g_slist_prepend (klass->priv, callback);
}

static void
tp_proxy_class_init (TpProxyClass *klass)
{
  GParamSpec *param_spec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Initialize our global GError if needed */
  if (tp_proxy_dgproxy_destroyed.domain == 0)
    {
      tp_proxy_dgproxy_destroyed.domain = DBUS_GERROR;
      tp_proxy_dgproxy_destroyed.code = DBUS_GERROR_NAME_HAS_NO_OWNER;
      tp_proxy_dgproxy_destroyed.message =
          "Name owner lost (service crashed?)";
    }

  g_type_class_add_private (klass, sizeof (TpProxyPrivate));

  object_class->constructor = tp_proxy_constructor;
  object_class->get_property = tp_proxy_get_property;
  object_class->set_property = tp_proxy_set_property;
  object_class->dispose = tp_proxy_dispose;
  object_class->finalize = tp_proxy_finalize;

  tp_proxy_class_hook_on_interface_add (klass, tp_cli_generic_add_signals);

  /**
   * TpProxy:dbus-daemon:
   *
   * The D-Bus daemon for this object (this object itself, if it is a
   * TpDBusDaemon). Read-only except during construction.
   */
  param_spec = g_param_spec_object ("dbus-daemon", "D-Bus daemon",
      "The D-Bus daemon used by this object, or this object itself if it's "
      "a TpDBusDaemon", TP_TYPE_DBUS_DAEMON,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_DBUS_DAEMON,
      param_spec);

  /**
   * TpProxy:dbus-connection:
   *
   * The D-Bus connection for this object. Read-only except during
   * construction.
   */
  param_spec = g_param_spec_boxed ("dbus-connection", "D-Bus connection",
      "The D-Bus connection used by this object", DBUS_TYPE_G_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
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
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
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
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      param_spec);

  /**
   * TpProxy::interface-added:
   * @self: the proxy object
   * @id: the GQuark representing the interface
   * @proxy: the dbus-glib proxy representing the interface
   *
   * Emitted when this proxy has gained an interface.
   */
  signals[SIGNAL_INTERFACE_ADDED] = g_signal_new ("interface-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__UINT_OBJECT,
      G_TYPE_NONE, 2, G_TYPE_UINT, DBUS_TYPE_G_PROXY);

  /**
   * TpProxy::destroyed:
   * @self: the proxy object
   * @error: a GError indicating why this proxy was destroyed
   *
   * Emitted when this proxy has been destroyed and become invalid for
   * whatever reason. Any more specific signal should be emitted first.
   */
  signals[SIGNAL_DESTROYED] = g_signal_new ("destroyed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__POINTER,
      G_TYPE_NONE, 1, G_TYPE_POINTER);
}
