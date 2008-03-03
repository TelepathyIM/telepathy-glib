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

#include <string.h>

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
 * TP_DBUS_ERRORS:
 *
 * #GError domain representing D-Bus errors not directly related to
 * Telepathy, for use by #TpProxy. The @code in a #GError with this
 * domain must be a member of #TpDBusError.
 *
 * This macro expands to a function call returning a #GQuark.
 *
 * Since: 0.7.1
 */
GQuark
tp_dbus_errors_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string ("tp_dbus_errors_quark");

  return q;
}

/**
 * TpDBusError:
 * @TP_DBUS_ERROR_UNKNOWN_REMOTE_ERROR: Raised if the error raised by
 *  a remote D-Bus object is not recognised
 * @TP_DBUS_ERROR_PROXY_UNREFERENCED: Emitted in #TpProxy:invalidated
 *  when the #TpProxy has lost its last reference
 * @TP_DBUS_ERROR_NO_INTERFACE: Raised by #TpProxy methods if the remote
 *  object does not appear to have the required interface
 * @TP_DBUS_ERROR_NAME_OWNER_LOST: Emitted in #TpProxy:invalidated if the
 *  remote process loses ownership of its bus name, and raised by
 *  any #TpProxy methods that have not had a reply at that time or are called
 *  after the proxy becomes invalid in this way (usually meaning it crashed)
 * @TP_DBUS_ERROR_INVALID_BUS_NAME: Raised if a D-Bus bus name given is not
 *  valid, or is of an unacceptable type (e.g. well-known vs. unique)
 * @TP_DBUS_ERROR_INVALID_INTERFACE_NAME: Raised if a D-Bus interface or
 *  error name given is not valid
 * @TP_DBUS_ERROR_INVALID_OBJECT_PATH: Raised if a D-Bus object path
 *  given is not valid
 * @TP_DBUS_ERROR_INVALID_MEMBER_NAME: Raised if a D-Bus method or signal
 *  name given is not valid
 * @TP_DBUS_ERROR_OBJECT_REMOVED: A generic error which can be used with
 *  #TpProxy:invalidated to indicate an application-specific indication
 *  that the remote object no longer exists, if no more specific error
 *  is available.
 * @TP_DBUS_ERROR_CANCELLED: Raised from calls that re-enter the main
 *  loop (*_run_*) if they are cancelled
 * @NUM_TP_DBUS_ERRORS: 1 more than the highest valid #TpDBusError
 *
 * #GError codes for use with the %TP_DBUS_ERRORS domain.
 *
 * Since: 0.7.1
 */

/**
 * SECTION:proxy
 * @title: TpProxy
 * @short_description: base class for Telepathy client proxy objects
 * @see_also: #TpChannel, #TpConnection, #TpConnectionManager
 *
 * #TpProxy is a base class for Telepathy client-side proxies, which represent
 * an object accessed via D-Bus and provide access to its methods and signals.
 *
 * Since: 0.7.1
 */

/**
 * SECTION:proxy-dbus-core
 * @title: TpProxy D-Bus core methods
 * @short_description: The D-Bus Introspectable, Peer and Properties interfaces
 * @see_also: #TpProxy
 *
 * All D-Bus objects support the Peer interface, and many support the
 * Introspectable and Properties interfaces.
 *
 * Since: 0.7.2
 */

/**
 * SECTION:proxy-tp-properties
 * @title: TpProxy Telepathy Properties
 * @short_description: The Telepathy Properties interface
 * @see_also: #TpProxy
 *
 * As well as #TpProxy, proxy.h includes auto-generated client wrappers for the
 * Telepathy Properties interface, which can be implemented by any type of
 * object.
 *
 * The Telepathy Properties interface should not be confused with the D-Bus
 * core Properties interface.
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
 *
 * Since: 0.7.1
 */

/**
 * TpProxyInterfaceAddedCb:
 * @self: the proxy
 * @quark: a quark whose string value is the interface being added
 * @proxy: the #DBusGProxy for the added interface
 * @unused: unused
 *
 * The signature of a #TpProxy::interface-added signal callback.
 *
 * Since: 0.7.1
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
 *
 * Since: 0.7.1
 */

typedef struct _TpProxyErrorMappingLink TpProxyErrorMappingLink;

struct _TpProxyErrorMappingLink {
    const gchar *prefix;
    GQuark domain;
    GEnumClass *code_enum_class;
    TpProxyErrorMappingLink *next;
};

typedef struct _TpProxyInterfaceAddLink TpProxyInterfaceAddLink;

struct _TpProxyInterfaceAddLink {
    TpProxyInterfaceAddedCb callback;
    TpProxyInterfaceAddLink *next;
};

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
 *
 * Since: 0.7.1
 */

/**
 * TpProxyPendingCall:
 *
 * Opaque structure representing a pending D-Bus call.
 *
 * Since: 0.7.1
 */

struct _TpProxyPendingCall {
    /* This structure's "reference count" is implicit:
     * - 1 if D-Bus has us
     * - 1 if results have come in but we haven't run the callback yet
     *   (idle_source is nonzero)
     */

    TpProxy *proxy;
    GQuark interface;
    gchar *member;

    /* Set to NULL after it's been invoked once, so we can assert that it
     * doesn't get called again. Supplied by the generated code */
    TpProxyInvokeFunc invoke_callback;

    /* dbus-glib-supplied arguments for invoke_callback */
    GError *error;
    GValueArray *args;

    /* user-supplied arguments for invoke_callback */
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;

    DBusGProxy *iface_proxy;
    DBusGProxyCall *pending_call;

