/* TpBaseProtocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 * Copyright © 2007-2009 Nokia Corporation
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

#include <telepathy-glib/base-protocol.h>
#include <telepathy-glib/base-protocol-internal.h>

#include <dbus/dbus-protocol.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-interface.h>
#include <telepathy-glib/svc-protocol.h>
#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/value-array.h>

#define DEBUG_FLAG TP_DEBUG_PARAMS
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"

/**
 * TpCMParamSpec:
 * @name: Name as passed over D-Bus
 * @dtype: D-Bus type signature.
 * @flags: Some combination of #TpConnMgrParamFlags
 * @def: Default value, as a #GVariant
 *
 * Structure representing a connection manager parameter, as accepted by
 * RequestConnection.
 */

/**
 * TpCMParamFilter:
 * @paramspec: The parameter specification.
 * @value: (transfer full): A #GVariant containing the value for that parameter
 *  provided by the user.
 * @user_data: data passed to tp_cm_param_spec_new()
 * @error: Used to raise %TP_ERROR_INVALID_ARGUMENT if the given value is
 *  rejected
 *
 * Signature of a callback used to validate and/or normalize user-provided
 * CM parameter values.
 *
 * The callback is responsible of unreffing @value if it returns %NULL or
 * another #GVariant.
 *
 * Returns: (transfer full): @value or another floating #GVariant of the same
 *  type to accept, %NULL (with @error set) to reject
 */

/**
 * tp_cm_param_filter_uint_nonzero:
 * @paramspec: The parameter specification for a guint parameter
 * @value: (transfer full): A #GVariant containing an guint16/32/64
 * @user_data: unused
 * @error: Used to return an error if the guint is 0
 *
 * A #TpCMParamFilter which rejects zero, useful for server port numbers.
 *
 * Returns: (transfer full): @value to accept, %NULL (with @error set) to reject
 */
GVariant *
tp_cm_param_filter_uint_nonzero (const TpCMParamSpec *paramspec,
                                 GVariant *value,
                                 gpointer user_data,
                                 GError **error)
{
  GVariant *uint64variant;

  if (g_variant_is_floating (value))
    g_variant_ref_sink (value);

  uint64variant = tp_variant_convert (value, G_VARIANT_TYPE_UINT64);
  g_assert (uint64variant);

  if (g_variant_get_uint64 (uint64variant) == 0)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' may not be set to zero",
          paramspec->name);
      g_variant_unref (uint64variant);
      g_variant_unref (value);
      return NULL;
    }
  g_variant_unref (uint64variant);
  return value;
}

/**
 * tp_cm_param_filter_string_nonempty:
 * @paramspec: The parameter specification for a string parameter
 * @value: (transfer full): A GVariant containing a string
 * @user_data: unused
 * @error: Used to return an error if the string is empty
 *
 * A #TpCMParamFilter which rejects empty strings.
 *
 * Returns: (transfer full): @value to accept, %NULL (with @error set) to reject
 */
GVariant *
tp_cm_param_filter_string_nonempty (const TpCMParamSpec *paramspec,
                                    GVariant *value,
                                    gpointer user_data,
                                    GError **error)
{
  const gchar *str = g_variant_get_string (value, NULL);

  if (tp_str_empty (str))
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' may not be set to an empty string",
          paramspec->name);
      g_variant_unref (value);
      return NULL;
    }
  return value;
}


/**
 * tp_cm_param_spec_new:
 * @name: The parameter's name
 * @flags: #TpConnMgrParamFlags
 * @def: A #GVariant for the default value
 * @filter: (allow-none) (scope notified): A filter function to validate
 *  parameter's value
 * @user_data: (allow-none): data to pass to @filter
 * @destroy: (allow-none): called with @user_data as its argument when the
 *  #TpCMParamSpec is freed.
 *
 * Create a new #TpCMParamSpec. @def must not be %NULL even if
 * %TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT is not set, in which case any dummy value
 * of the desired type is fine. If @def is floating, it is consumed.
 *
 * Returns: (transfer full): a new #TpCMParamSpec
 * Since: 0.UNRELEASED
 */
TpCMParamSpec *
tp_cm_param_spec_new (const gchar *name,
    TpConnMgrParamFlags flags,
    GVariant *def,
    TpCMParamFilter filter,
    gpointer user_data,
    GDestroyNotify destroy)
{
  TpCMParamSpec *self;

  g_return_val_if_fail (!tp_str_empty (name), NULL);
  g_return_val_if_fail (def != NULL, NULL);

  self = g_slice_new0 (TpCMParamSpec);
  self->name = g_strdup (name);
  self->dtype = g_variant_get_type_string (def);
  self->flags = flags;
  self->def = g_variant_ref_sink (def);

  self->filter = filter;
  self->user_data = user_data;
  self->destroy = destroy;

  self->ref_count = 1;

  return self;
}

/**
 * tp_cm_param_spec_ref:
 * @self: a #TpCMParamSpec
 *
 * Increment @self's ref count.
 *
 * Returns: (transfer full): @self
 * Since: 0.UNRELEASED
 */
TpCMParamSpec *
tp_cm_param_spec_ref (TpCMParamSpec *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * tp_cm_param_spec_unref:
 * @self: a #TpCMParamSpec
 *
 * Unref @self and free it if ref count reach 0.
 *
 * Since: 0.UNRELEASED
 */
void
tp_cm_param_spec_unref (TpCMParamSpec *self)
{
  g_return_if_fail (self != NULL);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      if (self->destroy != NULL)
        self->destroy (self->user_data);
      g_free (self->name);
      g_variant_unref (self->def);
      g_slice_free (TpCMParamSpec, self);
    }
}

G_DEFINE_BOXED_TYPE (TpCMParamSpec, tp_cm_param_spec,
    tp_cm_param_spec_ref,
    tp_cm_param_spec_unref)

