/*
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

#include "config.h"

#include "telepathy-glib/proxy-subclass.h"
#include "telepathy-glib/proxy-internal.h"

#define DEBUG_FLAG TP_DEBUG_PROXY
#include "telepathy-glib/debug-internal.h"
#include <telepathy-glib/util.h>

#if 0
#define MORE_DEBUG DEBUG
#else
#define MORE_DEBUG(...) G_STMT_START {} G_STMT_END
#endif

/**
 * TpProxySignalConnection:
 *
 * Opaque structure representing a D-Bus signal connection.
 *
 * Since: 0.7.1
 */

struct _TpProxySignalConnection {
    /* 1 if D-Bus has us
     * 1 if per callback being invoked (possibly nested!) right now */
    gsize refcount;

    /* (transfer full) */
    GDBusConnection *conn;

    TpProxy *proxy;

    guint id;
    GVariantType *expected_types;
    GCallback collect_args;
    TpProxyWrapperFunc wrapper;
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
};

static void tp_proxy_signal_connection_unref (TpProxySignalConnection *sc);

/**
 * tp_proxy_signal_connection_disconnect:
 * @sc: a signal connection
 *
 * Disconnect the given signal connection. After this function returns, you
 * must not assume that the signal connection remains valid, but you must not
 * explicitly free it either.
 *
 * It is not safe to call this function if @sc has been disconnected already,
 * which happens in each of these situations:
 *
 * <itemizedlist>
 * <listitem>the @weak_object used when @sc was created has been
 *  destroyed</listitem>
 * <listitem>tp_proxy_signal_connection_disconnect has already been
 *  used</listitem>
 * <listitem>the proxy has been invalidated</listitem>
 * </itemizedlist>
 *
 * Since: 0.7.1
 */
void
tp_proxy_signal_connection_disconnect (TpProxySignalConnection *sc)
{
  guint id;

  /* ignore if already done */
  if (sc->id == 0)
    {
      DEBUG ("%p: already done, ignoring", sc);
      return;
    }

  DEBUG ("%p", sc);

  id = sc->id;
  sc->id = 0;

  if (sc->proxy != NULL)
    {
      _tp_proxy_remove_signal_connection (sc->proxy, sc);
      sc->proxy = NULL;
    }

  /* likely to free @sc */
  sc->refcount++;
  g_dbus_connection_signal_unsubscribe (sc->conn, id);
  tp_proxy_signal_connection_unref (sc);
}

static void
tp_proxy_signal_connection_lost_weak_ref (gpointer data,
                                          GObject *dead)
{
  TpProxySignalConnection *sc = data;

  DEBUG ("%p: lost weak ref to %p", sc, dead);

  g_assert (dead == sc->weak_object);

  sc->weak_object = NULL;

  /* don't wrap this in a ref/unref, because we might already have had the
   * last-unref as a result of our TpProxy disappearing, in which case
   * we are waiting for an idle to avoid fd.o #14750 */
  tp_proxy_signal_connection_disconnect (sc);
}

static gboolean
_tp_proxy_signal_connection_finish_free (gpointer p)
{
  TpProxySignalConnection *sc = p;

  DEBUG ("%p", sc);

  if (sc->weak_object != NULL)
    {
      g_object_weak_unref (sc->weak_object,
          tp_proxy_signal_connection_lost_weak_ref, sc);
      sc->weak_object = NULL;
    }

  g_slice_free (TpProxySignalConnection, sc);

  return FALSE;
}

static void
tp_proxy_signal_connection_unref (TpProxySignalConnection *sc)
{
  if (--(sc->refcount) > 0)
    {
      MORE_DEBUG ("%p: %" G_GSIZE_FORMAT " refs left", sc, sc->refcount);
      return;
    }

  MORE_DEBUG ("removed last ref to %p", sc);

  if (sc->proxy != NULL)
    {
      _tp_proxy_remove_signal_connection (sc->proxy, sc);
      sc->proxy = NULL;
    }

  if (sc->destroy != NULL)
    sc->destroy (sc->user_data);

  sc->destroy = NULL;
  sc->user_data = NULL;
  g_clear_object (&sc->conn);
  g_clear_pointer (&sc->expected_types, g_variant_type_free);

  /* We can't inline this here, because of fd.o #14750. If our signal
   * connection gets destroyed by side-effects of something else losing a
   * weak reference to the same object (e.g. a pending call whose weak
   * object is the same as ours has the last ref to the TpProxy, causing
   * invalidation when the weak object goes away) then we need to avoid dying
   * til *our* weak-reference callback has run. So, don't actually free the
   * signal connection until we've re-entered the main loop. */
  g_idle_add_full (G_PRIORITY_HIGH, _tp_proxy_signal_connection_finish_free,
      sc, NULL);
}

