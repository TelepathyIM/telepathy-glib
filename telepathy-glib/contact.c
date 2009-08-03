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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CONTACTS
#include "telepathy-glib/connection-internal.h"
#include "telepathy-glib/debug-internal.h"


/**
 * SECTION:contact
 * @title: TpContact
 * @short_description: object representing a contact
 * @see_also: #TpConnection
 *
 * #TpContact objects represent the contacts on a particular #TpConnection.
 *
 * Since: 0.7.18
 */

/**
 * TpContact:
 *
 * An object representing a contact on a #TpConnection.
 *
 * Contact objects support tracking a number of attributes of contacts, as
 * described by the #TpContactFeature flags. Features can be specified when
 * instantiating contact objects (with tp_connection_get_contacts_by_id() or
 * tp_connection_get_contacts_by_handle()), or added to an existing contact
 * object with tp_connection_upgrade_contacts(). For example, a client wishing
 * to keep track of a contact's alias would set #TP_CONTACT_FEATURE_ALIAS, and
 * then listen for the "notify::alias" signal, emitted whenever the
 * #TpContact:alias property changes.
 *
 * Note that releasing a #TpContact object might release handle references
 * held by calling tp_cli_connection_call_request_handles(),
 * tp_cli_connection_run_request_handles(),
 * tp_cli_connection_call_hold_handles(),
 * tp_cli_connection_run_hold_handles(),
 * tp_cli_connection_interface_contacts_call_get_contact_attributes() or
 * tp_cli_connection_interface_contacts_run_get_contact_attributes() directly.
 * Those functions should be avoided in favour of using #TpContact,
 * tp_connection_hold_handles(), tp_connection_request_handles() and
 * tp_connection_get_contact_attributes().
 *
 * Since: 0.7.18
 */

struct _TpContactClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _TpContact {
    /*<private>*/
    GObject parent;
    TpContactPrivate *priv;
};

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
 *
 * Since: 0.7.18
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
 * Returns: a borrowed reference to the #TpContact:connection
 *  (it must be referenced with g_object_ref if it must remain valid
 *  longer than the contact)
 *
 * Since: 0.7.18
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
 * Return the contact's handle, which is of type %TP_HANDLE_TYPE_CONTACT,
 * or 0 if the #TpContact:connection has become invalid.
 *
 * This handle is referenced using the Telepathy D-Bus API and remains
 * referenced for as long as @self exists and the
 * #TpContact:connection remains valid.
 *
 * However, the caller of this function does not gain an additional reference
 * to the handle.
 *
 * Returns: the same handle as the #TpContact:handle property
 *
 * Since: 0.7.18
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
 * Return the contact's identifier. This remains valid for as long as @self
 * exists; if the caller requires a string that will persist for longer than
 * that, it must be copied with g_strdup().
 *
 * Returns: the same non-%NULL identifier as the #TpContact:identifier property
 *
 * Since: 0.7.18
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
 *
 * Since: 0.7.18
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
 * Return the contact's alias. This remains valid until the main loop
 * is re-entered; if the caller requires a string that will persist for
 * longer than that, it must be copied with g_strdup().
 *
 * Returns: the same non-%NULL alias as the #TpContact:alias
 *
 * Since: 0.7.18
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
 * Return the contact's avatar token. This remains valid until the main loop
 * is re-entered; if the caller requires a string that will persist for
 * longer than that, it must be copied with g_strdup().
 *
 * Returns: the same token as the #TpContact:avatar-token property
 *  (possibly %NULL)
 *
 * Since: 0.7.18
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
 * Returns: the same presence type as the #TpContact:presence-type property
 *
 * Since: 0.7.18
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
 * Return the name of the contact's presence status, or an empty string.
 * This remains valid until the main loop is re-entered; if the caller
 * requires a string that will persist for longer than that, it must be
 * copied with g_strdup().
 *
 * Returns: the same non-%NULL status name as the #TpContact:presence-status
 *  property
 *
 * Since: 0.7.18
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
 * Return the contact's user-defined status message, or an empty string.
 * This remains valid until the main loop is re-entered; if the caller
 * requires a string that will persist for longer than that, it must be
 * copied with g_strdup().
 *
 * Returns: the same non-%NULL message as the #TpContact:presence-message
 *  property
 *
 * Since: 0.7.18
 */
const gchar *
tp_contact_get_presence_message (TpContact *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (self->priv->presence_message == NULL ? "" :
      self->priv->presence_message);
}


void
_tp_contact_connection_invalidated (TpContact *contact)
{
  /* The connection has gone away, so we no longer have a meaningful handle,
   * and will never have one again. */
  g_assert (contact->priv->handle != 0);
  contact->priv->handle = 0;
  g_object_notify ((GObject *) contact, "handle");
}


static void
tp_contact_dispose (GObject *object)
{
  TpContact *self = TP_CONTACT (object);

  if (self->priv->handle != 0)
    {
      g_assert (self->priv->connection != NULL);

      _tp_connection_remove_contact (self->priv->connection,
          self->priv->handle, self);
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
   * However, getting this property does not cause an additional reference
   * to the handle to be held.
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
   * XMPP JID, SIP URI, AOL screenname or IRC nick - whatever the underlying
   * protocol uses to identify a user). This is never %NULL.
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
   * not been set up to track %TP_CONTACT_FEATURE_ALIAS. This is never %NULL.
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
   * to track %TP_CONTACT_FEATURE_PRESENCE. It is never %NULL.
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
   * %TP_CONTACT_FEATURE_PRESENCE. It is never %NULL.
   */
  param_spec = g_param_spec_string ("presence-message",
      "Presence message",
      "User-defined status message, or an empty string",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PRESENCE_MESSAGE,
      param_spec);
}


/* Consumes one reference to @handle. */
static TpContact *
tp_contact_ensure (TpConnection *connection,
                   TpHandle handle)
{
  TpContact *self = _tp_connection_lookup_contact (connection, handle);

  if (self != NULL)
    {
      g_assert (self->priv->handle == handle);

      /* we have one ref to this handle more than we need, so consume it */
      tp_connection_unref_handles (self->priv->connection,
          TP_HANDLE_TYPE_CONTACT, 1, &self->priv->handle);

      return g_object_ref (self);
    }

  self = TP_CONTACT (g_object_new (TP_TYPE_CONTACT, NULL));

  self->priv->handle = handle;
  _tp_connection_add_contact (connection, handle, self);
  self->priv->connection = g_object_ref (connection);

  return self;
}


static void
tp_contact_init (TpContact *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CONTACT,
      TpContactPrivate);
}


typedef struct _ContactsContext ContactsContext;
typedef void (*ContactsProc) (ContactsContext *self);
typedef enum { CB_BY_HANDLE, CB_BY_ID, CB_UPGRADE } ContactsSignature;

struct _ContactsContext {
    gsize refcount;

