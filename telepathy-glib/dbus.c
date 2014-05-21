/*
 * dbus.c - Source for D-Bus utilities
 *
 * Copyright (C) 2005-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2008 Nokia Corporation
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

/**
 * SECTION:dbus
 * @title: D-Bus utilities
 * @short_description: some D-Bus utility functions
 *
 * D-Bus utility functions used in telepathy-glib.
 */

#include "config.h"
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-internal.h>

#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/object-registration-internal.h>
#include <telepathy-glib/proxy.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-interface-skeleton-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_MISC
#include "debug-internal.h"

/**
 * TpDBusNameType:
 * @TP_DBUS_NAME_TYPE_UNIQUE: accept unique names like :1.123
 *  (not including the name of the bus daemon itself)
 * @TP_DBUS_NAME_TYPE_WELL_KNOWN: accept well-known names like
 *  com.example.Service (not including the name of the bus daemon itself)
 * @TP_DBUS_NAME_TYPE_BUS_DAEMON: accept the name of the bus daemon
 *  itself, which has the syntax of a well-known name, but behaves like a
 *  unique name
 * @TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON: accept either unique or well-known
 *  names, but not the bus daemon
 * @TP_DBUS_NAME_TYPE_ANY: accept any of the above
 *
 * A set of flags indicating which D-Bus bus names are acceptable.
 * They can be combined with the bitwise-or operator to accept multiple
 * types. %TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON and %TP_DBUS_NAME_TYPE_ANY are
 * the bitwise-or of other appropriate types, for convenience.
 *
 * Since 0.11.5, there is a corresponding #GFlagsClass type,
 * %TP_TYPE_DBUS_NAME_TYPE.
 *
 * Since: 0.7.1
 */

/**
 * TP_TYPE_DBUS_NAME_TYPE:
 *
 * The #GFlagsClass type of a #TpDBusNameType or a set of name types.
 *
 * Since: 0.11.5
 */

/**
 * tp_dbus_check_valid_bus_name:
 * @name: a possible bus name
 * @allow_types: some combination of %TP_DBUS_NAME_TYPE_UNIQUE,
 *  %TP_DBUS_NAME_TYPE_WELL_KNOWN or %TP_DBUS_NAME_TYPE_BUS_DAEMON
 *  (often this will be %TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON or
 *  %TP_DBUS_NAME_TYPE_ANY)
 * @error: used to raise %TP_DBUS_ERROR_INVALID_BUS_NAME if %FALSE is returned
 *
 * Check that the given string is a valid D-Bus bus name of an appropriate
 * type.
 *
 * Returns: %TRUE if @name is valid
 *
 * Since: 0.7.1
 */
