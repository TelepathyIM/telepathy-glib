/*
 * contacts-mixin.c - Source for TpContactsMixin
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nokia Corporation
 *   @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 * SECTION:contacts-mixin
 * @title: TpContactsMixin
 * @short_description: a mixin implementation of the contacts connection
 * interface
 * @see_also: #TpSvcConnectionInterfaceContacts
 *
 * This mixin can be added to a #TpBaseConnection subclass to implement the
 * Contacts interface in a generic way. FIXME
 */

#include <telepathy-glib/contacts-mixin.h>

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION

#include "debug-internal.h"

struct _TpContactsMixinPrivate
{
  /* String interface name -> GetAttributes func */
  GHashTable *interfaces;
};

enum {
  MIXIN_DP_CONTACTS_INSPECTABLE_INTERFACES,
  NUM_MIXIN_CONTACTS_DBUS_PROPERTIES
};

static TpDBusPropertiesMixinPropImpl known_contacts_props[] = {
  { "InspectableInterfaces", NULL, NULL },
  { NULL }
};

static void
tp_presence_mixin_get_contacts_dbus_property (GObject *object,
                                              GQuark interface,
                                              GQuark name,
                                              GValue *value,
                                              gpointer unused
                                              G_GNUC_UNUSED)
{
  static GQuark q[NUM_MIXIN_CONTACTS_DBUS_PROPERTIES] = { 0, };
  TpContactsMixin *self = TP_CONTACTS_MIXIN (object);

  DEBUG ("called.");

  if (G_UNLIKELY (q[0] == 0))
    {
      q[MIXIN_DP_CONTACTS_INSPECTABLE_INTERFACES] =
        g_quark_from_static_string ("InspectableInterfaces");
    }

  g_return_if_fail (object != NULL);

  if (name == q[MIXIN_DP_CONTACTS_INSPECTABLE_INTERFACES])
    {
      gchar **interfaces;
      GHashTableIter iter;
      gpointer key;
      int i = 0;

      g_assert (G_VALUE_HOLDS(value, G_TYPE_STRV));

      /* FIXME, cache this when connected ? */
      interfaces = g_malloc0(
        (g_hash_table_size (self->priv->interfaces) + 1) * sizeof (gchar *));

      g_hash_table_iter_init (&iter, self->priv->interfaces);
      while (g_hash_table_iter_next (&iter, &key, NULL))
          {
            interfaces[i] = g_strdup ((gchar *) key);
            i++;
          }
      g_value_set_boxed (value, interfaces);
    }
  else
    {
      g_assert_not_reached ();
    }
}


/**
 * tp_contacts_mixin_class_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObjectClass
 */
GQuark
tp_contacts_mixin_class_get_offset_quark ()
{
  static GQuark offset_quark = 0;

  if (G_UNLIKELY (offset_quark == 0))
    offset_quark = g_quark_from_static_string (
        "TpContactsMixinClassOffsetQuark");

  return offset_quark;
}

/**
 * tp_contacts_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
 */
GQuark
tp_contacts_mixin_get_offset_quark ()
{
  static GQuark offset_quark = 0;

  if (G_UNLIKELY (offset_quark == 0))
    offset_quark = g_quark_from_static_string ("TpContactsMixinOffsetQuark");

  return offset_quark;
}


/**
 * tp_contacts_mixin_class_init:
 * @obj_cls: The class of the implementation that uses this mixin
 * @offset: The byte offset of the TpContactsMixinClass within the class
 *          structure
 *
 * Initialize the contacts mixin. Should be called from the implementation's
 * class_init function like so:
 *
 * <informalexample><programlisting>
 * tp_contacts_mixin_class_init ((GObjectClass *)klass,
 *                          G_STRUCT_OFFSET (SomeObjectClass, contacts_mixin));
 * </programlisting></informalexample>
 */

void
tp_contacts_mixin_class_init (GObjectClass *obj_cls, glong offset)
{
  TpContactsMixinClass *mixin_cls;

  g_assert (G_IS_OBJECT_CLASS (obj_cls));

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (obj_cls),
      TP_CONTACTS_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  mixin_cls = TP_CONTACTS_MIXIN_CLASS (obj_cls);

  tp_dbus_properties_mixin_implement_interface (obj_cls,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS,
      tp_presence_mixin_get_contacts_dbus_property,
      NULL, known_contacts_props);
}


/**
 * tp_contacts_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpContactsMixin within the object structure
 *
 * Initialize the text mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_contacts_mixin_init ((GObject *)self,
 *                     G_STRUCT_OFFSET (SomeObject, text_mixin));
 * </programlisting></informalexample>
 */
