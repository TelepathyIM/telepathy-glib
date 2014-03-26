/*
 * dbus-daemon.c - Source for TpDBusDaemon
 *
 * Copyright (C) 2005-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2009 Nokia Corporation
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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-internal.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/svc-interface-skeleton-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_PROXY
#include "debug-internal.h"

/**
 * TpDBusDaemonClass:
 *
 * The class of #TpDBusDaemon.
 *
 * Since: 0.7.1
 */
struct _TpDBusDaemonClass
{
  /*<private>*/
  TpProxyClass parent_class;
  gpointer priv;
};

/**
 * TpDBusDaemon:
 *
 * A subclass of #TpProxy that represents the D-Bus daemon. It mainly provides
 * functionality to manage well-known names on the bus.
 *
 * Since: 0.7.1
 */
struct _TpDBusDaemon
{
  /*<private>*/
  TpProxy parent;

  TpDBusDaemonPrivate *priv;
};

struct _TpDBusDaemonPrivate
{
  gpointer dummy;
};

G_DEFINE_TYPE (TpDBusDaemon, tp_dbus_daemon, TP_TYPE_PROXY)

static gpointer default_bus_daemon = NULL;

/**
 * tp_dbus_daemon_dup:
 * @error: Used to indicate error if %NULL is returned
 *
 * Returns a proxy for signals and method calls on the D-Bus daemon on which
 * this process was activated (if it was launched by D-Bus service
 * activation), or the session bus (otherwise).
 *
 * If it is not possible to connect to the appropriate bus, raise an error
 * and return %NULL.
 *
 * The returned #TpDBusDaemon is cached; the same #TpDBusDaemon object will
 * be returned by this function repeatedly, as long as at least one reference
 * exists.
 *
 * Returns: (transfer full): a reference to a proxy for signals and method
 *  calls on the bus daemon, or %NULL
 *
 * Since: 0.7.26
 */
TpDBusDaemon *
tp_dbus_daemon_dup (GError **error)
{
  GDBusConnection *conn;

  if (default_bus_daemon != NULL)
    return g_object_ref (default_bus_daemon);

  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

  if (conn == NULL)
    return NULL;

  default_bus_daemon = tp_dbus_daemon_new (conn);
  g_assert (default_bus_daemon != NULL);
  g_object_add_weak_pointer (default_bus_daemon, &default_bus_daemon);

  return default_bus_daemon;
}

/**
 * tp_dbus_daemon_new: (skip)
 * @connection: a connection to D-Bus
 *
 * Returns a proxy for signals and method calls on a particular bus
 * connection.
 *
 * Use tp_dbus_daemon_dup() instead if you just want a connection to the
 * session bus (which is almost always the right thing for
 * Telepathy).
 *
 * Returns: a new proxy for signals and method calls on the bus daemon
 *  to which @connection is connected
 *
 * Since: 0.7.1
 */
TpDBusDaemon *
tp_dbus_daemon_new (GDBusConnection *connection)
{
  g_return_val_if_fail (connection != NULL, NULL);

  return TP_DBUS_DAEMON (g_object_new (TP_TYPE_DBUS_DAEMON,
        "dbus-connection", connection,
        "bus-name", DBUS_SERVICE_DBUS,
        "object-path", DBUS_PATH_DBUS,
        NULL));
}

