/*
 * presence-mixin.c - Source for TpPresenceMixin
 * Copyright (C) 2005-2008 Collabora Ltd.
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
 *
 * This mixin can be added to a #TpBaseConnection subclass to implement the
 * Presence interface.
 *
 * To use the presence mixin, include a #TpPresenceMixinClass somewhere in your
 * class structure and a #TpPresenceMixin somewhere in your instance structure,
 * and call tp_presence_mixin_class_init() from your class_init function,
 * tp_presence_mixin_init() from your init function or constructor, and
 * tp_presence_mixin_finalize() from your dispose or finalize function.
 *
 * <section>
 * <title>Implementing Presence</title>
 * <para>
 *   Since 0.7.13 this mixin supports the entire Presence interface.
 *   You can implement #TpSvcConnectionInterfacePresence as follows:
 *   <itemizedlist>
 *     <listitem>
 *       <para>use the #TpContactsMixin and
 *        <link linkend="telepathy-glib-dbus-properties-mixin">TpDBusPropertiesMixin</link>;</para>
 *     </listitem>
 *     <listitem>
 *       <para>pass tp_presence_mixin_iface_init() as an
 *         argument to G_IMPLEMENT_INTERFACE(), like so:
 *       </para>
 *       |[
 *       G_DEFINE_TYPE_WITH_CODE (MyConnection, my_connection,
 *           TP_TYPE_BASE_CONNECTION,
 *           // ...
 *           G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_PRESENCE,
 *               tp_presence_mixin_iface_init);
 *           // ...
 *           )
 *       ]|
 *     </listitem>
 *     <listitem>
 *       <para>
 *         call tp_presence_mixin_init_dbus_properties() in the
 *         #GTypeInfo class_init function;
 *       </para>
 *     </listitem>
 *     <listitem>
 *       <para>
 *         call tp_presence_mixin_register_with_contacts_mixin()
 *         in the #GObjectClass constructed function.
 *       </para>
 *     </listitem>
 *   </itemizedlist>
 * </para>
 * </section>
 *
 * Since: 0.5.13
 */

/**
 * TpPresenceStatusOptionalArgumentSpec:
 * @name: Name of the argument as passed over D-Bus
 * @dtype: D-Bus type signature of the argument
 *
 * Structure specifying a supported optional argument for a presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */

/**
 * TpPresenceStatusSpec:
 * @name: String identifier of the presence status
 * @presence_type: A type value, as specified by #TpConnectionPresenceType
 * @self: Indicates if this status may be set on yourself
 * @optional_arguments: An array of #TpPresenceStatusOptionalArgumentSpec
 *  structures representing the optional arguments for this status, terminated
 *  by a NULL name. If there are no optional arguments for a status, this can
 *  be NULL.
 *
 * Structure specifying a supported presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */

/**
 * TpPresenceStatus:
 * @index: Index of the presence status in the provided supported presence
 *  statuses array
 * @optional_arguments: A GHashTable mapping of string identifiers to GValues
 *  of the optional status arguments, if any. If there are no optional
 *  arguments, this pointer may be NULL.
 *
 * Structure representing a presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */


/**
 * TpPresenceMixinStatusAvailableFunc:
 * @obj: An instance of a #TpBaseConnection subclass implementing the presence
 *  interface with this mixin
 * @which: An index into the array of #TpPresenceStatusSpec provided to
 *  tp_presence_mixin_class_init()
 *
 * Signature of a callback to be used to determine if a given presence
 * status can be set on the connection. Most users of this mixin do not need to
 * supply an implementation of this callback: the value of
 * #TpPresenceStatusSpec.self is enough to determine whether this is a
 * user-settable presence, so %NULL should be passed to
 * tp_presence_mixin_class_init() for this callback.
 *
 * One place where this callback may be needed is on XMPP: not all server
 * implementation support the user becoming invisible. So an XMPP
 * implementation would implement this function, so that—once connected—the
 * hidden status is only available if the server supports it. Before the
 * connection is connected, this callback should return %TRUE for every status
 * that might possibly be supported: this allows the user to at least try to
 * sign in as invisible.
 *
 * Returns: %TRUE if the status can be set on this connection; %FALSE if not.
 */

