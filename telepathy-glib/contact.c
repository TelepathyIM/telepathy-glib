/* Object representing a Telepathy contact
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
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

#include <telepathy-glib/contact.h>


/**
 * SECTION:contact
 * @title: TpContact
 * @short_description: object representing a contact
 * @see_also: #TpConnection
 *
 * #TpContact objects represent the contacts on a particular #TpConnection.
 */

/**
 * TpContact:
 *
 * An object representing a contact on a #TpConnection.
 *
 * Contact objects are instantiated using
 * tp_connection_get_contacts_by_handle().
 */

/**
 * TpContactFeature:
 * @TP_CONTACT_FEATURE_ALIAS: #TpContact:alias
 * @TP_CONTACT_FEATURE_AVATAR_TOKEN: #TpContact:avatar-token
 * @TP_CONTACT_FEATURE_PRESENCE: #TpContact:presence-type,
 *  #TpContact:presence-status and #TpContact:presence-message
 * @NUM_TP_CONTACT_FEATURES: 1 higher than the highest TpContactFeature
 *  supported by this version of telepathy-glib
 *
 * Enumeration representing the features a #TpContact can optionally support.
 * When requesting a #TpContact, library users specify the desired features;
 * the #TpContact code will only initialize state for those features, to
 * avoid unwanted D-Bus round-trips and signal connections.
 */

G_DEFINE_TYPE (TpContact, tp_contact, G_TYPE_OBJECT);


enum {
    PROP_CONNECTION = 1,
    PROP_HANDLE,
    PROP_IDENTIFIER,
    PROP_ALIAS,
    PROP_AVATAR_TOKEN,
    PROP_PRESENCE_TYPE,
    PROP_PRESENCE_STATUS,
    PROP_PRESENCE_MESSAGE,
    N_PROPS
};


/* The API allows for more than 32 features, but this implementation does
 * not. We can easily expand this later. */
typedef enum {
    CONTACT_FEATURE_FLAG_ALIAS = 1 << TP_CONTACT_FEATURE_ALIAS,
    CONTACT_FEATURE_FLAG_AVATAR_TOKEN = 1 << TP_CONTACT_FEATURE_AVATAR_TOKEN,
    CONTACT_FEATURE_FLAG_PRESENCE = 1 << TP_CONTACT_FEATURE_PRESENCE,
} ContactFeatureFlags;

struct _TpContactPrivate {
    /* basics */
    TpConnection *connection;
    TpHandle handle;
    gchar *identifier;
    ContactFeatureFlags has_features;

    /* aliasing */
    gchar *alias;

    /* avatars */
    gchar *avatar_token;

    /* presence */
    TpConnectionPresenceType presence_type;
    gchar *presence_status;
    gchar *presence_message;
};


/**
 * tp_contact_get_connection:
 * @self: a contact
 *
 * <!-- nothing more to say -->
 *
 * Returns: a borrowed reference to the connection associated with this
 *  contact (it must be referenced with g_object_ref if it must remain valid
 *  longer than the contact).
 */
TpConnection *
tp_contact_get_connection (TpContact *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->priv->connection;
}


/**
 * tp_contact_get_handle:
 * @self: a contact
 *
 * <!-- nothing more to say -->
 *
 * Returns: the contact's handle, or 0 if the contact belongs to a connection
 *  that has become invalid.
 */
TpHandle
tp_contact_get_handle (TpContact *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->priv->handle;
}

/**
 * tp_contact_get_identifier:
 * @self: a contact
 *
 * <!-- nothing more to say -->
 *
 * Returns: the contact's identifier (XMPP JID, MSN Passport, AOL screen-name,
 *  etc. - whatever the underlying protocol uses to identify a user).
 */
const gchar *
tp_contact_get_identifier (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  /* identifier must be non-NULL by the time we're visible to library-user
   * code */
  g_return_val_if_fail (self->priv->identifier != NULL, NULL);

  return self->priv->identifier;
}

/**
 * tp_contact_has_feature:
 * @self: a contact
 * @feature: a desired feature
 *
 * <!-- -->
 *
 * Returns: %TRUE if @self has been set up to track the feature @feature
 */
gboolean
tp_contact_has_feature (TpContact *self,
                        TpContactFeature feature)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (feature < NUM_TP_CONTACT_FEATURES, FALSE);

  return ((self->priv->has_features & (1 << feature)) != 0);
}


/**
 * tp_contact_get_alias:
 * @self: a contact
 *
 * If this object has been set up to track %TP_CONTACT_FEATURE_ALIAS
 * and the underlying connection supports the Aliasing interface, return
 * this contact's alias.
 *
 * Otherwise, return this contact's identifier in the IM protocol.
 *
 * Returns: a reasonable name or alias to display for this contact
 */