    /* If nonzero, we have results (either args or error) and have queued
     * up tp_proxy_pending_call_idle_invoke (after which
     * tp_proxy_pending_call_free will run) */
    guint idle_source;

    gboolean cancel_must_raise:1;
    gconstpointer priv;
};

/**
 * TpProxySignalConnection:
 *
 * Opaque structure representing a D-Bus signal connection.
 *
 * Since: 0.7.1
 */

typedef struct _TpProxySignalInvocation TpProxySignalInvocation;

struct _TpProxySignalInvocation {
    TpProxySignalConnection *sc;
    GValueArray *args;
    guint idle_source;
};

struct _TpProxySignalConnection {
    /* 1 if D-Bus has us, 1 per member of @invocations, 1 per callback
     * being invoked right now */
    gsize refcount;

    /* weak ref + 1 per member of @invocations + 1 per callback
     * being invoked (possibly nested!) right now */
    TpProxy *proxy;

    GQuark interface;
    gchar *member;
    GCallback collect_args;
    TpProxyInvokeFunc invoke_callback;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
    /* queue of _TpProxySignalInvocation, not including any that are
     * being invoked right now */
    GQueue invocations;
};

struct _TpProxyPrivate {
    /* GQuark for interface => either a ref'd DBusGProxy *,
     * or the TpProxy itself used as a dummy value to indicate that
     * the DBusGProxy has not been needed yet */
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
  PROP_INTERFACES,
  N_PROPS
};

enum {
    SIGNAL_INTERFACE_ADDED,
    SIGNAL_INVALIDATED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

static void tp_proxy_iface_destroyed_cb (DBusGProxy *dgproxy, TpProxy *self);

/**
 * tp_proxy_borrow_interface_by_id:
 * @self: the TpProxy
 * @interface: quark representing the interface required
 * @error: used to raise TP_DBUS_ERROR_NO_INTERFACE if this object does not
 * have the required interface
 *
 * <!-- -->
 *
 * Returns: a borrowed reference to a #DBusGProxy
 * for which the bus name and object path are the same as for @self, but the
 * interface is as given (or %NULL if this proxy does not implement it).
 * The reference is only valid as long as @self is.
 *
 * Since: 0.7.1
 */
DBusGProxy *
tp_proxy_borrow_interface_by_id (TpProxy *self,
                                 GQuark interface,
                                 GError **error)
{
  gpointer dgproxy;

  if (!tp_dbus_check_valid_interface_name (g_quark_to_string (interface),
        error))
      return NULL;

  dgproxy = g_datalist_id_get_data (&self->priv->interfaces, interface);

  if (dgproxy == self)
    {
      /* dummy value - we've never actually needed the interface, so we
       * didn't create it, to avoid binding to all the signals */

      dgproxy = dbus_g_proxy_new_for_name (self->dbus_connection,
          self->bus_name, self->object_path, g_quark_to_string (interface));
      DEBUG ("%p: %s DBusGProxy is %p", self, g_quark_to_string (interface),
          dgproxy);

      g_signal_connect (dgproxy, "destroy",
          G_CALLBACK (tp_proxy_iface_destroyed_cb), self);

      g_datalist_id_set_data_full (&self->priv->interfaces, interface,
          dgproxy, g_object_unref);

      g_signal_emit (self, signals[SIGNAL_INTERFACE_ADDED], 0,
          (guint) interface, dgproxy);
    }

  if (dgproxy != NULL)
    {
      return dgproxy;
    }

  if (self->invalidated != NULL)
    {
      g_set_error (error, self->invalidated->domain, self->invalidated->code,
          "%s", self->invalidated->message);
    }
  else
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_NO_INTERFACE,
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
 *
 * Since: 0.7.1
 */
gboolean
tp_proxy_has_interface_by_id (gpointer self,
                              GQuark interface)
{
  TpProxy *proxy = TP_PROXY (self);

  return (g_datalist_id_get_data (&proxy->priv->interfaces, interface)
      != NULL);
}

/**
 * tp_proxy_has_interface:
 * @self: the #TpProxy (or subclass)
 * @interface: the interface required, as a string
 *
 * A macro wrapping tp_proxy_has_interface_by_id(). Returns %TRUE if this
 * proxy implements the given interface.
 *
 * Since: 0.7.1
 */

static void
tp_proxy_lose_interface (GQuark unused,
                         gpointer dgproxy_or_self,
                         gpointer self)
{
  if (dgproxy_or_self != self)
    g_signal_handlers_disconnect_by_func (dgproxy_or_self,
        G_CALLBACK (tp_proxy_iface_destroyed_cb), self);
}

static void
tp_proxy_lose_interfaces (TpProxy *self)
{
  g_datalist_foreach (&self->priv->interfaces,
      tp_proxy_lose_interface, self);

  g_datalist_clear (&self->priv->interfaces);
}

/* This signature is chosen to match GSourceFunc */
static gboolean
tp_proxy_emit_invalidated (gpointer p)
{
  TpProxy *self = p;

  g_signal_emit (self, signals[SIGNAL_INVALIDATED], 0,
      self->invalidated->domain, self->invalidated->code,
      self->invalidated->message);

  /* Don't clear the datalist until after we've emitted the signal, so
   * the pending call and signal connection friend classes can still get
   * to the proxies */
  tp_proxy_lose_interfaces (self);

  return FALSE;
}

/**
 * tp_proxy_invalidate:
 * @self: a proxy
 * @error: an error causing the invalidation
 *
 * Mark @self as having been invalidated - no further calls will work, and
 * if not already invalidated, the #TpProxy:invalidated signal will be emitted
 * with the given error.
 *
 * Since: 0.7.1
 */
void
tp_proxy_invalidate (TpProxy *self, const GError *error)
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

static void
tp_proxy_iface_destroyed_cb (DBusGProxy *dgproxy,
                             TpProxy *self)
{
  /* We can't call any API on the proxy now. Because the proxies are all
   * for the same bus name, we can assume that all of them are equally
   * useless now */
  tp_proxy_lose_interfaces (self);

  /* We need to be able to delay emitting the invalidated signal, so that
   * any queued-up method calls and signal handlers will run first, and so
   * it doesn't try to reenter libdbus.
   */
  if (self->invalidated == NULL)
    {
      DEBUG ("%p", self);
      self->invalidated = g_error_new_literal (TP_DBUS_ERRORS,
          TP_DBUS_ERROR_NAME_OWNER_LOST, "Name owner lost (service crashed?)");

      g_idle_add_full (G_PRIORITY_HIGH, tp_proxy_emit_invalidated,
          g_object_ref (self), g_object_unref);
    }

  /* this won't re-emit 'invalidated' because we already set
   * self->invalidated */
  tp_proxy_invalidate (self, self->invalidated);
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
 * tp_cli_* wrapper functions (strongly recommended).
 *
 * If the interface is the proxy's "main interface", or has already been
 * added, then do nothing.
 *
 * Returns: the borrowed DBusGProxy
 *
 * Since: 0.7.1
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
      /* we don't want to actually create it just yet - dbus-glib will
       * helpfully wake us up on every signal, if we do. So we set a
       * dummy value (self), and replace it with the real value in
       * tp_proxy_borrow_interface_by_id */
      g_datalist_id_set_data_full (&self->priv->interfaces, interface,
          self, NULL);
    }

  return iface_proxy;
}

static GQuark
error_mapping_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string ("TpProxyErrorMappingCb_0.7.1");
    }

  return q;
}