    /* owned */
    TpConnection *connection;
    /* array of owned TpContact; preallocated but empty until handles have
     * been held or requested */
    GPtrArray *contacts;
    /* array of handles; empty until RequestHandles has returned, if we
     * started from IDs */
    GArray *handles;
    /* array of handles; empty until RequestHandles has returned, if we
     * started from IDs */
    GArray *invalid;

    /* strv of IDs; NULL unless we started from IDs */
    GPtrArray *request_ids;
    /* ID => GError, NULL unless we started from IDs */
    GHashTable *request_errors;

    /* features we need before this request can finish */
    ContactFeatureFlags wanted;

    /* callback for when we've finished, plus the usual misc */
    ContactsSignature signature;
    union {
        TpConnectionContactsByHandleCb by_handle;
        TpConnectionContactsByIdCb by_id;
        TpConnectionUpgradeContactsCb upgrade;
    } callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;

    /* queue of ContactsProc */
    GQueue todo;

    /* index into handles or ids, only used when the first HoldHandles call
     * failed with InvalidHandle, or the RequestHandles call failed with
     * NotAvailable */
    guint next_index;
};


static ContactsContext *
contacts_context_new (TpConnection *connection,
                      guint n_contacts,
                      ContactFeatureFlags want_features,
                      ContactsSignature signature,
                      gpointer user_data,
                      GDestroyNotify destroy,
                      GObject *weak_object)
{
  ContactsContext *c = g_slice_new0 (ContactsContext);

  c->refcount = 1;
  c->connection = g_object_ref (connection);
  c->contacts = g_ptr_array_sized_new (n_contacts);
  c->handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), n_contacts);
  c->invalid = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), n_contacts);

  c->wanted = want_features;
  c->signature = signature;
  c->user_data = user_data;
  c->destroy = destroy;
  c->weak_object = weak_object;

  /* This code (and lots of telepathy-glib, really) won't work if this
   * assertion fails, because we put function pointers in a GQueue. If anyone
   * cares about platforms where this fails, fixing this would involve
   * slice-allocating sizeof (GCallback) bytes repeatedly, and putting *those*
   * in the queue. */
  tp_verify_statement (sizeof (GCallback) == sizeof (gpointer));

  g_queue_init (&c->todo);

  return c;
}


static void
contacts_context_unref (gpointer p)
{
  ContactsContext *c = p;

  if ((--c->refcount) > 0)
    return;

  g_assert (c->connection != NULL);
  g_object_unref (c->connection);
  c->connection = NULL;

  g_queue_clear (&c->todo);

  g_assert (c->contacts != NULL);
  g_ptr_array_foreach (c->contacts, (GFunc) g_object_unref, NULL);
  g_ptr_array_free (c->contacts, TRUE);
  c->contacts = NULL;

  g_assert (c->handles != NULL);
  g_array_free (c->handles, TRUE);
  c->handles = NULL;

  g_assert (c->invalid != NULL);
  g_array_free (c->invalid, TRUE);
  c->invalid = NULL;

  if (c->request_ids != NULL)
    g_strfreev ((gchar **) g_ptr_array_free (c->request_ids, FALSE));

  c->request_ids = NULL;

  if (c->request_errors != NULL)
    g_hash_table_destroy (c->request_errors);

  c->request_errors = NULL;

  if (c->destroy != NULL)
    c->destroy (c->user_data);

  c->destroy = NULL;
  c->user_data = NULL;

  c->weak_object = NULL;

  g_slice_free (ContactsContext, c);
}


static void
contacts_context_fail (ContactsContext *c,
                       const GError *error)
{
  guint i;

  switch (c->signature)
    {
    case CB_BY_HANDLE:
      g_array_append_vals (c->invalid, c->handles->data, c->handles->len);

      c->callback.by_handle (c->connection, 0, NULL,
          c->invalid->len, (const TpHandle *) c->invalid->data,
          error, c->user_data, c->weak_object);
      return;
    case CB_BY_ID:
      /* -1 because NULL terminator is explicit */
      for (i = 0; i < c->request_ids->len - 1; i++)
        {
          const gchar *id = g_ptr_array_index (c->request_ids, i);

          if (!g_hash_table_lookup (c->request_errors, id))
            {
              g_hash_table_insert (c->request_errors,
                  g_strdup (id), g_error_copy (error));
            }
        }

      c->callback.by_id (c->connection, 0, NULL, NULL,
          c->request_errors, error, c->user_data, c->weak_object);
      return;
    case CB_UPGRADE:
      c->callback.upgrade (c->connection,
          c->contacts->len, (TpContact * const *) c->contacts->pdata,
          error, c->user_data, c->weak_object);
      return;
    default:
      g_assert_not_reached ();
    }
}


/**
 * TpConnectionContactsByHandleCb:
 * @connection: The connection
 * @n_contacts: The number of TpContact objects successfully created
 *  (one per valid handle), or 0 on unrecoverable errors
 * @contacts: An array of @n_contacts TpContact objects (this callback is
 *  not given a reference to any of these objects, and must call g_object_ref()
 *  on any that it will keep), or %NULL on unrecoverable errors
 * @n_failed: The number of invalid handles that were passed to
 *  tp_connection_get_contacts_by_handle() (or on unrecoverable errors,
 *  the total number of handles that were given)
 * @failed: An array of @n_failed handles that were passed to
 *  tp_connection_get_contacts_by_handle() but turned out to be invalid
 *  (or on unrecoverable errors, all the handles that were given)
 * @error: %NULL on success, or an unrecoverable error that caused everything
 *  to fail
 * @user_data: the @user_data that was passed to
 *  tp_connection_get_contacts_by_handle()
 * @weak_object: the @weak_object that was passed to
 *  tp_connection_get_contacts_by_handle()
 *
 * Signature of a callback used to receive the result of
 * tp_connection_get_contacts_by_handle().
 *
 * If an unrecoverable error occurs (for instance, if @connection
 * becomes disconnected) the whole operation fails, and no contacts or
 * invalid handles are returned.
 *
 * If some or even all of the @handles passed to
 * tp_connection_get_contacts_by_handle() were not valid, this is not
 * considered to be a failure. @error will be %NULL in this situation,
 * @contacts will contain contact objects for those handles that were
 * valid (possibly none of them), and @invalid will contain the handles
 * that were not valid.
 *
 * Since: 0.7.18
 */