/**
 * TpPresenceMixinGetContactStatusesFunc:
 * @obj: An object with this mixin.
 * @contacts: An array of #TpHandle for the contacts to get presence status for
 * @error: Used to return a Telepathy D-Bus error if %NULL is returned
 *
 * Signature of the callback used to get the stored presence status of
 * contacts. The returned hash table should have contact handles mapped to
 * their respective presence statuses in #TpPresenceStatus structs.
 *
 * The returned hash table will be freed with g_hash_table_unref. The
 * callback is responsible for ensuring that this does any cleanup that
 * may be necessary.
 *
 * Returns: (transfer full): The contact presence on success, %NULL with
 *  error set on error
 */

/**
 * TpPresenceMixinSetOwnStatusFunc:
 * @obj: An object with this mixin.
 * @status: The status to set, or NULL for whatever the protocol defines as a
 *  "default" status
 * @error: Used to return a Telepathy D-Bus error if %FALSE is returned
 *
 * Signature of the callback used to commit changes to the user's own presence
 * status in SetStatuses. It is also used in ClearStatus and RemoveStatus to
 * reset the user's own status back to the "default" one with a %NULL status
 * argument.
 *
 * The optional_arguments hash table in @status, if not NULL, will have been
 * filtered so it only contains recognised parameters, so the callback
 * need not (and cannot) check for unrecognised parameters. However, the
 * types of the parameters are not currently checked, so the callback is
 * responsible for doing so.
 *
 * The callback is responsible for emitting PresenceUpdate, if appropriate,
 * by calling tp_presence_mixin_emit_presence_update().
 *
 * Returns: %TRUE if the operation was successful, %FALSE if not.
 */

/**
 * TpPresenceMixinGetMaximumStatusMessageLengthFunc:
 * @obj: An object with this mixin.
 *
 * Signature of a callback used to determine the maximum length of status
 * messages. If this callback is provided and returns non-zero, the
 * #TpPresenceMixinSetOwnStatusFunc implementation is responsible for
 * truncating the message to fit this limit, if necessary.
 *
 * Returns: the maximum number of UTF-8 characters which may appear in a status
 * message, or 0 if there is no limit.
 * Since: 0.14.5
 */

/**
 * TpPresenceMixinClass:
 * @status_available: The status-available function that was passed to
 *  tp_presence_mixin_class_init()
 * @get_contact_statuses: The get-contact-statuses function that was passed to
 *  tp_presence_mixin_class_init()
 * @set_own_status: The set-own-status function that was passed to
 *  tp_presence_mixin_class_init()
 * @statuses: The presence statuses array that was passed to
 *  tp_presence_mixin_class_init()
 * @get_maximum_status_message_length: The callback used to discover the
 *  the limit for status messages length, if any. Since: 0.14.5
 *
 * Structure to be included in the class structure of objects that
 * use this mixin. Initialize it with tp_presence_mixin_class_init().
 *
 * If the protocol imposes a limit on the length of status messages, one should
 * implement @get_maximum_status_message_length. If this callback is not
 * implemented, it is assumed that there is no limit. The callback function
 * should be set after calling tp_presence_mixin_class_init(), like so:
 *
 * |[
 * TpPresenceMixinClass *mixin_class;
 *
 * tp_presence_mixin_class_init ((GObjectClass *) klass,
 *     G_STRUCT_OFFSET (SomeObjectClass, presence_mixin));
 * mixin_class = TP_PRESENCE_MIXIN_CLASS (klass);
 * mixin_class->get_maximum_status_message_length =
 *     some_object_get_maximum_status_message_length;
 * ]|
 *
 * All other fields should be considered read-only.
 */

