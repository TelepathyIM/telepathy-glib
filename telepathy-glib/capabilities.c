/* Object representing the capabilities a Connection or a Contact supports.
 *
 * Copyright (C) 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "telepathy-glib/capabilities.h"
#include "telepathy-glib/capabilities-internal.h"

#include <telepathy-glib/asv.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-internal.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/value-array.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/variant-util-internal.h"

/**
 * SECTION:capabilities
 * @title: TpCapabilities
 * @short_description: object representing capabilities
 *
 * #TpCapabilities objects represent the capabilities a #TpConnection
 * or a #TpContact supports.
 *
 * Since: 0.11.3
 */

/**
 * TpCapabilities:
 *
 * An object representing capabilities a #TpConnection or #TpContact supports.
 *
 * Since: 0.11.3
 */

struct _TpCapabilitiesClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _TpCapabilities {
    /*<private>*/
    GObject parent;
    TpCapabilitiesPrivate *priv;
};

G_DEFINE_TYPE (TpCapabilities, tp_capabilities, G_TYPE_OBJECT)

enum {
    PROP_CHANNEL_CLASSES = 1,
    PROP_CONTACT_SPECIFIC,
    N_PROPS
};

struct _TpCapabilitiesPrivate {
    gboolean contact_specific;
    GVariant *classes_variant;
};

/**
 * tp_capabilities_is_specific_to_contact:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: the same #gboolean as the #TpCapabilities:contact-specific property
 *
 * Since: 0.11.3
 */
gboolean
tp_capabilities_is_specific_to_contact (TpCapabilities *self)
{
  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  return self->priv->contact_specific;
}

static void
tp_capabilities_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_capabilities_parent_class)->constructed;
  TpCapabilities *self = TP_CAPABILITIES (object);

  g_assert (self->priv->classes_variant != NULL);

  if (chain_up != NULL)
    chain_up (object);
}

static void
tp_capabilities_dispose (GObject *object)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  tp_clear_pointer (&self->priv->classes_variant, g_variant_unref);

  ((GObjectClass *) tp_capabilities_parent_class)->dispose (object);
}

static void
tp_capabilities_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  switch (property_id)
    {
    case PROP_CONTACT_SPECIFIC:
      g_value_set_boolean (value, self->priv->contact_specific);
      break;

    case PROP_CHANNEL_CLASSES:
      g_value_set_variant (value, self->priv->classes_variant);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_capabilities_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  switch (property_id)
    {
    case PROP_CHANNEL_CLASSES:
        {
          GVariant *variant = g_value_dup_variant (value);
          gchar *s;

          s = g_variant_print (variant, TRUE);
          DEBUG ("%s", s);
          g_free (s);
          g_return_if_fail (g_variant_is_of_type (variant,
                G_VARIANT_TYPE ("a(a{sv}as)")));
          self->priv->classes_variant = variant;
        }
      break;

    case PROP_CONTACT_SPECIFIC:
      self->priv->contact_specific = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_capabilities_class_init (TpCapabilitiesClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpCapabilitiesPrivate));
  object_class->get_property = tp_capabilities_get_property;
  object_class->set_property = tp_capabilities_set_property;
  object_class->constructed = tp_capabilities_constructed;
  object_class->dispose = tp_capabilities_dispose;

  /**
   * TpCapabilities:contact-specific:
   *
   * Whether this object accurately describes the capabilities of a particular
   * contact, or if it's only a guess based on the capabilities of the
   * underlying connection.
   */
  param_spec = g_param_spec_boolean ("contact-specific",
      "contact specific",
      "TRUE if this object describes the capabilities of a particular contact",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT_SPECIFIC,
      param_spec);

  /**
   * TpCapabilities:channel-classes:
   *
   * The underlying data structure used by Telepathy to represent the
   * requests that can succeed.
   *
   * This can be used by advanced clients to determine whether an unusually
   * complex request would succeed. See the Telepathy D-Bus API Specification
   * for details of how to interpret the returned #GVariant of type
   * a(a{sv}as).
   *
   * The higher-level methods like
   * tp_capabilities_supports_text_chats() are likely to be more useful to
   * the majority of clients.
   */
  param_spec = g_param_spec_variant ("channel-classes",
      "GVariant of type a(a{sv}as)",
      "The channel classes supported",
      G_VARIANT_TYPE ("a(a{sv}as)"),
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_CLASSES,
      param_spec);
}

