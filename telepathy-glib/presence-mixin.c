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
 *   You can implement #TpSvcConnectionInterfacePresence1 as follows:
 *   <itemizedlist>
 *     <listitem>
 *       <para>in the #TpBaseConnectionClass.fill_contact_attributes
 *       implementation, call tp_presence_mixin_fill_contact_attributes()
 *       and do not chain up if it returns %TRUE:
 *       </para>
 * |[
 * // ...
 * if (!tp_strdiff (dbus_interface, MY_IFACE_CONNECTION_INTERFACE_HATS))
 *   {
 *     // ... fill in Hats attributes ...
 *     return;
 *   }
 *
 * if (tp_presence_mixin_fill_contact_attributes (G_OBJECT (self),
 *         dbus_interface, contact, attributes))
 *   {
 *     return;
 *   }
 *
 * ((TpBaseConnectionClass *) my_connection_parent_class)->
 *     fill_contact_attributes (self, dbus_interface, contact, attributes);
 * ]|
 *      </listitem>
 *      </itemizedlist>
 *    </para>
 * </section>
 *
 * Since: 0.5.13
 */

/**
 * TpPresenceStatusSpec:
 * @name: String identifier of the presence status
 * @presence_type: A type value, as specified by #TpConnectionPresenceType
 * @self: Indicates if this status may be set on yourself
 * @has_message: %TRUE if a human-readable message can accompany this status.
 *
 * Structure specifying a supported presence status.
 *
 * In addition to the fields documented here, there are some reserved fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */

/**
 * TpPresenceStatus:
 * @index: Index of the presence status in the provided supported presence
 *  statuses array
 * @message: the non-%NULL human-readable status message
 *
 * Structure representing a presence status.
 *
 * In addition to the fields documented here, there are some gpointer fields
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
 * TpPresenceMixinGetContactStatusFunc:
 * @obj: An object with this mixin.
 * @contact: A #TpHandle of type %TP_ENTITY_TYPE_CONTACT
 *
 * Return the contact's status
 *
 * Returns: (transfer full): The contact's presence status
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
 * @get_contact_status: The get-contact-status function that was passed to
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

#include <telepathy-glib/_gdbus/Connection_Interface_Presence1.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-connection.h>
#include <telepathy-glib/value-array.h>

#define DEBUG_FLAG TP_DEBUG_PRESENCE

#include "debug-internal.h"

struct _TpPresenceMixinPrivate
{
  _TpGDBusConnectionInterfacePresence1 *skeleton;
};


static GVariant *construct_presence_hash (
  const TpPresenceStatusSpec *supported_statuses,
  GHashTable *contact_statuses);

/**
 * tp_presence_status_new: (skip)
 * @which: Index of the presence status in the provided supported presence
 *  statuses array
 * @message: (allow-none): a human-readable status message, or %NULL
 *
 * Construct a presence status structure. You should free the returned
 * structure with #tp_presence_status_free.
 *
 * Returns: A pointer to the newly allocated presence status structure.
 */