gboolean
tp_dbus_check_valid_bus_name (const gchar *name,
                              TpDBusNameType allow_types,
                              GError **error)
{
  gboolean dot = FALSE;
  gboolean unique;
  gchar last;
  const gchar *ptr;

  g_return_val_if_fail (name != NULL, FALSE);

  if (name[0] == '\0')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "The empty string is not a valid bus name");
      return FALSE;
    }

  if (!tp_strdiff (name, DBUS_SERVICE_DBUS))
    {
      if (allow_types & TP_DBUS_NAME_TYPE_BUS_DAEMON)
        return TRUE;

      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "The D-Bus daemon's bus name is not acceptable here");
      return FALSE;
    }

  unique = (name[0] == ':');
  if (unique && (allow_types & TP_DBUS_NAME_TYPE_UNIQUE) == 0)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "A well-known bus name not starting with ':'%s is required",
          allow_types & TP_DBUS_NAME_TYPE_BUS_DAEMON
            ? " (or the bus daemon itself)"
            : "");
      return FALSE;
    }

  if (!unique && (allow_types & TP_DBUS_NAME_TYPE_WELL_KNOWN) == 0)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "A unique bus name starting with ':'%s is required",
          allow_types & TP_DBUS_NAME_TYPE_BUS_DAEMON
            ? " (or the bus daemon itself)"
            : "");
      return FALSE;
    }

  if (strlen (name) > 255)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "Invalid bus name: too long (> 255 characters)");
      return FALSE;
    }

  last = '\0';

  for (ptr = name + (unique ? 1 : 0); *ptr != '\0'; ptr++)
    {
      if (*ptr == '.')
        {
          dot = TRUE;

          if (last == '.')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_BUS_NAME,
                  "Invalid bus name '%s': contains '..'", name);
              return FALSE;
            }
          else if (last == '\0')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_BUS_NAME,
                  "Invalid bus name '%s': must not start with '.'", name);
              return FALSE;
            }
        }
      else if (g_ascii_isdigit (*ptr))
        {
          if (!unique)
            {
              if (last == '.')
                {
                  g_set_error (error, TP_DBUS_ERRORS,
                      TP_DBUS_ERROR_INVALID_BUS_NAME,
                      "Invalid bus name '%s': a digit may not follow '.' "
                      "except in a unique name starting with ':'", name);
                  return FALSE;
                }
              else if (last == '\0')
                {
                  g_set_error (error, TP_DBUS_ERRORS,
                      TP_DBUS_ERROR_INVALID_BUS_NAME,
                      "Invalid bus name '%s': must not start with a digit",
                      name);
                  return FALSE;
                }
            }
        }
      else if (!g_ascii_isalpha (*ptr) && *ptr != '_' && *ptr != '-')
        {
          g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
              "Invalid bus name '%s': contains invalid character '%c'",
              name, *ptr);
          return FALSE;
        }

      last = *ptr;
    }

  if (last == '.')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "Invalid bus name '%s': must not end with '.'", name);
      return FALSE;
    }

  if (!dot)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_BUS_NAME,
          "Invalid bus name '%s': must contain '.'", name);
      return FALSE;
    }

  return TRUE;
}

/**
 * tp_dbus_check_valid_interface_name:
 * @name: a possible interface name
 * @error: used to raise %TP_DBUS_ERROR_INVALID_INTERFACE_NAME if %FALSE is
 *  returned
 *
 * Check that the given string is a valid D-Bus interface name. This is
 * also appropriate to use to check for valid error names.
 *
 * Since GIO 2.26, g_dbus_is_interface_name() should always return the same
 * thing, although the GLib function does not raise an error explaining why
 * the interface name is incorrect.
 *
 * Returns: %TRUE if @name is valid
 *
 * Since: 0.7.1
 */
gboolean
tp_dbus_check_valid_interface_name (const gchar *name,
                                    GError **error)
{
  gboolean dot = FALSE;
  gchar last;
  const gchar *ptr;

  g_return_val_if_fail (name != NULL, FALSE);

  if (name[0] == '\0')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
          "The empty string is not a valid interface name");
      return FALSE;
    }

  if (strlen (name) > 255)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
          "Invalid interface name: too long (> 255 characters)");
      return FALSE;
    }

  last = '\0';

  for (ptr = name; *ptr != '\0'; ptr++)
    {
      if (*ptr == '.')
        {
          dot = TRUE;

          if (last == '.')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
                  "Invalid interface name '%s': contains '..'", name);
              return FALSE;
            }
          else if (last == '\0')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
                  "Invalid interface name '%s': must not start with '.'",
                  name);
              return FALSE;
            }
        }
      else if (g_ascii_isdigit (*ptr))
        {
          if (last == '\0')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
                  "Invalid interface name '%s': must not start with a digit",
                  name);
              return FALSE;
            }
          else if (last == '.')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
                  "Invalid interface name '%s': a digit must not follow '.'",
                  name);
              return FALSE;
            }
        }
      else if (!g_ascii_isalpha (*ptr) && *ptr != '_')
        {
          g_set_error (error, TP_DBUS_ERRORS,
              TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
              "Invalid interface name '%s': contains invalid character '%c'",
              name, *ptr);
          return FALSE;
        }

      last = *ptr;
    }

  if (last == '.')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
          "Invalid interface name '%s': must not end with '.'", name);
      return FALSE;
    }

  if (!dot)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_INTERFACE_NAME,
          "Invalid interface name '%s': must contain '.'", name);
      return FALSE;
    }

  return TRUE;
}

