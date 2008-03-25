/*
 * presence-mixin.c - Source for TpPresenceMixin
 * Copyright (C) 2005-2007 Collabora Ltd.
 * Copyright (C) 2005-2007 Nokia Corporation
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
 * SECTION:presence-mixin
 * @title: TpPresenceMixin
 * @short_description: a mixin implementation of the Presence connection
 *  interface
 * @see_also: #TpSvcConnectionInterfacePresence
 *
 * This mixin can be added to a #TpBaseConnection subclass to implement the
 * presence interface in a general way. It does not support protocols where it
 * is possible to set multiple statuses on yourself at once, however. Hence all
 * presence statuses will have the exclusive flag set.
 *
 * There are plans to deprecate the notion of last activity time in the D-Bus
 * presence interface, so #TpPresenceStatus doesn't include it at all.
 * Consequently, the last activity time field in presence data presented over
 * D-Bus will always be zero and the SetLastActivityTime method doesn't actually
 * do anything.
 *
 * To use the presence mixin, include a #TpPresenceMixinClass somewhere in your
 * class structure and a #TpPresenceMixin somewhere in your instance structure,
 * and call tp_presence_mixin_class_init() from your class_init function,
 * tp_presence_mixin_init() from your init function or constructor, and
 * tp_presence_mixin_finalize() from your dispose or finalize function.
 *
 * To use the presence mixin as the implementation of
 * #TpSvcConnectionInterfacePresence, in the function you pass to
 * G_IMPLEMENT_INTERFACE, you should call tp_presence_mixin_iface_init.
 * TpPresenceMixin implements all of the D-Bus methods in the Presence
 * interface.
 *
 * Since: 0.5.13
 */

#include <telepathy-glib/presence-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>

#define DEBUG_FLAG TP_DEBUG_PRESENCE

#include "debug-internal.h"


struct _TpPresenceMixinPrivate
{
  /* ... */
};


/**
 * deep_copy_hashtable
 *
 * Make a deep copy of a GHashTable.
 */
static GHashTable *
deep_copy_hashtable (GHashTable *hash_table)
{
  GValue value = {0, };

  if (!hash_table)
    return NULL;

  g_value_init (&value, TP_HASH_TYPE_STRING_VARIANT_MAP);
  g_value_take_boxed (&value, hash_table);
  return g_value_dup_boxed (&value);
}


/**
 * tp_presence_status_new
 * @which: Index of the presence status in the provided supported presence
 *  statuses array
 * @optional_arguments: Optional arguments for the presence statuses. Can be
 *  NULL if there are no optional arguments. The presence status object makes a
 *  copy of the hashtable, so you should free the original.
 *
 * Construct a presence status structure. You should free the returned
 * structure with #tp_presence_status_free.
 *
 * Returns: A pointer to the newly allocated presence status structure.
 */
TpPresenceStatus *
tp_presence_status_new (guint which,
                        GHashTable *optional_arguments)
{
  TpPresenceStatus *status = g_slice_new (TpPresenceStatus);

  status->index = which;
  status->optional_arguments = deep_copy_hashtable (optional_arguments);

  return status;
}


/**
 * tp_presence_status_free
 * @status: A pointer to the presence status structure to free.
 *
 * Deallocate all resources associated with a presence status structure.
 */
void
tp_presence_status_free (TpPresenceStatus *status)
{
  if (!status)
    return;

  if (status->optional_arguments)
    g_hash_table_destroy (status->optional_arguments);

  g_slice_free(TpPresenceStatus, status);
}


/**
 * tp_presence_mixin_class_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObjectClass
 */
GQuark
tp_presence_mixin_class_get_offset_quark ()
{
  static GQuark offset_quark = 0;
  if (!offset_quark)
    offset_quark = g_quark_from_static_string ("TpPresenceMixinClassOffsetQuark");
  return offset_quark;
}

/**
 * tp_presence_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
 */