static void
tp_capabilities_init (TpCapabilities *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CAPABILITIES,
      TpCapabilitiesPrivate);
}

/* NULL-safe for @classes, sinks floating refs */
TpCapabilities *
_tp_capabilities_new (GVariant *classes,
    gboolean contact_specific)
{
  TpCapabilities *self;

  if (classes == NULL)
    classes = g_variant_new ("a(a{sv}as)", NULL);

  g_variant_ref_sink (classes);

  self = g_object_new (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", contact_specific,
      NULL);

  g_variant_unref (classes);

  return self;
}

static gboolean
supports_simple_channel (TpCapabilities *self,
    const gchar *expected_chan_type,
    TpEntityType expected_entity_type)
{
  GVariantIter iter;
  GVariant *fixed;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}@as)", &fixed, NULL))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;

      if (g_variant_n_children (fixed) != 2)
        continue;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);

      if (!valid)
        continue;

      if (!tp_strdiff (chan_type, expected_chan_type) &&
          entity_type == expected_entity_type)
        {
          g_variant_unref (fixed);
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * tp_capabilities_supports_text_chats:
 * @self: a #TpCapabilities object
 *
 * Return whether private text channels can be established by providing
 * a contact identifier, for instance by calling
 * tp_account_channel_request_new_text() followed by
 * tp_account_channel_request_set_target_contact().
 *
 * If the protocol is such that text chats can be established, but only via a
 * more elaborate D-Bus API than normal (because more information is needed),
 * then this method will return %FALSE.
 *
 * Returns: %TRUE if a channel request containing Text as ChannelType,
 * HandleTypeContact as TargetEntityType and a contact identifier can be
 * expected to work, %FALSE otherwise.
 *
 * Since: 0.11.3
 */
gboolean
tp_capabilities_supports_text_chats (TpCapabilities *self)
{
  return supports_simple_channel (self, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_ENTITY_TYPE_CONTACT);
}

/**
 * tp_capabilities_supports_text_chatrooms:
 * @self: a #TpCapabilities object
 *
 * If the #TpCapabilities:contact-specific property is %FALSE, this function
 * checks if named text chatrooms can be joined by providing a chatroom
 * identifier, for instance by calling
 * tp_account_channel_request_new_text() followed by
 * tp_account_channel_request_set_target_id() with %TP_ENTITY_TYPE_ROOM.
 *
 * If the #TpCapabilities:contact-specific property is %TRUE, this function
 * checks if the contact associated with this #TpCapabilities can be invited
 * to named text chatrooms.
 *
 * If the protocol is such that chatrooms can be joined or contacts can be
 * invited, but only via a more elaborate D-Bus API than normal
 * (because more information is needed), then this method will return %FALSE.
 *
 * Returns: %TRUE if a channel request containing Text as ChannelType,
 * HandleTypeRoom as TargetEntityType and a channel identifier can be
 * expected to work, %FALSE otherwise.
 *
 * Since: 0.11.3
 */
gboolean
tp_capabilities_supports_text_chatrooms (TpCapabilities *self)
{
  return supports_simple_channel (self, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_ENTITY_TYPE_ROOM);
}

/**
 * tp_capabilities_supports_sms:
 * @self: a #TpCapabilities object
 *
 * If the #TpCapabilities:contact-specific property is %FALSE, this function
 * checks if SMS text channels can be requested with the connection associated
 * with this #TpCapabilities.
 *
 * If the #TpCapabilities:contact-specific property is %TRUE, this function
 * checks if the contact associated with this #TpCapabilities supports
 * SMS text channels.
 *
 * Returns: %TRUE if a channel request containing Text as ChannelType,
 * HandleTypeContact as TargetEntityType, a channel identifier and
 * #TP_PROP_CHANNEL_INTERFACE_SMS_SMS_CHANNEL set to %TRUE can be
 * expected to work, %FALSE otherwise.
 *
 * Since: 0.19.0
 */
gboolean
tp_capabilities_supports_sms (TpCapabilities *self)
{
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed, &allowed))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;
      guint nb_fixed_props;

      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);

      if (!valid)
        continue;

      if (entity_type != TP_ENTITY_TYPE_CONTACT)
        continue;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);

      if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
        continue;

      /* SMSChannel be either in fixed or allowed properties */
      if (tp_vardict_get_boolean (fixed,
            TP_PROP_CHANNEL_INTERFACE_SMS1_SMS_CHANNEL, NULL))
        {
          /* In fixed, succeed if there is no more fixed properties required */
          nb_fixed_props = 3;
        }
      else
        {
          /* Not in fixed; check allowed */
          if (!tp_strv_contains (allowed,
                TP_PROP_CHANNEL_INTERFACE_SMS1_SMS_CHANNEL))
            continue;

          nb_fixed_props = 2;
        }

      if (g_variant_n_children (fixed) == nb_fixed_props)
        {
          g_variant_unref (fixed);
          g_free (allowed);
          return TRUE;
        }
    }

  return FALSE;

}