/**
 * tp_dbus_check_valid_member_name:
 * @name: a possible member name
 * @error: used to raise %TP_DBUS_ERROR_INVALID_MEMBER_NAME if %FALSE is
 *  returned
 *
 * Check that the given string is a valid D-Bus member (method or signal) name.
 *
 * Since GIO 2.26, g_dbus_is_member_name() should always return the same
 * thing, although the GLib function does not raise an error explaining why
 * the interface name is incorrect.
 *
 * Returns: %TRUE if @name is valid
 *
 * Since: 0.7.1
 */
gboolean
tp_dbus_check_valid_member_name (const gchar *name,
                                 GError **error)
{
  const gchar *ptr;

  g_return_val_if_fail (name != NULL, FALSE);

  if (name[0] == '\0')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_MEMBER_NAME,
          "The empty string is not a valid method or signal name");
      return FALSE;
    }

  if (strlen (name) > 255)
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_MEMBER_NAME,
          "Invalid method or signal name: too long (> 255 characters)");
      return FALSE;
    }

  for (ptr = name; *ptr != '\0'; ptr++)
    {
      if (g_ascii_isdigit (*ptr))
        {
          if (ptr == name)
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_MEMBER_NAME,
                  "Invalid method or signal name '%s': must not start with "
                  "a digit", name);
              return FALSE;
            }
        }
      else if (!g_ascii_isalpha (*ptr) && *ptr != '_')
        {
          g_set_error (error, TP_DBUS_ERRORS,
              TP_DBUS_ERROR_INVALID_MEMBER_NAME,
              "Invalid method or signal name '%s': contains invalid "
              "character '%c'",
              name, *ptr);
          return FALSE;
        }
    }

  return TRUE;
}

/**
 * tp_dbus_check_valid_object_path:
 * @path: a possible object path
 * @error: used to raise %TP_DBUS_ERROR_INVALID_OBJECT_PATH if %FALSE is
 *  returned
 *
 * Check that the given string is a valid D-Bus object path. Since GLib 2.24,
 * g_variant_is_object_path() should always return the same thing as this
 * function, although it doesn't provide an error explaining why the object
 * path is invalid.
 *
 * Returns: %TRUE if @path is valid
 *
 * Since: 0.7.1
 */
gboolean
tp_dbus_check_valid_object_path (const gchar *path, GError **error)
{
  const gchar *ptr;

  g_return_val_if_fail (path != NULL, FALSE);

  if (path[0] != '/')
    {
      g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_OBJECT_PATH,
          "Invalid object path '%s': must start with '/'",
          path);
      return FALSE;
    }

  if (path[1] == '\0')
    return TRUE;

  for (ptr = path + 1; *ptr != '\0'; ptr++)
    {
      if (*ptr == '/')
        {
          if (ptr[-1] == '/')
            {
              g_set_error (error, TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INVALID_OBJECT_PATH,
                  "Invalid object path '%s': contains '//'", path);
              return FALSE;
            }
        }
      else if (!g_ascii_isalnum (*ptr) && *ptr != '_')
        {
          g_set_error (error, TP_DBUS_ERRORS,
              TP_DBUS_ERROR_INVALID_OBJECT_PATH,
              "Invalid object path '%s': contains invalid character '%c'",
              path, *ptr);
          return FALSE;
        }
    }

  if (ptr[-1] == '/')
    {
        g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INVALID_OBJECT_PATH,
            "Invalid object path '%s': is not '/' but does end with '/'",
            path);
        return FALSE;
    }

  return TRUE;
}