/**
 * TpPresenceMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_presence_mixin_init().
 *
 * There are no public fields.
 */

#include "config.h"

#include <telepathy-glib/presence-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/contacts-mixin.h>
#include <telepathy-glib/svc-connection.h>

#define DEBUG_FLAG TP_DEBUG_PRESENCE

#include "debug-internal.h"


static GHashTable *construct_presence_hash (
  const TpPresenceStatusSpec *supported_statuses,
  GHashTable *contact_statuses);

/*
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
 * tp_presence_status_new: (skip)
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
 * tp_presence_status_free: (skip)
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
    g_hash_table_unref (status->optional_arguments);

  g_slice_free (TpPresenceStatus, status);
}


/**
 * tp_presence_mixin_class_get_offset_quark: (skip)
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
 * tp_presence_mixin_get_offset_quark: (skip)
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
 * tp_presence_mixin_class_init: (skip)
 * @obj_cls: The class of the implementation that uses this mixin
 * @offset: The byte offset of the TpPresenceMixinClass within the class
 * structure
 * @status_available: A callback to be used to determine if a given presence
 *  status can be set on a particular connection. Should usually be %NULL, to
 *  consider all statuses with #TpPresenceStatusSpec.self set to %TRUE to be
 *  settable.
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
 * tp_presence_mixin_class_init ((GObjectClass *) klass,
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
  guint i;

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
  mixin_cls->get_maximum_status_message_length = NULL;

  for (i = 0; statuses[i].name != NULL; i++)
    {
      if (statuses[i].self)
        {
          switch (statuses[i].presence_type)
            {
            case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
            case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
            case TP_CONNECTION_PRESENCE_TYPE_ERROR:
              WARNING ("Status \"%s\" of type %u should not be available "
                  "to set on yourself", statuses[i].name,
                  statuses[i].presence_type);
              break;

            default:
              break;
            }
        }
    }
}

/**
 * tp_presence_mixin_init: (skip)
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpPresenceMixin within the object structure
 *
 * Initialize the presence mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_presence_mixin_init ((GObject *) self,
 *                         G_STRUCT_OFFSET (SomeObject, presence_mixin));
 * </programlisting></informalexample>
 */
void
tp_presence_mixin_init (GObject *obj,
                        glong offset)
{
  DEBUG ("called.");

  g_assert (G_IS_OBJECT (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_PRESENCE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));
}

/**
 * tp_presence_mixin_finalize: (skip)
 * @obj: An object with this mixin.
 *
 * Free resources held by the presence mixin.
 */
void
tp_presence_mixin_finalize (GObject *obj)
{
  DEBUG ("%p", obj);

  /* free any data held directly by the object here */
}

/**
 * tp_presence_mixin_emit_presence_update: (skip)
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
  tp_svc_connection_interface_presence_emit_presences_changed (obj,
      presence_hash);

  g_hash_table_unref (presence_hash);
}


/**
 * tp_presence_mixin_emit_one_presence_update: (skip)
 * @obj: A connection object with this mixin
 * @handle: The handle of the contact to emit the signal for
 * @status: The new status to emit
 *
 * Emit a presence update signal for a single contact. This method is
 * just a convenience wrapper around
 * #tp_presence_mixin_emit_presence_update.
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

  g_hash_table_unref (contact_statuses);
}

static gboolean
check_status_available (GObject *object,
                        TpPresenceMixinClass *mixin_cls,
                        guint i,
                        GError **error,
                        gboolean for_self)
{
  if (for_self)
    {
      if (!mixin_cls->statuses[i].self)
        {
          g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "cannot set status '%s' on yourself",
              mixin_cls->statuses[i].name);
          return FALSE;
        }

      /* never allow OFFLINE, UNKNOWN or ERROR - if the CM says they're
       * OK to set on yourself, then it's wrong */
      switch (mixin_cls->statuses[i].presence_type)
        {
        case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
        case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
        case TP_CONNECTION_PRESENCE_TYPE_ERROR:
          g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "cannot set offline/unknown/error status '%s' on yourself",
              mixin_cls->statuses[i].name);
          return FALSE;

        default:
          break;
        }
    }

  if (mixin_cls->status_available
      && !mixin_cls->status_available (object, i))
    {
      DEBUG ("requested status %s is not available",
          mixin_cls->statuses[i].name);
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "requested status '%s' is not available on this connection",
          mixin_cls->statuses[i].name);
      return FALSE;
    }

  return TRUE;
}