/* for internal use (TpChannel, TpConnection _new convenience functions) */
gboolean
_tp_dbus_daemon_get_name_owner (TpDBusDaemon *self,
                                gint timeout_ms,
                                const gchar *well_known_name,
                                gchar **unique_name,
                                GError **error)
{
  const GError *invalidated;
  GVariant *tuple;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  invalidated = tp_proxy_get_invalidated (self);

  if (invalidated != NULL)
    {
      if (error != NULL)
        *error = g_error_copy (invalidated);

      return FALSE;
    }

  tuple = g_dbus_connection_call_sync (tp_proxy_get_dbus_connection (self),
      "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
      "GetNameOwner", g_variant_new ("(s)", well_known_name),
      G_VARIANT_TYPE ("(s)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

  if (tuple == NULL)
    return FALSE;

  if (unique_name != NULL)
    g_variant_get (tuple, "(s)", unique_name);

  g_variant_unref (tuple);
  return TRUE;
}

/**
 * tp_dbus_daemon_request_name:
 * @self: a TpDBusDaemon
 * @well_known_name: a well-known name to acquire
 * @idempotent: whether to consider it to be a success if this process
 *              already owns the name
 * @error: used to raise an error if %FALSE is returned
 *
 * Claim the given well-known name without queueing, allowing replacement
 * or replacing an existing name-owner. This makes a synchronous call to the
 * bus daemon.
 *
 * Returns: %TRUE if @well_known_name was claimed, or %FALSE and sets @error if
 *          an error occurred.
 *
 * Since: 0.7.30
 */
gboolean
tp_dbus_daemon_request_name (TpDBusDaemon *self,
                             const gchar *well_known_name,
                             gboolean idempotent,
                             GError **error)
{
  GVariant *tuple;
  guint32 result;
  const GError *invalidated;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (self), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (well_known_name,
        TP_DBUS_NAME_TYPE_WELL_KNOWN, error), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  DEBUG ("%s", well_known_name);

  invalidated = tp_proxy_get_invalidated (self);

  if (invalidated != NULL)
    {
      if (error != NULL)
        *error = g_error_copy (invalidated);

      DEBUG ("- not requesting, we have fallen off D-Bus");
      return FALSE;
    }

  tuple = g_dbus_connection_call_sync (tp_proxy_get_dbus_connection (self),
      "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
      "RequestName",
      g_variant_new ("(su)", well_known_name,
        (guint32) DBUS_NAME_FLAG_DO_NOT_QUEUE),
      G_VARIANT_TYPE ("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

  if (tuple == NULL)
    {
      DEBUG ("- D-Bus error");
      return FALSE;
    }

  g_variant_get (tuple, "(u)", &result);
  g_variant_unref (tuple);

  switch (result)
    {
    case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
      DEBUG ("- acquired");
      return TRUE;

    case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
      DEBUG ("- already owned by us");

      if (idempotent)
        {
          return TRUE;
        }
      else
        {
          g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
              "Name '%s' already in use by this process", well_known_name);
          return FALSE;
        }

    case DBUS_REQUEST_NAME_REPLY_EXISTS:
    case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
      DEBUG ("- already owned by someone else");
      /* the latter shouldn't actually happen since we said DO_NOT_QUEUE */
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "Name '%s' already in use by another process", well_known_name);
      return FALSE;

    default:
      DEBUG ("- unexpected code %u", result);
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "RequestName('%s') returned %u and I don't know what that means",
          well_known_name, result);
      return FALSE;
    }
}

/**
 * tp_dbus_daemon_release_name:
 * @self: a TpDBusDaemon
 * @well_known_name: a well-known name owned by this process to release
 * @error: used to raise an error if %FALSE is returned
 *
 * Release the given well-known name. This makes a synchronous call to the bus
 * daemon.
 *
 * Returns: %TRUE if @well_known_name was released, or %FALSE and sets @error
 *          if an error occurred.
 *
 * Since: 0.7.30
 */
gboolean
tp_dbus_daemon_release_name (TpDBusDaemon *self,
                             const gchar *well_known_name,
                             GError **error)
{
  guint32 result;
  const GError *invalidated;
  GVariant *tuple;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (self), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (well_known_name,
        TP_DBUS_NAME_TYPE_WELL_KNOWN, error), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  DEBUG ("%s", well_known_name);

  invalidated = tp_proxy_get_invalidated (self);

  if (invalidated != NULL)
    {
      if (error != NULL)
        *error = g_error_copy (invalidated);

      DEBUG ("- not releasing, we have fallen off D-Bus");
      return FALSE;
    }

  tuple = g_dbus_connection_call_sync (tp_proxy_get_dbus_connection (self),
      "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
      "ReleaseName", g_variant_new ("(s)", well_known_name),
      G_VARIANT_TYPE ("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

  if (tuple == NULL)
    {
      DEBUG ("- D-Bus error");
      return FALSE;
    }

  g_variant_get (tuple, "(u)", &result);
  g_variant_unref (tuple);

  switch (result)
    {
    case DBUS_RELEASE_NAME_REPLY_RELEASED:
      DEBUG ("- released");
      return TRUE;

    case DBUS_RELEASE_NAME_REPLY_NOT_OWNER:
      DEBUG ("- not ours");
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_YOURS,
          "Name '%s' owned by another process", well_known_name);
      return FALSE;

    case DBUS_RELEASE_NAME_REPLY_NON_EXISTENT:
      DEBUG ("- not owned");
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "Name '%s' not owned", well_known_name);
      return FALSE;

    default:
      DEBUG ("- unexpected code %u", result);
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "ReleaseName('%s') returned %u and I don't know what that means",
          well_known_name, result);
      return FALSE;
    }
}

typedef struct _Registration Registration;