/**
 * SECTION:base-protocol
 * @title: TpBaseProtocol
 * @short_description: base class for #TpSvcProtocol implementations
 * @see_also: #TpBaseConnectionManager, #TpSvcProtocol
 *
 * Base class for Telepathy Protocol objects.
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocol:
 *
 * An object providing static details of the implementation of one real-time
 * communications protocol.
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolDupParametersFunc:
 * @self: a protocol
 *
 * Signature of a virtual method to get the allowed parameters for connections
 * to a protocol.
 *
 * Returns the parameters supported by this protocol, as an array of
 * #TpCMParamSpec* which must remain valid at least as long as @self exists.
 *
 * Returns: (transfer container) (element-type TelepathyGLib.CMParamSpec): a
 *  description of the parameters supported by this protocol
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolNewConnectionFunc:
 * @self: a protocol
 * @asv: (transfer none) (element-type utf8 GObject.Value): the parameters
 *  provided via D-Bus
 * @error: used to return an error if %NULL is returned
 *
 * Signature of a virtual method to create a new connection to this protocol.
 * This is used to implement the RequestConnection D-Bus method.
 *
 * Implementations of #TpBaseProtocolClass.new_connection may assume that
 * the parameters in @asv conform to the specifications given by
 * #TpBaseProtocolClass.dup_parameters.
 *
 * Returns: (transfer full): a new connection, or %NULL on error
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolNormalizeContactFunc:
 * @self: a protocol
 * @contact: a contact's identifier
 * @error: used to return an error if %NULL is returned
 *
 * Signature of a virtual method to perform best-effort offline normalization
 * of a contact's identifier. It must either return a newly allocated string
 * that is the normalized form of @contact, or raise an error and return %NULL.
 *
 * Returns: (transfer full): a normalized identifier, or %NULL on error
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolIdentifyAccountFunc:
 * @self: a protocol
 * @asv: parameters that might be passed to the RequestConnection D-Bus method
 * @error: used to return an error if %NULL is returned
 *
 * Signature of a virtual method to choose a unique name for an account whose
 * connection parameters are @asv. This will typically return a copy of
 * the 'account' parameter from @asv, but may do something more complex (for
 * instance, on IRC it could combine the nickname and the IRC network).
 *
 * Implementations of #TpBaseProtocolClass.identify_account may assume that
 * the parameters in @asv conform to the specifications given by
 * #TpBaseProtocolClass.dup_parameters.
 *
 * Returns: (transfer full): a unique name for the account, or %NULL on error
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolGetConnectionDetailsFunc:
 * @self: a protocol
 * @connection_interfaces: (out) (transfer full): used to return a
 *  %NULL-terminated array of interfaces which might be implemented on
 *  connections to this protocol
 * @channel_manager_types: (out) (transfer full) (array zero-terminated=1):
 *  used to return a %G_TYPE_INVALID-terminated array of types that implement
 *  #TpChannelManager, which must include all channel managers that might be
 *  present on connections to this protocol; the channel managers should
 *  all implement #TpChannelManagerIface.type_foreach_channel_class. The
 *  array will be freed with g_free() by the caller.
 * @icon_name: (out) (transfer full): used to return the name of an icon
 *  for this protocol, such as "im-icq", or an empty string
 * @english_name: (out) (transfer full): used to return a human-readable
 *  but non-localized name for this protocol, or an empty string
 * @vcard_field: (out) (transfer full): used to return the name of the vCard
 *  field typically used with this protocol, or an empty string
 *
 * Signature of a virtual method to get the D-Bus interfaces implemented by
 * @self, in addition to the Protocol interface.
 *
 * Since: 0.11.11
 */

/**
 * TpBaseProtocolGetAvatarDetailsFunc:
 * @self: a protocol
 * @supported_mime_types: (out) (transfer full): used to return a
 *  %NULL-terminated array of supported avatar mime types
 * @min_height: (out): used to return the minimum height in pixels of an
 *  avatar on this protocol, which may be 0
 * @min_width: (out): used to return the minimum width in pixels of an avatar
 *  on this protocol, which may be 0
 * @rec_height: (out): used to return the rec height in pixels
 *  of an avatar on this protocol, or 0 if there is no preferred height
 * @rec_width: (out): used to return the rec width in pixels
 *  of an avatar on this protocol, or 0 if there is no preferred width
 * @max_height: (out): used to return the maximum height in pixels of an
 *  avatar on this protocol, or 0 if there is no limit
 * @max_width: (out): used to return the maximum width in pixels of an avatar
 *  on this protocol, or 0 if there is no limit
 * @max_bytes: (out): used to return the maximum size in bytes of an avatar on
 *  this protocol, or 0 if there is no limit
 *
 * Signature of a virtual method to get the supported avatar details for the
 * protocol implemented by @self.
 *
 * Returns: %TRUE if @self actually supports avatars and all the variables
 * have been to set a meaningful value, %FALSE otherwise
 * Since: 0.13.7
 */

/**
 * TP_TYPE_PROTOCOL_ADDRESSING:
 *
 * Interface representing a #TpBaseProtocol that implements
 * Protocol.Interface.Addressing.
 *
 * Since: 0.17.2
 */

/**
 * TpProtocolAddressingInterface:
 * @parent: the parent interface
 * @dup_supported_uri_schemes: provides the supported URI schemes. Must always
 * be implemented.
 * @dup_supported_vcard_fields: provides the supported vCard fields. Must
 * always be implemented.
 * @normalize_vcard_address: protocol-specific implementation for normalizing
 * vCard addresses.
 * @normalize_contact_uri: protocol-specific implementation for normalizing contact URIs.
 *
 * The interface vtable for a %TP_TYPE_PROTOCOL_ADDRESSING.
 *
 * Since: 0.17.2
 */

/**
 * TpBaseProtocolDupSupportedVCardFieldsFunc:
 * @self: a protocol
 *
 * Signature of a virtual method to get the supported vCard fields supported by
 * #self.
 *
 * Returns: (allow-none) (transfer full): a list of vCard fields in lower
 * case, e.g. [x-sip, tel]
 *
 * Since: 0.17.2
 */

/**
 * TpBaseProtocolDupSupportedURISchemesFunc:
 * @self: a protocol
 *
 * Signature of a virtual method to get the supported URI schemes supported by
 * #self.
 *
 * Returns: (allow-none) (transfer full): a list of uri schemes, e.g. [sip, sips, tel]
 *
 * Since: 0.17.2
 */