GQuark
tp_presence_mixin_get_offset_quark ()
{
  static GQuark offset_quark = 0;
  if (!offset_quark)
    offset_quark = g_quark_from_static_string ("TpPresenceMixinOffsetQuark");
  return offset_quark;
}


/**
 * tp_presence_mixin_class_init:
 * @obj_cls: The class of the implementation that uses this mixin
 * @offset: The byte offset of the TpPresenceMixinClass within the class
 * structure
 * @status_available: A callback to be used to determine if a given presence
 *  status is available to be set on the connection. If NULL, all statuses are
 *  always considered available.
 * @get_contact_statuses: A callback to be used get the current presence status
 *  for contacts. This is used in implementations of various D-Bus methods and
 *  hence must be provided.
 * @set_own_status: A callback to be used to commit changes to the user's own
 *  presence status to the server. This is used in implementations of various
 *  D-Bus methods and hence must be provided.
 * @statuses: An array of #TpPresenceStatusSpec structures representing all
 *  presence statuses supported by the protocol, terminated by a NULL name.
 *
 * Initialize the presence mixin. Should be called from the implementation's
 * class_init function like so:
 *
 * <informalexample><programlisting>
 * tp_presence_mixin_class_init ((GObjectClass *)klass,
 *                               G_STRUCT_OFFSET (SomeObjectClass,
 *                                                presence_mixin));
 * </programlisting></informalexample>
 */

void
tp_presence_mixin_class_init (GObjectClass *obj_cls,
                              glong offset,
                              TpPresenceMixinStatusAvailableFunc status_available,
                              TpPresenceMixinGetContactStatusesFunc get_contact_statuses,
                              TpPresenceMixinSetOwnStatusFunc set_own_status,
                              const TpPresenceStatusSpec *statuses)
{
  TpPresenceMixinClass *mixin_cls;

  DEBUG ("called.");

  g_assert (get_contact_statuses != NULL);
  g_assert (set_own_status != NULL);
  g_assert (statuses != NULL);

  g_assert (G_IS_OBJECT_CLASS (obj_cls));

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (obj_cls),
      TP_PRESENCE_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  mixin_cls = TP_PRESENCE_MIXIN_CLASS (obj_cls);

  mixin_cls->status_available = status_available;
  mixin_cls->get_contact_statuses = get_contact_statuses;
  mixin_cls->set_own_status = set_own_status;
  mixin_cls->statuses = statuses;
}


/**
 * tp_presence_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpPresenceMixin within the object structure
 *
 * Initialize the presence mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_presence_mixin_init ((GObject *)self,
 *                         G_STRUCT_OFFSET (SomeObject, presence_mixin));
 * </programlisting></informalexample>
 */
void
tp_presence_mixin_init (GObject *obj,
                        glong offset)
{
  TpPresenceMixin *mixin;

  DEBUG ("called.");

  g_assert (G_IS_OBJECT (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_PRESENCE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  mixin = TP_PRESENCE_MIXIN (obj);

  mixin->priv = g_slice_new0 (TpPresenceMixinPrivate);
}

/**
 * tp_presence_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the presence mixin.
 */
void
tp_presence_mixin_finalize (GObject *obj)
{
  TpPresenceMixin *mixin = TP_PRESENCE_MIXIN (obj);

  DEBUG ("%p", obj);

  /* free any data held directly by the object here */

  g_slice_free (TpPresenceMixinPrivate, mixin->priv);
}


struct _i_absolutely_love_g_hash_table_foreach {
    const TpPresenceStatusSpec *supported_statuses;
    GHashTable *contact_statuses;
    GHashTable *presence_hash;
};


static void
construct_presence_hash_foreach (gpointer key,
                                 gpointer value,
                                 gpointer user_data)
{
  TpHandle handle = GPOINTER_TO_UINT (key);
  TpPresenceStatus *status = (TpPresenceStatus *) value;
  struct _i_absolutely_love_g_hash_table_foreach *data =
    (struct _i_absolutely_love_g_hash_table_foreach *) user_data;
  GHashTable *parameters;
  GHashTable *contact_status;
  GValueArray *vals;

  contact_status = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) g_hash_table_destroy);

  parameters = deep_copy_hashtable (status->optional_arguments);

  if (!parameters)
    parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

  g_hash_table_insert (contact_status,
      (gpointer) data->supported_statuses[status->index].name, parameters);

  vals = g_value_array_new (2);

  /* last-activity sucks and will probably be removed soon */
  g_value_array_append (vals, NULL);
  g_value_init (g_value_array_get_nth (vals, 0), G_TYPE_UINT);
  g_value_set_uint (g_value_array_get_nth (vals, 0), 0);

  g_value_array_append (vals, NULL);
  g_value_init (g_value_array_get_nth (vals, 1),
      TP_HASH_TYPE_MULTIPLE_STATUS_MAP);
  g_value_take_boxed (g_value_array_get_nth (vals, 1), contact_status);

  g_hash_table_insert (data->presence_hash, GUINT_TO_POINTER (handle), vals);
}