void
tp_contacts_mixin_init (GObject *obj,
                    glong offset)
{
  TpContactsMixin *mixin;

  g_assert (G_IS_OBJECT (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_CONTACTS_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  mixin = TP_CONTACTS_MIXIN (obj);

  mixin->priv = g_slice_new0 (TpContactsMixinPrivate);
  mixin->priv->interfaces = g_hash_table_new_full (g_str_hash, g_str_equal,
    g_free, NULL);
}

/**
 * tp_contacts_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the text mixin.
 */
void
tp_contacts_mixin_finalize (GObject *obj)
{
  TpContactsMixin *mixin = TP_CONTACTS_MIXIN (obj);

  DEBUG ("%p", obj);

  /* free any data held directly by the object here */
  g_hash_table_destroy (mixin->priv->interfaces);
  g_slice_free (TpContactsMixinPrivate, mixin->priv);
}

static void
tp_contacts_mixin_inspect_contacts (
  TpSvcConnectionInterfaceContacts *iface, const GArray *handles,
  const char **interfaces, gboolean hold, DBusGMethodInvocation *context)
{
  TpContactsMixin *self = TP_CONTACTS_MIXIN (iface);
  GHashTable *result;
  guint i;
  TpBaseConnection *conn = TP_BASE_CONNECTION (iface);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
        TP_HANDLE_TYPE_CONTACT);
  GArray *valid_handles;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  /* first validate the given interfaces */
  for (i = 0; interfaces[i] != NULL; i++) {
    if (g_hash_table_lookup (self->priv->interfaces, interfaces[i]) == NULL)
      {
        GError einval = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Non-inspectable Interface given" };
        dbus_g_method_return_error (context, &einval);
        return;
      }
  }


  /* Setup handle array and hash with valid handles, optionally holding them */
  valid_handles = g_array_sized_new (TRUE, TRUE, sizeof(TpHandle),
      handles->len);
  result = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      (GDestroyNotify) g_hash_table_destroy);

  for (i = 0 ; i < handles->len ; i++)
    {
      TpHandle h;
      h = g_array_index (handles, TpHandle, i);
      if (tp_handle_is_valid (contact_repo, h, NULL))
        {
          GHashTable *attr_hash = g_hash_table_new_full (g_str_hash,
              g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free);
          g_array_append_val (valid_handles, h);
          g_hash_table_insert (result, GUINT_TO_POINTER(h), attr_hash);
        }
    }

  if (hold)
    {
      gchar *sender = dbus_g_method_get_sender (context);
      tp_handles_client_hold (contact_repo, sender, valid_handles, NULL);
    }

  /* ensure the handles don't dissappear while calling out to various functions
   */
  tp_handles_ref (contact_repo, valid_handles);

  for (i = 0; interfaces[i] != NULL; i++)
    {
      TpContactsMixinGetAttributesFunc func;

      func = g_hash_table_lookup (self->priv->interfaces, interfaces[i]);

      g_assert (func != NULL);

      func (G_OBJECT(iface), valid_handles, result);
    }

  tp_handles_unref (contact_repo, valid_handles);

  tp_svc_connection_interface_contacts_return_from_inspect_contacts (context,
      result);
}

/**
 * tp_contacts_mixin_iface_init:
 * @g_iface: A pointer to the #TpSvcConnectionInterfaceContacts in an object
 * class
 * @iface_data: Ignored
 *
 * FIXME
 */
void
tp_contacts_mixin_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionInterfaceContactsClass *klass =
    (TpSvcConnectionInterfaceContactsClass *) g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_contacts_implement_##x ( \
    klass, tp_contacts_mixin_##x)
  IMPLEMENT(inspect_contacts);
#undef IMPLEMENT
}

void
tp_contacts_mixin_add_inspectable_iface (GObject *obj, const gchar *interface,
    TpContactsMixinGetAttributesFunc get_attributes)
{
  TpContactsMixin *self = TP_CONTACTS_MIXIN (obj);

  g_assert (g_hash_table_lookup (self->priv->interfaces, interface) == NULL);
  g_assert (get_attributes != NULL);

  g_hash_table_insert (self->priv->interfaces, g_strdup (interface),
    get_attributes);
}

void
tp_contacts_mixin_set_contact_attribute (GHashTable *contact_attributes,
    TpHandle handle, gchar *attribute, GValue *value)
{
  GHashTable *attributes;

  attributes = g_hash_table_lookup (contact_attributes,
    GUINT_TO_POINTER (handle));

  g_assert (attributes != NULL);

  g_hash_table_insert (attributes, g_strdup (attribute), value);
}