/**
 * TpConnectionContactsByIdCb:
 * @connection: The connection
 * @n_contacts: The number of TpContact objects successfully created
 *  (one per valid ID), or 0 on unrecoverable errors
 * @contacts: An array of @n_contacts TpContact objects (this callback is
 *  not given a reference to any of these objects, and must call
 *  g_object_ref() on any that it will keep), or %NULL on unrecoverable errors
 * @requested_ids: An array of @n_contacts valid IDs (JIDs, SIP URIs etc.)
 *  that were passed to tp_connection_get_contacts_by_id(), in an order
 *  corresponding to @contacts, or %NULL on unrecoverable errors
 * @failed_id_errors: A hash table in which the keys are IDs
 *  and the values are errors (#GError)
 * @error: %NULL on success, or an unrecoverable error that caused everything
 *  to fail
 * @user_data: the @user_data that was passed to
 *  tp_connection_get_contacts_by_id()
 * @weak_object: the @weak_object that was passed to
 *  tp_connection_get_contacts_by_id()
 *
 * Signature of a callback used to receive the result of
 * tp_connection_get_contacts_by_id().
 *
 * @requested_ids contains the IDs that were converted to handles successfully.
 * The normalized form of requested_ids[i] is
 * tp_contact_get_identifier (contacts[i]).
 *
 * If some or even all of the @ids passed to
 * tp_connection_get_contacts_by_id() were not valid, this is not
 * considered to be a fatal error. @error will be %NULL in this situation,
 * @contacts will contain contact objects for those IDs that were
 * valid (it may be empty), and @failed_id_errors will map the IDs
 * that were not valid to a corresponding #GError (if the connection manager
 * complies with the Telepathy spec, it will have domain %TP_ERRORS and code
 * %TP_ERROR_INVALID_HANDLE).
 *
 * If an unrecoverable error occurs (for instance, if @connection
 * becomes disconnected) the whole operation fails, and no contacts
 * or requested IDs are returned. @failed_id_errors will contain all the IDs
 * that were requested, mapped to a corresponding #GError (either one
 * indicating that the ID was invalid, if that was determined before the
 * fatal error occurred, or a copy of @error).
 *
 * Since: 0.7.18
 */

/**
 * TpConnectionUpgradeContactsCb:
 * @connection: The connection
 * @n_contacts: The number of TpContact objects for which an upgrade was
 *  requested
 * @contacts: An array of @n_contacts TpContact objects (this callback is
 *  not given an extra reference to any of these objects, and must call
 *  g_object_ref() on any that it will keep)
 * @error: An unrecoverable error, or %NULL if the connection remains valid
 * @user_data: the @user_data that was passed to
 *  tp_connection_upgrade_contacts()
 * @weak_object: the @weak_object that was passed to
 *  tp_connection_upgrade_contacts()
 *
 * Signature of a callback used to receive the result of
 * tp_connection_upgrade_contacts().
 *
 * If an unrecoverable error occurs (for instance, if @connection becomes
 * disconnected) it is indicated by @error, but the contacts in @contacts
 * are still provided.
 *
 * Since: 0.7.18
 */


static void
contacts_context_continue (ContactsContext *c)
{
  if (g_queue_is_empty (&c->todo))
    {
      /* do some final sanity checking then hand over the contacts to the
       * library user */
      guint i;

      g_assert (c->contacts != NULL);
      g_assert (c->invalid != NULL);

      for (i = 0; i < c->contacts->len; i++)
        {
          TpContact *contact = TP_CONTACT (g_ptr_array_index (c->contacts, i));

          g_assert (contact->priv->identifier != NULL);
          g_assert (contact->priv->handle != 0);
        }

      switch (c->signature)
        {
        case CB_BY_HANDLE:
          c->callback.by_handle (c->connection,
              c->contacts->len, (TpContact * const *) c->contacts->pdata,
              c->invalid->len, (const TpHandle *) c->invalid->data,
              NULL, c->user_data, c->weak_object);
          break;
        case CB_BY_ID:
          c->callback.by_id (c->connection,
              c->contacts->len, (TpContact * const *) c->contacts->pdata,
              (const gchar * const *) c->request_ids->pdata,
              c->request_errors, NULL, c->user_data, c->weak_object);
          break;
        case CB_UPGRADE:
          c->callback.upgrade (c->connection,
              c->contacts->len, (TpContact * const *) c->contacts->pdata,
              NULL, c->user_data, c->weak_object);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      /* bah! */
      ContactsProc next = g_queue_pop_head (&c->todo);

      next (c);
    }
}


static void
contacts_held_one (TpConnection *connection,
                   TpHandleType handle_type,
                   guint n_handles,
                   const TpHandle *handles,
                   const GError *error,
                   gpointer user_data,
                   GObject *weak_object)
{
  ContactsContext *c = user_data;

  g_assert (handle_type == TP_HANDLE_TYPE_CONTACT);
  g_assert (c->next_index < c->handles->len);

  if (error == NULL)
    {
      /* I have a handle of my very own. Just what I always wanted! */
      TpContact *contact;

      g_assert (n_handles == 1);
      g_assert (handles[0] != 0);
      g_debug ("%u vs %u", g_array_index (c->handles, TpHandle, c->next_index),
          handles[0]);
      g_assert (g_array_index (c->handles, TpHandle, c->next_index)
          == handles[0]);

      contact = tp_contact_ensure (connection, handles[0]);
      g_ptr_array_add (c->contacts, contact);
      c->next_index++;
    }
  else if (error->domain == TP_ERRORS &&
      error->code == TP_ERROR_INVALID_HANDLE)
    {
      g_array_append_val (c->invalid,
          g_array_index (c->handles, TpHandle, c->next_index));
      /* ignore the bad handle - we just won't return a TpContact for it */
      g_array_remove_index_fast (c->handles, c->next_index);
      /* do not increment next_index - another handle has been moved into that
       * position */
    }
  else
    {
      /* the connection fell down a well or something */
      contacts_context_fail (c, error);
      return;
    }

  /* Either continue to hold handles, or proceed along the slow path. */
  contacts_context_continue (c);
}


static void
contacts_hold_one (ContactsContext *c)
{
  c->refcount++;
  tp_connection_hold_handles (c->connection, -1,
      TP_HANDLE_TYPE_CONTACT, 1,
      &g_array_index (c->handles, TpHandle, c->next_index),
      contacts_held_one, c, contacts_context_unref, c->weak_object);
}


static void
contacts_held_handles (TpConnection *connection,
                       TpHandleType handle_type,
                       guint n_handles,
                       const TpHandle *handles,
                       const GError *error,
                       gpointer user_data,
                       GObject *weak_object)
{
  ContactsContext *c = user_data;

  g_assert (handle_type == TP_HANDLE_TYPE_CONTACT);
  g_assert (weak_object == c->weak_object);

  if (error == NULL)
    {
      /* I now own all n handles. It's like Christmas morning! */
      guint i;

      g_assert (n_handles == c->handles->len);

      for (i = 0; i < c->handles->len; i++)
        {
          g_ptr_array_add (c->contacts,
              tp_contact_ensure (connection,
                g_array_index (c->handles, TpHandle, i)));
        }
    }
  else if (error->domain == TP_ERRORS &&
      error->code == TP_ERROR_INVALID_HANDLE)
    {
      /* One of the handles is bad. We don't know which one :-( so split
       * the batch into a chain of calls. */
      guint i;

      for (i = 0; i < c->handles->len; i++)
        {
          g_queue_push_head (&c->todo, contacts_hold_one);
        }

      g_assert (c->next_index == 0);
    }
  else
    {
      /* the connection fell down a well or something */
      contacts_context_fail (c, error);
      return;
    }

  /* Either hold the handles individually, or proceed along the slow path. */
  contacts_context_continue (c);
}


static void
contacts_inspected (TpConnection *connection,
                    const gchar **ids,
                    const GError *error,
                    gpointer user_data,
                    GObject *weak_object)
{
  ContactsContext *c = user_data;

  g_assert (weak_object == c->weak_object);
  g_assert (c->handles->len == c->contacts->len);

  if (error != NULL)
    {
      /* the connection fell down a well or something */
      contacts_context_fail (c, error);
      return;
    }
  else if (G_UNLIKELY (g_strv_length ((GStrv) ids) != c->handles->len))
    {
      GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Connection manager %s is broken: we inspected %u "
          "handles but InspectHandles returned %u strings",
          tp_proxy_get_bus_name (connection), c->handles->len,
          g_strv_length ((GStrv) ids));

      g_warning ("%s", e->message);
      contacts_context_fail (c, e);
      g_error_free (e);
      return;
    }
  else
    {
      guint i;

      for (i = 0; i < c->contacts->len; i++)
        {
          TpContact *contact = g_ptr_array_index (c->contacts, i);

          g_assert (ids[i] != NULL);

          if (contact->priv->identifier == NULL)
            {
              contact->priv->identifier = g_strdup (ids[i]);
            }
          else if (tp_strdiff (contact->priv->identifier, ids[i]))
            {
              GError *e = g_error_new (TP_DBUS_ERRORS,
                  TP_DBUS_ERROR_INCONSISTENT,
                  "Connection manager %s is broken: contact handle %u "
                  "identifier changed from %s to %s",
                  tp_proxy_get_bus_name (connection), contact->priv->handle,
                  contact->priv->identifier, ids[i]);

              g_warning ("%s", e->message);
              contacts_context_fail (c, e);
              g_error_free (e);
              return;
            }
        }
    }

  contacts_context_continue (c);
}


