/*
 * base-room-config.c - Channel.Interface.RoomConfig1 implementation
 * Copyright ©2011 Collabora Ltd.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <telepathy-glib/base-room-config.h>

#include <telepathy-glib/_gdbus/Channel_Interface_Room_Config1.h>

#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/sliced-gvalue.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-interface.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_ROOM_CONFIG
#include "debug-internal.h"
#include "util-internal.h"

/**
 * SECTION:base-room-config
 * @title: TpBaseRoomConfig
 * @short_description: implements the RoomConfig interface for chat rooms.
 *
 * This class implements the RoomConfig1 interface on
 * multi-user chat room channels. CMs are expected to subclass this base class
 * to implement the protocol-specific details of changing room configuration.
 *
 * If this protocol supports modifying some aspects of the room's
 * configuration, the subclass should call
 * tp_base_room_config_set_property_mutable() to mark appropriate properties as
 * potentially-modifiable, call
 * tp_base_room_config_set_can_update_configuration() to indicate whether the
 * local user has permission to modify those properties at present, and
 * implement #TpBaseRoomConfigClass.update_async. When updates to properties
 * are received from the network, they should be updated on this object using
 * g_object_set():
 *
 * |[
 *   g_object_self (room_config,
 *      "description", "A place to bury strangers",
 *      "private", TRUE,
 *      NULL);
 * ]|
 *
 * On joining the room, once the entire room configuration has been retrieved
 * from the network, the CM should call tp_base_room_config_set_retrieved().
 *
 * Since: 0.15.8
 */

/**
 * TpBaseRoomConfigClass:
 * @update_async: begins a request to modify the room's configuration.
 * @update_finish: completes a call to @update_async; the default
 *  implementation may be used if @update_async uses #GSimpleAsyncResult
 *
 * Class structure for #TpBaseRoomConfig. By default, @update_async is %NULL,
 * indicating that updating room configuration is not implemented; subclasses
 * should override it if they wish to support updating room configuration.
 */

/**
 * TpBaseRoomConfig:
 *
 * An object representing the configuration of a multi-user chat room.
 *
 * There are no public fields.
 */

/**
 * TpBaseRoomConfigUpdateAsync:
 * @self: a #TpBaseRoomConfig
 * @validated_properties: a #G_VARIANT_TYPE_VARDICT mapping from #TpBaseRoomConfig
 *  property names to #GVariant, whose types have already been validated.
 *  The function should not modify this variant.
 * @callback: a callback to call on success, failure or disconnection
 * @user_data: user data for the callback
 *
 * Signature for a function to begin a network request to update the room
 * configuration. It is guaranteed that @validated_properties will only contain
 * properties which were marked as mutable when the D-Bus method invocation
 * arrived.
 *
 * Note that #TpBaseRoomConfig will take care of applying the property updates
 * to itself if the operation succeeds.
 */

/**
 * TpBaseRoomConfigUpdateFinish:
 * @self: a #TpBaseRoomConfig
 * @result: the result passed to the callback
 * @error: used to return an error if %FALSE is returned.
 *
 * Signature for a function to complete a call to a corresponding
 * implementation of #TpBaseRoomConfigUpdateAsync.
 *
 * Returns: %TRUE if the room configuration update was accepted by the server;
 *  %FALSE, with @error set, otherwise.
 */

/**
 * TpBaseRoomConfigProperty:
 * @TP_BASE_ROOM_CONFIG_ANONYMOUS: corresponds to #TpBaseRoomConfig:anonymous
 * @TP_BASE_ROOM_CONFIG_INVITE_ONLY: corresponds to #TpBaseRoomConfig:invite-only
 * @TP_BASE_ROOM_CONFIG_LIMIT: corresponds to #TpBaseRoomConfig:limit
 * @TP_BASE_ROOM_CONFIG_MODERATED: corresponds to #TpBaseRoomConfig:moderated
 * @TP_BASE_ROOM_CONFIG_TITLE: corresponds to #TpBaseRoomConfig:title
 * @TP_BASE_ROOM_CONFIG_DESCRIPTION: corresponds to #TpBaseRoomConfig:description
 * @TP_BASE_ROOM_CONFIG_PERSISTENT: corresponds to #TpBaseRoomConfig:persistent
 * @TP_BASE_ROOM_CONFIG_PRIVATE: corresponds to #TpBaseRoomConfig:private
 * @TP_BASE_ROOM_CONFIG_PASSWORD_PROTECTED: corresponds to #TpBaseRoomConfig:password-protected
 * @TP_BASE_ROOM_CONFIG_PASSWORD: corresponds to #TpBaseRoomConfig:password
 * @TP_BASE_ROOM_CONFIG_PASSWORD_HINT: corresponds to #TpBaseRoomConfig:password-hint
 * @TP_NUM_BASE_ROOM_CONFIG_PROPERTIES: the number of configuration properties
 *  currently defined.
 *
 * An enumeration of room configuration fields, corresponding to GObject
 * properties and, in turn, to D-Bus properties.
 */

