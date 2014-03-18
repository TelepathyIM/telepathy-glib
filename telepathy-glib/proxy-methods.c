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
#include "telepathy-glib/errors.h"
#include <telepathy-glib/util.h>

#if 0
#define MORE_DEBUG DEBUG
#else
#define MORE_DEBUG(...) G_STMT_START {} G_STMT_END
#endif

/**
 * TpProxyWrapperFunc:
 * @self: a proxy
 * @error: (allow-none): an error, or %NULL for a successful reply
 * @args: (allow-none): the arguments of a successful reply, or %NULL
 *  on error
 * @callback: the callback to call
 * @user_data: user data to pass to the callback
 * @weak_object: object to pass to the callback
 *
 * A simplified reinvention of #GClosureMarshal for #TpProxy subclasses.
 * Functions with this signature are intended to be
 * programmatically-generated; there should be no need to use it in
 * hand-written code, other than the implementation of #TpProxy.
 */

/**
 * TpProxyPendingCall:
 *
 * Opaque structure representing a pending D-Bus call.
 *
 * Since: 0.7.1
 */

struct _TpProxyPendingCall {
    TpProxy *proxy;

    /* Set to NULL after it's been invoked once, or if cancellation means
     * it should never be called. Supplied by the generated code */
    TpProxyWrapperFunc wrapper;

    /* error if no interface */
    GError *error /* implicitly initialized */;

    /* user-supplied arguments for invoke_callback */
    GCallback callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;

    /* Used to cancel the call early */
    GCancellable *cancellable;

    /* Marker to indicate that this is, in fact, a valid TpProxyPendingCall */
    gconstpointer priv;
};

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
tp_proxy_pending_call_idle_error (gpointer p)
{
  TpProxyPendingCall *pc = p;
  TpProxyWrapperFunc wrapper = pc->wrapper;

  if (wrapper == NULL || pc->proxy == NULL ||
      g_cancellable_is_cancelled (pc->cancellable))
    {
      DEBUG ("%p: ignoring result due to invalidation, weak object "
          "disappearance or cancellation", pc);
      return FALSE;
    }

  g_assert (pc->error != NULL);

  DEBUG ("%p: %s #%d: %s", pc, g_quark_to_string (pc->error->domain),
      pc->error->code, pc->error->message);

  pc->wrapper = NULL;
  wrapper (pc->proxy, pc->error, NULL, pc->callback,
      pc->user_data, pc->weak_object);
  g_clear_error (&pc->error);

  return FALSE;
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
  DEBUG ("%p", pc);

  g_return_if_fail (pc->priv == pending_call_magic);

  g_cancellable_cancel (pc->cancellable);
  g_clear_object (&pc->proxy);
  pc->wrapper = NULL;
}

static void
tp_proxy_pending_call_free (TpProxyPendingCall *pc)
{
  DEBUG ("%p", pc);

  g_assert (pc->priv == pending_call_magic);

  if (pc->destroy != NULL)
    pc->destroy (pc->user_data);

  pc->destroy = NULL;
  pc->user_data = NULL;

  g_clear_error (&pc->error);

  if (pc->weak_object != NULL)
    g_object_weak_unref (pc->weak_object,
        tp_proxy_pending_call_lost_weak_ref, pc);

  g_clear_object (&pc->cancellable);
  g_clear_object (&pc->proxy);
  g_slice_free (TpProxyPendingCall, pc);
}

static void
tp_proxy_pending_call_async_ready_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpProxyPendingCall *pc = user_data;
  GVariant *args;
  GError *error = NULL;

  if (pc->proxy == NULL ||
      pc->wrapper == NULL ||
      g_cancellable_is_cancelled (pc->cancellable))
    {
      DEBUG ("%p: ignoring result due to invalidation, weak object "
          "disappearance or cancellation", pc);
      goto finally;
    }

  args = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
      result, &error);

  if (args != NULL)
    {
      DEBUG ("%p: success", pc);
      pc->wrapper (pc->proxy, NULL, args,
          pc->callback, pc->user_data, pc->weak_object);
      g_variant_unref (args);
    }
  else
    {
      DEBUG ("%p: %s #%d: %s", pc,
          g_quark_to_string (error->domain), error->code,
          error->message);
      pc->wrapper (pc->proxy, error, NULL,
          pc->callback, pc->user_data, pc->weak_object);
      g_clear_error (&error);
    }

  pc->wrapper = NULL;

finally:
  tp_proxy_pending_call_free (pc);
}