static void
contacts_inspect (ContactsContext *c)
{
  guint i;

  g_assert (c->handles->len == c->contacts->len);

  for (i = 0; i < c->contacts->len; i++)
    {
      TpContact *contact = g_ptr_array_index (c->contacts, i);

      if (contact->priv->identifier == NULL)
        {
          c->refcount++;
          tp_cli_connection_call_inspect_handles (c->connection, -1,
              TP_HANDLE_TYPE_CONTACT, c->handles, contacts_inspected,
              c, contacts_context_unref, c->weak_object);
          return;
        }
    }

  /* else there's no need to inspect the contacts' handles, because we already
   * know all their identifiers */
  contacts_context_continue (c);
}


static void
contacts_requested_aliases (TpConnection *connection,
                            const gchar **aliases,
                            const GError *error,
                            gpointer user_data,
                            GObject *weak_object)
{
  ContactsContext *c = user_data;

  g_assert (c->handles->len == c->contacts->len);

  if (error == NULL)
    {
      guint i;

      if (G_UNLIKELY (g_strv_length ((GStrv) aliases) != c->contacts->len))
        {
          g_warning ("Connection manager %s is broken: we requested %u "
              "handles' aliases but got %u strings back",
              tp_proxy_get_bus_name (connection), c->contacts->len,
              g_strv_length ((GStrv) aliases));

          /* give up on the possibility of getting aliases, and just
           * move on */
          contacts_context_continue (c);
          return;
        }

      for (i = 0; i < c->contacts->len; i++)
        {
          TpContact *contact = g_ptr_array_index (c->contacts, i);
          const gchar *alias = aliases[i];

          contact->priv->has_features |= CONTACT_FEATURE_FLAG_ALIAS;
          g_free (contact->priv->alias);
          contact->priv->alias = g_strdup (alias);
          g_object_notify ((GObject *) contact, "alias");
        }
    }
  else
    {
      /* never mind, we can live without aliases */
      DEBUG ("GetAliases failed with %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);
    }

  contacts_context_continue (c);
}


static void
contacts_got_aliases (TpConnection *connection,
                      GHashTable *handle_to_alias,
                      const GError *error,
                      gpointer user_data,
                      GObject *weak_object)
{
  ContactsContext *c = user_data;

  if (error == NULL)
    {
      guint i;

      for (i = 0; i < c->contacts->len; i++)
        {
          TpContact *contact = g_ptr_array_index (c->contacts, i);
          const gchar *alias = g_hash_table_lookup (handle_to_alias,
              GUINT_TO_POINTER (contact->priv->handle));

          contact->priv->has_features |= CONTACT_FEATURE_FLAG_ALIAS;
          g_free (contact->priv->alias);
          contact->priv->alias = NULL;

          if (alias != NULL)
            {
              contact->priv->alias = g_strdup (alias);
            }
          else
            {
              g_warning ("No alias returned for %u, will use ID instead",
                  contact->priv->handle);
            }

          g_object_notify ((GObject *) contact, "alias");
        }
    }
  else if ((error->domain == TP_ERRORS &&
      error->code == TP_ERROR_NOT_IMPLEMENTED) ||
      (error->domain == DBUS_GERROR &&
       error->code == DBUS_GERROR_UNKNOWN_METHOD))
    {
      /* GetAliases not implemented, fall back to (slow?) RequestAliases */
      c->refcount++;
      tp_cli_connection_interface_aliasing_call_request_aliases (connection,
          -1, c->handles, contacts_requested_aliases,
          c, contacts_context_unref, weak_object);
      return;
    }
  else
    {
      /* never mind, we can live without aliases */
      DEBUG ("GetAliases failed with %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);
    }

  contacts_context_continue (c);
}


static void
contacts_aliases_changed (TpConnection *connection,
                          const GPtrArray *alias_structs,
                          gpointer user_data G_GNUC_UNUSED,
                          GObject *weak_object G_GNUC_UNUSED)
{
  guint i;

  for (i = 0; i < alias_structs->len; i++)
    {
      GValueArray *pair = g_ptr_array_index (alias_structs, i);
      TpHandle handle = g_value_get_uint (pair->values + 0);
      const gchar *alias = g_value_get_string (pair->values + 1);
      TpContact *contact = _tp_connection_lookup_contact (connection, handle);

      if (contact != NULL)
        {
          contact->priv->has_features |= CONTACT_FEATURE_FLAG_ALIAS;
          DEBUG ("Contact \"%s\" alias changed from \"%s\" to \"%s\"",
              contact->priv->identifier, contact->priv->alias, alias);
          g_free (contact->priv->alias);
          contact->priv->alias = g_strdup (alias);
          g_object_notify ((GObject *) contact, "alias");
        }
    }
}


static void
contacts_bind_to_aliases_changed (TpConnection *connection)
{
  if (!connection->priv->tracking_aliases_changed)
    {
      connection->priv->tracking_aliases_changed = TRUE;

      tp_cli_connection_interface_aliasing_connect_to_aliases_changed (
          connection, contacts_aliases_changed, NULL, NULL, NULL, NULL);
    }
}


static void
contacts_get_aliases (ContactsContext *c)
{
  guint i;

  g_assert (c->handles->len == c->contacts->len);

  contacts_bind_to_aliases_changed (c->connection);

  for (i = 0; i < c->contacts->len; i++)
    {
      TpContact *contact = g_ptr_array_index (c->contacts, i);

      if ((contact->priv->has_features & CONTACT_FEATURE_FLAG_ALIAS) == 0)
        {
          c->refcount++;
          tp_cli_connection_interface_aliasing_call_get_aliases (c->connection,
              -1, c->handles, contacts_got_aliases, c, contacts_context_unref,
              c->weak_object);
          return;
        }
    }

  /* else there's no need to get the contacts' aliases, because we already
   * know them all */
  contacts_context_continue (c);
}


static void
contact_maybe_set_simple_presence (TpContact *contact,
                                   GValueArray *presence)
{
  if (contact == NULL || presence == NULL)
    return;

  contact->priv->has_features |= CONTACT_FEATURE_FLAG_PRESENCE;
  contact->priv->presence_type = g_value_get_uint (presence->values + 0);
  g_free (contact->priv->presence_status);
  contact->priv->presence_status = g_value_dup_string (
      presence->values + 1);
  g_free (contact->priv->presence_message);
  contact->priv->presence_message = g_value_dup_string (
      presence->values + 2);

  g_object_notify ((GObject *) contact, "presence-type");
  g_object_notify ((GObject *) contact, "presence-status");
  g_object_notify ((GObject *) contact, "presence-message");
}


static void
contacts_presences_changed (TpConnection *connection,
                            GHashTable *presences,
                            gpointer user_data G_GNUC_UNUSED,
                            GObject *weak_object G_GNUC_UNUSED)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, presences);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      TpContact *contact = _tp_connection_lookup_contact (connection,
          GPOINTER_TO_UINT (key));

      contact_maybe_set_simple_presence (contact, value);
    }
}


