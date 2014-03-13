/*
 * core-proxy.c - parts of TpProxy needed to link generated code
 *
 * Copyright © 2007-2012 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
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

#include <string.h>

#define DEBUG_FLAG TP_DEBUG_PROXY
#include "debug-internal.h"

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
 * Since: 0.7.6
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

static TpProxyImplementation _tp_proxy_implementation = { NULL };

DBusGProxy *
tp_proxy_get_interface_by_id (TpProxy *proxy,
    GQuark iface,
    GError **error)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.get_interface_by_id (proxy, iface, error);
}

gboolean
tp_proxy_check_interface_by_id (gpointer proxy,
    GQuark iface,
    GError **error)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.check_interface_by_id (proxy, iface, error);
}

TpProxyPendingCall *
tp_proxy_pending_call_v0_new (TpProxy *proxy,
    GQuark iface,
    const gchar *member,
    DBusGProxy *iface_proxy,
    TpProxyInvokeFunc invoke_callback,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object,
    gboolean cancel_must_raise)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.pending_call_new (proxy, iface, member,
      iface_proxy, invoke_callback, callback, user_data, destroy,
      weak_object, cancel_must_raise);
}

void
tp_proxy_pending_call_v0_take_pending_call (TpProxyPendingCall *pc,
    DBusGProxyCall *pending_call)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  _tp_proxy_implementation.pending_call_take_pending_call (pc, pending_call);
}

void
tp_proxy_pending_call_v0_completed (gpointer p)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  _tp_proxy_implementation.pending_call_completed (p);
}

void
tp_proxy_pending_call_v0_take_results (TpProxyPendingCall *pc,
    GError *error,
    GValueArray *args)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  _tp_proxy_implementation.pending_call_take_results (pc, error, args);
}

TpProxySignalConnection *
tp_proxy_signal_connection_v0_new (TpProxy *self,
    GQuark iface,
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
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.signal_connection_new (self, iface, member,
      expected_types, collect_args, invoke_callback, callback, user_data,
      destroy, weak_object, error);
}

void
tp_proxy_signal_connection_v0_take_results (TpProxySignalConnection *sc,
    GValueArray *args)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  _tp_proxy_implementation.signal_connection_take_results (sc, args);
}

void
tp_private_proxy_set_implementation (TpProxyImplementation *impl)
{
  g_assert_cmpstr (impl->version, ==, VERSION);
  g_assert_cmpuint (impl->size, ==, sizeof (TpProxyImplementation));
  g_assert_cmpstr (g_type_name (impl->type), ==, "TpProxy");
  g_assert (_tp_proxy_implementation.version == NULL);

  g_assert (impl->get_interface_by_id != NULL);
  g_assert (impl->check_interface_by_id != NULL);
  g_assert (impl->pending_call_new != NULL);
  g_assert (impl->pending_call_take_pending_call != NULL);
  g_assert (impl->pending_call_take_results != NULL);
  g_assert (impl->pending_call_completed != NULL);
  g_assert (impl->signal_connection_new != NULL);
  g_assert (impl->signal_connection_take_results != NULL);

  memcpy (&_tp_proxy_implementation, impl, sizeof (TpProxyImplementation));

  g_assert_cmpstr (_tp_proxy_implementation.version, ==, VERSION);
}