/**
 * TP_TYPE_BASE_ROOM_CONFIG_PROPERTY:
 *
 * The #GEnumClass type of #TpBaseRoomConfigProperty. (The nicknames are chosen
 * to correspond to unqualified D-Bus property names.)
 */

struct _TpBaseRoomConfigPrivate {
    GWeakRef channel;

    _TpGDBusChannelInterfaceRoomConfig1 *skeleton;
    TpIntset *mutable_properties;

    /* Details of a pending update, or both NULL if no call to
     * UpdateConfiguration is in progress.
     */
    GDBusMethodInvocation *update_configuration_ctx;
    GVariant *validated_properties;
};

enum {
    PROP_CHANNEL = 1,

    /* D-Bus properties */
    PROP_ANONYMOUS,
    PROP_INVITE_ONLY,
    PROP_LIMIT,
    PROP_MODERATED,
    PROP_TITLE,
    PROP_DESCRIPTION,
    PROP_PERSISTENT,
    PROP_PRIVATE,
    PROP_PASSWORD_PROTECTED,
    PROP_PASSWORD,
    PROP_PASSWORD_HINT,
    PROP_CAN_UPDATE_CONFIGURATION,
    PROP_MUTABLE_PROPERTIES,
    PROP_CONFIGURATION_RETRIEVED,
};

G_DEFINE_TYPE (TpBaseRoomConfig, tp_base_room_config, G_TYPE_OBJECT)

/* Must be in the same order than TpBaseRoomConfigProperty */
static gchar *room_config_properties[] = {
    "anonymous",
    "invite-only",
    "limit",
    "moderated",
    "title",
    "description",
    "persistent",
    "private",
    "password-protected",
    "password",
    "password-hint"
};

G_STATIC_ASSERT (G_N_ELEMENTS (room_config_properties) ==
    TP_NUM_BASE_ROOM_CONFIG_PROPERTIES);

static gboolean tp_base_room_config_update_finish (
    TpBaseRoomConfig *self,
    GAsyncResult *result,
    GError **error);

static void
tp_base_room_config_init (TpBaseRoomConfig *self)
{
  TpBaseRoomConfigPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_ROOM_CONFIG,
      TpBaseRoomConfigPrivate);
  priv = self->priv;

  priv->mutable_properties = tp_intset_new ();
}