static GError *
tp_proxy_take_and_remap_error (TpProxy *self,
                               GError *error)
{
  if (error == NULL ||
      error->domain != DBUS_GERROR ||
      error->code != DBUS_GERROR_REMOTE_EXCEPTION)
    {
      return error;
    }
  else
    {
      GError *replacement;
      const gchar *dbus = dbus_g_error_get_name (error);
      GType proxy_type = TP_TYPE_PROXY;
      GType type;

      for (type = G_TYPE_FROM_INSTANCE (self);
           type != proxy_type;
           type = g_type_parent (type))
        {
          TpProxyErrorMappingLink *iter;

          for (iter = g_type_get_qdata (type, error_mapping_quark ());
               iter != NULL;
               iter = iter->next)
            {
              size_t prefix_len = strlen (iter->prefix);

              if (!strncmp (dbus, iter->prefix, prefix_len)
                  && dbus[prefix_len] == '.')
                {
                  GEnumValue *code =
                    g_enum_get_value_by_nick (iter->code_enum_class,
                        dbus + prefix_len + 1);

                  if (code != NULL)
                    {
                      replacement = g_error_new_literal (iter->domain,
                          code->value, error->message);
                      g_error_free (error);
                      return replacement;
                    }
                }
            }
        }

      /* we don't have an error mapping - so let's just paste the
       * error name and message into TP_DBUS_ERROR_UNKNOWN_REMOTE_ERROR */
      replacement = g_error_new (TP_DBUS_ERRORS,
          TP_DBUS_ERROR_UNKNOWN_REMOTE_ERROR, "%s: %s", dbus, error->message);
      g_error_free (error);
      return replacement;
    }
}

static const gchar * const pending_call_magic = "TpProxyPendingCall";

static void
tp_proxy_pending_call_lost_weak_ref (gpointer data,
                                     GObject *dead)
{
  TpProxyPendingCall *pc = data;

  DEBUG ("%p lost weak ref to %p", pc, dead);

  g_assert (pc->priv == pending_call_magic);
  g_assert (dead == pc->weak_object);

  pc->weak_object = NULL;
  tp_proxy_pending_call_cancel (pc);
}

static gboolean
tp_proxy_pending_call_idle_invoke (gpointer p)
{
  TpProxyPendingCall *pc = p;
  TpProxyInvokeFunc invoke = pc->invoke_callback;

  g_return_val_if_fail (pc->invoke_callback != NULL, FALSE);

  MORE_DEBUG ("%p: invoking user callback", pc);

  pc->invoke_callback = NULL;
  invoke (pc->proxy, pc->error, pc->args, pc->callback,
      pc->user_data, pc->weak_object);
  pc->error = NULL;
  pc->args = NULL;

  /* don't clear pc->idle_source here! tp_proxy_pending_call_v0_completed
   * compares it to 0 to determine whether to free the object */

  return FALSE;
}

static void tp_proxy_pending_call_free (gpointer p);

static void
tp_proxy_pending_call_proxy_destroyed (DBusGProxy *iface_proxy,
                                       TpProxyPendingCall *pc)
{
  g_assert (iface_proxy != NULL);
  g_assert (pc != NULL);
  g_assert (pc->iface_proxy == iface_proxy);

  DEBUG ("%p: DBusGProxy %p invalidated", pc, iface_proxy);

  if (pc->idle_source == 0)
    {
      /* we haven't already received and queued a reply, so synthesize
       * one */
      g_assert (pc->args == NULL);
      g_assert (pc->error == NULL);

      pc->error = g_error_new_literal (TP_DBUS_ERRORS,
          TP_DBUS_ERROR_NAME_OWNER_LOST, "Name owner lost (service crashed?)");

      pc->idle_source = g_idle_add_full (G_PRIORITY_HIGH,
          tp_proxy_pending_call_idle_invoke, pc, tp_proxy_pending_call_free);
    }

  g_signal_handlers_disconnect_by_func (pc->iface_proxy,
      tp_proxy_pending_call_proxy_destroyed, pc);
  g_object_unref (pc->iface_proxy);
  pc->iface_proxy = NULL;
}

