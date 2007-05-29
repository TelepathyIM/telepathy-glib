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
 * interface
 * @see_also: #TpSvcConnectionInterfacePresence
 *
 * This mixin can be added to a connection GObject class to implement the
 * presence interface in a general way. It does not support protocols where it
 * is possible to set multiple statuses on yourself at once, however. Hence all
 * presence statuses will have the exclusive flag set.
 *
 * To use the presence mixin, include a #TpPresenceMixinClass somewhere in your
 * class structure and a #TpPresenceMixin somewhere in your instance structure,
 * and call tp_presence_mixin_class_init() from your class_init function,
 * tp_presence_mixin_init() from your init function or constructor, and
 * tp_presence_mixin_finalize() from your dispose or finalize function.
 *
 * To use the presence mixin as the implementation of
 * #TpSvcConnectionInterfacePresence, in the function you pass to
 * G_IMPLEMENT_INTERFACE, you should first call tp_presence_mixin_iface_init(),
 * then call ...
 */

#include <telepathy-glib/presence-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>

#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>

#define DEBUG_FLAG TP_DEBUG_PRESENCE

#include "internal-debug.h"

struct _TpPresenceMixinPrivate
{
  /* ... */
};

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
 *  status is available. If NULL, all statuses are always considered available.
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
                              const TpPresenceStatusSpec *statuses)
{
  TpPresenceMixinClass *mixin_cls;

  g_assert (statuses != NULL);

  g_assert (G_IS_OBJECT_CLASS (obj_cls));

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (obj_cls),
      TP_PRESENCE_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  mixin_cls = TP_PRESENCE_MIXIN_CLASS (obj_cls);

  mixin_cls->status_available = status_available;
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


static void
tp_presence_mixin_add_status (TpSvcConnectionInterfacePresence *iface,
                              const gchar *status,
                              GHashTable *parms,
                              DBusGMethodInvocation *context)
{
  GError error = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
    "Only one status is possible at a time with this protocol!" };

  dbus_g_method_return_error (context, &error);
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
  GObject *obj = (GObject *) iface;
  TpPresenceMixinClass *mixin_cls =
    TP_PRESENCE_MIXIN_CLASS (G_OBJECT_GET_CLASS (obj));
  GHashTable *ret;
  GValueArray *status;
  int i;

  DEBUG ("called.");

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
 */
static void
tp_presence_mixin_set_last_activity_time (TpSvcConnectionInterfacePresence *iface,
                                          guint time,
                                          DBusGMethodInvocation *context)
{
  tp_svc_connection_interface_presence_return_from_set_last_activity_time (
      context);
}


/**
 * tp_presence_mixin_iface_init:
 * @g_iface: A pointer to the #TpSvcConnectionInterfacePresenceClass in an
 * object class
 * @iface_data: Ignored
 *
 * ...
 */
void
tp_presence_mixin_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionInterfacePresenceClass *klass =
    (TpSvcConnectionInterfacePresenceClass *)g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_presence_implement_##x (klass,\
    tp_presence_mixin_##x)
  IMPLEMENT(add_status);
  IMPLEMENT(get_statuses);
  IMPLEMENT(set_last_activity_time);
#undef IMPLEMENT
}
