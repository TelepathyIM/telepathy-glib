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

static TpProxyImplementation _tp_proxy_implementation = { NULL };

gboolean
tp_proxy_check_interface_by_id (gpointer proxy,
    GQuark iface,
    GError **error)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.check_interface_by_id (proxy, iface, error);
}

TpProxyPendingCall *
tp_proxy_pending_call_v1_new (TpProxy *proxy,
    gint timeout_ms,
    GQuark iface,
    const gchar *member,
    GVariant *args,
    const GVariantType *reply_type,
    TpProxyWrapperFunc wrapper,
    GCallback callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object)
{
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.pending_call_new (proxy, timeout_ms,
      iface, member, args, reply_type, wrapper,
      callback, user_data, destroy, weak_object);
}

TpProxySignalConnection *
tp_proxy_signal_connection_v1_new (TpProxy *self,
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
  g_assert (_tp_proxy_implementation.version != NULL);
  return _tp_proxy_implementation.signal_connection_new (self, iface, member,
      expected_types, wrapper, callback, user_data, destroy, weak_object,
      error);
}

void
tp_private_proxy_set_implementation (TpProxyImplementation *impl)
{
  /* not using tp_strdiff because it isn't available in the core library */
  g_assert (g_str_equal (impl->version, VERSION));
  g_assert (impl->size == sizeof (TpProxyImplementation));
  g_assert (g_str_equal (g_type_name (impl->type), "TpProxy"));
  g_assert (_tp_proxy_implementation.version == NULL);

  g_assert (impl->check_interface_by_id != NULL);
  g_assert (impl->pending_call_new != NULL);
  g_assert (impl->signal_connection_new != NULL);

  memcpy (&_tp_proxy_implementation, impl, sizeof (TpProxyImplementation));

  g_assert (g_str_equal (_tp_proxy_implementation.version, VERSION));
}