static void
contacts_got_simple_presence (TpConnection *connection,
                              GHashTable *presences,
                              const GError *error,
                              gpointer user_data,
                              GObject *weak_object)
{
  ContactsContext *c = user_data;

  if (error == NULL)
    {
      contacts_presences_changed (connection, presences, NULL, NULL);
    }
  else
    {
      /* never mind, we can live without presences */
      DEBUG ("GetPresences failed with %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);
    }

  contacts_context_continue (c);
}


static void
contacts_bind_to_presences_changed (TpConnection *connection)
{
  if (!connection->priv->tracking_presences_changed)
    {
      connection->priv->tracking_presences_changed = TRUE;

      tp_cli_connection_interface_simple_presence_connect_to_presences_changed
        (connection, contacts_presences_changed, NULL, NULL, NULL, NULL);
    }
}

static void
contacts_get_simple_presence (ContactsContext *c)
{
  guint i;

  g_assert (c->handles->len == c->contacts->len);

  contacts_bind_to_presences_changed (c->connection);

  for (i = 0; i < c->contacts->len; i++)
    {
      TpContact *contact = g_ptr_array_index (c->contacts, i);

      if ((contact->priv->has_features & CONTACT_FEATURE_FLAG_PRESENCE) == 0)
        {
          c->refcount++;
          tp_cli_connection_interface_simple_presence_call_get_presences (
              c->connection, -1,
              c->handles, contacts_got_simple_presence,
              c, contacts_context_unref, c->weak_object);
          return;
        }
    }

  contacts_context_continue (c);
}


static void
contacts_avatar_updated (TpConnection *connection,
                         TpHandle handle,
                         const gchar *new_token,
                         gpointer user_data G_GNUC_UNUSED,
                         GObject *weak_object G_GNUC_UNUSED)
{
  TpContact *contact = _tp_connection_lookup_contact (connection, handle);

  DEBUG ("contact#%u token is %s", handle, new_token);

  if (contact == NULL)
    return;

  contact->priv->has_features |= CONTACT_FEATURE_FLAG_AVATAR_TOKEN;
  g_free (contact->priv->avatar_token);
  contact->priv->avatar_token = g_strdup (new_token);
  g_object_notify ((GObject *) contact, "avatar-token");
}


static void
contacts_got_known_avatar_tokens (TpConnection *connection,
                                  GHashTable *handle_to_token,
                                  const GError *error,
                                  gpointer user_data,
                                  GObject *weak_object)
{
  ContactsContext *c = user_data;
  GHashTableIter iter;
  gpointer key, value;

  if (error == NULL)
    {
      g_hash_table_iter_init (&iter, handle_to_token);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          contacts_avatar_updated (connection, GPOINTER_TO_UINT (key), value,
              NULL, NULL);
        }

    }
  /* FIXME: perhaps we could fall back to GetAvatarTokens (which should have
   * been called RequestAvatarTokens, because it blocks on network traffic)
   * if GetKnownAvatarTokens doesn't work? */
  else
    {
      /* never mind, we can live without avatar tokens */
      DEBUG ("GetKnownAvatarTokens failed with %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);
    }

  contacts_context_continue (c);
}


static void
contacts_bind_to_avatar_updated (TpConnection *connection)
{
  if (!connection->priv->tracking_avatar_updated)
    {
      connection->priv->tracking_avatar_updated = TRUE;

      tp_cli_connection_interface_avatars_connect_to_avatar_updated
        (connection, contacts_avatar_updated, NULL, NULL, NULL, NULL);
    }
}


static void
contacts_get_avatar_tokens (ContactsContext *c)
{
  guint i;

  g_assert (c->handles->len == c->contacts->len);

  contacts_bind_to_avatar_updated (c->connection);

  for (i = 0; i < c->contacts->len; i++)
    {
      TpContact *contact = g_ptr_array_index (c->contacts, i);

      if ((contact->priv->has_features & CONTACT_FEATURE_FLAG_AVATAR_TOKEN)
          == 0)
        {
          c->refcount++;
          tp_cli_connection_interface_avatars_call_get_known_avatar_tokens (
              c->connection, -1,
              c->handles, contacts_got_known_avatar_tokens,
              c, contacts_context_unref, c->weak_object);
          return;
        }
    }

  contacts_context_continue (c);
}


static void
contacts_context_queue_features (ContactsContext *context,
                                 ContactFeatureFlags feature_flags)
{
  if ((feature_flags & CONTACT_FEATURE_FLAG_ALIAS) != 0 &&
      tp_proxy_has_interface_by_id (context->connection,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_ALIASING))
    {
      g_queue_push_tail (&context->todo, contacts_get_aliases);
    }

  if ((feature_flags & CONTACT_FEATURE_FLAG_PRESENCE) != 0)
    {
      if (tp_proxy_has_interface_by_id (context->connection,
            TP_IFACE_QUARK_CONNECTION_INTERFACE_SIMPLE_PRESENCE))
        {
          g_queue_push_tail (&context->todo, contacts_get_simple_presence);
        }
#if 0
      /* FIXME: Before doing this for the first time, we'd need to download
       * from the CM the definition of what each status actually *means* */
      else if (tp_proxy_has_interface_by_id (context->connection,
            TP_IFACE_QUARK_CONNECTION_INTERFACE_PRESENCE))
        {
          g_queue_push_tail (&context->todo, contacts_get_complex_presence);
        }
#endif
    }

  if ((feature_flags & CONTACT_FEATURE_FLAG_AVATAR_TOKEN) != 0 &&
      tp_proxy_has_interface_by_id (context->connection,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS))
    {
      g_queue_push_tail (&context->todo, contacts_get_avatar_tokens);
    }
}


static void
contacts_got_attributes (TpConnection *connection,
                         GHashTable *attributes,
                         const GError *error,
                         gpointer user_data,
                         GObject *weak_object)
{
  ContactsContext *c = user_data;
  guint i;

  if (error != NULL)
    {
      contacts_context_fail (c, error);
      return;
    }

  i = 0;

  if (c->signature == CB_BY_HANDLE)
    {
      g_assert (c->contacts->len == 0);

      while (i < c->handles->len)
        {
          TpHandle handle = g_array_index (c->handles, guint, i);
          GHashTable *asv = g_hash_table_lookup (attributes,
              GUINT_TO_POINTER (handle));

          if (asv == NULL)
            {
              /* not in the hash table => not valid */
              g_array_append_val (c->invalid, handle);
              g_array_remove_index_fast (c->handles, i);
            }
          else
            {
              TpContact *contact = NULL;
              guint j;

              /* we might already have consumed the only reference we have to
               * the handle - if we have, we must recycle the same object
               * rather than calling tp_contact_ensure again */
              for (j = 0; j < i; j++)
                {
                  if (handle == g_array_index (c->handles, guint, j))
                    {
                      contact = g_object_ref (g_ptr_array_index (c->contacts,
                            j));
                    }
                }

              if (contact == NULL)
                contact = tp_contact_ensure (connection, handle);

              g_ptr_array_add (c->contacts, contact);

              /* save the contact and move on to the next handle */
              i++;
            }
        }
    }

  g_assert (c->contacts->len == c->handles->len);

  for (i = 0; i < c->handles->len; i++)
    {
      TpContact *contact = g_ptr_array_index (c->contacts, i);
      const gchar *s;
      gpointer boxed;
      GHashTable *asv = g_hash_table_lookup (attributes,
          GUINT_TO_POINTER (contact->priv->handle));

      if (asv == NULL)
        {
          GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
              "We hold a ref to handle #%u but it appears to be invalid",
              contact->priv->handle);

          contacts_context_fail (c, e);
          g_error_free (e);
          return;
        }

      /* set up the contact with its attributes */

      s = tp_asv_get_string (asv, TP_IFACE_CONNECTION "/contact-id");

      if (s == NULL)
        {
          GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
              "Connection manager %s is broken: contact #%u in the "
              "GetContactAttributes result has no contact-id",
              tp_proxy_get_bus_name (connection), contact->priv->handle);

          contacts_context_fail (c, e);
          g_error_free (e);
          return;
        }

      if (contact->priv->identifier == NULL)
        {
          contact->priv->identifier = g_strdup (s);
        }
      else if (tp_strdiff (contact->priv->identifier, s))
        {
          GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
              "Connection manager %s is broken: contact #%u identifier "
              "changed from %s to %s",
              tp_proxy_get_bus_name (connection), contact->priv->handle,
              contact->priv->identifier, s);

          contacts_context_fail (c, e);
          g_error_free (e);
          return;
        }

      s = tp_asv_get_string (asv,
          TP_IFACE_CONNECTION_INTERFACE_ALIASING "/alias");

      if (s != NULL)
        {
          contact->priv->has_features |= CONTACT_FEATURE_FLAG_ALIAS;
          g_free (contact->priv->alias);
          contact->priv->alias = g_strdup (s);
          g_object_notify ((GObject *) contact, "alias");
        }

      s = tp_asv_get_string (asv,
          TP_IFACE_CONNECTION_INTERFACE_AVATARS "/token");

      if (s != NULL)
        contacts_avatar_updated (connection, contact->priv->handle, s,
            NULL, NULL);

      boxed = tp_asv_get_boxed (asv,
          TP_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE "/presence",
          TP_STRUCT_TYPE_SIMPLE_PRESENCE);
      contact_maybe_set_simple_presence (contact, boxed);

      /* FIXME: TP_IFACE_CONNECTION_INTERFACE_CAPABILITIES "/caps" */
    }

  contacts_context_continue (c);
}