/**
 * tp_proxy_pending_call_v0_new:
 * @self: a proxy
 * @interface: a quark whose string value is the D-Bus interface
 * @member: the name of the method being called
 * @iface_proxy: the interface-specific #DBusGProxy for @interface
 * @invoke_callback: an implementation of #TpProxyInvokeFunc which will
 *  invoke @callback with appropriate arguments
 * @callback: a callback to be called when the call completes
 * @user_data: user-supplied data for the callback
 * @destroy: user-supplied destructor for the data
 * @weak_object: if not %NULL, a #GObject which will be weakly referenced by
 *   the signal connection - if it is destroyed, the pending call will
 *   automatically be cancelled
 * @cancel_must_raise: if %TRUE, the @invoke_callback will be run with
 *  error %TP_DBUS_ERROR_CANCELLED if the call is cancelled by a call to
 *  tp_proxy_pending_call_cancel() or by destruction of the weak_object();
 *  if %FALSE, the @invoke_callback will not be run at all in these cases
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
 *
 * Since: 0.7.1
 */
TpProxyPendingCall *
tp_proxy_pending_call_v0_new (TpProxy *self,
                              GQuark interface,
                              const gchar *member,
                              DBusGProxy *iface_proxy,
                              TpProxyInvokeFunc invoke_callback,
                              GCallback callback,
                              gpointer user_data,
                              GDestroyNotify destroy,
                              GObject *weak_object,
                              gboolean cancel_must_raise)
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
  ret->iface_proxy = g_object_ref (iface_proxy);
  ret->pending_call = NULL;
  ret->priv = pending_call_magic;
  ret->cancel_must_raise = cancel_must_raise;

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_pending_call_lost_weak_ref, ret);

  g_signal_connect (iface_proxy, "destroy",
      G_CALLBACK (tp_proxy_pending_call_proxy_destroyed), ret);

  return ret;
}

/**
 * tp_proxy_pending_call_cancel:
 * @pc: a pending call
 *
 * Cancel the given pending call. After this function returns, you
 * must not assume that the pending call remains valid, but you must not
 * explicitly free it either.
 *
 * Since: 0.7.1
 */
void
tp_proxy_pending_call_cancel (TpProxyPendingCall *pc)
{
  gpointer iface;
  TpProxyInvokeFunc invoke = pc->invoke_callback;

  DEBUG ("%p", pc);

  g_return_if_fail (pc->priv == pending_call_magic);

  /* Mark the pending call as expired */
  pc->invoke_callback = NULL;

  if (invoke != NULL && pc->cancel_must_raise)
    {
      GError *error = g_error_new_literal (TP_DBUS_ERRORS,
          TP_DBUS_ERROR_CANCELLED, "Re-entrant D-Bus call cancelled");

      MORE_DEBUG ("Telling user callback");
      invoke (pc->proxy, error, NULL, pc->callback, pc->user_data,
          pc->weak_object);
    }

  if (pc->idle_source != 0)
    {
      /* we aren't actually doing dbus-glib things any more anyway */
      MORE_DEBUG ("Removing idle source");
      g_source_remove (pc->idle_source);
      return;
    }

  iface = g_datalist_id_get_data (&pc->proxy->priv->interfaces,
      pc->interface);

  if (iface == NULL || pc->pending_call == NULL)
    {
      MORE_DEBUG ("I don't actually have a DBusGProxy, never mind; "
          "iface=%p proxy=%p pending_call=%p", iface, pc->proxy,
          pc->pending_call);
      return;
    }

  /* if we made a call, then we really ought to actually have a DBusGProxy */
  g_return_if_fail (iface != pc->proxy);

  /* It's possible that the DBusGProxy is only reffed by the TpProxy, and
   * the TpProxy is only reffed by this TpProxyPendingCall, which will be
   * freed as a side-effect of cancelling the DBusGProxyCall halfway through
   * dbus_g_proxy_cancel_call (), so temporarily ref the DBusGProxy to ensure
   * that it survives for the duration. (fd.o #14576) */
  g_object_ref (iface);
  MORE_DEBUG ("Cancelling pending call %p on DBusGProxy %p",
      pc->pending_call, iface);
  dbus_g_proxy_cancel_call (iface, pc->pending_call);
  MORE_DEBUG ("... done");
  g_object_unref (iface);
}

