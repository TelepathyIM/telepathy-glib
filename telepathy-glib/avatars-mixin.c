/*
 * avatars-mixin.c - Source for TpAvatarsMixin
 * Copyright (C) 2012 Collabora Ltd.
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
 * SECTION:avatars-mixin
 * @title: TpAvatarsMixin
 * @short_description: a mixin implementation of the Avatars connection
 *  interface
 *
 * This mixin can be added to a #TpBaseConnection subclass to implement the
 * Avatars interface.
 *
 * To use the avatars mixin, include a #TpAvatarsMixin somewhere in your
 * instance structure, and call tp_avatars_mixin_init() from your init function
 * or constructor, and tp_avatars_mixin_finalize() from your finalize function.
 *
 * <section>
 * <title>Implementing Avatars</title>
 * <para>
 *   You can implement #TpSvcConnectionInterfaceAvatars as follows:
 *   <itemizedlist>
 *     <listitem>
 *       <para>use the #TpContactsMixin and
 *        <link linkend="telepathy-glib-dbus-properties-mixin">TpDBusPropertiesMixin</link>;</para>
 *     </listitem>
 *     <listitem>
 *       <para>pass tp_avatars_mixin_iface_init() as an
 *         argument to G_IMPLEMENT_INTERFACE(), like so:
 *       </para>
 *       |[
 *       G_DEFINE_TYPE_WITH_CODE (MyConnection, my_connection,
 *           TP_TYPE_BASE_CONNECTION,
 *           // ...
 *           G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_AVATARS,
 *               tp_avatars_mixin_iface_init);
 *           // ...
 *           )
 *       ]|
 *     </listitem>
 *     <listitem>
 *       <para>
 *         call tp_avatars_mixin_init_dbus_properties() in the
 *         #GTypeInfo class_init function;
 *       </para>
 *     </listitem>
 *     <listitem>
 *       <para>
 *         call tp_avatars_mixin_register_with_contacts_mixin()
 *         in the #GObjectClass constructed function.
 *       </para>
 *     </listitem>
 *   </itemizedlist>
 * </para>
 * </section>
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpAvatarsMixinSetAvatarFunc:
 * @object: An instance of a #TpBaseConnection subclass implementing the avatars
 *  interface with this mixin
 * @avatar: An array containing the avatar data to set
 * @mime_type: The MIME Type of @avatar is known
 * @error: Used to return a Telepathy D-Bus error if %FALSE is returned
 *
 * Signature of a callback to be used to set the user's avatar.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.UNRELEASED
 */

/**
 * TpAvatarsMixinClearAvatarFunc:
 * @object: An instance of a #TpBaseConnection subclass implementing the avatars
 *  interface with this mixin
 * @error: Used to return a Telepathy D-Bus error if %FALSE is returned
 *
 * Signature of a callback to be used to clear the user's avatar.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.UNRELEASED
 */

/**
 * TpAvatarsMixinRequestAvatarsFunc:
 * @object: An instance of a #TpBaseConnection subclass implementing the avatars
 *  interface with this mixin
 * @contacts: An array of #TpHandle for the contacts to request avatars for
 * @error: Used to return a Telepathy D-Bus error if %FALSE is returned
 *
 * Signature of a callback to be used to start avatar request for the given
 * contacts.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.UNRELEASED
 */

/**
 * TpAvatarsMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_avatars_mixin_init().
 *
 * There are no public fields.
 * Since: 0.UNRELEASED
 */

#include "config.h"

#include <telepathy-glib/avatars-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>
#include <errno.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/connection-internal.h>
#include <telepathy-glib/contacts-mixin.h>
#include <telepathy-glib/svc-connection.h>

#define DEBUG_FLAG TP_DEBUG_CONTACTS
#include "debug-internal.h"
#include "base-connection-internal.h"

/* TYPE MACROS */
#define TP_AVATARS_MIXIN_OFFSET_QUARK (tp_avatars_mixin_get_offset_quark ())
#define TP_AVATARS_MIXIN_OFFSET(o) \
  tp_mixin_instance_get_offset (o, TP_AVATARS_MIXIN_OFFSET_QUARK)
#define TP_AVATARS_MIXIN(o) \
  ((TpAvatarsMixin *) tp_mixin_offset_cast (o, TP_AVATARS_MIXIN_OFFSET (o)))