static int
check_for_status (GObject *object, const gchar *status, GError **error)
{
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (object));
  int i;

  for (i = 0; mixin_cls->statuses[i].name != NULL; i++)
    {
      if (!tp_strdiff (mixin_cls->statuses[i].name, status))
        break;
    }

  if (mixin_cls->statuses[i].name != NULL)
    {
      DEBUG ("Found status \"%s\", checking if it's available...",
          (const gchar *) status);

      if (!check_status_available (object, mixin_cls, i, error, TRUE))
        return -1;
    }
  else
    {
      DEBUG ("got unknown status identifier %s", status);
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "unknown status identifier: %s", status);
      return -1;
    }

  return i;
}

enum {
  MIXIN_DP_STATUSES,
  MIXIN_DP_MAX_STATUS_MESSAGE_LENGTH,
  NUM_MIXIN_DBUS_PROPERTIES
};

static TpDBusPropertiesMixinPropImpl known_presence_props[] = {
  { "Statuses", NULL, NULL },
  { "MaximumStatusMessageLength", NULL, NULL },
  { NULL }
};

static void
tp_presence_mixin_get_dbus_property (GObject *object,
                                     GQuark interface,
                                     GQuark name,
                                     GValue *value,
                                     gpointer unused G_GNUC_UNUSED)
{
  TpPresenceMixinClass *mixin_cls =
      TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (object));
  static GQuark q[NUM_MIXIN_DBUS_PROPERTIES] = { 0, };

  DEBUG ("called.");

  if (G_UNLIKELY (q[0] == 0))
    {
      q[MIXIN_DP_STATUSES] = g_quark_from_static_string ("Statuses");
      q[MIXIN_DP_MAX_STATUS_MESSAGE_LENGTH] =
          g_quark_from_static_string ("MaximumStatusMessageLength");
    }

  g_return_if_fail (object != NULL);

  if (name == q[MIXIN_DP_STATUSES])
    {
      GHashTable *ret;
      GValueArray *status;
      int i;

      g_return_if_fail (G_VALUE_HOLDS_BOXED (value));

      ret = g_hash_table_new_full (g_str_hash, g_str_equal,
                               NULL, (GDestroyNotify) g_value_array_free);

      for (i=0; mixin_cls->statuses[i].name != NULL; i++)
        {
          const TpPresenceStatusOptionalArgumentSpec *specs;
          int j;
          gboolean message = FALSE;

          /* we include statuses here even if they're not available
           * to set on yourself */
          if (!check_status_available (object, mixin_cls, i, NULL, FALSE))
            continue;

          specs = mixin_cls->statuses[i].optional_arguments;

          for (j = 0; specs != NULL && specs[j].name != NULL; j++)
            {
              if (!tp_strdiff (specs[j].name, "message"))
                {
                  message = TRUE;
                  break;
                }
            }

         status = g_value_array_new (3);

         g_value_array_append (status, NULL);
         g_value_init (g_value_array_get_nth (status, 0), G_TYPE_UINT);
         g_value_set_uint (g_value_array_get_nth (status, 0),
             mixin_cls->statuses[i].presence_type);

         g_value_array_append (status, NULL);
         g_value_init (g_value_array_get_nth (status, 1), G_TYPE_BOOLEAN);
         g_value_set_boolean (g_value_array_get_nth (status, 1),
             mixin_cls->statuses[i].self);

         g_value_array_append (status, NULL);
         g_value_init (g_value_array_get_nth (status, 2), G_TYPE_BOOLEAN);
         g_value_set_boolean (g_value_array_get_nth (status, 2), message);

         g_hash_table_insert (ret, (gchar *) mixin_cls->statuses[i].name,
             status);
       }
       g_value_take_boxed (value, ret);
    }
  else if (name == q[MIXIN_DP_MAX_STATUS_MESSAGE_LENGTH])
    {
      guint max_status_message_length = 0;

      g_assert (G_VALUE_HOLDS (value, G_TYPE_UINT));

      if (mixin_cls->get_maximum_status_message_length != NULL)
        max_status_message_length =
            mixin_cls->get_maximum_status_message_length (object);

      g_value_set_uint (value, max_status_message_length);
    }
  else
    {
      g_return_if_reached ();
    }

}