static void
contacts_get_attributes (ContactsContext *context)
{
  GArray *contact_attribute_interfaces =
      context->connection->priv->contact_attribute_interfaces;
  GPtrArray *array;
  const gchar **supported_interfaces;
  guint i;

  /* tp_connection_get_contact_attributes insists that you have at least one
   * handle; skip it if we don't (can only happen if we started from IDs) */
  if (context->handles->len == 0)
    {
      contacts_context_continue (context);
      return;
    }

  g_assert (tp_proxy_has_interface_by_id (context->connection,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS));
  g_assert (contact_attribute_interfaces != NULL);

  array = g_ptr_array_sized_new (contact_attribute_interfaces->len);

  for (i = 0; i < contact_attribute_interfaces->len; i++)
    {
      GQuark q = g_array_index (contact_attribute_interfaces, GQuark, i);

      if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_ALIASING)
        {
          if ((context->wanted & CONTACT_FEATURE_FLAG_ALIAS) != 0)
            {
              g_ptr_array_add (array,
                  TP_IFACE_CONNECTION_INTERFACE_ALIASING);
              contacts_bind_to_aliases_changed (context->connection);
            }
        }
      else if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS)
        {
          if ((context->wanted & CONTACT_FEATURE_FLAG_AVATAR_TOKEN) != 0)
            {
              g_ptr_array_add (array,
                  TP_IFACE_CONNECTION_INTERFACE_AVATARS);
              contacts_bind_to_avatar_updated (context->connection);
            }
        }
      else if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_SIMPLE_PRESENCE)
        {
          if ((context->wanted & CONTACT_FEATURE_FLAG_PRESENCE) != 0)
            {
              g_ptr_array_add (array,
                  TP_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE);
              contacts_bind_to_presences_changed (context->connection);
            }
        }
    }

  g_ptr_array_add (array, NULL);
  supported_interfaces = (const gchar **) g_ptr_array_free (array, FALSE);

  /* we want to hold the handles if and only if the call is by_handle -
   * for the other modes, we already have handles */
  context->refcount++;
  tp_connection_get_contact_attributes (context->connection, -1,
      context->handles->len, (const TpHandle *) context->handles->data,
      supported_interfaces, (context->signature == CB_BY_HANDLE),
      contacts_got_attributes,
      context, contacts_context_unref, context->weak_object);
  g_free (supported_interfaces);
}