struct _TpAvatarsMixinPrivate
{
  /* Virtual functions */
  TpAvatarsMixinSetAvatarFunc set_avatar;
  TpAvatarsMixinClearAvatarFunc clear_avatar;
  TpAvatarsMixinRequestAvatarsFunc request_avatars;

  /* Immutable properties */
  gboolean avatar_persists;
  TpAvatarRequirements *requirements;

  /* TpHandle -> owned AvatarData
   * Contacts whose avatar is known and cached. If the contact is known to have
   * no avatar, value is NULL. */
  GHashTable *avatars;

  /* This is the set of contacts whose avatar needs to be requested but no
   * client is currently interested. This is used to request them all once a
   * client claims interest. */
  TpIntset *needs_request;
};

typedef struct
{
  gchar *token;
  gchar *uri;
} AvatarData;

static AvatarData *
avatar_data_new (const gchar *token,
    GFile *file)
{
  AvatarData *a;

  a = g_slice_new0 (AvatarData);

  a->token = g_strdup (token);
  a->uri = g_file_get_uri (file);

  return a;
}

static void
avatar_data_free (AvatarData *a)
{
  if (a != NULL)
    {
      g_free (a->token);
      g_free (a->uri);
      g_slice_free (AvatarData, a);
    }
}

static GQuark
tp_avatars_mixin_get_offset_quark (void)
{
  static GQuark offset_quark = 0;
  if (!offset_quark)
    offset_quark = g_quark_from_static_string ("TpAvatarsMixinOffsetQuark");
  return offset_quark;
}

static void
clients_interested_cb (TpBaseConnection *connection,
    gchar *token,
    gpointer user_data)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (connection);
  GArray *handles;

  DEBUG ("A client is now interested in avatars");

  if (tp_intset_is_empty (self->priv->needs_request))
    return;

  handles = tp_intset_to_array (self->priv->needs_request);
  self->priv->request_avatars ((GObject *) connection, handles, NULL);

  g_array_unref (handles);
  tp_intset_clear (self->priv->needs_request);
}