/**
 * TpBaseProtocolNormalizeVCardAddressFunc:
 * @self: a protocol
 * @vcard_field: The vCard field of the address to be normalized.
 * @vcard_address: The address to normalize.
 * @error: used to return an error if %NULL is returned
 *
 * Signature of a virtual method to perform best-effort offline normalization
 * of a vCard address. It must either return a newly allocated string
 * that is the normalized form of @vcard_address, or raise an error and
 * return %NULL.
 *
 * Returns: (transfer full): a normalized identifier, or %NULL on error
 *
 * Since: 0.17.2
 */

/**
 * TpBaseProtocolNormalizeURIFunc:
 * @self: a protocol
 * @uri: The URI to normalize.
 * @error: used to return an error if %NULL is returned
 *
 * Signature of a virtual method to perform best-effort offline normalization
 * of a URI. It must either return a newly allocated string
 * that is the normalized form of @uri, or raise an error and return %NULL.
 *
 * Returns: (transfer full): a normalized identifier, or %NULL on error
 *
 * Since: 0.13.11
 */

/**
 * TpBaseProtocolClass:
 * @parent_class: the parent class
 * @is_stub: if %TRUE, this protocol will not be advertised on D-Bus (for
 *  internal use by #TpBaseConnection)
 * @dup_parameters: a callback used to implement
 *  tp_base_protocol_dup_parameters(), which all subclasses must provide; see
 *  the documentation of that method for details
 * @new_connection: a callback used to implement
 *  tp_base_protocol_new_connection(), which all subclasses must provide;
 *  see the documentation of that method for details
 * @normalize_contact: a callback used to implement the NormalizeContact
 *  D-Bus method; it must either return a newly allocated string that is the
 *  normalized version of @contact, or raise an error via @error and
 *  return %NULL. If not implemented, %TP_ERROR_NOT_IMPLEMENTED will be raised
 *  instead.
 * @identify_account: a callback used to implement the IdentifyAccount
 *  D-Bus method; it takes as input a map from strings to #GValue<!---->s,
 *  and must either return a newly allocated string that represents the
 *  "identity" of the parameters in @asv (usually the "account" parameter),
 *  or %NULL with an error raised via @error
 * @get_connection_details: a callback used to implement the Protocol D-Bus
 *  properties that represent details of the connections provided by this
 *  protocol
 * @get_statuses: a callback used to implement the Protocol.Interface.Presence
 * interface's Statuses property. Since 0.13.5
 * @get_avatar_details: a callback used to implement the
 *  Protocol.Interface.Avatars interface's properties. Since 0.13.7
 * @dup_authentication_types: a callback used to implement the
 *  AuthenticationTypes D-Bus property; it must return a newly allocated #GStrv
 *  containing D-Bus interface names. Since 0.13.9
 *
 * The class of a #TpBaseProtocol.
 *
 * Since: 0.11.11
 */

static void protocol_iface_init (TpSvcProtocolClass *cls);
static void presence_iface_init (TpSvcProtocolInterfacePresence1Class *cls);
static void addressing_iface_init (TpSvcProtocolInterfaceAddressing1Class *cls);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TpBaseProtocol, tp_base_protocol,
    G_TYPE_DBUS_OBJECT_SKELETON,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_PROTOCOL, protocol_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_PROTOCOL_INTERFACE_PRESENCE1,
      presence_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_PROTOCOL_INTERFACE_AVATARS1,
      NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_PROTOCOL_INTERFACE_ADDRESSING1,
      addressing_iface_init))

G_DEFINE_INTERFACE (TpProtocolAddressing, tp_protocol_addressing,
    TP_TYPE_BASE_PROTOCOL)

typedef struct
{
  gchar **supported_mime_types;
  guint min_height;
  guint min_width;
  guint rec_height;
  guint rec_width;
  guint max_height;
  guint max_width;
  guint max_bytes;
} AvatarSpecs;

struct _TpBaseProtocolPrivate
{
  gchar *name;
  GStrv connection_interfaces;
  GStrv authentication_types;
  GPtrArray *requestable_channel_classes;
  gchar *icon;
  gchar *english_name;
  gchar *vcard_field;
  AvatarSpecs avatar_specs;
};

enum
{
    PROP_NAME = 1,
    PROP_IMMUTABLE_PROPERTIES,
    N_PROPS
};

static void
append_to_ptr_array (GType type G_GNUC_UNUSED,
    GHashTable *table,
    const gchar * const *allowed,
    gpointer user_data)
{
  g_ptr_array_add (user_data, tp_value_array_build (2,
        TP_HASH_TYPE_CHANNEL_CLASS, table,
        G_TYPE_STRV, allowed,
        G_TYPE_INVALID));
}

static GPtrArray *
tp_base_protocol_build_requestable_channel_classes (
    const GType *channel_managers)
{
  GPtrArray *ret = g_ptr_array_new ();
  gsize i;

  if (channel_managers != NULL)
    {
      for (i = 0; channel_managers[i] != G_TYPE_INVALID; i++)
        {
          if (!g_type_is_a (channel_managers[i], TP_TYPE_CHANNEL_MANAGER))
            {
              g_critical ("Channel manager type %s does not actually "
                  "implement TpChannelManager",
                  g_type_name (channel_managers[i]));
            }
          else
            {
              tp_channel_manager_type_foreach_channel_class (
                  channel_managers[i], append_to_ptr_array, ret);
            }
        }
    }

  return ret;
}

static void
object_skeleton_take_interface (GDBusObjectSkeleton *skel,
    GDBusInterfaceSkeleton *iface)
{
  g_dbus_object_skeleton_add_interface (skel, iface);
  g_object_unref (iface);
}

static void
object_skeleton_take_svc_interface (GDBusObjectSkeleton *skel,
    GType type)
{
  object_skeleton_take_interface (skel,
      tp_svc_interface_skeleton_new (skel, type));
}