const gchar *
tp_contact_get_alias (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  /* identifier must be non-NULL by the time we're visible to library-user
   * code */
  g_return_val_if_fail (self->priv->identifier != NULL, NULL);

  if (self->priv->alias != NULL)
    return self->priv->alias;

  return self->priv->identifier;
}


/**
 * tp_contact_get_avatar_token:
 * @self: a contact
 *
 * If this object has been set up to track %TP_CONTACT_FEATURE_AVATAR_TOKEN,
 * return the token identifying this contact's avatar, an empty string if they
 * are known to have no avatar, or %NULL if it is unknown whether they have
 * an avatar.
 *
 * Otherwise, return %NULL in all cases.
 *
 * Returns: either the avatar token, an empty string, or %NULL
 */
const gchar *
tp_contact_get_avatar_token (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->priv->avatar_token;
}


/**
 * tp_contact_get_presence_type:
 * @self: a contact
 *
 * If this object has been set up to track %TP_CONTACT_FEATURE_PRESENCE
 * and the underlying connection supports either the Presence or
 * SimplePresence interfaces, return the type of the contact's presence.
 *
 * Otherwise, return %TP_CONNECTION_PRESENCE_TYPE_UNSET.
 *
 * Returns: #TpContact:presence-type
 */
TpConnectionPresenceType
tp_contact_get_presence_type (TpContact *self)
{
  g_return_val_if_fail (self != NULL, TP_CONNECTION_PRESENCE_TYPE_UNSET);

  return self->priv->presence_type;
}


/**
 * tp_contact_get_presence_status:
 * @self: a contact
 *
 * If this object has been set up to track %TP_CONTACT_FEATURE_PRESENCE
 * and the underlying connection supports either the Presence or
 * SimplePresence interfaces, return the presence status, which may either
 * take a well-known value like "available", or a protocol-specific
 * (or even connection-manager-specific) value like "out-to-lunch".
 *
 * Otherwise, return an empty string.
 *
 * Returns: #TpContact:presence-status
 */
const gchar *
tp_contact_get_presence_status (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (self->priv->presence_status == NULL ? "" :
      self->priv->presence_status);
}


/**
 * tp_contact_get_presence_message:
 * @self: a contact
 *
 * If this object has been set up to track %TP_CONTACT_FEATURE_PRESENCE,
 * the underlying connection supports either the Presence or
 * SimplePresence interfaces, and the contact has set a message more specific
 * than the presence type or presence status, return that message.
 *
 * Otherwise, return an empty string.
 *
 * Returns: #TpContact:presence-message
 */
const gchar *
tp_contact_get_presence_message (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (self->priv->presence_message == NULL ? "" :
      self->priv->presence_message);
}


static void
tp_contact_dispose (GObject *object)
{
  TpContact *self = TP_CONTACT (object);

  if (self->priv->handle != 0)
    {
      g_assert (self->priv->connection != NULL);

      tp_connection_unref_handles (self->priv->connection,
          TP_HANDLE_TYPE_CONTACT, 1, &self->priv->handle);

      self->priv->handle = 0;
    }

  if (self->priv->connection != NULL)
    {
      g_object_unref (self->priv->connection);
      self->priv->connection = NULL;
    }

  ((GObjectClass *) tp_contact_parent_class)->dispose (object);
}


static void
tp_contact_finalize (GObject *object)
{
  TpContact *self = TP_CONTACT (object);

  g_free (self->priv->identifier);
  g_free (self->priv->alias);
  g_free (self->priv->avatar_token);
  g_free (self->priv->presence_status);
  g_free (self->priv->presence_message);

  ((GObjectClass *) tp_contact_parent_class)->finalize (object);
}