/**
 * tp_presence_mixin_init_dbus_properties: (skip)
 * @cls: The class of an object with this mixin
 *
 * Set up #TpDBusPropertiesMixinClass to use this mixin's implementation of
 * the Presence interface's properties.
 *
 * This automatically sets up a list of the supported properties for the
 * Presence interface.
 *
 * Since: 0.7.13
 */
void
tp_presence_mixin_init_dbus_properties (GObjectClass *cls)
{
  tp_dbus_properties_mixin_implement_interface (cls,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_PRESENCE,
      tp_presence_mixin_get_dbus_property,
      NULL, known_presence_props);
}

/*
 * tp_presence_mixin_set_presence:
 *
 * Implements D-Bus method SetPresence
 * on interface im.telepathy1.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_set_presence (
    TpSvcConnectionInterfacePresence *iface,
    const gchar *status,
    const gchar *message,
    DBusGMethodInvocation *context)
{
  GObject *obj = (GObject *) iface;
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  TpPresenceStatus status_to_set = { 0, };
  int s;
  GError *error = NULL;
  GHashTable *optional_arguments = NULL;

  DEBUG ("called.");

  s = check_for_status (obj, status, &error);
  if (s == -1)
    goto out;

  status_to_set.index = s;

  if (*message != '\0')
    {
      optional_arguments = g_hash_table_new_full (g_str_hash, g_str_equal,
          NULL, (GDestroyNotify) tp_g_value_slice_free);
      g_hash_table_insert (optional_arguments, "message",
          tp_g_value_slice_new_string (message));
      status_to_set.optional_arguments = optional_arguments;
    }

  mixin_cls->set_own_status (obj, &status_to_set, &error);

out:
  if (error == NULL)
    {
      tp_svc_connection_interface_presence_return_from_set_presence (
          context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }

  if (optional_arguments != NULL)
    g_hash_table_unref (optional_arguments);
}

static GValueArray *
construct_presence_value_array (TpPresenceStatus *status,
    const TpPresenceStatusSpec *supported_statuses)
{
  TpConnectionPresenceType status_type;
  const gchar *status_name;
  const gchar *message = NULL;
  GValueArray *presence;

  status_name = supported_statuses[status->index].name;
  status_type = supported_statuses[status->index].presence_type;

  if (status->optional_arguments != NULL)
    {
      GValue *val;
      val = g_hash_table_lookup (status->optional_arguments, "message");
      if (val != NULL)
        message = g_value_get_string (val);
    }

  if (message == NULL)
    message = "";

  presence = g_value_array_new (3);

  g_value_array_append (presence, NULL);
  g_value_init (g_value_array_get_nth (presence, 0), G_TYPE_UINT);
  g_value_set_uint (g_value_array_get_nth (presence, 0), status_type);

  g_value_array_append (presence, NULL);
  g_value_init (g_value_array_get_nth (presence, 1), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (presence, 1), status_name);

  g_value_array_append (presence, NULL);
  g_value_init (g_value_array_get_nth (presence, 2), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (presence, 2), message);

  return presence;
}

static void
construct_presence_hash_foreach (
    GHashTable *presence_hash,
    const TpPresenceStatusSpec *supported_statuses,
    TpHandle handle,
    TpPresenceStatus *status)
{
  GValueArray *presence;

  presence = construct_presence_value_array (status, supported_statuses);
  g_hash_table_insert (presence_hash, GUINT_TO_POINTER (handle), presence);
}

static GHashTable *
construct_presence_hash (const TpPresenceStatusSpec *supported_statuses,
                         GHashTable *contact_statuses)
{
  GHashTable *presence_hash = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_value_array_free);
  GHashTableIter iter;
  gpointer key, value;

  DEBUG ("called.");

  g_hash_table_iter_init (&iter, contact_statuses);
  while (g_hash_table_iter_next (&iter, &key, &value))
    construct_presence_hash_foreach (presence_hash, supported_statuses,
        GPOINTER_TO_UINT (key), value);

  return presence_hash;
}

/*
 * tp_presence_mixin_get_presence:
 *
 * Implements D-Bus method GetPresence
 * on interface im.telepathy1.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_presence_mixin_get_presences (
    TpSvcConnectionInterfacePresence *iface,
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
      tp_svc_connection_interface_presence_return_from_get_presences (
        context, presence_hash);
      g_hash_table_unref (presence_hash);
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
      g_error_free (error);
      return;
    }

  presence_hash = construct_presence_hash (mixin_cls->statuses,
      contact_statuses);
  tp_svc_connection_interface_presence_return_from_get_presences (
      context, presence_hash);
  g_hash_table_unref (presence_hash);
  g_hash_table_unref (contact_statuses);
}

/**
 * tp_presence_mixin_iface_init: (skip)
 * @g_iface: A pointer to the #TpSvcConnectionInterfacePresenceClass in
 * an object class
 * @iface_data: Ignored
 *
 * Fill in the vtable entries needed to implement the presence interface
 * using this mixin. This function should usually be called via
 * G_IMPLEMENT_INTERFACE.
 *
 * Since: 0.7.13
 */