static void
tp_base_protocol_constructed (GObject *object)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);
  GDBusObjectSkeleton *skel = G_DBUS_OBJECT_SKELETON (self);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_base_protocol_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  object_skeleton_take_svc_interface (skel, TP_TYPE_SVC_PROTOCOL);

  if (cls->get_connection_details != NULL)
    {
      GType *channel_managers = NULL;
      gchar *vcard_field = NULL;

      (cls->get_connection_details) (self,
          &self->priv->connection_interfaces,
          &channel_managers,
          &self->priv->icon,
          &self->priv->english_name,
          &vcard_field);

      self->priv->requestable_channel_classes =
        tp_base_protocol_build_requestable_channel_classes (channel_managers);
      g_free (channel_managers);

      /* normalize the case-insensitive vCard field to lower-case, and make
       * sure the strings are non-NULL */
      if (self->priv->icon == NULL)
        self->priv->icon = g_strdup ("");

      if (self->priv->english_name == NULL)
        self->priv->english_name = g_strdup ("");

      if (vcard_field == NULL)
        vcard_field = g_strdup ("");

      self->priv->vcard_field = g_ascii_strdown (vcard_field, -1);
      g_free (vcard_field);
    }
  else
    {
      self->priv->requestable_channel_classes = g_ptr_array_sized_new (0);
      self->priv->icon = g_strdup ("");
      self->priv->english_name = g_strdup ("");
      self->priv->vcard_field = g_strdup ("");
    }

  if (cls->get_avatar_details != NULL &&
      (cls->get_avatar_details) (self,
          &self->priv->avatar_specs.supported_mime_types,
          &self->priv->avatar_specs.min_height,
          &self->priv->avatar_specs.min_width,
          &self->priv->avatar_specs.rec_height,
          &self->priv->avatar_specs.rec_width,
          &self->priv->avatar_specs.max_height,
          &self->priv->avatar_specs.max_width,
          &self->priv->avatar_specs.max_bytes))
    {
      object_skeleton_take_svc_interface (skel,
          TP_TYPE_SVC_PROTOCOL_INTERFACE_AVATARS1);
    }

  if (cls->get_statuses != NULL)
    {
      object_skeleton_take_svc_interface (skel,
          TP_TYPE_SVC_PROTOCOL_INTERFACE_PRESENCE1);
    }

  if (TP_IS_PROTOCOL_ADDRESSING (self))
    {
      object_skeleton_take_svc_interface (skel,
          TP_TYPE_SVC_PROTOCOL_INTERFACE_ADDRESSING1);
    }

  if (self->priv->avatar_specs.supported_mime_types == NULL)
    self->priv->avatar_specs.supported_mime_types = g_new0 (gchar *, 1);

  if (cls->dup_authentication_types != NULL)
    {
      self->priv->authentication_types = cls->dup_authentication_types (self);
    }
  else
    {
      const gchar * const tmp[] = { NULL };
      self->priv->authentication_types = g_strdupv ((GStrv) tmp);
    }
}

/**
 * tp_base_protocol_get_name: (skip)
 * @self: a Protocol
 *
 * <!-- -->
 *
 * Returns: (transfer none): the value of #TpBaseProtocol:name
 *
 * Since: 0.11.11
 */
const gchar *
tp_base_protocol_get_name (TpBaseProtocol *self)
{
  g_return_val_if_fail (TP_IS_BASE_PROTOCOL (self), NULL);

  return self->priv->name;
}

/**
 * tp_base_protocol_get_immutable_properties:
 * @self: a Protocol
 *
 * Return a basic set of immutable properties for this Protocol object,
 * by using tp_dbus_properties_mixin_make_properties_hash().
 *
 * Additional keys and values can be inserted into the returned hash table;
 * if this is done, the inserted keys and values will be freed when the
 * hash table is destroyed. The keys must be allocated with g_strdup() or
 * equivalent, and the values must be slice-allocated (for instance with
 * tp_g_value_slice_new_string() or a similar function).
 *
 * Note that in particular, tp_asv_set_string() and similar functions should
 * not be used with this hash table.
 *
 * Returns: (transfer container): a hash table mapping (gchar *) fully-qualified
 *  property names to GValues, which must be freed by the caller (at which point
 *  its contents will also be freed).
 *
 * Since: 0.11.11
 */

GHashTable *
tp_base_protocol_get_immutable_properties (TpBaseProtocol *self)
{
  TpBaseProtocolClass *cls;
  GHashTable *table;

  g_return_val_if_fail (TP_IS_BASE_PROTOCOL (self), NULL);

  cls = TP_BASE_PROTOCOL_GET_CLASS (self);

  table = tp_dbus_properties_mixin_make_properties_hash ((GObject *) self,
      TP_IFACE_PROTOCOL, "Parameters",
      NULL);

  if (cls->is_stub)
    return table;

  tp_dbus_properties_mixin_fill_properties_hash ((GObject *) self, table,
      TP_IFACE_PROTOCOL, "Interfaces",
      TP_IFACE_PROTOCOL, "ConnectionInterfaces",
      TP_IFACE_PROTOCOL, "RequestableChannelClasses",
      TP_IFACE_PROTOCOL, "VCardField",
      TP_IFACE_PROTOCOL, "EnglishName",
      TP_IFACE_PROTOCOL, "Icon",
      TP_IFACE_PROTOCOL, "AuthenticationTypes",
      NULL);

  if (cls->get_avatar_details != NULL)
    {
      tp_dbus_properties_mixin_fill_properties_hash ((GObject *) self, table,
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "SupportedAvatarMIMETypes",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "MinimumAvatarHeight",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "MinimumAvatarWidth",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "RecommendedAvatarHeight",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "RecommendedAvatarWidth",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "MaximumAvatarHeight",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "MaximumAvatarWidth",
          TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, "MaximumAvatarBytes",
          NULL);
    }

  if (TP_IS_PROTOCOL_ADDRESSING (self))
    tp_dbus_properties_mixin_fill_properties_hash ((GObject *) self, table,
        TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING1, "AddressableVCardFields",
        TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING1, "AddressableURISchemes",
        NULL);

  if (cls->get_statuses != NULL)
    tp_dbus_properties_mixin_fill_properties_hash ((GObject *) self, table,
        TP_IFACE_PROTOCOL_INTERFACE_PRESENCE1, "Statuses",
        NULL);

  return table;
}