/**
 * tp_avatars_mixin_init: (skip)
 * @object: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpAvatarsMixin within the object structure
 * @set_avatar: a #TpAvatarsMixinSetAvatarFunc
 * @clear_avatar: a #TpAvatarsMixinClearAvatarFunc
 * @request_avatars: a #TpAvatarsMixinRequestAvatarsFunc
 * @avatar_persists: whether or not user's avatar is stored on server
 * @requirements: a #TpAvatarRequirements
 *
 * Initialize the avatars mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_avatars_mixin_init ((GObject *) self,
 *     G_STRUCT_OFFSET (SomeObject, avatars_mixin),
 *     _set_avatar, _clear_avatar, _request_avatars,
 *     TRUE, requirements);
 * </programlisting></informalexample>
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_init (GObject *object,
    glong offset,
    TpAvatarsMixinSetAvatarFunc set_avatar,
    TpAvatarsMixinClearAvatarFunc clear_avatar,
    TpAvatarsMixinRequestAvatarsFunc request_avatars,
    gboolean avatar_persists,
    TpAvatarRequirements *requirements)
{
  TpBaseConnection *base = (TpBaseConnection *) object;
  TpAvatarsMixin *self;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (set_avatar != NULL);
  g_return_if_fail (clear_avatar != NULL);
  g_return_if_fail (request_avatars != NULL);
  g_return_if_fail (requirements != NULL);

  g_type_set_qdata (G_OBJECT_TYPE (object),
      TP_AVATARS_MIXIN_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  self = TP_AVATARS_MIXIN (object);
  self->priv = g_slice_new0 (TpAvatarsMixinPrivate);

  self->priv->set_avatar = set_avatar;
  self->priv->clear_avatar = clear_avatar;
  self->priv->request_avatars = request_avatars;

  self->priv->avatar_persists = avatar_persists;
  self->priv->requirements = tp_avatar_requirements_copy (requirements);

  self->priv->avatars = g_hash_table_new_full (NULL, NULL,
      NULL, (GDestroyNotify) avatar_data_free);
  self->priv->needs_request = tp_intset_new ();

  tp_base_connection_add_possible_client_interest (base,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS);
  g_signal_connect (object,
      "clients-interested::" TP_IFACE_CONNECTION_INTERFACE_AVATARS,
      G_CALLBACK (clients_interested_cb), NULL);
}

/**
 * tp_avatars_mixin_finalize: (skip)
 * @object: An object with this mixin.
 *
 * Free resources held by the avatars mixin.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_finalize (GObject *object)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);

  tp_avatar_requirements_destroy (self->priv->requirements);
  g_hash_table_unref (self->priv->avatars);
  tp_intset_destroy (self->priv->needs_request);

  g_slice_free (TpAvatarsMixinPrivate, self->priv);
}

enum {
  MIXIN_DP_AVATAR_PERSISTS,
  MIXIN_DP_SUPPORTED_AVATAR_MIME_TYPES,
  MIXIN_DP_MINIMUM_AVATAR_HEIGHT,
  MIXIN_DP_MINIMUM_AVATAR_WIDTH,
  MIXIN_DP_RECOMMENDED_AVATAR_HEIGHT,
  MIXIN_DP_RECOMMENDED_AVATAR_WIDTH,
  MIXIN_DP_MAXIMUM_AVATAR_HEIGHT,
  MIXIN_DP_MAXIMUM_AVATAR_WIDTH,
  MIXIN_DP_MAXIMUM_AVATAR_BYTES,
  NUM_MIXIN_DBUS_PROPERTIES
};

static TpDBusPropertiesMixinPropImpl known_avatars_props[] = {
  { "AvatarPersists",
    GUINT_TO_POINTER (MIXIN_DP_AVATAR_PERSISTS), NULL },
  { "SupportedAvatarMIMETypes",
    GUINT_TO_POINTER (MIXIN_DP_SUPPORTED_AVATAR_MIME_TYPES), NULL },
  { "MinimumAvatarHeight",
    GUINT_TO_POINTER (MIXIN_DP_MINIMUM_AVATAR_HEIGHT), NULL },
  { "MinimumAvatarWidth",
    GUINT_TO_POINTER (MIXIN_DP_MINIMUM_AVATAR_WIDTH), NULL },
  { "RecommendedAvatarHeight",
    GUINT_TO_POINTER (MIXIN_DP_RECOMMENDED_AVATAR_HEIGHT), NULL },
  { "RecommendedAvatarWidth",
    GUINT_TO_POINTER (MIXIN_DP_RECOMMENDED_AVATAR_WIDTH), NULL },
  { "MaximumAvatarHeight",
    GUINT_TO_POINTER (MIXIN_DP_MAXIMUM_AVATAR_HEIGHT), NULL },
  { "MaximumAvatarWidth",
    GUINT_TO_POINTER (MIXIN_DP_MAXIMUM_AVATAR_WIDTH), NULL },
  { "MaximumAvatarBytes",
    GUINT_TO_POINTER (MIXIN_DP_MAXIMUM_AVATAR_BYTES), NULL },
  { NULL }
};

static void
tp_avatars_mixin_get_dbus_property (GObject *object,
    GQuark interface,
    GQuark name,
    GValue *value,
    gpointer user_data)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);

  switch (GPOINTER_TO_UINT (user_data))
    {
    case MIXIN_DP_AVATAR_PERSISTS:
      g_value_set_boolean (value, self->priv->avatar_persists);
      break;

    case MIXIN_DP_SUPPORTED_AVATAR_MIME_TYPES:
      g_value_set_boxed (value, self->priv->requirements->supported_mime_types);
      break;

    case MIXIN_DP_MINIMUM_AVATAR_HEIGHT:
      g_value_set_uint (value, self->priv->requirements->minimum_height);
      break;

    case MIXIN_DP_MINIMUM_AVATAR_WIDTH:
      g_value_set_uint (value, self->priv->requirements->minimum_width);
      break;

    case MIXIN_DP_RECOMMENDED_AVATAR_HEIGHT:
      g_value_set_uint (value, self->priv->requirements->recommended_height);
      break;

    case MIXIN_DP_RECOMMENDED_AVATAR_WIDTH:
      g_value_set_uint (value, self->priv->requirements->recommended_width);
      break;

    case MIXIN_DP_MAXIMUM_AVATAR_HEIGHT:
      g_value_set_uint (value, self->priv->requirements->maximum_height);
      break;

    case MIXIN_DP_MAXIMUM_AVATAR_WIDTH:
      g_value_set_uint (value, self->priv->requirements->maximum_width);
      break;

    case MIXIN_DP_MAXIMUM_AVATAR_BYTES:
      g_value_set_uint (value, self->priv->requirements->maximum_bytes);
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * tp_avatars_mixin_init_dbus_properties: (skip)
 * @klass: The class of an object with this mixin
 *
 * Set up #TpDBusPropertiesMixinClass to use this mixin's implementation of
 * the Avatars interface's properties.
 *
 * This automatically sets up a list of the supported properties for the
 * Avatars interface.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_init_dbus_properties (GObjectClass *klass)
{
  tp_dbus_properties_mixin_implement_interface (klass,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS,
      tp_avatars_mixin_get_dbus_property,
      NULL, known_avatars_props);
}

static void
tp_avatars_mixin_refresh_avatars (TpSvcConnectionInterfaceAvatars *iface,
    const GArray *contacts,
    DBusGMethodInvocation *context)
{
  GObject *object = (GObject *) iface;
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  TpBaseConnection *conn = TP_BASE_CONNECTION (object);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_HANDLE_TYPE_CONTACT);
  GArray *real_contacts;
  guint i;
  GError *error = NULL;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (!tp_handles_are_valid (contact_repo, contacts, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }

  real_contacts = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle),
      contacts->len);

  /* Keep only contacts for which we don't already have the avatar image */
  for (i = 0; i < contacts->len; i++)
    {
      TpHandle contact = g_array_index (contacts, TpHandle, i);

      if (g_hash_table_contains (self->priv->avatars,
              GUINT_TO_POINTER (contact)))
        continue;

      g_array_append_val (real_contacts, contact);
    }

  if (real_contacts->len > 0 &&
      !self->priv->request_avatars (object, real_contacts, &error))
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
    }
  else
    {
      tp_svc_connection_interface_avatars_return_from_refresh_avatars (context);
    }

  g_array_unref (real_contacts);
}