struct _Registration {
    /* (transfer full) */
    GDBusConnection *conn;
    /* (transfer full) */
    gchar *object_path;
    /* (transfer full) */
    GList *skeletons;
};

static GQuark
registration_quark (void)
{
  static GQuark q = 0;

  if (G_UNLIKELY (q == 0))
    {
      q = g_quark_from_static_string ("tp_dbus_daemon_register_object");
    }

  return q;
}

static void
tp_dbus_daemon_registration_free (gpointer p)
{
  Registration *r = p;
  GList *iter;

  DEBUG ("%s (r=%p)", r->object_path, r);

  for (iter = r->skeletons; iter != NULL; iter = iter->next)
    {
      DEBUG ("%p", iter->data);
      g_assert (TP_IS_SVC_INTERFACE_SKELETON (iter->data));
      g_dbus_interface_skeleton_unexport (iter->data);
      g_object_unref (iter->data);
    }

  g_list_free (r->skeletons);
  g_free (r->object_path);
  g_clear_object (&r->conn);
  g_slice_free (Registration, r);
}

/**
 * tp_dbus_daemon_register_object:
 * @self: object representing a connection to a bus
 * @object_path: an object path
 * @object: (type GObject.Object) (transfer none): an object to export
 *
 * Export @object at @object_path. Its `TpSvc` interfaces will all
 * be exported.
 *
 * It is considered to be a programming error to register an object
 * at a path where another object already exists.
 *
 * Since 0.UNRELEASED, as a simplification, exporting an object in this
 * way at more than one location or on more than one bus is not allowed,
 * and is also considered to be a programming error.
 * However, redundantly re-exporting the same object at the same path
 * on the same bus is allowed.
 *
 * Also since 0.UNRELEASED, this function must be called *before* taking any
 * bus name whose presence is meant to correspond to the existence of this
 * object. It is *not* sufficient to take the bus name within the same
 * main-loop iteration as registering the object (even though that
 * was sufficient under dbus-glib), because GDBus dispatches
 * method calls in a separate thread.
 */
void
tp_dbus_daemon_register_object (TpDBusDaemon *self,
    const gchar *object_path,
    gpointer object)
{
  GError *error = NULL;

  if (!tp_dbus_daemon_try_register_object (self, object_path, object, &error))
    {
      CRITICAL ("Unable to register %s %p at %s:%s: %s #%d: %s",
          G_OBJECT_TYPE_NAME (object), object,
          g_dbus_connection_get_unique_name (
            tp_proxy_get_dbus_connection (self)),
          object_path,
          g_quark_to_string (error->domain), error->code,
          error->message);
    }
}

/**
 * tp_dbus_daemon_try_register_object:
 * @self: object representing a connection to a bus
 * @object_path: an object path
 * @object: (type GObject.Object) (transfer none): an object to export
 * @error: used to raise %G_IO_ERROR_EXISTS if an object exists at that path
 *
 * The same as tp_dbus_daemon_register_object(), except that it is not
 * considered to be a programming error to register an object at a path
 * where another object exists.
 *
 * Returns: %TRUE if the object is successfully registered
 */