static void
tp_base_protocol_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->priv->name);
      break;

    case PROP_IMMUTABLE_PROPERTIES:
      g_value_take_boxed (value,
          tp_base_protocol_get_immutable_properties (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_protocol_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (property_id)
    {
    case PROP_NAME:
      g_assert (self->priv->name == NULL);    /* construct-only */
      self->priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_protocol_finalize (GObject *object)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;
  GObjectFinalizeFunc finalize =
    ((GObjectClass *) tp_base_protocol_parent_class)->finalize;

  g_free (self->priv->name);
  g_strfreev (self->priv->connection_interfaces);
  g_strfreev (self->priv->authentication_types);
  g_free (self->priv->icon);
  g_free (self->priv->english_name);
  g_free (self->priv->vcard_field);

  g_strfreev (self->priv->avatar_specs.supported_mime_types);

  if (self->priv->requestable_channel_classes != NULL)
    g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
        self->priv->requestable_channel_classes);

  if (finalize != NULL)
    finalize (object);
}

typedef enum {
    PP_PARAMETERS,
    PP_INTERFACES,
    PP_CONNECTION_INTERFACES,
    PP_REQUESTABLE_CHANNEL_CLASSES,
    PP_VCARD_FIELD,
    PP_ENGLISH_NAME,
    PP_ICON,
    PP_AUTHENTICATION_TYPES,
    N_PP
} ProtocolProp;

typedef enum {
    PPP_STATUSES,
    N_PPP
} ProtocolPresenceProp;

typedef enum {
    PAP_SUPPORTED_AVATAR_MIME_TYPES,
    PAP_MIN_AVATAR_HEIGHT,
    PAP_MIN_AVATAR_WIDTH,
    PAP_REC_AVATAR_HEIGHT,
    PAP_REC_AVATAR_WIDTH,
    PAP_MAX_AVATAR_HEIGHT,
    PAP_MAX_AVATAR_WIDTH,
    PAP_MAX_AVATAR_BYTES,
    N_PPA
} ProtocolAvatarProp;

typedef enum {
    PADP_ADDRESSABLE_VCARD_FIELDS,
    PADP_ADDRESSABLE_URI_SCHEMES,
    N_PADP
} ProtocolAddressingProp;

static void
protocol_prop_presence_getter (GObject *object,
    GQuark iface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer getter_data)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (GPOINTER_TO_INT (getter_data))
    {
      case PPP_STATUSES:
        {
          const TpPresenceStatusSpec *status =
            tp_base_protocol_get_statuses (self);
          GHashTable *ret = g_hash_table_new_full (
              g_str_hash, g_str_equal,
              g_free, (GDestroyNotify) tp_value_array_free);

          for (; status->name != NULL; status++)
            {
              GValueArray *val = NULL;
              gchar *key = NULL;
              gboolean settable = status->self;
              gboolean message = (settable && status->has_message);
              TpConnectionPresenceType type = status->presence_type;

              key = g_strdup (status->name);

              val = tp_value_array_build (3,
                  G_TYPE_UINT, type,
                  G_TYPE_BOOLEAN, settable,
                  G_TYPE_BOOLEAN, message,
                  G_TYPE_INVALID);

              g_hash_table_insert (ret, key, val);
            }

          g_value_take_boxed (value, ret);
        }
        break;

      default:
        g_assert_not_reached ();
    }
}

static void
protocol_prop_avatar_getter (GObject *object,
    GQuark iface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer getter_data)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (GPOINTER_TO_INT (getter_data))
    {
      case PAP_SUPPORTED_AVATAR_MIME_TYPES:
        g_value_set_boxed (value,
            self->priv->avatar_specs.supported_mime_types);
        break;

      case PAP_MIN_AVATAR_HEIGHT:
        g_value_set_uint (value, self->priv->avatar_specs.min_height);
        break;

      case PAP_MIN_AVATAR_WIDTH:
        g_value_set_uint (value, self->priv->avatar_specs.min_width);
        break;

      case PAP_REC_AVATAR_HEIGHT:
        g_value_set_uint (value, self->priv->avatar_specs.rec_height);
        break;

      case PAP_REC_AVATAR_WIDTH:
        g_value_set_uint (value, self->priv->avatar_specs.rec_width);
        break;

      case PAP_MAX_AVATAR_HEIGHT:
        g_value_set_uint (value, self->priv->avatar_specs.max_height);
        break;

      case PAP_MAX_AVATAR_WIDTH:
        g_value_set_uint (value, self->priv->avatar_specs.max_width);
        break;

      case PAP_MAX_AVATAR_BYTES:
        g_value_set_uint (value, self->priv->avatar_specs.max_bytes);
        break;

      default:
        g_assert_not_reached ();
    }
}

static void
protocol_prop_addressing_getter (GObject *object,
    GQuark iface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer getter_data)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;
  TpProtocolAddressingInterface *addr_iface;

  g_return_if_fail (TP_IS_PROTOCOL_ADDRESSING (self));

  addr_iface = TP_PROTOCOL_ADDRESSING_GET_INTERFACE (self);

  switch (GPOINTER_TO_INT (getter_data))
    {
      case PADP_ADDRESSABLE_VCARD_FIELDS:
        g_assert (addr_iface->dup_supported_vcard_fields != NULL);
        g_value_take_boxed (value,
            addr_iface->dup_supported_vcard_fields (self));
        break;

      case PADP_ADDRESSABLE_URI_SCHEMES:
        g_assert (addr_iface->dup_supported_uri_schemes != NULL);
        g_value_take_boxed (value,
            addr_iface->dup_supported_uri_schemes (self));
        break;

      default:
        g_assert_not_reached ();
    }
}