/* for internal use (TpChannel, TpConnection _new convenience functions) */
gboolean
_tp_dbus_connection_get_name_owner (GDBusConnection *dbus_connection,
    gint timeout_ms,
    const gchar *well_known_name,
    gchar **unique_name,
    GError **error)
{
  GVariant *tuple;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  tuple = g_dbus_connection_call_sync (dbus_connection,
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
 * tp_dbus_connection_request_name:
 * @dbus_connection: a #GDBusConnection
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
tp_dbus_connection_request_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name,
    gboolean idempotent,
    GError **error)
{
  GVariant *tuple;
  guint32 result;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (well_known_name,
        TP_DBUS_NAME_TYPE_WELL_KNOWN, error), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  DEBUG ("%s", well_known_name);

  tuple = g_dbus_connection_call_sync (dbus_connection,
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
 * tp_dbus_connection_release_name:
 * @dbus_connection: a #GDBusConnection
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
tp_dbus_connection_release_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name,
    GError **error)
{
  guint32 result;
  GVariant *tuple;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_bus_name (well_known_name,
        TP_DBUS_NAME_TYPE_WELL_KNOWN, error), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  DEBUG ("%s", well_known_name);

  tuple = g_dbus_connection_call_sync (dbus_connection,
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

static void
tp_dbus_connection_registration_iface_added_cb (GDBusObject *object,
    GDBusInterface *iface,
    gpointer user_data)
{
  TpDBusConnectionRegistration *r = user_data;
  GError *error = NULL;

  if (!G_IS_DBUS_INTERFACE_SKELETON (iface))
    {
      DEBUG ("Not a GDBusInterfaceSkeleton: %p", iface);
      return;
    }

  /* This can't happen if @object is a GDBusObjectSkeleton and has no
   * TpSvc interfaces, since GDBusObjectSkeleton always removes the old
   * interface before replacing it */
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (iface),
        r->conn, r->object_path, &error))
    {
      WARNING ("Cannot export %s %p as %s at %s: %s",
          G_OBJECT_TYPE_NAME (iface), iface,
          g_dbus_interface_get_info (iface)->name,
          r->object_path,
          error->message);
      g_error_free (error);
      return;
    }

  r->skeletons = g_list_prepend (r->skeletons, g_object_ref (iface));
}

static void
tp_dbus_connection_registration_iface_removed_cb (GDBusObject *object,
    GDBusInterface *iface,
    gpointer user_data)
{
  TpDBusConnectionRegistration *r = user_data;
  GList *iface_link;

  iface_link = g_list_find (r->skeletons, iface);

  if (iface_link != NULL)
    {
      g_assert (iface_link->data == (gpointer) iface);
      r->skeletons = g_list_delete_link (r->skeletons, iface_link);
      g_object_unref (iface);
    }
}

static void
tp_dbus_connection_registration_free (gpointer p)
{
  TpDBusConnectionRegistration *r = p;
  GList *iter;

  DEBUG ("%s (r=%p)", r->object_path, r);

  for (iter = r->skeletons; iter != NULL; iter = iter->next)
    {
      DEBUG ("%p", iter->data);
      g_assert (G_IS_DBUS_INTERFACE_SKELETON (iter->data));
      g_dbus_interface_skeleton_unexport (iter->data);
      g_object_unref (iter->data);
    }

  g_list_free (r->skeletons);
  g_free (r->object_path);
  g_clear_object (&r->conn);

  if (r->object != NULL)
    {
      /* If the object is not explicitly unexported, this won't run until
       * g_object_dispose() after all signal handlers have already been
       * disconnected, so we can't just disconnect by the gulong ID - but we
       * can do this instead. */
      g_signal_handlers_disconnect_by_func (r->object,
          tp_dbus_connection_registration_iface_added_cb, r);
      g_signal_handlers_disconnect_by_func (r->object,
          tp_dbus_connection_registration_iface_removed_cb, r);
    }

  g_slice_free (TpDBusConnectionRegistration, r);
}

/**
 * tp_dbus_connection_register_object:
 * @dbus_connection: a #GDBusConnection
 * @object_path: an object path
 * @object: (type GObject.Object) (transfer none): an object to export
 *
 * Export @object at @object_path:
 *
 * * if it is a #GDBusObjectSkeleton, it will be exported
 * * if it is a #GDBusObject but not a #GDBusObjectSkeleton, it is
 *   considered to be a programming error
 * * otherwise, its `TpSvc` interfaces will all be exported
 *
 * It is considered to be a programming error to register an object
 * at a path where another object already exists.
 *
 * Since 0.99.10, as a simplification, exporting an object in this
 * way at more than one location or on more than one bus is not allowed,
 * and is also considered to be a programming error.
 * However, redundantly re-exporting the same object at the same path
 * on the same bus is allowed.
 *
 * Also since 0.99.10, this function must be called *before* taking any
 * bus name whose presence is meant to correspond to the existence of this
 * object. It is *not* sufficient to take the bus name within the same
 * main-loop iteration as registering the object (even though that
 * was sufficient under dbus-glib), because GDBus dispatches
 * method calls in a separate thread.
 */
void
tp_dbus_connection_register_object (GDBusConnection *dbus_connection,
    const gchar *object_path,
    gpointer object)
{
  GError *error = NULL;

  if (!tp_dbus_connection_try_register_object (dbus_connection, object_path,
          object, &error))
    {
      CRITICAL ("Unable to register %s %p at %s:%s: %s #%d: %s",
          G_OBJECT_TYPE_NAME (object), object,
          g_dbus_connection_get_unique_name (dbus_connection),
          object_path,
          g_quark_to_string (error->domain), error->code,
          error->message);
    }
}

static void
_tp_dbus_connection_gather_gdbus_skeletons (GDBusObject *object,
    GHashTable *skeletons)
{
  GList *object_interfaces;
  GList *list_iter;

  DEBUG ("Getting GDBusObject skeletons");

  object_interfaces = g_dbus_object_get_interfaces (object);

  for (list_iter = object_interfaces;
      list_iter != NULL;
      list_iter = list_iter->next)
    {
      GDBusInterface *iface = list_iter->data;
      const gchar *iface_name = g_dbus_interface_get_info (iface)->name;

      g_assert (iface != NULL);
      if (!G_IS_DBUS_INTERFACE_SKELETON (iface))
        {
          DEBUG ("- not a GDBusInterfaceSkeleton: %s %p",
              G_OBJECT_TYPE_NAME (iface), iface);
          continue;
        }

      if (g_hash_table_contains (skeletons, iface_name))
        {
          WARNING ("%s %p has more than one implementation of %s",
              G_OBJECT_TYPE_NAME (object), object, iface_name);
          /* use the second one added - that's consistent with
           * GDBusObjectManagerServer */
        }

      DEBUG ("- %s skeleton: %s %p",
          iface_name, G_OBJECT_TYPE_NAME (iface), iface);

      g_hash_table_replace (skeletons, (gchar *) iface_name,
          g_object_ref (iface));
    }

  g_list_free_full (object_interfaces, g_object_unref);
}

static void
_tp_dbus_connection_gather_tp_svc_skeletons (GObject *object,
    GHashTable *skeletons)
{
  GDBusInterfaceSkeleton *skeleton;
  GType *interfaces = NULL;
  guint n = 0;
  guint i;

  DEBUG ("Getting TpSvc* skeletons");

  interfaces = g_type_interfaces (G_OBJECT_TYPE (object), &n);

  /* Get the TpSvc interfaces if any. These take precedence over whatever
   * was in the GDBusObject, because in practice CMs rely on overriding
   * base-classes' interfaces. */
  for (i = 0; i < n; i++)
    {
      GType iface = interfaces[i];
      const TpSvcInterfaceInfo *iinfo;

      iinfo = tp_svc_interface_peek_dbus_interface_info (iface);

      if (iinfo == NULL)
        {
          DEBUG ("- %s is not a D-Bus interface", g_type_name (iface));
          continue;
        }

      skeleton = tp_svc_interface_skeleton_new (object, iface);

      DEBUG ("- %s skeleton %p (wrapping %s %p)",
          iinfo->interface_info->name, skeleton, g_type_name (iface),
          object);

      if (g_hash_table_contains (skeletons, iinfo->interface_info->name))
        {
          DEBUG ("  (overriding existing implementation of %s)",
              iinfo->interface_info->name);
        }

      g_hash_table_replace (skeletons,
          (gchar *) iinfo->interface_info->name, skeleton);
    }

  g_free (interfaces);
}

/**
 * tp_dbus_connection_try_register_object:
 * @dbus_connection: a #GDBusConnection
 * @object_path: an object path
 * @object: (type GObject.Object) (transfer none): an object to export
 * @error: used to raise %G_IO_ERROR_EXISTS if an object exists at that path
 *
 * The same as tp_dbus_connection_register_object(), except that it is not
 * considered to be a programming error to register an object at a path
 * where another object exists.
 *
 * Returns: %TRUE if the object is successfully registered
 */
gboolean
tp_dbus_connection_try_register_object (GDBusConnection *dbus_connection,
    const gchar *object_path,
    gpointer object,
    GError **error)
{
  GDBusConnection *conn;
  TpDBusConnectionRegistration *r;
  gboolean ret = FALSE;
  GHashTable *skeletons = NULL;
  GHashTableIter hash_iter;
  gpointer skeleton;
  gpointer iface_name_p;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), FALSE);
  g_return_val_if_fail (tp_dbus_check_valid_object_path (object_path, error),
      FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  /* If it implements GDBusObject then it must currently be a
   * GDBusObjectSkeleton. The implementation doesn't actually assume that,
   * as long as its interfaces are GDBusInterfaceSkeletons - we could
   * relax this (to allow for objects that proxy a GDBusObjectSkeleton's
   * interfaces, for instance) if needed. */
  g_return_val_if_fail (G_IS_DBUS_OBJECT_SKELETON (object) ||
      !G_IS_DBUS_OBJECT (object), FALSE);

  conn = dbus_connection;
  r = g_slice_new0 (TpDBusConnectionRegistration);
  r->conn = g_object_ref (conn);
  r->object = object;
  r->object_path = g_strdup (object_path);
  r->skeletons = NULL;

  DEBUG ("%p (r=%p) on %s (%p) at %s", object, r,
      g_dbus_connection_get_unique_name (conn), conn, object_path);

  if (!g_object_replace_qdata (object,
        _tp_dbus_connection_registration_quark (),
        NULL, /* if old value is NULL... */
        r, /* ... replace it with r... */
        tp_dbus_connection_registration_free, /* ... with this free-function... */
        NULL /* ... and don't retrieve the old free-function */ ))
    {
      DEBUG ("already exported, discarding %p", r);
      tp_dbus_connection_registration_free (r);

      /* dbus-glib silently allowed duplicate registrations; to avoid
       * breaking too much existing code, so must we. We don't allow
       * registrations on different connections or at different object
       * paths, though, in the hope that nobody actually does that. */

      r = g_object_get_qdata (object,
          _tp_dbus_connection_registration_quark ());

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

  skeletons = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      g_object_unref);

  if (G_IS_DBUS_OBJECT (object))
    {
      _tp_dbus_connection_gather_gdbus_skeletons (object, skeletons);

      g_signal_connect (object, "interface-added",
          G_CALLBACK (tp_dbus_connection_registration_iface_added_cb), r);
      g_signal_connect (object, "interface-removed",
          G_CALLBACK (tp_dbus_connection_registration_iface_removed_cb), r);
    }
  else
    {
      _tp_dbus_connection_gather_tp_svc_skeletons (object, skeletons);
    }

  DEBUG ("Exporting skeletons");

  g_hash_table_iter_init (&hash_iter, skeletons);

  while (g_hash_table_iter_next (&hash_iter, &iface_name_p, &skeleton))
    {
      const gchar *iface_name = iface_name_p;
      GError *inner_error = NULL;

      if (!g_dbus_interface_skeleton_export (
            G_DBUS_INTERFACE_SKELETON (skeleton), conn, object_path,
            &inner_error))
        {
          DEBUG ("cannot export %p skeleton %p as '%s': %s #%d: %s",
              object, skeleton, iface_name,
              g_quark_to_string (inner_error->domain), inner_error->code,
              inner_error->message);
          g_object_unref (skeleton);
          g_propagate_error (error, inner_error);

          /* roll back */
          tp_dbus_connection_unregister_object (dbus_connection, object);
          goto finally;
        }

      r->skeletons = g_list_prepend (r->skeletons, g_object_ref (skeleton));

      DEBUG ("- %s skeleton %p (wrapping %p)", iface_name, skeleton, object);
    }

  ret = TRUE;
finally:
  g_clear_pointer (&skeletons, g_hash_table_unref);
  return ret;
}

/**
 * tp_dbus_connection_unregister_object:
 * @dbus_connection: a #GDBusConnection
 * @object: (type GObject.Object) (transfer none): an object previously exported
 * with tp_dbus_connection_register_object()
 *
 * Stop exporting @object on D-Bus. This is a convenience wrapper around
 * dbus_g_connection_unregister_g_object(), and behaves similarly.
 *
 * Since: 0.11.3
 */
void
tp_dbus_connection_unregister_object (GDBusConnection *dbus_connection,
    gpointer object)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (dbus_connection));
  g_return_if_fail (G_IS_OBJECT (object));

  DEBUG ("%p", object);

  /* The free-function for the qdata, tp_dbus_connection_registration_free(),
   * will automatically unregister the object (if registered) */
  g_object_set_qdata (object, _tp_dbus_connection_registration_quark (), NULL);
}