static GHashTable *
construct_presence_hash (const TpPresenceStatusSpec *supported_statuses,
                         GHashTable *contact_statuses)
{
  struct _i_absolutely_love_g_hash_table_foreach data = { supported_statuses,
    contact_statuses, NULL };

  DEBUG ("called.");

  data.presence_hash = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_value_array_free);

  g_hash_table_foreach (contact_statuses, construct_presence_hash_foreach,
      &data);

  return data.presence_hash;
}


/**
 * tp_presence_mixin_emit_presence_update:
 * @obj: A connection object with this mixin
 * @contact_presences: A mapping of contact handles to #TpPresenceStatus
 *  structures with the presence data to emit
 *
 * Emit the PresenceUpdate signal for multiple contacts. For emitting
 * PresenceUpdate for a single contact, there is a convenience wrapper called
 * #tp_presence_mixin_emit_one_presence_update.
 */
void
tp_presence_mixin_emit_presence_update (GObject *obj,
                                        GHashTable *contact_statuses)
{
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GHashTable *presence_hash;

  DEBUG ("called.");

  presence_hash = construct_presence_hash (mixin_cls->statuses,
      contact_statuses);
  tp_svc_connection_interface_presence_emit_presence_update (obj,
      presence_hash);

  g_hash_table_destroy (presence_hash);
}


/**
 * tp_presence_mixin_emit_one_presence_update:
 * @obj: A connection object with this mixin
 * @handle: The handle of the contact to emit the signal for
 * @status: The new status to emit
 *
 * Emit the PresenceUpdate signal for a single contact. This method is just a
 * convenience wrapper around #tp_presence_mixin_emit_presence_update.
 */
void
tp_presence_mixin_emit_one_presence_update (GObject *obj,
                                            TpHandle handle,
                                            const TpPresenceStatus *status)
{
  GHashTable *contact_statuses;

  DEBUG ("called.");

  contact_statuses = g_hash_table_new (NULL, NULL);
  g_hash_table_insert (contact_statuses, GUINT_TO_POINTER (handle),
      (gpointer) status);
  tp_presence_mixin_emit_presence_update (obj, contact_statuses);

  g_hash_table_destroy (contact_statuses);
}