static void
tp_proxy_pending_call_free (gpointer p)
{
  TpProxyPendingCall *pc = p;

  MORE_DEBUG ("%p", pc);

  g_return_if_fail (pc->priv == pending_call_magic);

  if (pc->proxy != NULL)
    {
      g_object_unref (pc->proxy);
      pc->proxy = NULL;
    }

  if (pc->iface_proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (pc->iface_proxy,
          tp_proxy_pending_call_proxy_destroyed, pc);
      g_object_unref (pc->iface_proxy);
      pc->iface_proxy = NULL;
    }

  if (pc->error != NULL)
    g_error_free (pc->error);

  if (pc->args != NULL)
    g_value_array_free (pc->args);

  g_free (pc->member);

  if (pc->destroy != NULL)
    pc->destroy (pc->user_data);

  if (pc->weak_object != NULL)
    g_object_weak_unref (pc->weak_object,
        tp_proxy_pending_call_lost_weak_ref, pc);

  g_slice_free (TpProxyPendingCall, pc);
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
 * This function is for use by #TpProxy subclass implementations only, and
 * should usually only be called from code generated by
 * tools/glib-client-gen.py.
 *
 * Since: 0.7.1
 */
void
tp_proxy_pending_call_v0_completed (gpointer p)
{
  TpProxyPendingCall *pc = p;

  MORE_DEBUG ("%p", p);

  g_return_if_fail (pc->priv == pending_call_magic);

  if (pc->idle_source != 0)
    {
      /* we've kicked off an idle function, so we don't want to die until
       * that function runs */
      MORE_DEBUG ("Refusing to die til the idle function runs");
      return;
    }

  if (pc->proxy != NULL)
    {
      /* dbus-glib frees its user_data *before* it emits destroy; if we
       * haven't yet run the callback, assume that's what's going on. */
      if (pc->invoke_callback != NULL)
        {
          MORE_DEBUG ("Looks like this pending call hasn't finished, assuming "
              "the DBusGProxy is about to die");
          /* this causes the pending call to be freed */
          tp_proxy_pending_call_proxy_destroyed (pc->iface_proxy, pc);
          return;
        }
    }

  MORE_DEBUG ("Freeing myself");
  tp_proxy_pending_call_free (pc);
}

/**
 * tp_proxy_pending_call_v0_take_pending_call:
 * @pc: A pending call on which this function has not yet been called
 * @pending_call: The underlying dbus-glib pending call
 *
 * Set the underlying pending call to be used by this object.
 * See also tp_proxy_pending_call_v0_new().
 *
 * This function is for use by #TpProxy subclass implementations only, and
 * should usually only be called from code generated by
 * tools/glib-client-gen.py.
 *
 * Since: 0.7.1
 */
void
tp_proxy_pending_call_v0_take_pending_call (TpProxyPendingCall *pc,
                                            DBusGProxyCall *pending_call)
{
  g_return_if_fail (pc->priv == pending_call_magic);
  pc->pending_call = pending_call;
}

/**
 * tp_proxy_pending_call_v0_take_results:
 * @pc: A pending call on which this function has not yet been called
 * @error: %NULL if the call was successful, or an error (whose ownership
 *  is taken over by the pending call object). Because of dbus-glib
 *  idiosyncrasies, this must be the error produced by dbus-glib, not a copy.
 * @args: %NULL if the call failed or had no "out" arguments, or an array
 *  of "out" arguments (whose ownership is taken over by the pending call
 *  object)
 *
 * Set the "out" arguments (return values) from this pending call.
 * See also tp_proxy_pending_call_v0_new().
 *
 * This function is for use by #TpProxy subclass implementations only, and
 * should usually only be called from code generated by
 * tools/glib-client-gen.py.
 *
 * Since: 0.7.1
 */
void
tp_proxy_pending_call_v0_take_results (TpProxyPendingCall *pc,
                                       GError *error,
                                       GValueArray *args)
{
  g_return_if_fail (pc->priv == pending_call_magic);
  g_return_if_fail (pc->args == NULL);
  g_return_if_fail (pc->error == NULL);
  g_return_if_fail (pc->idle_source == 0);
  g_return_if_fail (error == NULL || args == NULL);

  MORE_DEBUG ("%p (error: %s)", pc,
      error == NULL ? "(none)" : error->message);

  pc->args = args;
  pc->error = tp_proxy_take_and_remap_error (pc->proxy, error);

  /* queue up the actual callback to run after we go back to the event loop */
  pc->idle_source = g_idle_add_full (G_PRIORITY_HIGH,
      tp_proxy_pending_call_idle_invoke, pc, tp_proxy_pending_call_free);
}

static void
tp_proxy_signal_connection_disconnect_dbus_glib (TpProxySignalConnection *sc)
{
  gpointer iface;

  if (sc->proxy == NULL)
    return;

  iface = g_datalist_id_get_data (&sc->proxy->priv->interfaces,
      sc->interface);

  g_return_if_fail (iface != sc->proxy);

  if (iface == NULL)
    return;

  dbus_g_proxy_disconnect_signal (iface, sc->member, sc->collect_args,
      (gpointer) sc);
}

static void
tp_proxy_signal_connection_proxy_invalidated (TpProxy *proxy,
                                              guint domain,
                                              gint code,
                                              const gchar *message,
                                              TpProxySignalConnection *sc)
{
  g_assert (sc != NULL);
  g_assert (domain != 0);
  g_assert (message != NULL);

  DEBUG ("%p: TpProxy %p invalidated (I have %p): %s", sc, proxy,
      sc->proxy, message);
  g_assert (proxy == sc->proxy);

  tp_proxy_signal_connection_disconnect_dbus_glib (sc);
}

static void
tp_proxy_signal_connection_lost_proxy (gpointer data,
                                       GObject *dead)
{
  TpProxySignalConnection *sc = data;
  TpProxy *proxy = TP_PROXY (dead);

  g_assert (sc != NULL);
  g_assert (sc->invocations.length == 0);

  DEBUG ("%p: lost TpProxy %p (I have %p)", sc, proxy, sc->proxy);
  g_assert (proxy == sc->proxy);

  sc->proxy = NULL;
  tp_proxy_signal_connection_disconnect_dbus_glib (sc);
}

static void
tp_proxy_signal_connection_lost_weak_ref (gpointer data,
                                          GObject *dead)
{
  TpProxySignalConnection *sc = data;

  DEBUG ("%p: lost weak ref to %p", sc, dead);

  g_assert (dead == sc->weak_object);

  sc->weak_object = NULL;

  tp_proxy_signal_connection_disconnect (sc);
}

/* Return TRUE if it dies. */
static gboolean
tp_proxy_signal_connection_unref (TpProxySignalConnection *sc)
{
  if (--(sc->refcount) > 0)
    {
      MORE_DEBUG ("%p: %" G_GSIZE_FORMAT " refs left", sc, sc->refcount);
      return FALSE;
    }

  MORE_DEBUG ("destroying %p", sc);

  if (sc->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (sc->proxy,
          tp_proxy_signal_connection_proxy_invalidated, sc);
      g_object_weak_unref ((GObject *) sc->proxy,
          tp_proxy_signal_connection_lost_proxy, sc);
      sc->proxy = NULL;
    }

  g_assert (sc->invocations.length == 0);

  if (sc->destroy != NULL)
    sc->destroy (sc->user_data);

  sc->destroy = NULL;
  sc->user_data = NULL;

  if (sc->weak_object != NULL)
    {
      g_object_weak_unref (sc->weak_object,
          tp_proxy_signal_connection_lost_weak_ref, sc);
      sc->weak_object = NULL;
    }

  g_free (sc->member);

  g_slice_free (TpProxySignalConnection, sc);

  return TRUE;
}

/**
 * tp_proxy_signal_connection_disconnect:
 * @sc: a signal connection
 *
 * Disconnect the given signal connection. After this function returns, you
 * must not assume that the signal connection remains valid, but you must not
 * explicitly free it either.
 *
 * Since: 0.7.1
 */
void
tp_proxy_signal_connection_disconnect (TpProxySignalConnection *sc)
{
  TpProxySignalInvocation *invocation;

  while ((invocation = g_queue_pop_head (&sc->invocations)) != NULL)
    {
      g_assert (invocation->sc == sc);
      g_object_unref (invocation->sc->proxy);
      invocation->sc = NULL;
      g_source_remove (invocation->idle_source);

      if (tp_proxy_signal_connection_unref (sc))
        return;
    }

  tp_proxy_signal_connection_disconnect_dbus_glib (sc);
}

static void
tp_proxy_signal_invocation_free (gpointer p)
{
  TpProxySignalInvocation *invocation = p;

  if (invocation->sc != NULL)
    {
      /* this shouldn't really happen - it'll get run if the idle source
       * is removed by something other than t_p_s_c_disconnect or
       * t_p_s_i_run */
      g_warning ("%s: idle source removed by someone else", G_STRFUNC);

      g_queue_remove (&invocation->sc->invocations, invocation);
      g_object_unref (invocation->sc->proxy);
      tp_proxy_signal_connection_unref (invocation->sc);
    }

  if (invocation->args != NULL)
    g_value_array_free (invocation->args);

  g_slice_free (TpProxySignalInvocation, invocation);
}

static gboolean
tp_proxy_signal_invocation_run (gpointer p)
{
  TpProxySignalInvocation *invocation = p;
  TpProxySignalInvocation *popped = g_queue_pop_head
      (&invocation->sc->invocations);

  /* if GLib is running idle handlers in the wrong order, then we've lost */
  MORE_DEBUG ("%p: popped %p", invocation->sc, popped);
  g_assert (popped == invocation);

  invocation->sc->invoke_callback (invocation->sc->proxy, NULL,
      invocation->args, invocation->sc->callback, invocation->sc->user_data,
      invocation->sc->weak_object);

  /* the invoke callback steals args */
  invocation->args = NULL;

  /* there's one ref to the proxy per queued invocation, to keep it
   * alive */
  MORE_DEBUG ("%p refcount-- due to %p run, sc=%p", invocation->sc->proxy,
      invocation, invocation->sc);
  g_object_unref (invocation->sc->proxy);
  tp_proxy_signal_connection_unref (invocation->sc);
  invocation->sc = NULL;

  return FALSE;
}

static void
tp_proxy_signal_connection_dropped (gpointer p,
                                    GClosure *unused)
{
  TpProxySignalConnection *sc = p;

  MORE_DEBUG ("%p (%u invocations queued)", sc, sc->invocations.length);

  tp_proxy_signal_connection_unref (sc);
}

static void
collect_none (DBusGProxy *dgproxy, TpProxySignalConnection *sc)
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
 * @error: If not %NULL, used to raise an error if %NULL is returned
 *
 * Allocate a new structure representing a signal connection, and connect to
 * the signal, arranging for @invoke_callback to be called when it arrives.
 *
 * This function is for use by #TpProxy subclass implementations only, and
 * should usually only be called from code generated by
 * tools/glib-client-gen.py.
 *
 * Returns: a signal connection structure, or %NULL if the proxy does not
 *  have the desired interface or has become invalid
 *
 * Since: 0.7.1
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
                                   GObject *weak_object,
                                   GError **error)
{
  TpProxySignalConnection *ret;
  DBusGProxy *iface = tp_proxy_borrow_interface_by_id (self,
      interface, error);

  if (iface == NULL)
    {
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

  ret->refcount = 1;
  ret->proxy = self;
  ret->interface = interface;
  ret->member = g_strdup (member);
  ret->collect_args = collect_args;
  ret->invoke_callback = invoke_callback;
  ret->callback = callback;
  ret->user_data = user_data;
  ret->destroy = destroy;
  ret->weak_object = weak_object;

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_signal_connection_lost_weak_ref,
        ret);

  g_signal_connect (self, "invalidated",
      G_CALLBACK (tp_proxy_signal_connection_proxy_invalidated), ret);

  g_object_weak_ref ((GObject *) self,
      tp_proxy_signal_connection_lost_proxy, ret);

  dbus_g_proxy_connect_signal (iface, member, collect_args, ret,
      tp_proxy_signal_connection_dropped);

  return ret;
}