static void
tp_proxy_signal_connection_cb (GDBusConnection *connection,
    const gchar *sender_name,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *signal_name,
    GVariant *parameters,
    gpointer user_data)
{
  TpProxySignalConnection *sc = user_data;
  TpProxy *proxy;

  DEBUG ("%p: %s.%s from %s:%s", sc, interface_name, signal_name, sender_name,
      object_path);

  if (!g_variant_is_of_type (parameters, sc->expected_types))
    {
      DEBUG ("... expected parameters of type '%.*s', got '%s', ignoring",
          (int) g_variant_type_get_string_length (sc->expected_types),
          g_variant_type_peek_string (sc->expected_types),
          g_variant_get_type_string (parameters));
      return;
    }

  /* we shouldn't get here if we already disconnected the GDBus-level
   * signal connection... */
  g_assert (sc->id != 0);
  /* ... or if the proxy has already been disposed, which disconnects
   * the GDBus-level signal connection */
  g_assert (sc->proxy != NULL);

  /* The callback might invalidate proxy, which disconnects the signal
   * and sets sc->proxy to NULL, so we need to use a temporary here */
  sc->refcount++;
  proxy = g_object_ref (sc->proxy);
  sc->wrapper (proxy, NULL, parameters,
      sc->callback, sc->user_data, sc->weak_object);
  g_object_unref (proxy);
  tp_proxy_signal_connection_unref (sc);
}

/**
 * tp_proxy_signal_connection_v1_new:
 * @self: a proxy
 * @iface: a quark whose string value is the D-Bus interface
 * @member: the name of the signal to which we're connecting
 * @expected_types: an array of expected GTypes for the arguments, terminated
 *  by %G_TYPE_INVALID
 * @wrapper: a function which will be called with @error = %NULL,
 *  which should invoke @callback with @user_data, @weak_object and other
 *  appropriate arguments taken from @args
 * @callback: user callback to be invoked by @wrapper
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
 * the signal, arranging for @wrapper to be called when it arrives.
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

/* that's implemented in the core library, but it calls this: */

TpProxySignalConnection *
_tp_proxy_signal_connection_v1_new (TpProxy *self,
    GQuark iface,
    const gchar *member,
    const GVariantType *expected_types,
    TpProxyWrapperFunc wrapper,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object,
    GError **error)
{
  TpProxySignalConnection *sc;

  if (!tp_proxy_check_interface_by_id (self, iface, error))
    {
      if (destroy != NULL)
        destroy (user_data);

      return NULL;
    }

  sc = g_slice_new0 (TpProxySignalConnection);

  sc->refcount = 1;
  sc->expected_types = g_variant_type_copy (expected_types);
  sc->conn = g_object_ref (tp_proxy_get_dbus_connection (self));
  sc->proxy = self;
  sc->wrapper = wrapper;
  sc->callback = callback;
  sc->user_data = user_data;
  sc->destroy = destroy;
  sc->weak_object = weak_object;

  DEBUG ("%p: %s.%s from %s:%s %p",
      sc, g_quark_to_string (iface), member, tp_proxy_get_bus_name (self),
      tp_proxy_get_object_path (self), self);

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_signal_connection_lost_weak_ref,
        sc);

  sc->id = g_dbus_connection_signal_subscribe (sc->conn,
      tp_proxy_get_bus_name (self),
      g_quark_to_string (iface),
      member,
      tp_proxy_get_object_path (self),
      NULL, /* arg0 */
      G_DBUS_SIGNAL_FLAGS_NONE,
      tp_proxy_signal_connection_cb,
      sc,
      (GDestroyNotify) tp_proxy_signal_connection_unref);
  _tp_proxy_add_signal_connection (self, sc);
  return sc;
}