TpPresenceStatus *
tp_presence_status_new (guint which,
    const gchar *message)
{
  TpPresenceStatus *status = g_slice_new (TpPresenceStatus);

  status->index = which;

  if (message == NULL)
    message = "";

  status->message = g_strdup (message);

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

  g_free (status->message);
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
 * @get_contact_status: A callback to be used get the current presence status
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
                              TpPresenceMixinGetContactStatusFunc get_contact_status,
                              TpPresenceMixinSetOwnStatusFunc set_own_status,
                              const TpPresenceStatusSpec *statuses)
{
  TpPresenceMixinClass *mixin_cls;
  guint i;

  DEBUG ("called.");

  g_assert (get_contact_status != NULL);
  g_assert (set_own_status != NULL);
  g_assert (statuses != NULL);

  g_assert (G_IS_OBJECT_CLASS (obj_cls));

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (obj_cls),
      TP_PRESENCE_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  mixin_cls = TP_PRESENCE_MIXIN_CLASS (obj_cls);

  mixin_cls->status_available = status_available;
  mixin_cls->get_contact_status = get_contact_status;
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

static void update_statuses_property (TpPresenceMixin *self,
    GObject *object);
static void update_max_status_message_len_property (TpPresenceMixin *self,
    GObject *object);
static gboolean tp_presence_mixin_set_presence (
    _TpGDBusConnectionInterfacePresence1 *skeleton,
    GDBusMethodInvocation *context,
    const gchar *status,
    const gchar *message,
    GObject *obj);

static void
connection_status_changed_cb (GObject *object,
    TpConnectionStatus status,
    TpConnectionStatusReason reason,
    TpPresenceMixin *self)
{
  if (status == TP_CONNECTION_STATUS_CONNECTED)
    {
      update_statuses_property (self, object);
      update_max_status_message_len_property (self, object);
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
 *
 * Since 0.99.1 @obj must be a #TpBaseConnection subclass.
 */
void
tp_presence_mixin_init (GObject *obj,
                        glong offset)
{
  TpPresenceMixin *self;

  DEBUG ("called.");

  g_assert (TP_IS_BASE_CONNECTION (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_PRESENCE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  self = TP_PRESENCE_MIXIN (obj);
  self->priv = g_slice_new0 (TpPresenceMixinPrivate);

  self->priv->skeleton = _tp_gdbus_connection_interface_presence1_skeleton_new ();
  g_signal_connect_object (self->priv->skeleton, "handle-set-presence",
      G_CALLBACK (tp_presence_mixin_set_presence), obj, 0);

  /* Set the initial properties values, we'll update them once CONNECTED */
  update_max_status_message_len_property (self, obj);
  update_statuses_property (self, obj);
  g_signal_connect (obj, "status-changed",
      G_CALLBACK (connection_status_changed_cb), self);

  g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (obj),
      G_DBUS_INTERFACE_SKELETON (self->priv->skeleton));
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
  TpPresenceMixin *self = TP_PRESENCE_MIXIN (obj);

  DEBUG ("%p", obj);

  g_object_unref (self->priv->skeleton);

  g_slice_free (TpPresenceMixinPrivate, self->priv);
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
  TpPresenceMixin *self = TP_PRESENCE_MIXIN (obj);
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));

  DEBUG ("called.");

  _tp_gdbus_connection_interface_presence1_emit_presences_changed (
      self->priv->skeleton,
      construct_presence_hash (mixin_cls->statuses, contact_statuses));
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

static void
update_statuses_property (TpPresenceMixin *self,
    GObject *object)
{
  TpPresenceMixinClass *mixin_cls =
      TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (object));
  GVariantBuilder builder;
  int i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{s(ubb)}"));
  for (i = 0; mixin_cls->statuses[i].name != NULL; i++)
    {
      gboolean message;

      /* we include statuses here even if they're not available
       * to set on yourself */
      if (!check_status_available (object, mixin_cls, i, NULL, FALSE))
        continue;

      message = tp_presence_status_spec_has_message (
          &mixin_cls->statuses[i]);

      g_variant_builder_add (&builder, "{s(ubb)}",
          mixin_cls->statuses[i].name,
          mixin_cls->statuses[i].presence_type,
          mixin_cls->statuses[i].self,
          message);
   }

  _tp_gdbus_connection_interface_presence1_set_statuses (self->priv->skeleton,
      g_variant_builder_end (&builder));
}

static void
update_max_status_message_len_property (TpPresenceMixin *self,
    GObject *object)
{
  TpPresenceMixinClass *mixin_cls =
      TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (object));
  guint max_status_message_length = 0;

  if (mixin_cls->get_maximum_status_message_length != NULL)
    max_status_message_length =
        mixin_cls->get_maximum_status_message_length (object);

  _tp_gdbus_connection_interface_presence1_set_maximum_status_message_length (
      self->priv->skeleton, max_status_message_length);
}

/*
 * tp_presence_mixin_set_presence:
 *
 * Implements D-Bus method SetPresence
 * on interface im.telepathy.v1.Connection.Interface.Presence
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static gboolean
tp_presence_mixin_set_presence (
    _TpGDBusConnectionInterfacePresence1 *skeleton,
    GDBusMethodInvocation *context,
    const gchar *status,
    const gchar *message,
    GObject *obj)
{
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  TpPresenceStatus status_to_set = { 0, };
  int s;
  GError *error = NULL;

  DEBUG ("called.");

  s = check_for_status (obj, status, &error);
  if (s == -1)
    goto out;

  if (message == NULL)
    message = "";

  status_to_set.index = s;
  status_to_set.message = g_strdup (message);

  mixin_cls->set_own_status (obj, &status_to_set, &error);

  g_free (status_to_set.message);

out:
  if (error == NULL)
    {
      _tp_gdbus_connection_interface_presence1_complete_set_presence (skeleton,
          context);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
    }

  return TRUE;
}

static GVariant *
construct_presence_variant (TpPresenceStatus *status,
    const TpPresenceStatusSpec *supported_statuses)
{
  TpConnectionPresenceType status_type;
  const gchar *status_name;
  const gchar *message = NULL;

  status_name = supported_statuses[status->index].name;
  status_type = supported_statuses[status->index].presence_type;

  message = status->message;

  if (message == NULL)
    message = "";

  return g_variant_new ("(uss)", status_type, status_name, message);
}

static GVariant *
construct_presence_hash (const TpPresenceStatusSpec *supported_statuses,
                         GHashTable *contact_statuses)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key, value;

  DEBUG ("called.");

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{u(uss)}"));
  g_hash_table_iter_init (&iter, contact_statuses);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      TpHandle handle = GPOINTER_TO_UINT (key);
      GVariant *presence;

      presence = construct_presence_variant (value, supported_statuses);
      g_variant_builder_add (&builder, "{u@(uss)}", handle, presence);
    }

  return g_variant_builder_end (&builder);
}

/**
 * tp_presence_mixin_fill_contact_attributes:
 * @obj: an object with a #TpPresenceMixin
 * @dbus_interface: a D-Bus interface
 * @contact: a contact
 * @attributes: used to return attributes
 *
 * If @dbus_interface is an interface that is relevant for this
 * object, fill @attributes with the attributes for @contact
 * and return %TRUE.
 *
 * Returns: %TRUE if @dbus_interface was recognised
 */
gboolean
tp_presence_mixin_fill_contact_attributes (GObject *obj,
  const gchar *dbus_interface,
  TpHandle contact,
  GVariantDict *attributes)
{
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  TpPresenceStatus *status;

  if (tp_strdiff (dbus_interface, TP_IFACE_CONNECTION_INTERFACE_PRESENCE1))
    return FALSE;

  status = mixin_cls->get_contact_status (obj, contact);

  if (status == NULL)
    {
      CRITICAL ("get_contact_status returned NULL");
    }
  else
    {
      g_variant_dict_insert_value (attributes,
          TP_TOKEN_CONNECTION_INTERFACE_PRESENCE1_PRESENCE,
          construct_presence_variant (status, mixin_cls->statuses));
      tp_presence_status_free (status);
    }
  return TRUE;
}

/* For now, self->priv is just self if heap-allocated, NULL if not. */
static gboolean
_tp_presence_status_spec_is_heap_allocated (const TpPresenceStatusSpec *self)
{
  return (self->priv == (TpPresenceStatusSpecPrivate *) self);
}

/**
 * tp_presence_status_spec_get_presence_type:
 * @self: a presence status specification
 *
 * Return the category into which this presence type falls. For instance,
 * for XMPP's "" (do not disturb) status, this would return
 * %TP_CONNECTION_PRESENCE_TYPE_BUSY.
 *
 * Returns: a #TpConnectionPresenceType
 * Since: 0.99.5
 */
TpConnectionPresenceType
tp_presence_status_spec_get_presence_type (const TpPresenceStatusSpec *self)
{
  g_return_val_if_fail (self != NULL, TP_CONNECTION_PRESENCE_TYPE_UNSET);

  return self->presence_type;
}

/**
 * tp_presence_status_spec_get_name:
 * @self: a presence status specification
 *
 * <!-- -->
 *
 * Returns: (transfer none): the name of this presence status,
 *  such as "available" or "out-to-lunch".
 * Since: 0.99.5
 */
const gchar *
tp_presence_status_spec_get_name (const TpPresenceStatusSpec *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->name;
}

/**
 * tp_presence_status_spec_can_set_on_self:
 * @self: a presence status specification
 *
 * <!-- -->
 *
 * Returns: %TRUE if the user can set this presence status on themselves (most
 *  statuses), or %FALSE if they cannot directly set it on
 *  themselves (typically used for %TP_CONNECTION_PRESENCE_TYPE_OFFLINE
 *  and %TP_CONNECTION_PRESENCE_TYPE_ERROR)
 * Since: 0.99.5
 */
gboolean
tp_presence_status_spec_can_set_on_self (const TpPresenceStatusSpec *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->self;
}

/**
 * tp_presence_status_spec_has_message:
 * @self: a presence status specification
 *
 * <!-- -->
 *
 * Returns: %TRUE if this presence status is accompanied by an optional
 *  human-readable message
 * Since: 0.99.5
 */
gboolean
tp_presence_status_spec_has_message (const TpPresenceStatusSpec *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->has_message;
}

/**
 * tp_presence_status_spec_new:
 * @name: the name of the new presence status
 * @type: the category into which this presence status falls
 * @can_set_on_self: %TRUE if the user can set this presence status
 *  on themselves
 * @has_message: %TRUE if this presence status is accompanied by an
 *  optional human-readable message
 *
 * <!-- -->
 *
 * Returns: (transfer full): a new #TpPresenceStatusSpec
 * Since: 0.99.5
 */
TpPresenceStatusSpec *
tp_presence_status_spec_new (const gchar *name,
    TpConnectionPresenceType type,
    gboolean can_set_on_self,
    gboolean has_message)
{
  TpPresenceStatusSpec *ret;

  g_return_val_if_fail (!tp_str_empty (name), NULL);
  g_return_val_if_fail (type >= 0 && type < TP_NUM_CONNECTION_PRESENCE_TYPES,
      NULL);

  ret = g_slice_new0 (TpPresenceStatusSpec);

  ret->name = g_strdup (name);
  ret->presence_type = type;
  ret->self = can_set_on_self;
  ret->has_message = has_message;

  /* dummy marker for "this is on the heap" rather than a real struct */
  ret->priv = (TpPresenceStatusSpecPrivate *) ret;

  return ret;
}

/**
 * tp_presence_status_spec_copy:
 * @self: a presence status specification
 *
 * Copy a presence status specification.
 *
 * Returns: (transfer full): a new #TpPresenceStatusSpec resembling @self
 * Since: 0.99.5
 */
TpPresenceStatusSpec *
tp_presence_status_spec_copy (const TpPresenceStatusSpec *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return tp_presence_status_spec_new (self->name, self->presence_type,
      self->self, tp_presence_status_spec_has_message (self));
}

/**
 * tp_presence_status_spec_free:
 * @self: (transfer full): a presence status specification
 *
 * Free a presence status specification produced by
 * tp_presence_status_spec_new() or tp_presence_status_spec_copy().
 *
 * Since: 0.99.5
 */
void
tp_presence_status_spec_free (TpPresenceStatusSpec *self)
{
  g_return_if_fail (_tp_presence_status_spec_is_heap_allocated (self));

  /* This struct was designed to always be on the stack, so freeing this
   * needs a non-const-correct cast */
  g_free ((gchar *) self->name);

  g_slice_free (TpPresenceStatusSpec, self);
}

G_DEFINE_BOXED_TYPE (TpPresenceStatusSpec, tp_presence_status_spec,
    tp_presence_status_spec_copy, tp_presence_status_spec_free)