GStrv
_tp_g_dbus_object_dup_interface_names_except (GDBusObject *obj,
    ...)
{
  GList *ifaces = g_dbus_object_get_interfaces (obj);
  GPtrArray *ret = g_ptr_array_new ();
  GList *iter;
  const gchar *skip;

  for (iter = ifaces; iter != NULL; iter = iter->next)
    {
      GDBusInterfaceInfo *info = g_dbus_interface_get_info (iter->data);

      if (info != NULL)
        {
          if (info->name == NULL)
            {
              WARNING ("%s %p lists NULL as a GDBusInterfaceInfo->name",
                  G_OBJECT_TYPE_NAME (iter->data), info);
            }
          else if (!tp_strdiff (TP_IFACE_DBUS_PROPERTIES, info->name))
            {
              /* ignore org.freedesktop.DBus, which is implied/assumed */
            }
          else
            {
              va_list ap;

              va_start (ap, obj);

              while ((skip = va_arg (ap, const gchar *)) != NULL)
                {
                  if (!tp_strdiff (skip, info->name))
                    break;
                }

              va_end (ap);

              if (skip == NULL)
                g_ptr_array_add (ret, g_strdup (info->name));
            }
        }
      else
        {
          CRITICAL ("%s %p has no GDBusInterfaceInfo",
              G_OBJECT_TYPE_NAME (iter->data), info);
        }
    }

  g_list_free_full (ifaces, g_object_unref);

  g_ptr_array_add (ret, NULL);
  return (GStrv) g_ptr_array_free (ret, FALSE);
}