static void
protocol_properties_getter (GObject *object,
    GQuark iface G_GNUC_UNUSED,
    GQuark name G_GNUC_UNUSED,
    GValue *value,
    gpointer getter_data)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (GPOINTER_TO_INT (getter_data))
    {
    case PP_PARAMETERS:
        {
          GVariantBuilder builder;
          GVariant *variant;
          GPtrArray *parameters;
          guint i;

          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(susv)"));
          parameters = tp_base_protocol_dup_parameters (self);
          for (i = 0; i < parameters->len; i++)
            {
              TpCMParamSpec *param = g_ptr_array_index (parameters, i);

              g_variant_builder_add (&builder, "(susv)", param->name,
                  param->flags, param->dtype, param->def);
            }
          g_ptr_array_unref (parameters);

          variant = g_variant_ref_sink (g_variant_builder_end (&builder));
          g_value_unset (value);
          dbus_g_value_parse_g_variant (variant, value);
          g_variant_unref (variant);
        }
      break;

    case PP_INTERFACES:
        g_value_take_boxed (value,
            _tp_g_dbus_object_dup_interface_names_except (G_DBUS_OBJECT (self),
              TP_IFACE_PROTOCOL, NULL));
      break;

    case PP_CONNECTION_INTERFACES:
      g_value_set_boxed (value, self->priv->connection_interfaces);
      break;

    case PP_REQUESTABLE_CHANNEL_CLASSES:
      g_value_set_boxed (value, self->priv->requestable_channel_classes);
      break;

    case PP_VCARD_FIELD:
      g_value_set_string (value, self->priv->vcard_field);
      break;

    case PP_ENGLISH_NAME:
      g_value_set_string (value, self->priv->english_name);
      break;

    case PP_ICON:
      g_value_set_string (value, self->priv->icon);
      break;

    case PP_AUTHENTICATION_TYPES:
      g_value_set_boxed (value, self->priv->authentication_types);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
tp_base_protocol_class_init (TpBaseProtocolClass *klass)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "Parameters", GINT_TO_POINTER (PP_PARAMETERS), NULL },
      { "Interfaces", GINT_TO_POINTER (PP_INTERFACES), NULL },
      { "ConnectionInterfaces", GINT_TO_POINTER (PP_CONNECTION_INTERFACES),
        NULL },
      { "RequestableChannelClasses",
        GINT_TO_POINTER (PP_REQUESTABLE_CHANNEL_CLASSES), NULL },
      { "VCardField", GINT_TO_POINTER (PP_VCARD_FIELD), NULL },
      { "EnglishName", GINT_TO_POINTER (PP_ENGLISH_NAME), NULL },
      { "Icon", GINT_TO_POINTER (PP_ICON), NULL },
      { "AuthenticationTypes", GINT_TO_POINTER (PP_AUTHENTICATION_TYPES),
        NULL },
      { NULL }
  };

  static TpDBusPropertiesMixinPropImpl presence_props[] = {
      { "Statuses", GINT_TO_POINTER (PPP_STATUSES), NULL },
      { NULL }
  };

  static TpDBusPropertiesMixinPropImpl avatar_props[] = {
      { "SupportedAvatarMIMETypes",
        GINT_TO_POINTER (PAP_SUPPORTED_AVATAR_MIME_TYPES), NULL },
      { "MinimumAvatarHeight", GINT_TO_POINTER (PAP_MIN_AVATAR_HEIGHT), NULL },
      { "MinimumAvatarWidth", GINT_TO_POINTER (PAP_MIN_AVATAR_WIDTH), NULL },
      { "RecommendedAvatarHeight",
        GINT_TO_POINTER (PAP_REC_AVATAR_HEIGHT), NULL },
      { "RecommendedAvatarWidth",
        GINT_TO_POINTER (PAP_REC_AVATAR_WIDTH), NULL },
      { "MaximumAvatarHeight", GINT_TO_POINTER (PAP_MAX_AVATAR_HEIGHT), NULL },
      { "MaximumAvatarWidth", GINT_TO_POINTER (PAP_MAX_AVATAR_WIDTH), NULL },
      { "MaximumAvatarBytes", GINT_TO_POINTER (PAP_MAX_AVATAR_BYTES), NULL },
      { NULL }
  };

  static TpDBusPropertiesMixinPropImpl addressing_props[] = {
    { "AddressableVCardFields",
      GINT_TO_POINTER (PADP_ADDRESSABLE_VCARD_FIELDS), NULL },
    { "AddressableURISchemes",
      GINT_TO_POINTER (PADP_ADDRESSABLE_URI_SCHEMES), NULL },
    { NULL }
  };

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpBaseProtocolPrivate));

  object_class->constructed = tp_base_protocol_constructed;
  object_class->get_property = tp_base_protocol_get_property;
  object_class->set_property = tp_base_protocol_set_property;
  object_class->finalize = tp_base_protocol_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name",
        "Name of this protocol",
        "The Protocol from telepathy-spec, such as 'jabber' or 'local-xmpp'",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * TpBaseProtocol:immutable-properties:
   *
   * The D-Bus properties to be announced in the ConnectionManager
   * interface's Protocols property, as a map from
   * interface.name.propertyname to GValue.
   *
   * A protocol's immutable properties are constant for its lifetime on the
   * bus, so this property should never change. All of the D-Bus
   * properties mentioned here should also be exposed through the D-Bus
   * properties interface.
   *
   * The #TpBaseProtocol base class implements this property to be correct
   * for the basic set of properties. It can be reimplemented by
   * subclasses to have more immutable properties; if so, the subclass
   * should use tp_base_protocol_get_immutable_properties(),
   * then augment the result using
   * tp_dbus_properties_mixin_fill_properties_hash().
   *
   * Since: 0.11.11
   */
  g_object_class_install_property (object_class, PROP_IMMUTABLE_PROPERTIES,
      g_param_spec_boxed ("immutable-properties",
        "Immutable properties",
        "The protocol's immutable properties",
          TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_PROTOCOL, protocol_properties_getter, NULL,
      channel_props);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_PROTOCOL_INTERFACE_PRESENCE1,
      protocol_prop_presence_getter, NULL, presence_props);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_PROTOCOL_INTERFACE_AVATARS1, protocol_prop_avatar_getter,
        NULL, avatar_props);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_PROTOCOL_INTERFACE_ADDRESSING1,
      protocol_prop_addressing_getter, NULL, addressing_props);
}

