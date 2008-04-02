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
#include "telepathy-glib/proxy-internal.h"

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

GError *
_tp_proxy_take_and_remap_error (TpProxy *self,
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

/**
 * tp_proxy_dbus_g_proxy_claim_for_signal_adding:
 * @proxy: a #DBusGProxy
 *
 * Attempt to "claim" a #DBusGProxy for addition of signal signatures.
 * If this function has not been called on @proxy before, %TRUE is
 * returned, and the caller may safely call dbus_g_proxy_add_signal()
 * on @proxy. If this function has already been caled, %FALSE is
 * returned, and the caller may not safely call dbus_g_proxy_add_signal().
 *
 * This is intended for use by auto-generated signal-adding functions,
 * to allow interfaces provided as local extensions to override those in
 * telepathy-glib without causing assertion failures.
 *
 * Returns: %TRUE if it is safe to call dbus_g_proxy_add_signal()
 */
gboolean
tp_proxy_dbus_g_proxy_claim_for_signal_adding (DBusGProxy *proxy)
{
  static GQuark q = 0;

  g_return_val_if_fail (proxy != NULL, FALSE);

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string (
          "tp_proxy_dbus_g_proxy_claim_for_signal_adding@0.7.6");
    }

  if (g_object_get_qdata ((GObject *) proxy, q) != NULL)
    {
      /* Someone else has already added signal signatures for this interface.
       * We can't do it again or it'll cause an assertion */
      return FALSE;
    }

  /* the proxy is just used as qdata here because it's a convenient
   * non-NULL pointer */
  g_object_set_qdata ((GObject *) proxy, q, proxy);
  return TRUE;
}