static void
tp_contact_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  TpContact *self = TP_CONTACT (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->connection);
      break;

    case PROP_HANDLE:
      g_value_set_uint (value, self->priv->handle);
      break;

    case PROP_IDENTIFIER:
      g_assert (self->priv->identifier != NULL);
      g_value_set_string (value, self->priv->identifier);
      break;

    case PROP_ALIAS:
      /* tp_contact_get_alias actually has some logic, so avoid
       * duplicating it */
      g_value_set_string (value, tp_contact_get_alias (self));
      break;

    case PROP_AVATAR_TOKEN:
      g_value_set_string (value, self->priv->avatar_token);
      break;

    case PROP_PRESENCE_TYPE:
      g_value_set_uint (value, self->priv->presence_type);
      break;

    case PROP_PRESENCE_STATUS:
      g_value_set_string (value, tp_contact_get_presence_status (self));
      break;

    case PROP_PRESENCE_MESSAGE:
      g_value_set_string (value, tp_contact_get_presence_message (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
tp_contact_class_init (TpContactClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpContactPrivate));
  object_class->get_property = tp_contact_get_property;
  object_class->dispose = tp_contact_dispose;
  object_class->finalize = tp_contact_finalize;

  /**
   * TpContact:connection:
   *
   * The #TpConnection to which this contact belongs.
   */
  param_spec = g_param_spec_object ("connection", "TpConnection object",
      "Connection object that owns this channel",
      TP_TYPE_CONNECTION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  /**
   * TpContact:handle:
   *
   * The contact's handle in the Telepathy D-Bus API, a handle of type
   * %TP_HANDLE_TYPE_CONTACT representing the string
   * given by #TpContact:identifier.
   *
   * This handle is referenced using the Telepathy D-Bus API and remains
   * referenced for as long as the #TpContact exists and the
   * #TpContact:connection remains valid.
   *
   * If the #TpContact:connection becomes invalid, this property is no longer
   * meaningful and will be set to 0.
   */
  param_spec = g_param_spec_uint ("handle",
      "Handle",
      "The TP_HANDLE_TYPE_CONTACT handle for this contact",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HANDLE, param_spec);

  /**
   * TpContact:identifier:
   *
   * The contact's identifier in the instant messaging protocol (e.g.
   * XMPP JID, SIP URI, AOL screenname or IRC nick).
   */
  param_spec = g_param_spec_string ("identifier",
      "IM protocol identifier",
      "The contact's identifier in the instant messaging protocol (e.g. "
        "XMPP JID, SIP URI, AOL screenname or IRC nick)",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_IDENTIFIER, param_spec);

  /**
   * TpContact:alias:
   *
   * The contact's alias if available, falling back to their
   * #TpContact:identifier if no alias is available or if the #TpContact has
   * not been set up to track %TP_CONTACT_FEATURE_ALIAS.
   *
   * This alias may have been supplied by the contact themselves, or by the
   * local user, so it does not necessarily unambiguously identify the contact.
   * However, it is suitable for use as a main "display name" for the contact.
   */
  param_spec = g_param_spec_string ("alias",
      "Alias",
      "The contact's alias (display name)",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ALIAS, param_spec);

  /**
   * TpContact:avatar-token:
   *
   * An opaque string representing state of the contact's avatar (depending on
   * the protocol, this might be a hash, a timestamp or something else), or
   * an empty string if there is no avatar.
   *
   * This may be %NULL if it is not known whether this contact has an avatar
   * or not (either for network protocol reasons, or because this #TpContact
   * has not been set up to track %TP_CONTACT_FEATURE_AVATAR_TOKEN).
   */
  param_spec = g_param_spec_string ("avatar-token",
      "Avatar token",
      "Opaque string representing the contact's avatar, or \"\", or NULL",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_AVATAR_TOKEN,
      param_spec);

  /**
   * TpContact:presence-type:
   *
   * The #TpConnectionPresenceType representing the type of presence status
   * for this contact.
   *
   * This is provided so even unknown values for #TpContact:presence-status
   * can be classified into their fundamental types.
   *
   * This may be %TP_CONNECTION_PRESENCE_TYPE_UNSET if this #TpContact
   * has not been set up to track %TP_CONTACT_FEATURE_PRESENCE.
   */
  param_spec = g_param_spec_uint ("presence-type",
      "Presence type",
      "The TpConnectionPresenceType for this contact",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PRESENCE_TYPE,
      param_spec);

  /**
   * TpContact:presence-status:
   *
   * A string representing the presence status of this contact. This may be
   * a well-known string from the Telepathy specification, like "available",
   * or a connection-manager-specific string, like "out-to-lunch".
   *
   * This may be an empty string if this #TpContact object has not been set up
   * to track %TP_CONTACT_FEATURE_PRESENCE.
   *
   * FIXME: reviewers, should this be "" or NULL when not available?
   */
  param_spec = g_param_spec_string ("presence-status",
      "Presence status",
      "Possibly connection-manager-specific string representing the "
        "contact's presence status",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PRESENCE_STATUS,
      param_spec);

  /**
   * TpContact:presence-message:
   *
   * If this contact has set a user-defined status message, that message;
   * if not, an empty string (which user interfaces may replace with a
   * localized form of the #TpContact:presence-status or
   * #TpContact:presence-type).
   *
   * This may be an empty string even if the contact has set a message,
   * if this #TpContact object has not been set up to track
   * %TP_CONTACT_FEATURE_PRESENCE.
   */
  param_spec = g_param_spec_string ("presence-message",
      "Presence message",
      "User-defined status message, or an empty string",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PRESENCE_MESSAGE,
      param_spec);
}


static void
tp_contact_init (TpContact *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CONTACT,
      TpContactPrivate);
}