/**
 * tp_proxy_signal_connection_v0_take_results:
 * @sc: The signal connection
 * @args: The arguments of the signal
 *
 * Feed the results of a signal invocation back into the signal connection
 * machinery.
 *
 * This method should only be called from #TpProxy subclass implementations,
 * in the callback that implements @collect_args.
 *
 * Since: 0.7.1
 */
void
tp_proxy_signal_connection_v0_take_results (TpProxySignalConnection *sc,
                                            GValueArray *args)
{
  TpProxySignalInvocation *invocation = g_slice_new0 (TpProxySignalInvocation);
  /* FIXME: assert that the GValueArray is the right length, or
   * even that it contains the right types? */

  /* as long as there are queued invocations, we keep one ref to the TpProxy
   * and one ref to the TpProxySignalConnection per invocation */
  MORE_DEBUG ("%p refcount++ due to %p, sc=%p", sc->proxy, invocation, sc);
  g_object_ref (sc->proxy);
  sc->refcount++;

  invocation->sc = sc;
  invocation->args = args;

  g_queue_push_tail (&sc->invocations, invocation);

  MORE_DEBUG ("invocations: head=%p tail=%p count=%u",
      sc->invocations.head, sc->invocations.tail,
      sc->invocations.length);

  invocation->idle_source = g_idle_add_full (G_PRIORITY_HIGH,
      tp_proxy_signal_invocation_run, invocation,
      tp_proxy_signal_invocation_free);
}