/**
 * tp_proxy_pending_call_v1_new:
 * @self: the proxy
 * @timeout_ms: the timeout in milliseconds, usually -1 to use a default
 * @iface: a quark representing the D-Bus interface name
 * @member: the method name
 * @args: arguments for the call; if this is a floating reference, this
 *  method will take ownership as if via g_variant_ref_sink()
 * @reply_type: the expected type of the reply, which must be a tuple type
 * @wrapper: (allow-none): a wrapper function to call when the call completes,
 *  or %NULL if the method reply/error should be ignored
 * @callback: (allow-none): callback to pass to the wrapper function. If it
 *  is non-%NULL, @wrapper must also be non-%NULL.
 * @user_data: (allow-none): user data to pass to the wrapper function
 * @destroy: (allow-none): callback to destroy @user_data
 * @weak_object: (allow-none): object to pass to the wrapper function; if this
 *  object is finalized before the call completes, @wrapper will not be
 *  called at all
 *
 * Make a D-Bus call. If it is not cancelled, call @wrapper when it completes.
 * The @wrapper will usually call @callback, but is not required to do so.
 *
 * If the call is cancelled with tp_proxy_pending_call_cancel() or by
 * finalization of the @weak_object, then @wrapper is not called at all,
 * but @destroy will still be called.
 *
 * This function is intended to be called by generated code. If possible,
 * use g_dbus_connection_call() or g_dbus_proxy_call() instead.
 */
/* implemented in the core library as a call to this: */
TpProxyPendingCall *
_tp_proxy_pending_call_v1_new (TpProxy *self,
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
  TpProxyPendingCall *pc = NULL;

  g_return_val_if_fail (callback != NULL || user_data == NULL, NULL);
  g_return_val_if_fail (callback != NULL || destroy == NULL, NULL);
  g_return_val_if_fail (callback != NULL || weak_object == NULL, NULL);
  g_return_val_if_fail (iface != 0, NULL);
  g_return_val_if_fail (member != NULL, NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (reply_type != NULL, NULL);
  g_return_val_if_fail (callback == NULL || wrapper != NULL, NULL);

  g_variant_ref_sink (args);

  if (callback == NULL)
    {
      if (tp_proxy_has_interface_by_id (self, iface))
        {
          DEBUG ("%s.%s on %s:%s %p, ignoring reply",
              g_quark_to_string (iface), member,
              tp_proxy_get_bus_name (self), tp_proxy_get_object_path (self),
              self);

          g_dbus_connection_call (tp_proxy_get_dbus_connection (self),
              tp_proxy_get_bus_name (self),
              tp_proxy_get_object_path (self),
              g_quark_to_string (iface),
              member,
              args,
              reply_type,
              G_DBUS_CALL_FLAGS_NONE,
              timeout_ms,
              NULL,
              NULL,
              NULL);
        }
      else
        {
          DEBUG ("%s.%s on %s:%s %p would fail, but ignoring reply",
              g_quark_to_string (iface), member,
              tp_proxy_get_bus_name (self), tp_proxy_get_object_path (self),
              self);
        }

      goto finally;
    }

  pc = g_slice_new0 (TpProxyPendingCall);
  pc->proxy = g_object_ref (self);
  pc->wrapper = wrapper;
  pc->callback = callback;
  pc->user_data = user_data;
  pc->destroy = destroy;
  pc->weak_object = weak_object;
  pc->priv = pending_call_magic;
  pc->cancellable = g_cancellable_new ();

  DEBUG ("%p: %s.%s on %s:%s %p",
      pc, g_quark_to_string (iface), member, tp_proxy_get_bus_name (self),
      tp_proxy_get_object_path (self), self);

  if (weak_object != NULL)
    g_object_weak_ref (weak_object, tp_proxy_pending_call_lost_weak_ref, pc);

  /* very slight optimization: intra-library call to the real implementation
   * rather than calling across library boundaries via the core library */
  if (_tp_proxy_check_interface_by_id (self, iface, &pc->error))
    {
      DEBUG ("... doing GDBus call");

      g_dbus_connection_call (tp_proxy_get_dbus_connection (self),
          tp_proxy_get_bus_name (self),
          tp_proxy_get_object_path (self),
          g_quark_to_string (iface),
          member,
          /* consume floating ref */
          args,
          reply_type,
          G_DBUS_CALL_FLAGS_NONE,
          timeout_ms,
          pc->cancellable,
          tp_proxy_pending_call_async_ready_cb,
          pc);
    }
  else
    {
      DEBUG ("... raising error immediately");
      g_idle_add_full (G_PRIORITY_HIGH,
          tp_proxy_pending_call_idle_error, pc,
          (GDestroyNotify) tp_proxy_pending_call_free);
    }

finally:
  g_variant_unref (args);
  return pc;
}