static gboolean
supports_call_full (TpCapabilities *self,
    TpEntityType expected_entity_type,
    gboolean expected_initial_audio,
    gboolean expected_initial_video)
{
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed_prop;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed, &allowed_prop))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;
      guint nb_fixed_props = 2;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
      if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_CALL1))
        continue;

      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);
      if (!valid || entity_type != expected_entity_type)
        continue;

      if (expected_initial_audio)
        {
          /* We want audio, INITIAL_AUDIO must be in either fixed or allowed */
          if (tp_vardict_get_boolean (fixed,
                  TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_AUDIO, NULL))
            {
              nb_fixed_props++;
            }
          else if (!tp_strv_contains (allowed_prop,
                  TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_AUDIO))
            {
              continue;
            }
        }

      if (expected_initial_video)
        {
          /* We want video, INITIAL_VIDEO must be in either fixed or allowed */
          if (tp_vardict_get_boolean (fixed,
                  TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_VIDEO, NULL))
            {
              nb_fixed_props++;
            }
          else if (!tp_strv_contains (allowed_prop,
                  TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_VIDEO))
            {
              continue;
            }
        }

      /* We found the right class */
      if (g_variant_n_children (fixed) == nb_fixed_props)
        {
          g_variant_unref (fixed);
          g_free (allowed_prop);
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * tp_capabilities_supports_audio_call:
 * @self: a #TpCapabilities object
 * @entity_type: the entity type of the call; #TP_ENTITY_TYPE_CONTACT for
 *  private, #TP_ENTITY_TYPE_ROOM or #TP_ENTITY_TYPE_NONE for conference
 *  (depending on the protocol)
 *
 * Return whether audio calls can be established, for instance by calling
 * tp_account_channel_request_new_audio_call(), followed by
 * tp_account_channel_request_set_target_id() with @entity_type.
 *
 * To check whether requests using
 * tp_account_channel_request_set_target_contact() would work, set
 * @entity_type to %TP_ENTITY_TYPE_CONTACT.
 *
 * Returns: %TRUE if a channel request containing Call as ChannelType,
 * @entity_type as TargetEntityType, a True value for InitialAudio and an
 * identifier of the appropriate type can be expected to work, %FALSE otherwise.
 *
 * Since: 0.17.6
 */
gboolean
tp_capabilities_supports_audio_call (TpCapabilities *self,
    TpEntityType entity_type)
{
  return supports_call_full (self, entity_type, TRUE, FALSE);
}

/**
 * tp_capabilities_supports_audio_video_call:
 * @self: a #TpCapabilities object
 * @entity_type: the entity type of the call; #TP_ENTITY_TYPE_CONTACT for
 *  private, #TP_ENTITY_TYPE_ROOM or #TP_ENTITY_TYPE_NONE for conference
 *  (depending on the protocol)
 *
 * Return whether audio/video calls can be established, for instance by calling
 * tp_account_channel_request_new_audio_video_call(), followed by
 * tp_account_channel_request_set_target_id() with @entity_type.
 *
 * To check whether requests using
 * tp_account_channel_request_set_target_contact() would work, set
 * @entity_type to %TP_ENTITY_TYPE_CONTACT.
 *
 * Returns: %TRUE if a channel request containing Call as ChannelType,
 * @entity_type as TargetEntityType, a True value for
 * InitialAudio/InitialVideo and an identifier of the appropriate type can be
 * expected to work,
 * %FALSE otherwise.
 *
 * Since: 0.17.6
 */
gboolean
tp_capabilities_supports_audio_video_call (TpCapabilities *self,
    TpEntityType entity_type)
{
  return supports_call_full (self, entity_type, TRUE, TRUE);
}

typedef enum {
    FT_CAP_FLAGS_NONE = 0,
    FT_CAP_FLAG_URI = (1<<0),
    FT_CAP_FLAG_OFFSET = (1<<1),
    FT_CAP_FLAG_DATE = (1<<2),
    FT_CAP_FLAG_DESCRIPTION = (1<<3)
} FTCapFlags;

static gboolean
supports_file_transfer (TpCapabilities *self,
    FTCapFlags flags)
{
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed, &allowed))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;
      guint n_fixed = 2;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);

      if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1))
        continue;

      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);

      if (!valid)
        continue;

      if (entity_type != TP_ENTITY_TYPE_CONTACT)
        continue;

      /* ContentType, Filename, Size are mandatory. In principle we could check
       * that the CM allows them, but not allowing them would be ridiculous,
       * so we don't.
       */

      if ((flags & FT_CAP_FLAG_DESCRIPTION) != 0)
        {
          /* Description makes no sense as a fixed property so we assume
           * the CM won't be ridiculous */
          if (!tp_strv_contains (allowed,
                TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_DESCRIPTION))
            continue;
        }

      if ((flags & FT_CAP_FLAG_DATE) != 0)
        {
          /* makes no sense as a fixed property */
          if (!tp_strv_contains (allowed,
                TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_DATE))
            continue;
        }

      if ((flags & FT_CAP_FLAG_URI) != 0)
        {
          /* makes no sense as a fixed property */
          if (!tp_strv_contains (allowed,
                TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_URI))
            continue;
        }

      if ((flags & FT_CAP_FLAG_OFFSET) != 0)
        {
          /* makes no sense as a fixed property */
          if (!tp_strv_contains (allowed,
                TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_INITIAL_OFFSET))
            continue;
        }

      if (n_fixed != g_variant_n_children (fixed))
        continue;

      g_variant_unref (fixed);
      g_free (allowed);
      return TRUE;
    }

  return FALSE;
}