static void
tp_base_room_config_get_property (
    GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (object);
  TpBaseRoomConfigPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CHANNEL:
        g_value_take_object (value, g_weak_ref_get (&priv->channel));
        break;

      /* DBus properties are stored in the skeleton with the same name */
      case PROP_ANONYMOUS:
      case PROP_INVITE_ONLY:
      case PROP_LIMIT:
      case PROP_MODERATED:
      case PROP_TITLE:
      case PROP_DESCRIPTION:
      case PROP_PERSISTENT:
      case PROP_PRIVATE:
      case PROP_PASSWORD_PROTECTED:
      case PROP_PASSWORD:
      case PROP_PASSWORD_HINT:
      case PROP_CAN_UPDATE_CONFIGURATION:
      case PROP_MUTABLE_PROPERTIES:
      case PROP_CONFIGURATION_RETRIEVED:
        g_object_get_property (G_OBJECT (priv->skeleton),
            g_param_spec_get_name (pspec), value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
tp_base_room_config_set_property (
    GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (object);
  TpBaseRoomConfigPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CHANNEL:
        g_weak_ref_set (&priv->channel, g_value_get_object (value));
        break;

      /* DBus properties are stored in the skeleton with the same name */
      case PROP_ANONYMOUS:
      case PROP_INVITE_ONLY:
      case PROP_LIMIT:
      case PROP_MODERATED:
      case PROP_TITLE:
      case PROP_DESCRIPTION:
      case PROP_PERSISTENT:
      case PROP_PRIVATE:
      case PROP_PASSWORD_PROTECTED:
      case PROP_PASSWORD:
      case PROP_PASSWORD_HINT:
      case PROP_CAN_UPDATE_CONFIGURATION:
        g_object_set_property (G_OBJECT (priv->skeleton),
            g_param_spec_get_name (pspec), value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static gboolean tp_base_room_config_update_configuration (
    _TpGDBusChannelInterfaceRoomConfig1 *skeleton,
    GDBusMethodInvocation *context,
    GVariant *properties,
    TpBaseRoomConfig *self);

static void
tp_base_room_config_constructed (GObject *object)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (object);
  TpBaseRoomConfigPrivate *priv = self->priv;
  GObjectClass *parent_class = tp_base_room_config_parent_class;
  TpBaseChannel *channel;

  if (parent_class->constructed != NULL)
    parent_class->constructed (object);


  channel = g_weak_ref_get (&priv->channel);
  g_assert (channel != NULL);
  DEBUG ("associated (TpBaseChannel *)%p with (TpBaseRoomConfig *)%p",
      channel, self);

  priv->skeleton = _tp_gdbus_channel_interface_room_config1_skeleton_new ();
  g_signal_connect_object (priv->skeleton, "handle-update-configuration",
      G_CALLBACK (tp_base_room_config_update_configuration), self, 0);

  g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (channel),
      G_DBUS_INTERFACE_SKELETON (priv->skeleton));

  g_object_unref (channel);
}

static void
tp_base_room_config_dispose (GObject *object)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (object);
  GObjectClass *parent_class = tp_base_room_config_parent_class;
  TpBaseRoomConfigPrivate *priv = self->priv;

  g_weak_ref_clear (&priv->channel);
  g_clear_object (&priv->skeleton);

  if (parent_class->dispose != NULL)
    parent_class->dispose (object);
}

static void
tp_base_room_config_finalize (GObject *object)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (object);
  GObjectClass *parent_class = tp_base_room_config_parent_class;
  TpBaseRoomConfigPrivate *priv = self->priv;

  tp_intset_destroy (priv->mutable_properties);

  if (priv->update_configuration_ctx != NULL)
    {
      CRITICAL ("finalizing (TpBaseRoomConfig *) %p with a pending "
          "UpdateConfiguration() call; this should not be possible",
          object);
    }
  g_warn_if_fail (priv->validated_properties == NULL);

  if (parent_class->finalize != NULL)
    parent_class->finalize (object);
}

