/*
 * proxy.c - Base class for Telepathy client proxies
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#if 0
#define MORE_DEBUG DEBUG
#else
#define MORE_DEBUG(...) G_STMT_START {} G_STMT_END
#endif

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
    /* set to NULL after it's been invoked once, so we can assert that it
     * doesn't get called again */
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

typedef struct _TpProxySignalInvocation TpProxySignalInvocation;

struct _TpProxySignalInvocation {
    TpProxySignalConnection *sc;
    GValueArray *args;
    guint idle_source;
};

struct _TpProxySignalConnection {
    TpProxy *proxy;
    GQuark interface;
    gchar *member;
    /* this is NULL if and only if dbus-glib has dropped us */
    GCallback collect_args;
    TpProxyInvokeFunc invoke_callback;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    /* queue of _TpProxySignalInvocation */
    GQueue invocations;
    gconstpointer priv;
};

struct _TpProxyPrivate {
    /* GQuark for interface => ref'd DBusGProxy * */
    GData *interfaces;

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

  proxy = g_datalist_id_get_data (&self->priv->interfaces, interface);

  if (proxy != NULL)
    {
      return proxy;
    }

  if (self->invalidated != NULL)
    {
      g_set_error (error, self->invalidated->domain, self->invalidated->code,
          "%s", self->invalidated->message);
    }
  else
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Object %s does not have interface %s",
          self->object_path, g_quark_to_string (interface));
    }

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