/**
 * tp_capabilities_supports_file_transfer:
 * @self: a #TpCapabilities object
 *
 * Return whether private file transfer can be established by providing
 * a contact identifier.
 *
 * Returns: %TRUE if a channel request containing FileTransfer as ChannelType,
 * HandleTypeContact as TargetEntityType and a contact identifier can be
 * expected to work, %FALSE otherwise.
 *
 * Since: 0.17.6
 */
gboolean
tp_capabilities_supports_file_transfer (TpCapabilities *self)
{
  return supports_file_transfer (self, FT_CAP_FLAGS_NONE);
}

/**
 * tp_capabilities_supports_file_transfer_uri:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: %TRUE if requests as described for
 *  tp_capabilities_supports_file_transfer() can also specify the outgoing
 *  file's URI
 *
 * Since: 0.19.0
 */
gboolean
tp_capabilities_supports_file_transfer_uri (TpCapabilities *self)
{
  return supports_file_transfer (self, FT_CAP_FLAG_URI);
}

/**
 * tp_capabilities_supports_file_transfer_description:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: %TRUE if requests as described for
 *  tp_capabilities_supports_file_transfer() can also specify the outgoing
 *  file's description
 *
 * Since: 0.19.0
 */
gboolean
tp_capabilities_supports_file_transfer_description (TpCapabilities *self)
{
  return supports_file_transfer (self, FT_CAP_FLAG_DESCRIPTION);
}

/**
 * tp_capabilities_supports_file_transfer_initial_offset:
 * @self: a #TpCapabilities object
 *
 * Return whether an initial offset other than 0 can be specified on
 * outgoing file transfers. This can be used to resume partial transfers,
 * by omitting the part that has already been sent.
 *
 * Returns: %TRUE if requests as described for
 *  tp_capabilities_supports_file_transfer() can also specify an
 *  initial offset greater than 0
 *
 * Since: 0.19.0
 */
gboolean
tp_capabilities_supports_file_transfer_initial_offset (TpCapabilities *self)
{
  return supports_file_transfer (self, FT_CAP_FLAG_OFFSET);
}

/**
 * tp_capabilities_supports_file_transfer_timestamp:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: %TRUE if requests as described for
 *  tp_capabilities_supports_file_transfer() can also specify the outgoing
 *  file's timestamp
 *
 * Since: 0.19.0
 */
gboolean
tp_capabilities_supports_file_transfer_timestamp (TpCapabilities *self)
{
  return supports_file_transfer (self, FT_CAP_FLAG_DATE);
}