void
tp_presence_mixin_iface_init (gpointer g_iface,
                                       gpointer iface_data)
{
  TpSvcConnectionInterfacePresenceClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_presence_implement_##x\
 (klass, tp_presence_mixin_##x)
  IMPLEMENT(set_presence);
  IMPLEMENT(get_presences);
#undef IMPLEMENT
}

static void
tp_presence_mixin_fill_contact_attributes (GObject *obj,
  const GArray *contacts, GHashTable *attributes_hash)
{
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GHashTable *contact_statuses;
  GError *error = NULL;

  contact_statuses = mixin_cls->get_contact_statuses (obj, contacts, &error);

  if (contact_statuses == NULL)
    {
      DEBUG ("get_contact_statuses failed: %s", error->message);
      g_error_free (error);
    }
  else
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, contact_statuses);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          TpHandle handle = GPOINTER_TO_UINT (key);
          TpPresenceStatus *status = value;
          GValueArray *presence = construct_presence_value_array (
              status, mixin_cls->statuses);

          tp_contacts_mixin_set_contact_attribute (attributes_hash, handle,
              TP_TOKEN_CONNECTION_INTERFACE_PRESENCE_PRESENCE,
              tp_g_value_slice_new_take_boxed (G_TYPE_VALUE_ARRAY, presence));
        }

      g_hash_table_unref (contact_statuses);
    }
}

/**
 * tp_presence_mixin_register_with_contacts_mixin: (skip)
 * @obj: An instance that of the implementation that uses both the Contacts
 * mixin and this mixin
 *
 * Register the Presence interface with the Contacts interface to make it
 * inspectable. The Contacts mixin should be initialized before this function
 * is called
 */
void
tp_presence_mixin_register_with_contacts_mixin (GObject *obj)
{
  tp_contacts_mixin_add_contact_attributes_iface (obj,
      TP_IFACE_CONNECTION_INTERFACE_PRESENCE,
      tp_presence_mixin_fill_contact_attributes);
}