/**
 * tp_connection_get_contacts_by_handle:
 * @self: A connection, which must be ready (#TpConnection:connection-ready
 *  must be %TRUE)
 * @n_handles: The number of handles in @handles (must be at least 1)
 * @handles: An array of handles of type %TP_HANDLE_TYPE_CONTACT representing
 *  the desired contacts
 * @n_features: The number of features in @features (may be 0)
 * @features: An array of features that must be ready for use (if supported)
 *  before the callback is called (may be %NULL if @n_features is 0)
 * @callback: A user callback to call when the contacts are ready
 * @user_data: Data to pass to the callback
 * @destroy: Called to destroy @user_data either after @callback has been
 *  called, or if the operation is cancelled
 * @weak_object: An object to pass to the callback, which will be weakly
 *  referenced; if this object is destroyed, the operation will be cancelled
 *
 * Create a number of #TpContact objects and make asynchronous method calls
 * to hold their handles and ensure that all the features specified in
 * @features are ready for use (if they are supported at all).
 *
 * It is not an error to put features in @features even if the connection
 * manager doesn't support them - users of this method should have a static
 * list of features they would like to use if possible, and use it for all
 * connection managers.
 *
 * Since: 0.7.18
 */
void
tp_connection_get_contacts_by_handle (TpConnection *self,
                                      guint n_handles,
                                      const TpHandle *handles,
                                      guint n_features,
                                      const TpContactFeature *features,
                                      TpConnectionContactsByHandleCb callback,
                                      gpointer user_data,
                                      GDestroyNotify destroy,
                                      GObject *weak_object)
{
  ContactFeatureFlags feature_flags = 0;
  ContactsContext *context;
  guint i;

  g_return_if_fail (tp_connection_is_ready (self));
  g_return_if_fail (tp_proxy_get_invalidated (self) == NULL);
  g_return_if_fail (n_handles >= 1);
  g_return_if_fail (handles != NULL);
  g_return_if_fail (n_features == 0 || features != NULL);
  g_return_if_fail (callback != NULL);

  for (i = 0; i < n_features; i++)
    {
      g_return_if_fail (features[i] < NUM_TP_CONTACT_FEATURES);
      feature_flags |= (1 << features[i]);
    }

  context = contacts_context_new (self, n_handles, feature_flags,
      CB_BY_HANDLE, user_data, destroy, weak_object);
  context->callback.by_handle = callback;

  g_array_append_vals (context->handles, handles, n_handles);

  if (tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS))
    {
      /* we support the Contacts interface, so we can hold the handles and
       * simultaneously inspect them. After that, we'll fill in any
       * features that are necessary (this becomes a no-op if Contacts
       * gave us everything). */
      contacts_get_attributes (context);
      contacts_context_queue_features (context, feature_flags);
      /* we have one excess ref to the context because we create it,
       * and then contacts_get_attributes refs it */
      contacts_context_unref (context);
      return;
    }

  /* if we haven't already returned, we're on the slow path */

  /* Before we return anything we'll want to inspect the handles */
  g_queue_push_head (&context->todo, contacts_inspect);

  /* After that we'll get the features */
  contacts_context_queue_features (context, feature_flags);

  /* but first, we need to hold onto them */
  tp_connection_hold_handles (self, -1,
      TP_HANDLE_TYPE_CONTACT, n_handles, handles,
      contacts_held_handles, context, contacts_context_unref, weak_object);
}


static gboolean
contacts_context_idle_continue (gpointer data)
{
  contacts_context_continue (data);
  return FALSE;
}


/**
 * tp_connection_upgrade_contacts:
 * @self: A connection, which must be ready (#TpConnection:connection-ready
 *  must be %TRUE)
 * @n_contacts: The number of contacts in @contacts (must be at least 1)
 * @contacts: An array of #TpContact objects associated with @self
 * @n_features: The number of features in @features (must be at least 1)
 * @features: An array of features that must be ready for use (if supported)
 *  before the callback is called
 * @callback: A user callback to call when the contacts are ready
 * @user_data: Data to pass to the callback
 * @destroy: Called to destroy @user_data either after @callback has been
 *  called, or if the operation is cancelled
 * @weak_object: An object to pass to the callback, which will be weakly
 *  referenced; if this object is destroyed, the operation will be cancelled
 *
 * Given several #TpContact objects, make asynchronous method calls
 * ensure that all the features specified in @features are ready for use
 * (if they are supported at all).
 *
 * It is not an error to put features in @features even if the connection
 * manager doesn't support them - users of this method should have a static
 * list of features they would like to use if possible, and use it for all
 * connection managers.
 *
 * Since: 0.7.18
 */
void
tp_connection_upgrade_contacts (TpConnection *self,
                                guint n_contacts,
                                TpContact * const *contacts,
                                guint n_features,
                                const TpContactFeature *features,
                                TpConnectionUpgradeContactsCb callback,
                                gpointer user_data,
                                GDestroyNotify destroy,
                                GObject *weak_object)
{
  ContactFeatureFlags feature_flags = 0;
  ContactsContext *context;
  guint i;

  g_return_if_fail (tp_connection_is_ready (self));
  g_return_if_fail (tp_proxy_get_invalidated (self) == NULL);
  g_return_if_fail (n_contacts >= 1);
  g_return_if_fail (contacts != NULL);
  g_return_if_fail (n_features == 0 || features != NULL);
  g_return_if_fail (callback != NULL);

  for (i = 0; i < n_contacts; i++)
    {
      g_return_if_fail (contacts[i]->priv->connection == self);
    }

  for (i = 0; i < n_features; i++)
    {
      g_return_if_fail (features[i] < NUM_TP_CONTACT_FEATURES);
      feature_flags |= (1 << features[i]);
    }

  context = contacts_context_new (self, n_contacts, feature_flags,
      CB_UPGRADE, user_data, destroy, weak_object);
  context->callback.upgrade = callback;

  for (i = 0; i < n_contacts; i++)
    {
      g_ptr_array_add (context->contacts, g_object_ref (contacts[i]));
      g_array_append_val (context->handles, contacts[i]->priv->handle);
    }

  g_assert (context->handles->len == n_contacts);

  if (tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS))
    {
      g_queue_push_head (&context->todo, contacts_get_attributes);
    }

  contacts_context_queue_features (context, feature_flags);

  /* use an idle to make sure the callback is called after we return,
   * even if all the contacts actually have all the features, just to be
   * consistent */
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
      contacts_context_idle_continue, context, contacts_context_unref);
}