static gboolean
tp_capabilities_supports_tubes_common (TpCapabilities *self,
    const gchar *expected_channel_type,
    TpEntityType expected_entity_type,
    const gchar *service_prop,
    const gchar *expected_service)
{
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);
  g_return_val_if_fail (expected_entity_type == TP_ENTITY_TYPE_CONTACT ||
      expected_entity_type == TP_ENTITY_TYPE_ROOM, FALSE);

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed, &allowed))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;
      guint nb_fixed_props = 2;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
      if (tp_strdiff (chan_type, expected_channel_type))
        continue;

      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);
      if (!valid || entity_type != expected_entity_type)
        continue;

      if (expected_service != NULL && self->priv->contact_specific)
        {
          const gchar *service;

          nb_fixed_props++;
          service = tp_vardict_get_string (fixed, service_prop);
          if (tp_strdiff (service, expected_service))
            continue;
        }

      if (g_variant_n_children (fixed) == nb_fixed_props)
        {
          g_variant_unref (fixed);
          g_free (allowed);
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * tp_capabilities_supports_stream_tubes:
 * @self: a #TpCapabilities object
 * @entity_type: the entity type of the tube (either #TP_ENTITY_TYPE_CONTACT
 * or #TP_ENTITY_TYPE_ROOM)
 * @service: the service of the tube, or %NULL
 *
 * If the #TpCapabilities:contact-specific property is %TRUE, this function
 * checks if the contact associated with this #TpCapabilities supports
 * stream tubes with @entity_type as TargetEntityType.
 * If @service is not %NULL, it also checks if it supports stream tubes
 * with @service as #TP_PROP_CHANNEL_TYPE_STREAM_TUBE_SERVICE.
 *
 * If the #TpCapabilities:contact-specific property is %FALSE, this function
 * checks if the connection supports requesting stream tube channels with
 * @entity_type as ChannelType. The @service argument is unused in this case.
 *
 * Returns: %TRUE if the contact or connection supports this type of stream
 * tubes.
 *
 * Since: 0.13.0
 */
gboolean
tp_capabilities_supports_stream_tubes (TpCapabilities *self,
    TpEntityType entity_type,
    const gchar *service)
{
  return tp_capabilities_supports_tubes_common (self,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1, entity_type,
      TP_PROP_CHANNEL_TYPE_STREAM_TUBE1_SERVICE, service);
}

/**
 * tp_capabilities_supports_dbus_tubes:
 * @self: a #TpCapabilities object
 * @entity_type: the entity type of the tube (either #TP_ENTITY_TYPE_CONTACT
 * or #TP_ENTITY_TYPE_ROOM)
 * @service_name: the service name of the tube, or %NULL
 *
 * If the #TpCapabilities:contact-specific property is %TRUE, this function
 * checks if the contact associated with this #TpCapabilities supports
 * D-Bus tubes with @entity_type as TargetEntityType.
 * If @service_name is not %NULL, it also checks if it supports stream tubes
 * with @service as #TP_PROP_CHANNEL_TYPE_DBUS_TUBE_SERVICE_NAME.
 *
 * If the #TpCapabilities:contact-specific property is %FALSE, this function
 * checks if the connection supports requesting D-Bus tube channels with
 * @entity_type as ChannelType. The @service_name argument is unused in
 * this case.
 *
 * Returns: %TRUE if the contact or connection supports this type of D-Bus
 * tubes.
 *
 * Since: 0.13.0
 */
gboolean
tp_capabilities_supports_dbus_tubes (TpCapabilities *self,
    TpEntityType entity_type,
    const gchar *service_name)
{
  return tp_capabilities_supports_tubes_common (self,
      TP_IFACE_CHANNEL_TYPE_DBUS_TUBE1, entity_type,
      TP_PROP_CHANNEL_TYPE_DBUS_TUBE1_SERVICE_NAME, service_name);
}

/**
 * tp_capabilities_supports_contact_search:
 * @self: a #TpCapabilities object
 * @with_limit: (out): if not %NULL, used to return %TRUE if the limit
 * parameter to tp_contact_search_new_async() and
 * tp_contact_search_reset_async() can be nonzero
 * @with_server: (out): if not %NULL, used to return %TRUE if the server
 * parameter to tp_contact_search_new_async() and
 * tp_contact_search_reset_async() can be non-%NULL
 *
 * Return whether this protocol or connection can perform contact
 * searches. Optionally, also return whether a limited number of
 * results can be specified, and whether alternative servers can be
 * searched.
 *
 * Returns: %TRUE if #TpContactSearch can be used.
 *
 * Since: 0.13.11
 */
gboolean
tp_capabilities_supports_contact_search (TpCapabilities *self,
    gboolean *with_limit,
    gboolean *with_server)
{
  gboolean ret = FALSE;
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed_properties;
  guint j;

  g_return_val_if_fail (TP_IS_CAPABILITIES (self), FALSE);

  if (with_limit)
    *with_limit = FALSE;

  if (with_server)
    *with_server = FALSE;

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed,
        &allowed_properties))
    {
      const gchar *chan_type;

      /* ContactSearch channel should have ChannelType and TargetEntityType=NONE
       * but CM implementations are wrong and omitted TargetEntityType,
       * so it's set in stone now.  */
      if (g_variant_n_children (fixed) != 1)
        continue;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
      if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH1))
        continue;

      ret = TRUE;

      for (j = 0; allowed_properties[j] != NULL; j++)
        {
          if (with_limit)
            {
              if (!tp_strdiff (allowed_properties[j],
                       TP_PROP_CHANNEL_TYPE_CONTACT_SEARCH1_LIMIT))
                *with_limit = TRUE;
            }

          if (with_server)
            {
              if (!tp_strdiff (allowed_properties[j],
                       TP_PROP_CHANNEL_TYPE_CONTACT_SEARCH1_SERVER))
                *with_server = TRUE;
            }
        }
    }

  return ret;
}