/**
 * tp_presence_mixin_add_status:
 *
 * Implements D-Bus method AddStatus
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_add_status (TpSvcConnectionInterfacePresence *iface,
                              const gchar *status,
                              GHashTable *parms,
                              DBusGMethodInvocation *context)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  GError error = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
    "Only one status is possible at a time with this protocol!" };

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  dbus_g_method_return_error (context, &error);
}


/**
 * tp_presence_mixin_clear_status:
 *
 * Implements D-Bus method ClearStatus
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_clear_status (TpSvcConnectionInterfacePresence *iface,
                                DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GError *error = NULL;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (!mixin_cls->set_own_status (obj, NULL, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  tp_svc_connection_interface_presence_return_from_clear_status (context);
}


/**
 * tp_presence_mixin_get_presence:
 *
 * Implements D-Bus method GetPresence
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_get_presence (TpSvcConnectionInterfacePresence *iface,
                                const GArray *contacts,
                                DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpBaseConnection *conn = TP_BASE_CONNECTION (obj);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_HANDLE_TYPE_CONTACT);
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GHashTable *contact_statuses;
  GHashTable *presence_hash;
  GError *error = NULL;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (contacts->len == 0)
    {
      presence_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
      tp_svc_connection_interface_presence_return_from_get_presence (context,
          presence_hash);
      g_hash_table_destroy (presence_hash);
      return;
    }

  if (!tp_handles_are_valid (contact_repo, contacts, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  contact_statuses = mixin_cls->get_contact_statuses (obj, contacts, &error);

  if (!contact_statuses)
    {
      dbus_g_method_return_error (context, error);
      g_error_free(error);
      return;
    }

  presence_hash = construct_presence_hash (mixin_cls->statuses,
      contact_statuses);
  tp_svc_connection_interface_presence_return_from_get_presence (context,
      presence_hash);
  g_hash_table_destroy (presence_hash);
  g_hash_table_destroy (contact_statuses);
}


static GHashTable *
get_statuses_arguments (const TpPresenceStatusOptionalArgumentSpec *specs)
{
  GHashTable *arguments = g_hash_table_new (g_str_hash, g_str_equal);
  int i;

  for (i=0; specs != NULL && specs[i].name != NULL; i++)
    g_hash_table_insert (arguments, (gchar *) specs[i].name,
        (gchar *) specs[i].dtype);

  return arguments;
}


/**
 * tp_presence_mixin_get_statuses:
 *
 * Implements D-Bus method GetStatuses
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_get_statuses (TpSvcConnectionInterfacePresence *iface,
                                DBusGMethodInvocation *context)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  GObject *obj = (GObject *) conn;
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GHashTable *ret;
  GValueArray *status;
  int i;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  ret = g_hash_table_new_full (g_str_hash, g_str_equal,
                               NULL, (GDestroyNotify) g_value_array_free);

  for (i=0; mixin_cls->statuses[i].name != NULL; i++)
    {
      if (mixin_cls->status_available && !mixin_cls->status_available(obj, i))
        continue;

      status = g_value_array_new (5);

      g_value_array_append (status, NULL);
      g_value_init (g_value_array_get_nth (status, 0), G_TYPE_UINT);
      g_value_set_uint (g_value_array_get_nth (status, 0),
          mixin_cls->statuses[i].presence_type);

      g_value_array_append (status, NULL);
      g_value_init (g_value_array_get_nth (status, 1), G_TYPE_BOOLEAN);
      g_value_set_boolean (g_value_array_get_nth (status, 1),
          mixin_cls->statuses[i].self);

      /* everything is exclusive */
      g_value_array_append (status, NULL);
      g_value_init (g_value_array_get_nth (status, 2), G_TYPE_BOOLEAN);
      g_value_set_boolean (g_value_array_get_nth (status, 2),
          TRUE);

      g_value_array_append (status, NULL);
      g_value_init (g_value_array_get_nth (status, 3),
          DBUS_TYPE_G_STRING_STRING_HASHTABLE);
      g_value_set_static_boxed (g_value_array_get_nth (status, 3),
          get_statuses_arguments (mixin_cls->statuses[i].optional_arguments));

      g_hash_table_insert (ret, (gchar*) mixin_cls->statuses[i].name,
          status);
    }

  tp_svc_connection_interface_presence_return_from_get_statuses (context, ret);
  g_hash_table_destroy (ret);
}