static void
contacts_requested_one_handle (TpConnection *connection,
                               TpHandleType handle_type,
                               guint n_handles,
                               const TpHandle *handles,
                               const gchar * const *ids,
                               const GError *error,
                               gpointer user_data,
                               GObject *weak_object)
{
  ContactsContext *c = user_data;

  if (error == NULL)
    {
      TpContact *contact;

      g_assert (handle_type == TP_HANDLE_TYPE_CONTACT);
      /* -1 because NULL terminator is explicit */
      g_assert (c->next_index < c->request_ids->len - 1);

      g_assert (n_handles == 1);
      g_assert (handles[0] != 0);

      contact = tp_contact_ensure (connection, handles[0]);
      g_array_append_val (c->handles, handles[0]);
      g_ptr_array_add (c->contacts, contact);
      c->next_index++;
    }
  else if (error->domain == TP_ERRORS &&
      (error->code == TP_ERROR_INVALID_HANDLE ||
       error->code == TP_ERROR_NOT_AVAILABLE ||
       error->code == TP_ERROR_INVALID_ARGUMENT))
    {
      g_hash_table_insert (c->request_errors,
          g_ptr_array_index (c->request_ids, c->next_index),
          g_error_copy (error));
      /* shift the rest of the IDs down one and do not increment next_index */
      g_ptr_array_remove_index (c->request_ids, c->next_index);
    }
  else
    {
      contacts_context_fail (c, error);
      return;
    }

  contacts_context_continue (c);
}


static void
contacts_request_one_handle (ContactsContext *c)
{
  const gchar *ids[] = { NULL, NULL };

  ids[0] = g_ptr_array_index (c->request_ids, c->next_index);
  g_assert (ids[0] != NULL);

  c->refcount++;
  tp_connection_request_handles (c->connection, -1,
      TP_HANDLE_TYPE_CONTACT, ids,
      contacts_requested_one_handle, c, contacts_context_unref,
      c->weak_object);
}


static void
contacts_requested_handles (TpConnection *connection,
                            TpHandleType handle_type,
                            guint n_handles,
                            const TpHandle *handles,
                            const gchar * const *ids,
                            const GError *error,
                            gpointer user_data,
                            GObject *weak_object)
{
  ContactsContext *c = user_data;

  g_assert (handle_type == TP_HANDLE_TYPE_CONTACT);
  g_assert (weak_object == c->weak_object);

  if (error == NULL)
    {
      guint i;

      for (i = 0; i < n_handles; i++)
        {
          TpContact *contact = tp_contact_ensure (connection, handles[i]);

          g_array_append_val (c->handles, handles[i]);
          g_ptr_array_add (c->contacts, contact);
        }
    }
  else if (error->domain == TP_ERRORS &&
      (error->code == TP_ERROR_INVALID_HANDLE ||
       error->code == TP_ERROR_NOT_AVAILABLE ||
       error->code == TP_ERROR_INVALID_ARGUMENT))
    {
      /* One of the strings is bad. We don't know which, so split them. */
      guint i;

      DEBUG ("A handle was bad, trying to recover: %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);

      /* -1 because NULL terminator is explicit */
      for (i = 0; i < c->request_ids->len - 1; i++)
        {
          g_queue_push_head (&c->todo, contacts_request_one_handle);
        }

      g_assert (c->next_index == 0);
    }
  else
    {
      DEBUG ("RequestHandles failed: %s %u: %s",
          g_quark_to_string (error->domain), error->code, error->message);
      contacts_context_fail (c, error);
      return;
    }

  contacts_context_continue (c);
}


/**
 * tp_connection_get_contacts_by_id:
 * @self: A connection, which must be ready (#TpConnection:connection-ready
 *  must be %TRUE)
 * @n_ids: The number of IDs in @ids (must be at least 1)
 * @ids: An array of strings representing the desired contacts by their
 *  identifiers in the IM protocol (XMPP JIDs, SIP URIs, MSN Passports,
 *  AOL screen-names etc.)
 * @n_features: The number of features in @features (may be 0)
 * @features: An array of features that must be ready for use (if supported)
 *  before the callback is called (may be %NULL if @n_features is 0)
 * @callback: A user callback to call when the contacts are ready
 * @user_data: Data to pass to the callback
 * @destroy: Called to destroy @user_data either after @callback has been
 *  called, or if the operation is cancelled
 * @weak_object: An object to pass to the callback, which will be weakly
 *  referenced; if this object is destroyed, the operation will be cancelled
 *
 * Create a number of #TpContact objects and make asynchronous method calls
 * to obtain their handles and ensure that all the features specified in
 * @features are ready for use (if they are supported at all).
 *
 * It is not an error to put features in @features even if the connection
 * manager doesn't support them - users of this method should have a static
 * list of features they would like to use if possible, and use it for all
 * connection managers.
 *
 * Since: 0.7.18
 */
void
tp_connection_get_contacts_by_id (TpConnection *self,
                                  guint n_ids,
                                  const gchar * const *ids,
                                  guint n_features,
                                  const TpContactFeature *features,
                                  TpConnectionContactsByIdCb callback,
                                  gpointer user_data,
                                  GDestroyNotify destroy,
                                  GObject *weak_object)
{
  ContactFeatureFlags feature_flags = 0;
  ContactsContext *context;
  guint i;

  g_return_if_fail (tp_connection_is_ready (self));
  g_return_if_fail (tp_proxy_get_invalidated (self) == NULL);
  g_return_if_fail (n_ids >= 1);
  g_return_if_fail (ids != NULL);
  g_return_if_fail (ids[0] != NULL);
  g_return_if_fail (n_features == 0 || features != NULL);
  g_return_if_fail (callback != NULL);

  for (i = 0; i < n_features; i++)
    {
      g_return_if_fail (features[i] < NUM_TP_CONTACT_FEATURES);
      feature_flags |= (1 << features[i]);
    }

  context = contacts_context_new (self, n_ids, feature_flags,
      CB_BY_ID, user_data, destroy, weak_object);
  context->callback.by_id = callback;
  context->request_errors = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_error_free);

  context->request_ids = g_ptr_array_sized_new (n_ids);

  for (i = 0; i < n_ids; i++)
    {
      g_return_if_fail (ids[i] != NULL);
      g_ptr_array_add (context->request_ids, g_strdup (ids[i]));
    }

  g_ptr_array_add (context->request_ids, NULL);

  /* set up the queue of feature introspection */

  if (tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS))
    {
      g_queue_push_head (&context->todo, contacts_get_attributes);
    }
  else
    {
      g_queue_push_head (&context->todo, contacts_inspect);
    }

  contacts_context_queue_features (context, feature_flags);

  /* but first, we need to get the handles in the first place */
  tp_connection_request_handles (self, -1,
      TP_HANDLE_TYPE_CONTACT,
      (const gchar * const *) context->request_ids->pdata,
      contacts_requested_handles, context, contacts_context_unref,
      weak_object);
}