static void
dup_quark_into_ptr_array (GQuark q,
                          gpointer unused,
                          gpointer user_data)
{
  GPtrArray *strings = user_data;

  g_ptr_array_add (strings, g_strdup (g_quark_to_string (q)));
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
      g_value_set_boxed (value, self->dbus_connection);
      break;
    case PROP_BUS_NAME:
      g_value_set_string (value, self->bus_name);
      break;
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->object_path);
      break;
    case PROP_INTERFACES:
        {
          GPtrArray *strings = g_ptr_array_new ();

          g_datalist_foreach (&self->priv->interfaces,
              dup_quark_into_ptr_array, strings);
          g_ptr_array_add (strings, NULL);
          g_value_take_boxed (value, g_ptr_array_free (strings, FALSE));
        }
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
          TpProxy *daemon_as_proxy = TP_PROXY (g_value_get_object (value));

          g_assert (self->dbus_daemon == NULL);

          if (daemon_as_proxy != NULL)
            self->dbus_daemon = TP_DBUS_DAEMON (g_object_ref
                (daemon_as_proxy));

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

static GQuark
interface_added_cb_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string ("TpProxyInterfaceAddedCb_0.7.1");
    }

  return q;
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
  TpProxyInterfaceAddLink *iter;
  GType ancestor_type = type;
  GType proxy_parent_type = G_TYPE_FROM_CLASS (tp_proxy_parent_class);

  _tp_register_dbus_glib_marshallers ();

  for (ancestor_type = type;
       ancestor_type != proxy_parent_type && ancestor_type != 0;
       ancestor_type = g_type_parent (ancestor_type))
    {
      for (iter = g_type_get_qdata (ancestor_type,
              interface_added_cb_quark ());
           iter != NULL;
           iter = iter->next)
        g_signal_connect (self, "interface-added", G_CALLBACK (iter->callback),
            NULL);
    }

  g_return_val_if_fail (self->dbus_connection != NULL, NULL);
  g_return_val_if_fail (self->object_path != NULL, NULL);
  g_return_val_if_fail (self->bus_name != NULL, NULL);

  g_return_val_if_fail (tp_dbus_check_valid_object_path (self->object_path,
        NULL), NULL);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (self->bus_name,
        TP_DBUS_NAME_TYPE_ANY, NULL), NULL);

  tp_proxy_add_interface_by_id (self, TP_IFACE_QUARK_DBUS_INTROSPECTABLE);
  tp_proxy_add_interface_by_id (self, TP_IFACE_QUARK_DBUS_PEER);
  tp_proxy_add_interface_by_id (self, TP_IFACE_QUARK_DBUS_PROPERTIES);

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
  GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_PROXY_UNREFERENCED,
      "Proxy unreferenced" };

  if (self->priv->dispose_has_run)
    return;
  self->priv->dispose_has_run = TRUE;

  DEBUG ("%p", self);

  tp_proxy_invalidate (self, &e);

  G_OBJECT_CLASS (tp_proxy_parent_class)->dispose (object);
}

static void
tp_proxy_finalize (GObject *object)
{
  TpProxy *self = TP_PROXY (object);

  DEBUG ("%p", self);

  g_assert (self->invalidated != NULL);
  g_error_free (self->invalidated);

  g_free (self->bus_name);
  g_free (self->object_path);

  G_OBJECT_CLASS (tp_proxy_parent_class)->finalize (object);
}

/**
 * tp_proxy_or_subclass_hook_on_interface_add:
 * @proxy_or_subclass: The #GType of #TpProxy or a subclass
 * @callback: A signal handler for #TpProxy::interface-added
 *
 * Arrange for @callback to be connected to #TpProxy::interface-added
 * during the #TpProxy constructor. This is done sufficiently early that
 * it will see the signal for the default interface (@interface member of
 * #TpProxyClass), if any, being added. The intended use is for the callback
 * to call dbus_g_proxy_add_signal() on the new #DBusGProxy.
 *
 * Since: 0.7.1
 */