/* This signature is chosen to match GSourceFunc */
static gboolean
tp_proxy_emit_invalidated (gpointer p)
{
  TpProxy *self = p;

  g_signal_emit (self, signals[SIGNAL_DESTROYED], 0, self->invalidated);

  /* Don't clear the datalist until after we've emitted the signal, so
   * the pending call and signal connection friend classes can still get
   * to the proxies */
  g_datalist_clear (&self->priv->interfaces);

  return FALSE;
}

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

      tp_proxy_emit_invalidated (self);
    }

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

  /* We need to be able to delay emitting the destroyed signal, so that
   * any queued-up method calls and signal handlers will run first, and so
   * it doesn't try to reenter libdbus.
   */
  if (self->invalidated == NULL)
    {
      DEBUG ("%p", self);
      self->invalidated = g_error_copy (&tp_proxy_dgproxy_destroyed);

      g_idle_add_full (G_PRIORITY_HIGH, tp_proxy_emit_invalidated,
          g_object_ref (self), g_object_unref);
    }

  /* this won't re-emit 'destroyed' because we already set self->invalidated */
  tp_proxy_invalidated (self, self->invalidated);
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
  TpProxyInvokeFunc invoke = self->invoke_callback;

  g_assert (self != NULL);
  g_assert (why != NULL);
  DEBUG ("%p: proxy %p invalidated (I have %p): %s", self, proxy, self->proxy,
      why->message);
  g_assert (proxy == self->proxy);

  if (self->idle_source != 0)
    {
      g_source_remove (self->idle_source);
    }

  if (invoke != NULL)
    {
      self->invoke_callback = NULL;
      invoke (proxy, g_error_copy (why), NULL, self->callback,
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
  TpProxyPendingCall *ret;

  g_return_val_if_fail (invoke_callback != NULL, NULL);

  ret = g_slice_new0 (TpProxyPendingCall);

  MORE_DEBUG ("(proxy=%p, if=%s, meth=%s, ic=%p; cb=%p, ud=%p, dn=%p, wo=%p)"
      " -> %p", self, g_quark_to_string (interface), member, invoke_callback,
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
  self->invoke_callback = NULL;

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

  g_return_if_fail (self->priv == pending_call_magic);

  if (self->idle_source != 0)
    {
      /* we've kicked off an idle function, so we don't want to die until
       * that function runs */
      return;
    }

  if (self->proxy != NULL)
    {
      /* dbus-glib frees its user_data *before* it emits destroy; if we
       * haven't yet run the callback, assume that's what's going on. */
      if (self->invoke_callback != NULL)
        {
          MORE_DEBUG ("Looks like this pending call hasn't finished, assuming "
              "the DBusGProxy is about to die");
          tp_proxy_pending_call_proxy_invalidated (self->proxy,
              &tp_proxy_dgproxy_destroyed, self);
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
  g_return_if_fail (self->priv == pending_call_magic);
  self->pending_call = pending_call;
}

static gboolean
tp_proxy_pending_call_idle_invoke (gpointer p)
{
  TpProxyPendingCall *self = p;
  TpProxyInvokeFunc invoke = self->invoke_callback;

  g_return_val_if_fail (self->invoke_callback != NULL, FALSE);

  self->invoke_callback = NULL;
  invoke (self->proxy, self->error, self->args, self->callback,
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
tp_proxy_signal_connection_disconnect_dbus_glib (TpProxySignalConnection *self)
{
  DBusGProxy *iface;

  g_assert (self->priv == signal_conn_magic);
  /* we can't be disconnecting like this unless dbus-glib still knows about
   * self */
  g_assert (self->collect_args != NULL);

  if (self->proxy == NULL)
    return;

  iface = g_datalist_id_get_data (&self->proxy->priv->interfaces,
      self->interface);

  if (iface == NULL)
    return;

  dbus_g_proxy_disconnect_signal (iface, self->member, self->collect_args,
      (gpointer) self);
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

  tp_proxy_signal_connection_disconnect_dbus_glib (self);
}

static void
tp_proxy_signal_connection_lost_proxy (gpointer data,
                                       GObject *dead)
{
  TpProxySignalConnection *self = data;
  TpProxy *proxy = TP_PROXY (dead);

  g_assert (self != NULL);
  g_assert (self->invocations.length == 0);

  DEBUG ("%p: lost proxy %p (I have %p)", self, proxy, self->proxy);
  g_assert (proxy == self->proxy);

  self->proxy = NULL;
  tp_proxy_signal_connection_disconnect_dbus_glib (self);
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
tp_proxy_signal_connection_free (TpProxySignalConnection *self)
{
  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->proxy,
          tp_proxy_signal_connection_proxy_invalidated, self);
      g_object_weak_unref ((GObject *) self->proxy,
          tp_proxy_signal_connection_lost_proxy, self);
      self->proxy = NULL;
    }

  if (self->destroy != NULL)
    self->destroy (self->user_data);

  self->destroy = NULL;
  self->user_data = NULL;

  if (self->weak_object != NULL)
    {
      g_object_weak_unref (self->weak_object,
          tp_proxy_signal_connection_lost_weak_ref, self);
      self->weak_object = NULL;
    }

  g_free (self->member);

  g_slice_free (TpProxySignalConnection, self);
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
  TpProxySignalInvocation *invocation;

  g_return_if_fail (self->priv == signal_conn_magic);

  while ((invocation = g_queue_pop_head (&self->invocations)) != NULL)
    {
      g_source_remove (invocation->idle_source);
      g_assert (invocation->sc == self);
      g_object_unref (invocation->sc->proxy);
      invocation->sc = NULL;
    }

  if (self->collect_args == NULL)
    {
      /* indicates that tp_proxy_signal_connection_dropped has run -
       * so now there are no more pending callback invocations, the signal
       * connection falls off. */
      tp_proxy_signal_connection_free (self);

      return;
    }

  tp_proxy_signal_connection_disconnect_dbus_glib (self);
}

static void
tp_proxy_signal_invocation_free (gpointer p)
{
  TpProxySignalInvocation *self = p;

  if (self->sc != NULL)
    {
      /* this shouldn't really happen - it'll get run if the idle source
       * is removed by something other than t_p_s_c_disconnect or
       * t_p_s_i_run */
      g_warning ("%s: idle source removed by someone else", G_STRFUNC);

      g_queue_remove (&self->sc->invocations, self);

      /* there's one ref to the proxy per queued invocation, to keep it
       * alive */
      g_object_unref (self->sc->proxy);
    }

  if (self->args != NULL)
    g_value_array_free (self->args);

  g_slice_free (TpProxySignalInvocation, self);
}

static gboolean
tp_proxy_signal_invocation_run (gpointer p)
{
  TpProxySignalInvocation *self = p;
  TpProxySignalInvocation *popped = g_queue_pop_head (&self->sc->invocations);

  /* if GLib is running idle handlers in the wrong order, then we've lost */
  g_assert (popped == self);

  self->sc->invoke_callback (self->sc->proxy, NULL, self->args,
      self->sc->callback, self->sc->user_data, self->sc->weak_object);

  /* the invoke callback steals args */
  self->args = NULL;

  if (self->sc->collect_args == NULL && self->sc->invocations.length == 0)
    {
      /* indicates that tp_proxy_signal_connection_dropped has run and there
       * are no more pending callback invocations - so the signal connection
       * falls off */
      tp_proxy_signal_connection_free (self->sc);
    }

  /* there's one ref to the proxy per queued invocation, to keep it
   * alive */
  g_object_unref (self->sc->proxy);

  /* avoid the bits of tp_proxy_signal_invocation_free that rely on self->sc
   * still existing */
  self->sc = NULL;

  return FALSE;
}

static void
tp_proxy_signal_connection_dropped (gpointer p,
                                    GClosure *unused)
{
  TpProxySignalConnection *self = p;

  DEBUG ("%p", self);

  g_return_if_fail (self->priv == signal_conn_magic);

  if (self->invocations.length > 0)
    {
      /* indicate that we're no longer registered with dbus-glib */
      self->collect_args = NULL;
      return;
    }

  tp_proxy_signal_connection_free (self);
}

static void
collect_none (DBusGProxy *proxy, TpProxySignalConnection *sc)
{
  tp_proxy_signal_connection_v0_take_results (sc, NULL);
}

/**
 * tp_proxy_signal_connection_v0_new:
 * @self: a proxy
 * @interface: a quark whose string value is the D-Bus interface
 * @member: the name of the signal to which we're connecting
 * @expected_types: an array of expected GTypes for the arguments, terminated
 *  by %G_TYPE_INVALID
 * @collect_args: a callback to be given to dbus_g_proxy_connect_signal(),
 *  which must marshal the arguments into a #GValueArray and use them to call
 *  tp_proxy_signal_connection_v0_take_results(); this callback is not
 *  guaranteed to be called by future versions of telepathy-glib, which might
 *  be able to implement its functionality internally. If no arguments are
 *  expected at all (expected_types = { G_TYPE_INVALID }) then this callback
 *  should instead be %NULL
 * @invoke_callback: a function which will be called with @error = %NULL,
 *  which should invoke @callback with @user_data, @weak_object and other
 *  appropriate arguments taken from @args
 * @callback: user callback to be invoked by @invoke_callback
 * @user_data: user-supplied data for the callback
 * @destroy: user-supplied destructor for the data, which will be called
 *   when the signal connection is disconnected for any reason,
 *   or will be called before this function returns if an error occurs
 * @weak_object: if not %NULL, a #GObject which will be weakly referenced by
 *   the signal connection - if it is destroyed, the signal connection will
 *   automatically be disconnected
 *
 * Allocate a new structure representing a signal connection. The
 * public members are set from the arguments.
 *
 * This function is for use by #TpProxy subclass implementations only.
 *
 * Returns: a signal connection structure
 */
TpProxySignalConnection *
tp_proxy_signal_connection_v0_new (TpProxy *self,
                                   GQuark interface,
                                   const gchar *member,
                                   const GType *expected_types,
                                   GCallback collect_args,
                                   TpProxyInvokeFunc invoke_callback,
                                   GCallback callback,
                                   gpointer user_data,
                                   GDestroyNotify destroy,
                                   GObject *weak_object)
{
  TpProxySignalConnection *ret;
  DBusGProxy *iface = tp_proxy_borrow_interface_by_id (self,
      interface, NULL);

  if (iface == NULL)
    {
      DEBUG ("Proxy already invalidated - not connecting to signal %s",
          member);

      if (destroy != NULL)
        destroy (user_data);

      return NULL;
    }

  if (expected_types[0] == G_TYPE_INVALID)
    {
      collect_args = G_CALLBACK (collect_none);
    }
  else
    {
      g_return_val_if_fail (collect_args != NULL, NULL);
    }

  ret = g_slice_new0 (TpProxySignalConnection);

  MORE_DEBUG ("(proxy=%p, if=%s, sig=%s, collect=%p, invoke=%p, "
      "cb=%p, ud=%p, dn=%p, wo=%p) -> %p",
      self, g_quark_to_string (interface), member, collect_args,
      invoke_callback, callback, user_data, destroy, weak_object, ret);

  ret->proxy = self;
  ret->interface = interface;
  ret->member = g_strdup (member);
  ret->collect_args = collect_args;
  ret->invoke_callback = invoke_callback;
  ret->callback = callback;
  ret->user_data = user_data;
  ret->destroy = destroy;
  ret->weak_object = weak_object;
  ret->priv = signal_conn_magic;

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_signal_connection_lost_weak_ref,
        ret);

  g_signal_connect (self, "destroyed",
      G_CALLBACK (tp_proxy_signal_connection_proxy_invalidated), ret);

  g_object_weak_ref ((GObject *) self,
      tp_proxy_signal_connection_lost_proxy, ret);

  dbus_g_proxy_connect_signal (iface, member, collect_args, ret,
      tp_proxy_signal_connection_dropped);

  return ret;
}

/**
 * tp_proxy_signal_connection_v0_take_results:
 * @self: The signal connection
 * @args: The arguments of the signal
 *
 * Feed the results of a signal invocation back into the signal connection
 * machinery.
 *
 * This method should only be called from #TpProxy subclass implementations,
 * in the callback that implements @collect_args.
 */
void
tp_proxy_signal_connection_v0_take_results (TpProxySignalConnection *self,
                                            GValueArray *args)
{
  TpProxySignalInvocation *invocation = g_slice_new0 (TpProxySignalInvocation);
  /* FIXME: assert that the GValueArray is the right length, or
   * even that it contains the right types? */

  /* as long as there are queued invocations, we keep one ref to the TpProxy
   * per invocation, to stop it dying */
  g_object_ref (self->proxy);
  invocation->sc = self;
  invocation->args = args;

  g_queue_push_tail (&self->invocations, invocation);

  invocation->idle_source = g_idle_add_full (G_PRIORITY_HIGH,
      tp_proxy_signal_invocation_run, invocation,
      tp_proxy_signal_invocation_free);
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