static void
tp_base_room_config_class_init (TpBaseRoomConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = tp_base_room_config_get_property;
  object_class->set_property = tp_base_room_config_set_property;
  object_class->constructed = tp_base_room_config_constructed;
  object_class->dispose = tp_base_room_config_dispose;
  object_class->finalize = tp_base_room_config_finalize;

  g_type_class_add_private (klass, sizeof (TpBaseRoomConfigPrivate));

  klass->update_finish = tp_base_room_config_update_finish;

  param_spec = g_param_spec_object ("channel", "Channel",
      "Parent TpBaseChannel",
      TP_TYPE_BASE_CHANNEL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL, param_spec);

  /* D-Bus properties. */
  param_spec = g_param_spec_boolean ("anonymous", "Anonymous",
      "True if people may join the channel without other members being made "
      "aware of their identity.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ANONYMOUS, param_spec);

  param_spec = g_param_spec_boolean ("invite-only", "InviteOnly",
      "True if people may not join the channel until they have been invited.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INVITE_ONLY, param_spec);

  param_spec = g_param_spec_uint ("limit", "Limit",
      "The limit to the number of members; or 0 if there is no limit.",
      0, G_MAXUINT32, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LIMIT, param_spec);

  param_spec = g_param_spec_boolean ("moderated", "Moderated",
      "True if channel membership is not sufficient to allow participation.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MODERATED, param_spec);

  param_spec = g_param_spec_string ("title", "Title",
      "A human-visible name for the channel, if it differs from "
      "Room.DRAFT.RoomName; the empty string, otherwise.",
      "",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TITLE, param_spec);

  param_spec = g_param_spec_string ("description", "Description",
      "A human-readable description of the channel's overall purpose; if any.",
      "",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DESCRIPTION, param_spec);

  param_spec = g_param_spec_boolean ("persistent", "Persistent",
      "True if the channel will remain in existence on the server after all "
      "members have left it.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PERSISTENT, param_spec);

  param_spec = g_param_spec_boolean ("private", "Private",
      "True if the channel is not visible to non-members.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PRIVATE, param_spec);

  param_spec = g_param_spec_boolean ("password-protected", "PasswordProtected",
      "True if contacts joining this channel must provide a password to be "
      "granted entry.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PASSWORD_PROTECTED,
      param_spec);

  param_spec = g_param_spec_string ("password", "Password",
      "If PasswordProtected is True, the password required to enter the "
      "channel, if known. If the password is unknown, or PasswordProtected "
      "is False, the empty string.",
      "",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PASSWORD, param_spec);

  param_spec = g_param_spec_string ("password-hint", "PasswordHint",
      "If PasswordProtected is True, a hint for the password. If the password"
      "password is unknown, or PasswordProtected is False, the empty string.",
      "",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PASSWORD_HINT, param_spec);

  param_spec = g_param_spec_boolean ("can-update-configuration",
      "CanUpdateConfiguration",
      "If True, the user may call UpdateConfiguration to change the values of "
      "the properties listed in MutableProperties.",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CAN_UPDATE_CONFIGURATION,
      param_spec);

  param_spec = g_param_spec_boxed ("mutable-properties", "MutableProperties",
      "A list of (unqualified) property names on this interface which may be "
      "modified using UpdateConfiguration (if CanUpdateConfiguration is "
      "True). Properties not listed here cannot be modified.",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MUTABLE_PROPERTIES,
      param_spec);

  param_spec = g_param_spec_boolean ("configuration-retrieved",
      "ConfigurationRetrieved",
      "Becomes True once the room config has been fetched from the network",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONFIGURATION_RETRIEVED,
      param_spec);
}

static gboolean
validate_property_type (TpBaseRoomConfig *self,
    const gchar *property_name,
    GVariant *value,
    GError **error)
{
  GDBusInterfaceInfo *iinfo = g_dbus_interface_get_info (
      G_DBUS_INTERFACE (self->priv->skeleton));
  guint i;

  g_return_val_if_fail (value != NULL, FALSE);

  for (i = 0; iinfo->properties[i] != NULL; i++)
    {
      if (g_str_equal (iinfo->properties[i]->name, property_name))
        {
          if (!g_variant_is_of_type (value,
                  G_VARIANT_TYPE (iinfo->properties[i]->signature)))
            {
              g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
                  "'%s' has type '%s', not '%s'", property_name,
                  iinfo->properties[i]->signature,
                  g_variant_get_type_string (value));
              return FALSE;
            }
          return TRUE;
        }
    }

  g_assert_not_reached ();
}

static gboolean
validate_property (TpBaseRoomConfig *self,
    GVariantDict *validated_properties,
    const gchar *property_name,
    GVariant *value,
    GError **error)
{
  TpBaseRoomConfigPrivate *priv = self->priv;
  gint property_id;

  if (!_tp_enum_from_nick (TP_TYPE_BASE_ROOM_CONFIG_PROPERTY,
          property_name, &property_id))
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "'%s' is not a known RoomConfig property.", property_name);
      return FALSE;
    }

  if (!tp_intset_is_member (priv->mutable_properties, property_id))
    {
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "'%s' cannot be changed on this protocol", property_name);
      return FALSE;
    }

  if (!validate_property_type (self, property_name, value, error))
    return FALSE;

  g_variant_dict_insert_value (validated_properties,
      room_config_properties[property_id], value);
  return TRUE;
}

/*
 * validate_properties:
 * @self: it's me!
 * @properties: a mapping from unqualified property names (gchar *) to
 *              corresponding new GVariant.
 * @error: set to a TP_ERROR if validation fails.
 *
 * Validates the names and types and mutability of @properties.
 *
 * Returns: a new a{sv} GVariant containing the mappings from
 *          TpBaseRoomConfig property names to their new values,
 *          or NULL if validation failed
 */
static GVariant *
validate_properties (TpBaseRoomConfig *self,
    GVariant *properties,
    GError **error)
{
  GVariantDict validated_properties;
  GVariantIter iter;
  const gchar *k;
  GVariant *v;

  g_variant_dict_init (&validated_properties, NULL);

  g_variant_iter_init (&iter, properties);
  while (g_variant_iter_loop (&iter, "{&sv}", &k, &v))
    {
      if (!validate_property (self, &validated_properties, k, v, error))
        {
          g_variant_dict_clear (&validated_properties);
          return NULL;
        }
    }

  return g_variant_dict_end (&validated_properties);
}

static void
update_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseRoomConfig *self = TP_BASE_ROOM_CONFIG (source);
  TpBaseRoomConfigPrivate *priv = self->priv;
  TpBaseChannel *channel = user_data;
  GError *error = NULL;

  g_return_if_fail (priv->update_configuration_ctx != NULL);
  g_return_if_fail (priv->validated_properties != NULL);

  if (TP_BASE_ROOM_CONFIG_GET_CLASS (self)->update_finish (
        self, result, &error))
    {
      GVariantIter iter;
      const gchar *k;
      GVariant *v;

      g_variant_iter_init (&iter, priv->validated_properties);
      while (g_variant_iter_next (&iter, "{&sv}", &k, &v))
        {
          GValue value = G_VALUE_INIT;

          g_dbus_gvariant_to_gvalue (v, &value);
          /* set properties on 'self' instead of 'self->skeleton'
           * to emit the notify signal properly */
          g_object_set_property (G_OBJECT (self), k, &value);
          g_value_unset (&value);
        }

      _tp_gdbus_channel_interface_room_config1_complete_update_configuration (
          priv->skeleton, priv->update_configuration_ctx);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (priv->update_configuration_ctx, error);
      g_clear_error (&error);
    }

  priv->update_configuration_ctx = NULL;
  tp_clear_pointer (&priv->validated_properties, g_variant_unref);
  g_object_unref (channel);
}