static void
tp_avatars_mixin_set_avatar (TpSvcConnectionInterfaceAvatars *iface,
    const GArray *avatar,
    const gchar *mime_type,
    DBusGMethodInvocation *context)
{
  GObject *object = (GObject *) iface;
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  TpBaseConnection *conn = TP_BASE_CONNECTION (object);
  GError *error = NULL;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (!self->priv->set_avatar (object, avatar, mime_type, &error))
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }

  tp_svc_connection_interface_avatars_return_from_set_avatar (context);
}

static void
tp_avatars_mixin_clear_avatar (TpSvcConnectionInterfaceAvatars *iface,
    DBusGMethodInvocation *context)
{
  GObject *object = (GObject *) iface;
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  TpBaseConnection *conn = TP_BASE_CONNECTION (object);
  GError *error = NULL;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (conn, context);

  if (!self->priv->clear_avatar (object, &error))
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }

  tp_svc_connection_interface_avatars_return_from_clear_avatar (context);
}

/**
 * tp_avatars_mixin_iface_init: (skip)
 * @g_iface: A pointer to the #TpSvcConnectionInterfaceAvatarsClass in
 *  an object class
 * @iface_data: Ignored
 *
 * Fill in the vtable entries needed to implement the avatars interface
 * using this mixin. This function should usually be called via
 * G_IMPLEMENT_INTERFACE.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TpSvcConnectionInterfaceAvatarsClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_connection_interface_avatars_implement_##x\
 (klass, tp_avatars_mixin_##x)
  IMPLEMENT(refresh_avatars);
  IMPLEMENT(set_avatar);
  IMPLEMENT(clear_avatar);
#undef IMPLEMENT
}

static void
tp_avatars_mixin_fill_contact_attributes (GObject *object,
    const GArray *contacts,
    GHashTable *attributes)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  guint i;

  for (i = 0; i < contacts->len; i++)
    {
      TpHandle contact = g_array_index (contacts, guint, i);
      AvatarData *a;

      /* If we don't know the avatar, omit it from reply */
      if (!g_hash_table_lookup_extended (self->priv->avatars,
              GUINT_TO_POINTER (contact), NULL, (gpointer *) &a))
        continue;

      tp_contacts_mixin_set_contact_attribute (attributes, contact,
          TP_TOKEN_CONNECTION_INTERFACE_AVATARS_AVATAR,
          tp_g_value_slice_new_string ((a != NULL) ? a->uri : ""));
    }
}