static void
tp_base_protocol_init (TpBaseProtocol *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_PROTOCOL,
      TpBaseProtocolPrivate);
}

/**
 * tp_base_protocol_get_statuses:
 * @self: a Protocol object
 *
 * Get the statuses supported by this object. Subclasses implement this via
 * the #TpBaseProtocolClass.get_statuses virtual method.
 *
 * If the object does not implement the Protocol.Interface.Presences
 * interface, it need not implement this virtual method.
 *
 * Returns: an array of #TpPresenceStatusSpec structs describing the
 *  standard statuses supported by this protocol, with a final element
 *  whose name element is guaranteed to be %NULL. The array must remain
 *  valid at least as long as @self does.
 *
 * Since: 0.13.5
 */
const TpPresenceStatusSpec *
tp_base_protocol_get_statuses (TpBaseProtocol *self)
{
  static const TpPresenceStatusSpec none[] = { { NULL } };
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, NULL);

  if (cls->get_statuses != NULL)
    return cls->get_statuses (self);

  return none;
}

/**
 * tp_base_protocol_dup_parameters:
 * @self: a Protocol object
 *
 * Returns the parameters supported by this protocol, as an array of structs
 * which must remain valid at least as long as @self exists (it will typically
 * be a global static array).
 *
 * Returns: (transfer container) (element-type TelepathyGLib.CMParamSpec): a
 *  description of the parameters supported by this protocol
 *
 * Since: 0.11.11
 */
GPtrArray *
tp_base_protocol_dup_parameters (TpBaseProtocol *self)
{
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, NULL);
  g_return_val_if_fail (cls->dup_parameters != NULL, NULL);

  return cls->dup_parameters (self);
}

static gboolean
_tp_cm_param_spec_check_all_allowed (GPtrArray *parameters,
    GHashTable *asv,
    GError **error)
{
  GHashTable *tmp = g_hash_table_new (g_str_hash, g_str_equal);
  guint i;
  gboolean ret = TRUE;

  tp_g_hash_table_update (tmp, asv, NULL, NULL);

  for (i = 0; i < parameters->len; i++)
    {
      TpCMParamSpec *param = g_ptr_array_index (parameters, i);

      g_hash_table_remove (tmp, param->name);
    }

  if (g_hash_table_size (tmp) != 0)
    {
      gchar *error_txt;
      GString *error_str = g_string_new ("unknown parameters provided:");
      GHashTableIter h_iter;
      gpointer k;

      g_hash_table_iter_init (&h_iter, tmp);

      while (g_hash_table_iter_next (&h_iter, &k, NULL))
        {
          g_string_append_c (error_str, ' ');
          g_string_append (error_str, k);
        }

      error_txt = g_string_free (error_str, FALSE);

      DEBUG ("%s", error_txt);
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "%s", error_txt);
      g_free (error_txt);
      ret = FALSE;
    }

  g_hash_table_unref (tmp);

  return ret;
}

static GHashTable *
tp_base_protocol_sanitize_parameters (TpBaseProtocol *self,
    GHashTable *asv,
    GError **error)
{
  GVariantBuilder builder;
  GVariant *combined_variant;
  GHashTable *combined;
  GPtrArray *parameters;
  guint i;
  guint mandatory_flag;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  parameters = tp_base_protocol_dup_parameters (self);

  if (!_tp_cm_param_spec_check_all_allowed (parameters, asv, error))
    goto except;

  if (tp_asv_get_boolean (asv, "register", NULL))
    {
      mandatory_flag = TP_CONN_MGR_PARAM_FLAG_REGISTER;
    }
  else
    {
      mandatory_flag = TP_CONN_MGR_PARAM_FLAG_REQUIRED;
    }

  for (i = 0; i < parameters->len; i++)
    {
      TpCMParamSpec *param = g_ptr_array_index (parameters, i);
      const gchar *name = param->name;
      const GValue *value = tp_asv_lookup (asv, name);

      if (value != NULL)
        {
          GVariant *coerced;

          /* coerce to the expected type */
          coerced = tp_variant_convert (dbus_g_value_build_g_variant (value),
              G_VARIANT_TYPE (param->dtype));

          if (coerced == NULL)
            {
              g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
                  "failed to convert value of parameter '%s' to the expected type '%s'",
                  name, param->dtype);
              goto except;
            }

          if (param->filter != NULL)
            {
              GError *error2 = NULL;

              coerced = param->filter (param, coerced, param->user_data,
                  &error2);

              if (coerced == NULL)
                {
                  DEBUG ("parameter %s rejected by filter: %s", name,
                      error2->message);
                  g_propagate_error (error, error2);
                  goto except;
                }

              if (!g_variant_is_of_type (coerced,
                      G_VARIANT_TYPE (param->dtype)))
                {
                  g_error ("parameter %s filter changed its type from %s to %s",
                      name, param->dtype, g_variant_get_type_string (coerced));
                }
            }

          if (DEBUGGING)
            {
              gchar *to_free = NULL;
              const gchar *contents = "<secret>";

              if (!(param->flags & TP_CONN_MGR_PARAM_FLAG_SECRET))
                contents = to_free = g_variant_print (coerced, TRUE);

              DEBUG ("using specified value for %s: %s", name, contents);
              g_free (to_free);
            }

          g_variant_builder_add (&builder, "{sv}", name, coerced);
          g_variant_unref (coerced);
        }
      else if ((param->flags & mandatory_flag) != 0)
        {
          DEBUG ("missing mandatory account parameter %s", name);
          g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "missing mandatory account parameter %s",
              name);
          goto except;
        }
      else if ((param->flags & TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT) != 0)
        {
          g_variant_builder_add (&builder, "{sv}", name, param->def);
        }
      else
        {
          /* no default */
        }
    }
  g_ptr_array_unref (parameters);

  combined_variant = g_variant_ref_sink (g_variant_builder_end (&builder));
  combined = tp_asv_from_vardict (combined_variant);
  g_variant_unref (combined_variant);

  return combined;

except:
  g_ptr_array_unref (parameters);
  g_variant_builder_clear (&builder);
  return NULL;
}