static gboolean
tp_base_room_config_update_finish (
    TpBaseRoomConfig *self,
    GAsyncResult *result,
    GError **error)
{
  gpointer source_tag = TP_BASE_ROOM_CONFIG_GET_CLASS (self)->update_async;

  _tp_implement_finish_void (self, source_tag);
}

static gboolean
tp_base_room_config_update_configuration (
    _TpGDBusChannelInterfaceRoomConfig1 *skeleton,
    GDBusMethodInvocation *context,
    GVariant *properties,
    TpBaseRoomConfig *self)
{
  TpBaseRoomConfigPrivate *priv;
  TpBaseRoomConfigUpdateAsync update_async;
  GError *error = NULL;

  priv = self->priv;
  update_async = TP_BASE_ROOM_CONFIG_GET_CLASS (self)->update_async;

  if (update_async == NULL)
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "This protocol does not implement updating the room configuration");
      goto err;
    }

  if (priv->update_configuration_ctx != NULL)
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "Another UpdateConfiguration() call is still in progress");
      goto err;
    }

  /* If update_configuration_ctx == NULL, then validated_properties should be,
   * too.
   */
  g_warn_if_fail (priv->validated_properties == NULL);

  if (!_tp_gdbus_channel_interface_room_config1_get_can_update_configuration (
          priv->skeleton))
    {
      g_set_error (&error, TP_ERROR, TP_ERROR_PERMISSION_DENIED,
          "The user doesn't have permission to modify this room's "
          "configuration (maybe they're not an op/admin/owner?)");
      goto err;
    }

  if (g_variant_n_children (properties) == 0)
    {
      _tp_gdbus_channel_interface_room_config1_complete_update_configuration (
          priv->skeleton, context);
      goto out;
    }

  priv->validated_properties = validate_properties (self, properties, &error);

  if (priv->validated_properties == NULL)
    goto err;

  priv->update_configuration_ctx = context;
  /* We ensure our channel stays alive for the duration of the call. This is
   * mainly as a convenience to the subclass, which would probably like
   * tp_base_room_config_get_channel() to work reliably. If the
   * GDBusMethodInvocation kept the object alive, we wouldn't need this.
   *
   * The CM could modify validated_properties if it wanted. This is good in some
   * ways: it means it can further sanitize the values if it wants, for
   * instance. But I guess it's also possible for the CM to mess up.
   */
  update_async (self, priv->validated_properties, update_cb,
      g_weak_ref_get (&priv->channel));
  goto out;

err:
  g_dbus_method_invocation_return_gerror (context, error);
  g_clear_error (&error);

out:
  return TRUE;
}

/**
 * tp_base_room_config_dup_channel:
 * @self: a #TpBaseChannel
 *
 * Returns the channel to which @self is attached.
 *
 * Returns: (transfer full): the #TpBaseRoomConfig:channel property.
 */