/**
 * tp_avatars_mixin_register_with_contacts_mixin: (skip)
 * @object: An instance of the implementation that uses both the Contacts
 *  mixin and this mixin
 *
 * Register the Avatars interface with the Contacts interface to make it
 * inspectable. The Contacts mixin should be initialized before this function
 * is called
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_register_with_contacts_mixin (GObject *object)
{
  tp_contacts_mixin_add_contact_attributes_iface (object,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS,
      tp_avatars_mixin_fill_contact_attributes);
}

static gboolean
build_avatar_filename (GObject *object,
    const gchar *avatar_token,
    gboolean create_dir,
    gchar **ret_filename,
    gchar **ret_mime_filename)
{
  TpBaseConnection *base = (TpBaseConnection *) object;
  gchar *dir;
  gchar *token_escaped;
  gboolean success = TRUE;

  token_escaped = tp_escape_as_identifier (avatar_token);
  dir = g_build_filename (g_get_user_cache_dir (), "telepathy", "avatars",
      _tp_base_connection_get_cm_name (base),
      _tp_base_connection_get_protocol_name (base), NULL);

  if (create_dir)
    {
      if (g_mkdir_with_parents (dir, 0700) == -1)
        {
          DEBUG ("Error creating avatar cache dir: %s", g_strerror (errno));
          success = FALSE;
          goto out;
        }
    }

  if (ret_filename != NULL)
    *ret_filename = g_strconcat (dir, G_DIR_SEPARATOR_S, token_escaped, NULL);

  if (ret_mime_filename != NULL)
    *ret_mime_filename = g_strconcat (dir, G_DIR_SEPARATOR_S, token_escaped,
        ".mime", NULL);

out:
  g_free (dir);
  g_free (token_escaped);

  return success;
}

static GFile *
avatar_cache_save (GObject *object,
    const gchar *avatar_token,
    const GArray *avatar,
    const gchar *mime_type)
{
  gchar *filename;
  gchar *mime_filename;
  GFile *file = NULL;
  GError *error = NULL;

  if (!build_avatar_filename (object, avatar_token, TRUE, &filename,
          &mime_filename))
    return NULL;

  if (!g_file_set_contents (filename, avatar->data, avatar->len, &error))
    {
      DEBUG ("Failed to store avatar in cache (%s): %s", filename,
          error ? error->message : "No error message");
      g_clear_error (&error);
      goto OUT;
    }

  if (!g_file_set_contents (mime_filename, mime_type, -1, &error))
    {
      DEBUG ("Failed to store MIME type in cache (%s): %s", mime_filename,
          error ? error->message : "No error message");
      g_clear_error (&error);
      goto OUT;
    }

  DEBUG ("Avatar stored in cache: %s, %s", filename, mime_type);

  file = g_file_new_for_path (filename);

OUT:
  g_free (filename);
  g_free (mime_filename);

  return file;
}

static GFile *
avatar_cache_lookup (GObject *object,
    const gchar *avatar_token)
{
  gchar *filename;
  GFile *file = NULL;

  if (!build_avatar_filename (object, avatar_token,
          FALSE, &filename, NULL))
    return NULL;

  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      DEBUG ("Avatar found in cache: %s", filename);
      file = g_file_new_for_path (filename);
    }

  g_free (filename);

  return file;
}

static void
update_avatar_take_data (GObject *object,
    TpHandle contact,
    AvatarData *a)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  GHashTable *table;

  DEBUG ("Update avatar for handle %u: %s",
      contact, (a != NULL) ? a->uri : "no avatar");

  g_hash_table_insert (self->priv->avatars, GUINT_TO_POINTER (contact), a);
  tp_intset_remove (self->priv->needs_request, contact);

  /* FIXME: Could queue to aggregate signals */
  table = g_hash_table_new (NULL, NULL);
  g_hash_table_insert (table, GUINT_TO_POINTER (contact),
      (a != NULL) ? a->uri : "");

  tp_svc_connection_interface_avatars_emit_avatars_updated (object, table);

  g_hash_table_unref (table);
}