/**
 * tp_capabilities_supports_room_list:
 * @self: a #TpCapabilities object
 * @with_server: (out): if not %NULL, used to return %TRUE if the
 * #TP_PROP_CHANNEL_TYPE_ROOM_LIST_SERVER property can be defined when
 * requesting a RoomList channel.
 *
 * Discovers whether this protocol or connection supports listing rooms.
 * Specifically, if this function returns %TRUE, a room list channel can be
 * requested as follows:
 * |[
 * GHashTable *request;
 * TpAccountChannelRequest *req;
 *
 * request = tp_asv_new (
 *     TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *       TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
 *     TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT, TP_ENTITY_TYPE_NONE,
 *     NULL);
 *
 * req = tp_account_channel_request_new (account, request,
 *    TP_USER_ACTION_TIME_CURRENT_TIME);
 *
 * tp_account_channel_request_create_and_handle_channel_async (req, NULL,
 *     create_channel_cb, NULL);
 *
 * g_object_unref (req);
 * g_hash_table_unref (request);
 * ]|
 *
 * If @with_server is set to %TRUE, a list of rooms on a particular server can
 * be requested as follows:
 * |[
 * /\* Same code as above but with request defined using: *\/
 * request = tp_asv_new (
 *     TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *       TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
 *     TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT, TP_ENTITY_TYPE_NONE,
 *     TP_PROP_CHANNEL_TYPE_ROOM_LIST_SERVER, G_TYPE_STRING,
 *       "characters.shakespeare.lit",
 *     NULL);
 * ]|
 *
 * Returns: %TRUE if a channel request containing RoomList as ChannelType,
 * HandleTypeNone as TargetEntityType can be expected to work,
 * %FALSE otherwise.
 *
 * Since: 0.13.14
 */
gboolean
tp_capabilities_supports_room_list (TpCapabilities *self,
    gboolean *with_server)
{
  gboolean result = FALSE;
  gboolean server = FALSE;
  GVariantIter iter;
  GVariant *fixed;
  const gchar **allowed_properties;

  g_variant_iter_init (&iter, self->priv->classes_variant);

  while (g_variant_iter_loop (&iter, "(@a{sv}^a&s)", &fixed,
        &allowed_properties))
    {
      const gchar *chan_type;
      TpEntityType entity_type;
      gboolean valid;

      if (g_variant_n_children (fixed) != 2)
        continue;

      chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
      if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_ROOM_LIST1))
        continue;

      entity_type = tp_vardict_get_uint32 (fixed,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);
      if (!valid || entity_type != TP_ENTITY_TYPE_NONE)
        continue;

      result = TRUE;

      server = tp_strv_contains (allowed_properties,
          TP_PROP_CHANNEL_TYPE_ROOM_LIST1_SERVER);
      break;
    }

  if (with_server != NULL)
    *with_server = server;

  return result;
}

/**
 * tp_capabilities_dup_channel_classes:
 * @self: a #TpCapabilities
 *
 * Return the #TpCapabilities:channel-classes property
 *
 * Returns: (transfer full): the value of the
 * #TpCapabilities:channel-classes property
 */
GVariant *
tp_capabilities_dup_channel_classes (TpCapabilities *self)
{
  return g_variant_ref (self->priv->classes_variant);
}