TpBaseChannel *
tp_base_room_config_dup_channel (TpBaseRoomConfig *self)
{
  g_return_val_if_fail (TP_IS_BASE_ROOM_CONFIG (self), NULL);

  return g_weak_ref_get (&self->priv->channel);
}

/**
 * tp_base_room_config_set_can_update_configuration:
 * @self: a #TpBaseRoomConfig object.
 * @can_update_configuration: %TRUE if the local user has permission to modify
 *  properties marked as mutable.
 *
 * Specify whether or not the local user currently has permission to modify the
 * room configuration.
 */
void
tp_base_room_config_set_can_update_configuration (
    TpBaseRoomConfig *self,
    gboolean can_update_configuration)
{
  g_return_if_fail (TP_IS_BASE_ROOM_CONFIG (self));

  g_object_set (self,
      "can-update-configuration", can_update_configuration,
      NULL);
}

/**
 * tp_base_room_config_set_property_mutable:
 * @self: a #TpBaseRoomConfig object.
 * @property_id: a property identifier (not including
 *  %TP_NUM_BASE_ROOM_CONFIG_PROPERTIES)
 * @is_mutable: %TRUE if it is possible for Telepathy clients to modify
 *  @property_id when #TpBaseRoomConfig:can-update-configuration is %TRUE.
 *
 * Specify whether it is possible for room members to modify the value of
 * @property_id (possibly dependent on them having channel-operator powers), or
 * whether @property_id's value is an intrinsic fact about the protocol.
 *
 * For example, on IRC it is impossible to configure a channel to hide the
 * identities of participants from others, so %TP_BASE_ROOM_CONFIG_ANONYMOUS
 * should be marked as immutable on IRC; whereas channel operators can mark
 * rooms as invite-only, so %TP_BASE_ROOM_CONFIG_INVITE_ONLY should be marked as
 * mutable on IRC.
 *
 * By default, all properties are considered immutable.
 *
 * Call tp_base_room_config_set_can_update_configuration() to specify whether or
 * not it is currently possible for the local user to alter properties marked
 * as mutable.
 */
void
tp_base_room_config_set_property_mutable (
    TpBaseRoomConfig *self,
    TpBaseRoomConfigProperty property_id,
    gboolean is_mutable)
{
  TpBaseRoomConfigPrivate *priv = self->priv;
  gboolean changed = FALSE;

  g_return_if_fail (TP_IS_BASE_ROOM_CONFIG (self));
  g_return_if_fail (property_id < TP_NUM_BASE_ROOM_CONFIG_PROPERTIES);

  /* Grr. Damn _add and _remove functions for being asymmetrical. */
  if (!is_mutable)
    {
      changed = tp_intset_remove (priv->mutable_properties, property_id);
    }
  else if (!tp_intset_is_member (priv->mutable_properties, property_id))
    {
      tp_intset_add (priv->mutable_properties, property_id);
      changed = TRUE;
    }

  if (changed)
    {
      GPtrArray *property_names;
      TpIntsetFastIter iter;
      guint i;

      /* Construct an strv of mutable property names */
      property_names = g_ptr_array_sized_new (
          tp_intset_size (priv->mutable_properties));
      tp_intset_fast_iter_init (&iter, priv->mutable_properties);
      while (tp_intset_fast_iter_next (&iter, &i))
        {
          const gchar *property_name = _tp_enum_to_nick (
              TP_TYPE_BASE_ROOM_CONFIG_PROPERTY, i);

          g_assert (property_name != NULL);
          g_ptr_array_add (property_names, (gchar *) property_name);
        }
      g_ptr_array_add (property_names, NULL);

      _tp_gdbus_channel_interface_room_config1_set_mutable_properties (
          priv->skeleton, (const gchar * const *) property_names->pdata);
      g_object_notify ((GObject *) self, "mutable-properties");

      g_ptr_array_unref (property_names);
   }
}

/**
 * tp_base_room_config_set_retrieved:
 * @self: a #TpBaseRoomConfig object
 *
 * Signal that the room's configuration has been retrieved, as well as
 * signalling any queued property changes. This function should be called once
 * all properties have been set to meaningful values.
 */
void
tp_base_room_config_set_retrieved (TpBaseRoomConfig *self)
{
  g_return_if_fail (TP_IS_BASE_ROOM_CONFIG (self));

  _tp_gdbus_channel_interface_room_config1_set_configuration_retrieved (
      self->priv->skeleton, TRUE);
}