void
tp_proxy_or_subclass_hook_on_interface_add (GType proxy_or_subclass,
    TpProxyInterfaceAddedCb callback)
{
  GQuark q = interface_added_cb_quark ();
  TpProxyInterfaceAddLink *old_link = g_type_get_qdata (proxy_or_subclass, q);
  TpProxyInterfaceAddLink *new_link;

  g_return_if_fail (g_type_is_a (proxy_or_subclass, TP_TYPE_PROXY));
  g_return_if_fail (callback != NULL);

  new_link = g_slice_new0 (TpProxyInterfaceAddLink);
  new_link->callback = callback;
  new_link->next = old_link;    /* may be NULL */
  g_type_set_qdata (proxy_or_subclass, q, new_link);
}

/**
 * tp_proxy_subclass_add_error_mapping:
 * @proxy_subclass: The #GType of a subclass of #TpProxy (which must not be
 *  #TpProxy itself)
 * @static_prefix: A prefix for D-Bus error names, not including the trailing
 *  dot (which must remain valid forever, and should usually be in static
 *  storage)
 * @domain: A quark representing the corresponding #GError domain
 * @code_enum_type: The type of a subclass of #GEnumClass
 *
 * Register a mapping from D-Bus errors received from the given proxy
 * subclass to #GError instances.
 *
 * When a D-Bus error is received, the #TpProxy code checks for error
 * mappings registered for the class of the proxy receiving the error,
 * then for all of its parent classes.
 *
 * If there is an error mapping for which the D-Bus error name
 * starts with the mapping's @static_prefix, the proxy will check the
 * corresponding @code_enum_type for a value whose @value_nick is
 * the rest of the D-Bus error name (with the leading dot removed). If there
 * isn't such a value, it will continue to try other error mappings.
 *
 * If a suitable error mapping and code are found, the #GError that is raised
 * will have its error domain set to the @domain from the error mapping,
 * and its error code taken from the enum represented by the @code_enum_type.
 *
 * If no suitable error mapping or code is found, the #GError will have
 * error domain %TP_DBUS_ERRORS and error code
 * %TP_DBUS_ERROR_UNKNOWN_REMOTE_ERROR.
 *
 * Since: 0.7.1
 */
void
tp_proxy_subclass_add_error_mapping (GType proxy_subclass,
                                     const gchar *static_prefix,
                                     GQuark domain,
                                     GType code_enum_type)
{
  GQuark q = error_mapping_quark ();
  TpProxyErrorMappingLink *old_link = g_type_get_qdata (proxy_subclass, q);
  TpProxyErrorMappingLink *new_link;
  GType tp_type_proxy = TP_TYPE_PROXY;

  g_return_if_fail (proxy_subclass != tp_type_proxy);
  g_return_if_fail (g_type_is_a (proxy_subclass, tp_type_proxy));
  g_return_if_fail (static_prefix != NULL);
  g_return_if_fail (domain != 0);
  g_return_if_fail (code_enum_type != G_TYPE_INVALID);

  new_link = g_slice_new0 (TpProxyErrorMappingLink);
  new_link->prefix = static_prefix;
  new_link->domain = domain;
  new_link->code_enum_class = g_type_class_ref (code_enum_type);
  new_link->next = old_link;    /* may be NULL */
  g_type_set_qdata (proxy_subclass, q, new_link);
}

static void
tp_proxy_class_init (TpProxyClass *klass)
{
  GParamSpec *param_spec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpProxyPrivate));

  object_class->constructor = tp_proxy_constructor;
  object_class->get_property = tp_proxy_get_property;
  object_class->set_property = tp_proxy_set_property;
  object_class->dispose = tp_proxy_dispose;
  object_class->finalize = tp_proxy_finalize;

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_PROXY,
      tp_cli_generic_add_signals);

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
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
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
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
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
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
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
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      param_spec);

  /**
   * TpProxy:interfaces:
   *
   * Known D-Bus interface names for this object.
   */
  param_spec = g_param_spec_boxed ("interfaces", "D-Bus interfaces",
      "Known D-Bus interface names for this object", G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK
      | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_INTERFACES,
      param_spec);

  /**
   * TpProxy::interface-added:
   * @self: the proxy object
   * @id: the GQuark representing the interface
   * @proxy: the dbus-glib proxy representing the interface
   *
   * Emitted when this proxy has gained an interface. It is not guaranteed
   * to be emitted immediately, but will be emitted before the interface is
   * first used (at the latest: before it's returned from
   * tp_proxy_borrow_interface_by_id(), any signal is connected, or any
   * method is called).
   *
   * The intended use is to call dbus_g_proxy_add_signals(). This signal
   * should only be used by TpProy implementations
   */
  signals[SIGNAL_INTERFACE_ADDED] = g_signal_new ("interface-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__UINT_OBJECT,
      G_TYPE_NONE, 2, G_TYPE_UINT, DBUS_TYPE_G_PROXY);

  /**
   * TpProxy::invalidated:
   * @self: the proxy object
   * @domain: domain of a GError indicating why this proxy was invalidated
   * @code: error code of a GError indicating why this proxy was invalidated
   * @message: a message associated with the error
   *
   * Emitted when this proxy has been become invalid for
   * whatever reason. Any more specific signal should be emitted first.
   */
  signals[SIGNAL_INVALIDATED] = g_signal_new ("invalidated",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__UINT_INT_STRING,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);
}