/**
 * tp_presence_mixin_set_last_activity_time
 *
 * Implements D-Bus method SetLastActivityTime
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_set_last_activity_time (TpSvcConnectionInterfacePresence *iface,
                                          guint timestamp,
                                          DBusGMethodInvocation *context)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  tp_svc_connection_interface_presence_return_from_set_last_activity_time (
      context);
}


/**
 * tp_presence_mixin_remove_status:
 *
 * Implements D-Bus method GetStatuses
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_remove_status (TpSvcConnectionInterfacePresence *iface,
                                 const gchar *status,
                                 DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GArray *self_contacts;
  GError *error = NULL;
  GHashTable *self_contact_statuses;
  TpPresenceStatus *self_status;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  self_contacts = g_array_sized_new (TRUE, TRUE, sizeof(TpHandle), 1);
  g_array_append_val (self_contacts, conn->self_handle);
  self_contact_statuses = mixin_cls->get_contact_statuses (obj, self_contacts,
      &error);

  if (!self_contact_statuses)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      g_array_free (self_contacts, TRUE);
      return;
    }

  self_status = (TpPresenceStatus *) g_hash_table_lookup (self_contact_statuses,
      GUINT_TO_POINTER (conn->self_handle));

  if (!self_status)
    {
      DEBUG ("Got no self status, assuming we already have default status");
      g_array_free (self_contacts, TRUE);
      g_hash_table_destroy (self_contact_statuses);
      tp_svc_connection_interface_presence_return_from_remove_status (context);
      return;
    }

  if (!tp_strdiff (status, mixin_cls->statuses[self_status->index].name))
    {
      if (mixin_cls->set_own_status (obj, NULL, &error))
        {
          tp_svc_connection_interface_presence_return_from_remove_status (context);
        }
      else
        {
          dbus_g_method_return_error (context, error);
          g_error_free (error);
        }
    }
  else
    {
      GError nonexistent = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Attempting to remove non-existent presence." };
      dbus_g_method_return_error (context, &nonexistent);
    }

  g_array_free (self_contacts, TRUE);
  g_hash_table_destroy (self_contact_statuses);
}


/**
 * tp_presence_mixin_request_presence
 *
 * Implements D-Bus method RequestPresence
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_request_presence (TpSvcConnectionInterfacePresence *iface,
                                    const GArray *contacts,
                                    DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_HANDLE_TYPE_CONTACT);
  GHashTable *contact_statuses;
  GError *error = NULL;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (contacts->len == 0)
    {
      tp_svc_connection_interface_presence_return_from_request_presence (context);
      return;
    }

  if (!tp_handles_are_valid (contact_repo, contacts, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  contact_statuses = mixin_cls->get_contact_statuses (obj, contacts, &error);

  if (!contact_statuses)
    {
      dbus_g_method_return_error (context, error);
      g_error_free(error);
      return;
    }

  tp_presence_mixin_emit_presence_update (obj, contact_statuses);
  tp_svc_connection_interface_presence_return_from_request_presence (context);

  g_hash_table_destroy (contact_statuses);
}


struct _i_hate_g_hash_table_foreach {
  GObject *obj;
  GError **error;
  gboolean retval;
};


static void
set_status_foreach (gpointer key, gpointer value, gpointer user_data)
{
  struct _i_hate_g_hash_table_foreach *data =
    (struct _i_hate_g_hash_table_foreach*) user_data;
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (data->obj));
  TpPresenceStatus status_to_set = { 0, };
  int i;

  DEBUG ("called.");

  /* This function will actually only be invoked once for one SetStatus request,
   * since we check that the hash table has size 1 in
   * tp_presence_mixin_set_status(). Therefore there are no problems with
   * sharing the foreach data like this.
   */

  for (i = 0; mixin_cls->statuses[i].name != NULL; i++)
    {
      if (!tp_strdiff (mixin_cls->statuses[i].name, (const gchar *) key))
        break;
    }

  if (mixin_cls->statuses[i].name != NULL)
    {
      GHashTable *optional_arguments = NULL;

      DEBUG ("Found status \"%s\", checking if it's available...",
          (const gchar *) key);

      if (mixin_cls->status_available
          && !mixin_cls->status_available (data->obj, i))
        {
          DEBUG ("requested status %s is not available", (const gchar *) key);
          g_set_error (data->error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
              "requested status '%s' is not available on this connection",
              (const gchar *) key);
          data->retval = FALSE;
          return;
        }

      DEBUG ("The status is available.");

      if (value)
        {
          GHashTable *provided_arguments = (GHashTable *) value;
          int j;
          const TpPresenceStatusOptionalArgumentSpec *specs =
            mixin_cls->statuses[i].optional_arguments;

          for (j=0; specs != NULL && specs[j].name != NULL; j++)
            {
              GValue *provided_value =
                g_hash_table_lookup (provided_arguments, specs[j].name);
              GValue *new_value;

              if (!provided_value)
                continue;
              new_value = tp_g_value_slice_dup (provided_value);

              if (!optional_arguments)
                optional_arguments =
                  g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                      (GDestroyNotify) tp_g_value_slice_free);

              if (DEBUGGING)
                {
                  gchar *value_contents = g_strdup_value_contents (new_value);
                  DEBUG ("Got optional argument (\"%s\", %s)", specs[j].name,
                      value_contents);
                  g_free (value_contents);
                }

              g_hash_table_insert (optional_arguments,
                  (gpointer) specs[j].name, new_value);
            }
        }

      status_to_set.index = i;
      status_to_set.optional_arguments = optional_arguments;

      DEBUG ("About to try setting status \"%s\"", mixin_cls->statuses[i].name);

      if (!mixin_cls->set_own_status (data->obj, &status_to_set, data->error))
        {
          DEBUG ("failed to set status");
          data->retval = FALSE;
        }

      if (optional_arguments)
        g_hash_table_destroy (optional_arguments);
    }
  else
    {
      DEBUG ("got unknown status identifier %s", (const gchar *) key);
      g_set_error (data->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "unknown status identifier: %s", (const gchar *) key);
      data->retval = FALSE;
    }
}