gboolean
tp_dbus_daemon_try_register_object (TpDBusDaemon *self,
    const gchar *object_path,
    gpointer object,
    GError **error)
{
  GDBusConnection *conn;
  GType *interfaces;
  guint n = 0;
  guint i;
  Registration *r;
  gboolean ret = FALSE;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (self), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_object_path (object_path, error),
      FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  conn = tp_proxy_get_dbus_connection (self);
  r = g_slice_new0 (Registration);
  r->conn = g_object_ref (conn);
  r->object_path = g_strdup (object_path);
  r->skeletons = NULL;

  DEBUG ("%p (r=%p) on %s (%p) at %s", object, r,
      g_dbus_connection_get_unique_name (conn), conn, object_path);

  if (!g_object_replace_qdata (object, registration_quark (),
        NULL, /* if old value is NULL... */
        r, /* ... replace it with r... */
        tp_dbus_daemon_registration_free, /* ... with this free-function... */
        NULL /* ... and don't retrieve the old free-function */ ))
    {
      DEBUG ("already exported, discarding %p", r);
      tp_dbus_daemon_registration_free (r);

      /* dbus-glib silently allowed duplicate registrations; to avoid
       * breaking too much existing code, so must we. We don't allow
       * registrations on different connections or at different object
       * paths, though, in the hope that nobody actually does that. */

      r = g_object_get_qdata (object, registration_quark ());

      if (!tp_strdiff (r->object_path, object_path) &&
          r->conn == conn)
        {
          DEBUG ("already exported at identical (connection, path), ignoring");
          return TRUE;
        }

      CRITICAL ("%s %p has already been exported on %s (%p) at %s, cannot "
          "export on %s (%p) at %s",
          G_OBJECT_TYPE_NAME (object), object,
          g_dbus_connection_get_unique_name (r->conn), r->conn, r->object_path,
          g_dbus_connection_get_unique_name (conn), conn, object_path);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_BUSY,
          "Already exported with different connection or object-path");
      return FALSE;
    }

  /* FIXME: if @object is a GDBusObject or GDBusObjectManagerServer,
   * export it that way instead? */

  interfaces = g_type_interfaces (G_OBJECT_TYPE (object), &n);

  for (i = 0; i < n; i++)
    {
      GType iface = interfaces[i];
      const TpSvcInterfaceInfo *iinfo;
      TpSvcInterfaceSkeleton *skeleton;
      GError *inner_error = NULL;

      iinfo = tp_svc_interface_peek_dbus_interface_info (iface);

      if (iinfo == NULL)
        {
          DEBUG ("- %s is not a D-Bus interface", g_type_name (iface));
          continue;
        }

      skeleton = _tp_svc_interface_skeleton_new (object, iface, iinfo);

      if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON (skeleton), conn, object_path,
            &inner_error))
        {
          DEBUG ("cannot export %s %p skeleton %p as '%s': %s #%d: %s",
              g_type_name (iface), object, skeleton,
              iinfo->interface_info->name,
              g_quark_to_string (inner_error->domain), inner_error->code,
              inner_error->message);
          g_object_unref (skeleton);
          g_propagate_error (error, inner_error);

          /* roll back */
          tp_dbus_daemon_unregister_object (self, object);
          goto finally;
        }

      r->skeletons = g_list_prepend (r->skeletons, skeleton);

      DEBUG ("- %s skeleton %p (wrapping %s %p)",
          iinfo->interface_info->name, skeleton, g_type_name (iface), object);
    }

  ret = TRUE;
finally:
  g_free (interfaces);
  return ret;
}

/**
 * tp_dbus_daemon_unregister_object:
 * @self: object representing a connection to a bus
 * @object: (type GObject.Object) (transfer none): an object previously exported
 * with tp_dbus_daemon_register_object()
 *
 * Stop exporting @object on D-Bus. This is a convenience wrapper around
 * dbus_g_connection_unregister_g_object(), and behaves similarly.
 *
 * Since: 0.11.3
 */
void
tp_dbus_daemon_unregister_object (TpDBusDaemon *self,
    gpointer object)
{
  g_return_if_fail (TP_IS_DBUS_DAEMON (self));
  g_return_if_fail (G_IS_OBJECT (object));

  DEBUG ("%p", object);

  g_object_set_qdata (object, registration_quark (), NULL);
}

/**
 * tp_dbus_daemon_get_unique_name:
 * @self: object representing a connection to a bus
 *
 * <!-- Returns: is enough -->
 *
 * Returns: the unique name of this connection to the bus, which is valid for
 *  as long as this #TpDBusDaemon is
 * Since: 0.7.35
 */
const gchar *
tp_dbus_daemon_get_unique_name (TpDBusDaemon *self)
{
  g_return_val_if_fail (TP_IS_DBUS_DAEMON (self), NULL);

  return g_dbus_connection_get_unique_name (
      tp_proxy_get_dbus_connection (self));
}

static GObject *
tp_dbus_daemon_constructor (GType type,
                            guint n_params,
                            GObjectConstructParam *params)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_dbus_daemon_parent_class);

  TpDBusDaemon *self = TP_DBUS_DAEMON (object_class->constructor (type,
        n_params, params));

  g_assert (!tp_strdiff (tp_proxy_get_bus_name (self), DBUS_SERVICE_DBUS));
  g_assert (!tp_strdiff (tp_proxy_get_object_path (self), DBUS_PATH_DBUS));

  return (GObject *) self;
}

static void
tp_dbus_daemon_init (TpDBusDaemon *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_DBUS_DAEMON,
      TpDBusDaemonPrivate);
}

static void
tp_dbus_daemon_class_init (TpDBusDaemonClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpDBusDaemonPrivate));

  object_class->constructor = tp_dbus_daemon_constructor;

  proxy_class->interface = TP_IFACE_QUARK_DBUS_DAEMON;
}

gboolean
_tp_dbus_daemon_is_the_shared_one (TpDBusDaemon *self)
{
  return (self != NULL && self == default_bus_daemon);
}