/**
 * tp_base_protocol_new_connection:
 * @self: a Protocol object
 * @asv: (transfer none) (element-type utf8 GObject.Value): the parameters
 *  provided via D-Bus
 * @error: used to return an error if %NULL is returned
 *
 * Create a new connection using the #TpBaseProtocolClass.dup_parameters and
 * #TpBaseProtocolClass.new_connection implementations provided by a subclass.
 * This is used to implement the RequestConnection() D-Bus method.
 *
 * If the parameters in @asv do not fit the result of @dup_parameters (unknown
 * parameters are given, types are inappropriate, required parameters are
 * not given, or a #TpCMParamSpec.filter fails), then this method raises an
 * error and @new_connection is not called.
 *
 * Otherwise, @new_connection is called. Its @asv argument is a copy of the
 * @asv given to this method, with default values for missing parameters
 * filled in where available, and parameters' types converted to the #GType
 * specified by #TpCMParamSpec.gtype.
 *
 * Returns: (transfer full): a new connection, or %NULL on error
 *
 * Since: 0.11.11
 */
TpBaseConnection *
tp_base_protocol_new_connection (TpBaseProtocol *self,
    GHashTable *asv,
    GError **error)
{
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);
  GHashTable *combined;
  TpBaseConnection *conn = NULL;

  g_return_val_if_fail (cls != NULL, NULL);
  g_return_val_if_fail (cls->new_connection != NULL, NULL);

  combined = tp_base_protocol_sanitize_parameters (self, asv, error);

  if (combined != NULL)
    {
      conn = cls->new_connection (self, combined, error);
      g_hash_table_unref (combined);
    }

  return conn;
}

static void
protocol_normalize_contact (TpSvcProtocol *protocol,
    const gchar *contact,
    GDBusMethodInvocation *context)
{
  TpBaseProtocol *self = TP_BASE_PROTOCOL (protocol);
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);
  GError *error = NULL;
  gchar *ret = NULL;

  g_return_if_fail (cls != NULL);

  if (cls->normalize_contact != NULL)
    {
      ret = cls->normalize_contact (self, contact, &error);
    }
  else
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "This Protocol does not implement NormalizeContact");
    }

  if (ret == NULL)
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
    }
  else
    {
      tp_svc_protocol_return_from_normalize_contact (context, ret);
      g_free (ret);
    }
}

static void
protocol_identify_account (TpSvcProtocol *protocol,
    GHashTable *parameters,
    GDBusMethodInvocation *context)
{
  TpBaseProtocol *self = TP_BASE_PROTOCOL (protocol);
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);
  GError *error = NULL;
  gchar *ret = NULL;

  g_return_if_fail (cls != NULL);

  if (cls->identify_account != NULL)
    {
      GHashTable *sanitized = tp_base_protocol_sanitize_parameters (self,
          parameters, &error);

      if (sanitized != NULL)
        {
          ret = cls->identify_account (self, sanitized, &error);
          g_hash_table_unref (sanitized);
        }
    }
  else
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "This Protocol does not implement IdentifyAccount");
    }

  if (ret == NULL)
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
    }
  else
    {
      tp_svc_protocol_return_from_identify_account (context, ret);
      g_free (ret);
    }
}

static void
addressing_normalize_contact_uri (TpSvcProtocolInterfaceAddressing1 *protocol,
    const gchar *uri,
    GDBusMethodInvocation *context)
{
  TpBaseProtocol *self = TP_BASE_PROTOCOL (protocol);
  TpProtocolAddressingInterface *iface;
  GError *error = NULL;
  gchar *ret = NULL;

  if (!TP_IS_PROTOCOL_ADDRESSING (self))
    goto notimplemented;

  iface = TP_PROTOCOL_ADDRESSING_GET_INTERFACE (self);

  if (iface->normalize_contact_uri == NULL)
    goto notimplemented;

  ret = iface->normalize_contact_uri (self, uri, &error);

  if (ret == NULL)
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
      return;
    }

  tp_svc_protocol_interface_addressing1_return_from_normalize_contact_uri (context,
      ret);

  g_free (ret);

  return;

 notimplemented:
  tp_dbus_g_method_return_not_implemented (context);
}

static void
addressing_normalize_vcard_address (TpSvcProtocolInterfaceAddressing1 *protocol,
    const gchar *vcard_field,
    const gchar *vcard_address,
    GDBusMethodInvocation *context)
{
  TpBaseProtocol *self = TP_BASE_PROTOCOL (protocol);
  TpProtocolAddressingInterface *iface;
  GError *error = NULL;
  gchar *ret = NULL;

  if (!TP_IS_PROTOCOL_ADDRESSING (self))
    goto notimplemented;

  iface = TP_PROTOCOL_ADDRESSING_GET_INTERFACE (self);

  if (iface->normalize_vcard_address == NULL)
    goto notimplemented;

  ret = iface->normalize_vcard_address (self, vcard_field, vcard_address,
      &error);

  if (ret == NULL)
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
      return;
    }

  tp_svc_protocol_interface_addressing1_return_from_normalize_vcard_address (
      context,
      ret);

  g_free (ret);

  return;

 notimplemented:
  tp_dbus_g_method_return_not_implemented (context);
}


static void
protocol_iface_init (TpSvcProtocolClass *cls)
{
#define IMPLEMENT(x) tp_svc_protocol_implement_##x (cls, protocol_##x)
  IMPLEMENT (normalize_contact);
  IMPLEMENT (identify_account);
#undef IMPLEMENT
}

static void
presence_iface_init (TpSvcProtocolInterfacePresence1Class *cls)
{
}

static void
addressing_iface_init (TpSvcProtocolInterfaceAddressing1Class *cls)
{
#define IMPLEMENT(x) tp_svc_protocol_interface_addressing1_implement_##x (cls, addressing_##x)
  IMPLEMENT (normalize_contact_uri);
  IMPLEMENT (normalize_vcard_address);
#undef IMPLEMENT
}

static void
tp_protocol_addressing_default_init (TpProtocolAddressingInterface *iface)
{
}