/**
 * tp_avatars_mixin_avatar_retrieved: (skip)
 * @object: An instance of the implementation that uses this mixin
 * @contact: A contact #TpHandle
 * @token: The new token for @contact's avatar
 * @data: The new image data for @contact's avatar
 * @mime_type: The new image MIME type for @contact's avatar, or %NULL if unknown
 *
 * Update @contact's avatar. This should be called by the Connection Manager
 * when avatar data is received from the server for any contact.
 *
 * The image is stored on a disk cache, to avoid unnecessary future refetch of
 * the data from the server.
 *
 * Use tp_avatars_mixin_avatar_changed() in the case the avatar data is unknown.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_avatar_retrieved (GObject *object,
    TpHandle contact,
    const gchar *token,
    GArray *data,
    const gchar *mime_type)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  AvatarData *a;
  GFile *file;

  g_return_if_fail (contact != 0);
  g_return_if_fail (!tp_str_empty (token));
  g_return_if_fail (data != NULL);

  /* Check if we already have the same in memory */
  a = g_hash_table_lookup (self->priv->avatars, GUINT_TO_POINTER (contact));
  if (a != NULL && !tp_strdiff (a->token, token))
    return;

  /* Store on disk cache */
  file = avatar_cache_save (object, token, data, mime_type);

  /* Update */
  update_avatar_take_data (object, contact,
      avatar_data_new (token, file));

  g_object_unref (file);
}

/**
 * tp_avatars_mixin_avatar_changed: (skip)
 * @object: An instance of the implementation that uses this mixin
 * @contact: A contact #TpHandle
 * @token: The new token for @contact's avatar
 *
 * Update @contact's avatar. This should be called by the Connection Manager
 * when it knows that the avatar image changed, but did not receive the image
 * data. If the avatar got removed, then this should be called with %NULL
 * @token.
 *
 * If @token is not empty and the image data is found on disk cache, it will be
 * used. Otherwise request_avatars virtual method will be called to fetch the
 * avatar from server.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_avatar_changed (GObject *object,
    TpHandle contact,
    const gchar *token)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);
  TpBaseConnection *base = (TpBaseConnection *) object;
  AvatarData *a;
  GFile *file;

  g_return_if_fail (contact != 0);

  /* Avoid confusion between NULL and "" */
  if (tp_str_empty (token))
    token = NULL;

  /* Check if we already have the same in memory */
  if (g_hash_table_lookup_extended (self->priv->avatars,
          GUINT_TO_POINTER (contact), NULL, (gpointer *) &a) &&
      ((a == NULL && token == NULL) ||
       (a != NULL && !tp_strdiff (a->token, token))))
    return;

  /* Avatar has been removed? */
  if (token == NULL)
    {
      update_avatar_take_data (object, contact, NULL);
      return;
    }

  /* There is an avatar set, search it in the cache */
  file = avatar_cache_lookup (object, token);
  if (file == NULL)
    {
      GArray *handles;

      /* Avatar not found in cache. Request the avatar if it's for self contact
       * or if a client claims interest in avatars. Keep the last known avatar
       * in the meantime. */
      if (contact != tp_base_connection_get_self_handle (base) &&
          !_tp_base_connection_has_client_interest (base,
              TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS))
        {
          tp_intset_add (self->priv->needs_request, contact);
          return;
        }

      /* FIXME: Could queue to aggregate calls */
      handles = g_array_new (FALSE, FALSE, sizeof (TpHandle));
      g_array_append_val (handles, contact);

      self->priv->request_avatars (object, handles, NULL);

      g_array_unref (handles);
      return;
    }

  update_avatar_take_data (object, contact,
      avatar_data_new (token, file));

  g_object_unref (file);
}

/**
 * tp_avatars_mixin_drop_avatar: (skip)
 * @object: An instance of the implementation that uses this mixin
 * @contact: A contact #TpHandle
 *
 * To be called to free allocated memory when the contact's avatar is not
 * relevant anymore. For example when the contact is removed from roster, or
 * when a channel with channel-specific contacts is left.
 *
 * With XMPP, this could also be called when a contact goes offline because its
 * avatar is not known anymore.
 *
 * Note that this won't tell the client about the change, so last known avatar
 * will still be displayed. If it is known that the contact has no avatar,
 * tp_avatars_mixin_avatar_changed() with %NULL token should be used instead.
 *
 * Since: 0.UNRELEASED
 */
void
tp_avatars_mixin_drop_avatar (GObject *object,
    TpHandle contact)
{
  TpAvatarsMixin *self = TP_AVATARS_MIXIN (object);

  g_hash_table_remove (self->priv->avatars, GUINT_TO_POINTER (contact));
  tp_intset_remove (self->priv->needs_request, contact);
}