/**
 * tp_presence_mixin_set_status
 *
 * Implements D-Bus method SetStatus
 * on interface org.freedesktop.Telepathy.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_set_status (TpSvcConnectionInterfacePresence *iface,
                              GHashTable *statuses,
                              DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  struct _i_hate_g_hash_table_foreach data = { NULL, NULL, TRUE };
  GError *error = NULL;

  DEBUG ("called.");

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (g_hash_table_size (statuses) != 1)
    {
      GError invalid = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Only one status may be set at a time in this protocol" };
      DEBUG ("got more than one status");
      dbus_g_method_return_error (context, &invalid);
      return;
    }

  data.obj = obj;
  data.error = &error;
  g_hash_table_foreach (statuses, set_status_foreach, &data);

  if (data.retval)
    {
      tp_svc_connection_interface_presence_return_from_set_status (
          context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}


/**
 * tp_presence_mixin_iface_init:
 * @g_iface: A pointer to the #TpSvcConnectionInterfacePresenceClass in an
 *  object class
 * @iface_data: Ignored
 *
 * Fill in the vtable entries needed to implement the presence interface using
 * this mixin. This function should usually be called via G_IMPLEMENT_INTERFACE.
 */
void
tp_presence_mixin_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionInterfacePresenceClass *klass =
    (TpSvcConnectionInterfacePresenceClass *)g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_presence_implement_##x (klass,\
    tp_presence_mixin_##x)
  IMPLEMENT(add_status);
  IMPLEMENT(clear_status);
  IMPLEMENT(get_presence);
  IMPLEMENT(get_statuses);
  IMPLEMENT(remove_status);
  IMPLEMENT(request_presence);
  IMPLEMENT(set_last_activity_time);
  IMPLEMENT(set_status);
#undef IMPLEMENT
}
