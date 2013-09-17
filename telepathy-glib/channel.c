/*
 * channel.c - proxy for a Telepathy channel
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#include "telepathy-glib/channel-internal.h"

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/util-internal.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/proxy-internal.h"
#include "telepathy-glib/client-factory-internal.h"
#include "telepathy-glib/variant-util-internal.h"

/**
 * SECTION:channel
 * @title: TpChannel
 * @short_description: proxy object for a Telepathy channel
 * @see_also: #TpConnection, channel-group, channel-text, channel-media
 *
 * #TpChannel objects provide convenient access to Telepathy channels.
 *
 * Compared with a simple proxy for method calls, they add the following
 * features:
 *
 * * calling GetChannelType(), GetInterfaces(), GetHandles() automatically
 *
 *
 * Since: 0.7.1
 */


/**
 * TpChannelClass:
 * @parent_class: parent class
 *
 * The class of a #TpChannel. In addition to @parent_class there are four
 * pointers reserved for possible future use.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */


/**
 * TpChannel:
 *
 * A proxy object for a Telepathy channel.
 * A proxy object for a Telepathy channel. There are no interesting
 * public struct fields.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */


enum
{
  PROP_CONNECTION = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_IDENTIFIER,
  PROP_CHANNEL_PROPERTIES,
  PROP_GROUP_FLAGS,
  PROP_REQUESTED,
  PROP_PASSWORD_NEEDED,
  PROP_TARGET_CONTACT,
  PROP_INITIATOR_CONTACT,
  PROP_GROUP_SELF_CONTACT,
  N_PROPS
};

enum {
  SIGNAL_GROUP_FLAGS_CHANGED,
  SIGNAL_GROUP_MEMBERS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };


G_DEFINE_TYPE_WITH_CODE (TpChannel,
    tp_channel,
    TP_TYPE_PROXY,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL))

/**
 * TP_CHANNEL_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core" feature
 * on a #TpChannel.
 *
 * When this feature is prepared, the basic Channel properties of the
 * Channel have been retrieved and are available for use.
 *
 * Specifically, this implies that:
 *
 * - #TpChannelIface:channel-type is set
 * - #TpChannelIface:handle-type and #TpChannelIface:handle are set
 * - any extra interfaces will have been set up in TpProxy (i.e.
 *   #TpProxy:interfaces contains at least all extra Channel interfaces)
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.11.3
 */

GQuark
tp_channel_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-channel-feature-core");
}

/**
 * TP_CHANNEL_FEATURE_GROUP:
 *
 * Expands to a call to a function that returns a quark representing the
 * group features of a TpChannel.
 *
 * When this feature is prepared, the Group properties of the
 * Channel have been retrieved and are available for use, and
 * change-notification has been set up for those that can change:
 *
 * - the initial value of the #TpChannel:group-self-contact property will
 *   have been fetched and change notification will have been set up
 * - the initial value of the #TpChannel:group-flags property will
 *   have been fetched and change notification will have been set up
 *
 * All #TpContact objects are guaranteed to have all of the features previously
 * passed to tp_client_factory_add_contact_features() prepared.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.99.1
 */

GQuark
tp_channel_get_feature_quark_group (void)
{
  return g_quark_from_static_string ("tp-channel-feature-group");
}


/* Convenient property accessors for C (these duplicate the properties) */


/**
 * tp_channel_get_channel_type:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type,
 * if it has been discovered.
 *
 * This is the same as the #TpChannelIface:channel-type property; it isn't
 * guaranteed to be non-%NULL until the %TP_CHANNEL_FEATURE_CORE feature has
 * been prepared.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or %NULL, if the channel is not yet ready.
 * Since: 0.7.12
 */
const gchar *
tp_channel_get_channel_type (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return g_quark_to_string (self->priv->channel_type);
}


/**
 * tp_channel_get_channel_type_id:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type, as a GQuark,
 * if it has been discovered.
 *
 * This is the same as the #TpChannelIface:channel-type property, except that it
 * is a GQuark rather than a string. It isn't guaranteed to be nonzero until
 * the %TP_CHANNEL_FEATURE_CORE property is ready.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or 0, if the channel is not yet ready.
 * Since: 0.7.12
 */
GQuark
tp_channel_get_channel_type_id (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  return self->priv->channel_type;
}


/**
 * tp_channel_get_handle:
 * @self: a channel
 * @handle_type: (out): if not %NULL, used to return the type of this handle
 *
 * Get the handle representing the contact, chatroom, etc. with which this
 * channel communicates for its whole lifetime, or 0 if there is no such
 * handle or it has not yet been discovered.
 *
 * This is the same as the #TpChannelIface:handle property. It isn't
 * guaranteed to have its final value until the %TP_CHANNEL_FEATURE_CORE
 * feature is ready.
 *
 * If @handle_type is not %NULL, the type of handle is written into it.
 * This will be %TP_UNKNOWN_HANDLE_TYPE if the handle has not yet been
 * discovered, or %TP_HANDLE_TYPE_NONE if there is no handle with which this
 * channel will always communicate. This is the same as the
 * #TpChannelIface:handle-type property.
 *
 * Returns: the handle
 * Since: 0.7.12
 */
TpHandle
tp_channel_get_handle (TpChannel *self,
                       TpHandleType *handle_type)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  if (handle_type != NULL)
    {
      *handle_type = self->priv->handle_type;
    }

  return self->priv->handle;
}

/**
 * tp_channel_get_identifier:
 * @self: a channel
 *
 * This channel's associated identifier, or the empty string if no identifier
 * or unknown.
 *
 * This is the same as the #TpChannel:identifier property, and isn't guaranteed
 * to be set until the %TP_CHANNEL_FEATURE_CORE property is ready.
 *
 * Changed in 0.11.4: as with #TpChannel:identifier, this could
 * previously either be %NULL or the empty string if there was no suitable
 * value. It is now non-%NULL in all cases.
 *
 * Returns: the identifier
 * Since: 0.7.21
 */
const gchar *
tp_channel_get_identifier (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  if (self->priv->identifier == NULL)
    return "";

  return self->priv->identifier;
}

/**
 * tp_channel_get_connection:
 * @self: a channel
 *
 * Returns the connection for this channel. The returned pointer is only valid
 * while this channel is valid - reference it with g_object_ref() if needed.
 *
 * Returns: (transfer none): the value of #TpChannel:connection
 * Since: 0.19.9
 */
TpConnection *
tp_channel_get_connection (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->connection;
}

GHashTable *
_tp_channel_get_immutable_properties (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->channel_properties;
}

/**
 * tp_channel_dup_immutable_properties:
 * @self: a channel
 *
 * Returns the immutable D-Bus properties of this channel, in a variant of type
 * %G_VARIANT_TYPE_VARDICT where the keys are strings,
 * D-Bus interface name + "." + property name. Use g_variant_lookup() or
 * g_variant_lookup_value() for convenient access to the values.
 *
 * If the #TpChannel:channel-properties property was not set during
 * construction (e.g. by calling tp_channel_new_from_properties()), a
 * reasonable but possibly incomplete version will be made up from the values
 * of individual properties; reading this property repeatedly may yield
 * progressively more complete values until the %TP_CHANNEL_FEATURE_CORE
 * feature is prepared.
 *
 * This function should be used only by #TpChannel subclasses, otherwise it is
 * recommended to use individual property getters instead.
 *
 * Returns: (transfer full): a dictionary where the keys are strings,
 *  D-Bus interface name + "." + property name.
 * Since: 0.19.9
 */
GVariant *
tp_channel_dup_immutable_properties (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return _tp_asv_to_vardict (self->priv->channel_properties);
}

/**
 * tp_channel_get_target_contact:
 * @self: a channel
 *
 * <!-- -->
 *
 * Returns: (transfer none): the value of #TpChannel:target-contact
 * Since: 0.15.6
 */
TpContact *
tp_channel_get_target_contact (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->target_contact;
}

/**
 * tp_channel_get_initiator_contact:
 * @self: a channel
 *
 * <!-- -->
 *
 * Returns: (transfer none): the value of #TpChannel:initiator-contact
 * Since: 0.15.6
 */
TpContact *
tp_channel_get_initiator_contact (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->initiator_contact;
}

static void
tp_channel_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->connection);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value,
          g_quark_to_string (self->priv->channel_type));
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->priv->handle_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, self->priv->handle);
      break;
    case PROP_IDENTIFIER:
      g_value_set_string (value, tp_channel_get_identifier (self));
      break;
    case PROP_CHANNEL_PROPERTIES:
      g_value_set_boxed (value, self->priv->channel_properties);
      break;
    case PROP_GROUP_FLAGS:
      g_value_set_uint (value, self->priv->group_flags);
      break;
    case PROP_REQUESTED:
      g_value_set_boolean (value, tp_channel_get_requested (self));
      break;
    case PROP_PASSWORD_NEEDED:
      g_value_set_boolean (value, tp_channel_password_needed (self));
      break;
    case PROP_TARGET_CONTACT:
      g_value_set_object (value, tp_channel_get_target_contact (self));
      break;
    case PROP_INITIATOR_CONTACT:
      g_value_set_object (value, tp_channel_get_initiator_contact (self));
      break;
    case PROP_GROUP_SELF_CONTACT:
      g_value_set_object (value, tp_channel_group_get_self_contact (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


/* These functions, maybe_set_whatever, ignore attempts to set a null value.
 * This means we can indiscriminately set everything from every source
 * (channel-properties, other construct-time properties, GetAll fast path,
 * 0.16.x slow path), and if only one of the sources supplied a value, it'll
 * still all be fine. */


static void
_tp_channel_maybe_set_channel_type (TpChannel *self,
                                    const gchar *type)
{
  GQuark q = g_quark_from_string (type);

  if (type == NULL)
    return;

  self->priv->channel_type = q;
  g_hash_table_insert (self->priv->channel_properties,
      g_strdup (TP_PROP_CHANNEL_CHANNEL_TYPE),
      tp_g_value_slice_new_static_string (g_quark_to_string (q)));

  tp_proxy_add_interface_by_id ((TpProxy *) self,
      self->priv->channel_type);
}


static void
_tp_channel_maybe_set_handle (TpChannel *self,
                              TpHandle handle,
                              gboolean valid)
{
  if (valid)
    {
      self->priv->handle = handle;
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE),
          tp_g_value_slice_new_uint (handle));
    }
}


static void
_tp_channel_maybe_set_handle_type (TpChannel *self,
                                   TpHandleType handle_type,
                                   gboolean valid)
{
  if (valid)
    {
      self->priv->handle_type = handle_type;
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE_TYPE),
          tp_g_value_slice_new_uint (handle_type));
    }
}


static void
_tp_channel_maybe_set_identifier (TpChannel *self,
                                  const gchar *identifier)
{
  if (identifier != NULL && self->priv->identifier == NULL)
    {
      self->priv->identifier = g_strdup (identifier);
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_ID),
          tp_g_value_slice_new_string (identifier));
    }
}

static void
_tp_channel_maybe_set_interfaces (TpChannel *self,
                                  const gchar **interfaces)
{
  if (interfaces == NULL)
    return;

  tp_proxy_add_interfaces ((TpProxy *) self, interfaces);

  g_hash_table_insert (self->priv->channel_properties,
      g_strdup (TP_PROP_CHANNEL_INTERFACES),
      tp_g_value_slice_new_boxed (G_TYPE_STRV, interfaces));
}


static void
tp_channel_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      self->priv->connection = TP_CONNECTION (g_value_dup_object (value));
      break;

    case PROP_CHANNEL_TYPE:
      _tp_channel_maybe_set_channel_type (self, g_value_get_string (value));
      break;

    case PROP_HANDLE_TYPE:
      _tp_channel_maybe_set_handle_type (self, g_value_get_uint (value),
          (g_value_get_uint (value) != TP_UNKNOWN_HANDLE_TYPE));
      break;

    case PROP_HANDLE:
      _tp_channel_maybe_set_handle (self, g_value_get_uint (value),
          (g_value_get_uint (value) != 0));
      break;

    case PROP_CHANNEL_PROPERTIES:
        {
          GHashTable *asv = g_value_get_boxed (value);
          gboolean valid;

          /* default value at construct time is NULL, we need to ignore that */
          if (asv != NULL)
            {
              guint u;

              /* no need to emit GObject::notify for any of these since this
               * can only happen at construct time, before anyone has
               * connected to it */

              tp_g_hash_table_update (self->priv->channel_properties,
                  asv, (GBoxedCopyFunc) g_strdup,
                  (GBoxedCopyFunc) tp_g_value_slice_dup);

              u = tp_asv_get_uint32 (self->priv->channel_properties,
                  TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid);
              _tp_channel_maybe_set_handle_type (self, u, valid);

              u = tp_asv_get_uint32 (self->priv->channel_properties,
                  TP_PROP_CHANNEL_TARGET_HANDLE, &valid);
              _tp_channel_maybe_set_handle (self, u, valid);

              _tp_channel_maybe_set_identifier (self,
                  tp_asv_get_string (self->priv->channel_properties,
                      TP_PROP_CHANNEL_TARGET_ID));

              _tp_channel_maybe_set_channel_type (self,
                  tp_asv_get_string (self->priv->channel_properties,
                      TP_PROP_CHANNEL_CHANNEL_TYPE));

              _tp_channel_maybe_set_interfaces (self,
                  tp_asv_get_boxed (self->priv->channel_properties,
                      TP_PROP_CHANNEL_INTERFACES,
                      G_TYPE_STRV));
            }
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


/* Introspection etc. */


void
_tp_channel_abort_introspection (TpChannel *self,
                                 const gchar *debug,
                                 const GError *error)
{
  DEBUG ("%p: Introspection failed: %s: %s", self, debug, error->message);

  g_assert (self->priv->introspect_needed != NULL);
  g_queue_free (self->priv->introspect_needed);
  self->priv->introspect_needed = NULL;
  tp_proxy_invalidate ((TpProxy *) self, error);
}

void
_tp_channel_continue_introspection (TpChannel *self)
{
  DEBUG ("%p", self);

  g_assert (self->priv->introspect_needed != NULL);

  if (tp_proxy_get_invalidated (self))
    {
      DEBUG ("invalidated; giving up");

      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;
    }
  else if (g_queue_peek_head (self->priv->introspect_needed) == NULL)
    {
      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;

      DEBUG ("%p: channel ready", self);

      _tp_proxy_set_feature_prepared ((TpProxy *) self,
          TP_CHANNEL_FEATURE_CORE, TRUE);
    }
  else
    {
      TpChannelProc next = g_queue_pop_head (self->priv->introspect_needed);

      next (self);
    }
}

static void
_tp_channel_got_properties (TpProxy *proxy,
                            GHashTable *asv,
                            const GError *error,
                            gpointer unused G_GNUC_UNUSED,
                            GObject *object G_GNUC_UNUSED)
{
  TpChannel *self = TP_CHANNEL (proxy);
  gboolean valid;
  guint u;
  const gchar *s;
  gboolean b;


  if (error != NULL)
    {
      _tp_channel_abort_introspection (self, "GetAll failed", error);
      return;
    }

  DEBUG ("Received %u channel properties", g_hash_table_size (asv));

  self->priv->exists = TRUE;

  _tp_channel_maybe_set_channel_type (self,
      tp_asv_get_string (asv, "ChannelType"));
  _tp_channel_maybe_set_interfaces (self,
      tp_asv_get_boxed (asv, "Interfaces", G_TYPE_STRV));

  u = tp_asv_get_uint32 (asv, "TargetHandleType", &valid);
  _tp_channel_maybe_set_handle_type (self, u, valid);

  u = tp_asv_get_uint32 (asv, "TargetHandle", &valid);
  _tp_channel_maybe_set_handle (self, u, valid);

  _tp_channel_maybe_set_identifier (self,
      tp_asv_get_string (asv, "TargetID"));

  u = tp_asv_get_uint32 (asv, "InitiatorHandle", &valid);

  if (valid)
    {
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_INITIATOR_HANDLE),
          tp_g_value_slice_new_uint (u));
    }

  s = tp_asv_get_string (asv, "InitiatorID");

  if (s != NULL)
    {
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_INITIATOR_ID),
          tp_g_value_slice_new_string (s));
    }

  b = tp_asv_get_boolean (asv, "Requested", &valid);

  if (valid)
    {
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_REQUESTED),
          tp_g_value_slice_new_boolean (b));
    }

  g_object_notify ((GObject *) self, "channel-type");
  g_object_notify ((GObject *) self, "interfaces");
  g_object_notify ((GObject *) self, "handle-type");
  g_object_notify ((GObject *) self, "handle");
  g_object_notify ((GObject *) self, "identifier");

  _tp_channel_continue_introspection (self);
}

static void
_tp_channel_get_properties (TpChannel *self)
{
  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CHANNEL, _tp_channel_got_properties, NULL, NULL, NULL);
}

static void
connection_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpChannel *self = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    {
      _tp_channel_abort_introspection (self, "Preparing connection failed", error);
      g_clear_error (&error);
    }
  else
    {
      _tp_channel_continue_introspection (self);
    }

  g_object_unref (self);
}

static void
_tp_channel_prepare_connection (TpChannel *self)
{
  /* Skip if connection is already prepared */
  if (tp_proxy_is_prepared (self->priv->connection, TP_CONNECTION_FEATURE_CORE))
    {
      _tp_channel_continue_introspection (self);
      return;
    }

  tp_proxy_prepare_async (self->priv->connection, NULL,
      connection_prepared_cb, g_object_ref (self));
}

static void
upgrade_contacts_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpClientFactory *factory = (TpClientFactory *) object;
  TpChannel *self = user_data;
  GError *error = NULL;

  if (!tp_client_factory_upgrade_contacts_finish (factory, result, NULL,
          &error))
    {
      _tp_channel_abort_introspection (self, "Upgrading contacts failed",
          error);
      g_clear_error (&error);
    }
  else
    {
      _tp_channel_continue_introspection (self);
    }

  g_object_unref (self);
}

static void
_tp_channel_create_contacts (TpChannel *self)
{
  GPtrArray *contacts;
  TpHandle initiator_handle;
  const gchar *initiator_id;

  g_assert (self->priv->target_contact == NULL);
  g_assert (self->priv->initiator_contact == NULL);

  contacts = g_ptr_array_new ();

  /* Create target contact */
  if (self->priv->handle_type == TP_HANDLE_TYPE_CONTACT)
    {
      if (self->priv->handle == 0 || self->priv->identifier == NULL)
        {
          GError e = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "Channel with HandleType CONTACT must have a valid handle "
              "and identifier" };
          _tp_channel_abort_introspection (self, e.message, &e);
          return;
        }

      self->priv->target_contact = tp_client_factory_ensure_contact (
          tp_proxy_get_factory (self->priv->connection), self->priv->connection,
          self->priv->handle, self->priv->identifier);
      g_ptr_array_add (contacts, self->priv->target_contact);
    }

  /* Create initiator contact */
  initiator_handle = tp_asv_get_uint32 (self->priv->channel_properties,
      TP_PROP_CHANNEL_INITIATOR_HANDLE, NULL);
  initiator_id = tp_asv_get_string (self->priv->channel_properties,
      TP_PROP_CHANNEL_INITIATOR_ID);

  if (initiator_handle != 0 && !tp_str_empty (initiator_id))
    {
      self->priv->initiator_contact = tp_client_factory_ensure_contact (
          tp_proxy_get_factory (self->priv->connection), self->priv->connection,
          initiator_handle, initiator_id);
      g_ptr_array_add (contacts, self->priv->initiator_contact);
    }
  else if ((initiator_handle == 0) != (tp_str_empty (initiator_id)))
    {
      GError e = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Channel must have both initiator handle and identifier, "
          "or none of them" };
      _tp_channel_abort_introspection (self, e.message, &e);
      return;
    }

  /* Prepare initiator and target contacts */
  if (contacts->len > 0)
    {
      tp_client_factory_upgrade_contacts_async (
          tp_proxy_get_factory (self->priv->connection),
          self->priv->connection,
          contacts->len, (TpContact **) contacts->pdata,
          upgrade_contacts_cb, g_object_ref (self));
    }
  else
    {
      _tp_channel_continue_introspection (self);
    }

  g_ptr_array_unref (contacts);
}

static void
tp_channel_closed_cb (TpChannel *self,
                      gpointer user_data,
                      GObject *weak_object)
{

  if (self->priv->group_remove_error != NULL)
    {
      /* use the error provided by the Group code */
      tp_proxy_invalidate ((TpProxy *) self, self->priv->group_remove_error);
      g_clear_error (&self->priv->group_remove_error);
    }
  else
    {
      GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
          "Channel was closed" };

      tp_proxy_invalidate ((TpProxy *) self, &e);
    }
}

static void
tp_channel_connection_invalidated_cb (TpConnection *conn,
                                      guint domain,
                                      guint code,
                                      gchar *message,
                                      TpChannel *self)
{
  const GError e = { domain, code, message };

  g_signal_handler_disconnect (conn, self->priv->conn_invalidated_id);
  self->priv->conn_invalidated_id = 0;

  /* tp_proxy_invalidate and g_object_notify call out to user code - add a
   * temporary ref to ensure that we don't become finalized while doing so */
  g_object_ref (self);

  tp_proxy_invalidate ((TpProxy *) self, &e);

  /* this channel's handle is now meaningless */
  if (self->priv->handle != 0)
    {
      self->priv->handle = 0;
      g_object_notify ((GObject *) self, "handle");
    }

  g_object_unref (self);
}

static GObject *
tp_channel_constructor (GType type,
                        guint n_params,
                        GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_channel_parent_class;
  TpChannel *self = TP_CHANNEL (object_class->constructor (type,
        n_params, params));
  GError *error = NULL;
  TpProxySignalConnection *sc;

  g_assert (tp_proxy_get_factory (self) ==
      tp_proxy_get_factory (self->priv->connection));

  /* If our TpConnection dies, so do we. */
  self->priv->conn_invalidated_id = g_signal_connect (self->priv->connection,
      "invalidated", G_CALLBACK (tp_channel_connection_invalidated_cb),
      self);

  /* Connect to my own Closed signal and self-destruct when it arrives.
   * The channel hasn't had a chance to become invalid yet (it was just
   * constructed!), so we assert that this signal connection will work */
  sc = tp_cli_channel_connect_to_closed (self, tp_channel_closed_cb, NULL, NULL,
      NULL, &error);

  if (sc == NULL)
    {
      CRITICAL ("Couldn't connect to Closed: %s", error->message);
      g_assert_not_reached ();
      g_error_free (error);
      return NULL;
    }

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self,
      (self->priv->channel_type != 0)
          ? g_quark_to_string (self->priv->channel_type)
          : "(null)",
      self->priv->handle, self->priv->handle_type);

  self->priv->introspect_needed = g_queue_new ();

  /* this does nothing if connection already has CORE prepared */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_prepare_connection);

  /* this does nothing if we already know all the Channel properties this
   * code is aware of */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_properties);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_create_contacts);

  _tp_channel_continue_introspection (self);

  return (GObject *) self;
}

static void
tp_channel_init (TpChannel *self)
{
  DEBUG ("%p", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CHANNEL,
      TpChannelPrivate);
  self->priv->channel_type = 0;
  self->priv->handle_type = TP_UNKNOWN_HANDLE_TYPE;
  self->priv->handle = 0;
  self->priv->channel_properties = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free);
  self->priv->contacts_queue = g_queue_new ();
}

static void
tp_channel_dispose (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  if (self->priv->connection == NULL)
    goto finally;

  if (self->priv->conn_invalidated_id != 0)
    g_signal_handler_disconnect (self->priv->connection,
        self->priv->conn_invalidated_id);

  self->priv->conn_invalidated_id = 0;

  g_clear_object (&self->priv->connection);
  g_clear_object (&self->priv->target_contact);
  g_clear_object (&self->priv->initiator_contact);
  g_clear_object (&self->priv->group_self_contact);
  tp_clear_pointer (&self->priv->group_members,
      g_hash_table_unref);
  tp_clear_pointer (&self->priv->group_local_pending,
      g_hash_table_unref);
  tp_clear_pointer (&self->priv->group_local_pending_info,
      g_hash_table_unref);
  tp_clear_pointer (&self->priv->group_remote_pending,
      g_hash_table_unref);
  tp_clear_pointer (&self->priv->group_contact_owners,
      g_hash_table_unref);

finally:
  ((GObjectClass *) tp_channel_parent_class)->dispose (object);
}

static void
tp_channel_finalize (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  g_clear_error (&self->priv->group_remove_error);
  tp_clear_pointer (&self->priv->introspect_needed, g_queue_free);
  tp_clear_pointer (&self->priv->channel_properties, g_hash_table_unref);
  tp_clear_pointer (&self->priv->contacts_queue, g_queue_free);

  g_free (self->priv->identifier);

  ((GObjectClass *) tp_channel_parent_class)->finalize (object);
}

static void
got_password_flags_cb (TpChannel *self,
    guint password_flags,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to get password flags: %s", error->message);
    }
  else
    {
      self->priv->password_flags = password_flags;

      if (tp_channel_password_needed (self))
        {
          /* password-needed is FALSE by default */
          g_object_notify (G_OBJECT (self), "password-needed");
        }
    }

  g_simple_async_result_complete (result);
}

static void
password_flags_changed_cb (TpChannel *self,
    guint added,
    guint removed,
    gpointer user_data,
    GObject *weak_object)
{
  gboolean was_needed, needed;

  was_needed = tp_channel_password_needed (self);

  self->priv->password_flags |= added;
  self->priv->password_flags ^= removed;

  needed = tp_channel_password_needed (self);

  if (was_needed != needed)
    g_object_notify (G_OBJECT (self), "password-needed");
}

static void
tp_channel_prepare_password_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) proxy;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_channel_prepare_password_async);

  tp_cli_channel_interface_password_connect_to_password_flags_changed (self,
      password_flags_changed_cb, self, NULL, G_OBJECT (self), NULL);

  tp_cli_channel_interface_password_call_get_password_flags (self, -1,
      got_password_flags_cb, result, g_object_unref, G_OBJECT (self));
}


enum {
    FEAT_CORE,
    FEAT_GROUP,
    FEAT_PASSWORD,
    N_FEAT
};

static const TpProxyFeature *
tp_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };
  static GQuark need_password[2] = {0, 0};

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name = TP_CHANNEL_FEATURE_CORE;
  features[FEAT_CORE].core = TRUE;

  features[FEAT_GROUP].name = TP_CHANNEL_FEATURE_GROUP;
  features[FEAT_GROUP].prepare_async =
    _tp_channel_group_prepare_async;

  features[FEAT_PASSWORD].name = TP_CHANNEL_FEATURE_PASSWORD;
  features[FEAT_PASSWORD].prepare_async =
    tp_channel_prepare_password_async;
  need_password[0] = TP_IFACE_QUARK_CHANNEL_INTERFACE_PASSWORD;
  features[FEAT_PASSWORD].interfaces_needed = need_password;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_channel_class_init (TpChannelClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  tp_channel_init_known_interfaces ();

  g_type_class_add_private (klass, sizeof (TpChannelPrivate));

  object_class->constructor = tp_channel_constructor;
  object_class->get_property = tp_channel_get_property;
  object_class->set_property = tp_channel_set_property;
  object_class->dispose = tp_channel_dispose;
  object_class->finalize = tp_channel_finalize;

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL;
  proxy_class->must_have_unique_name = TRUE;
  proxy_class->list_features = tp_channel_list_features;

  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");
  g_object_class_override_property (object_class, PROP_HANDLE,
      "handle");

  /**
   * TpChannel:identifier:
   *
   * This channel's associated identifier, or the empty string if it has
   * handle type %TP_HANDLE_TYPE_NONE.
   *
   * For channels where #TpChannelIface:handle is non-zero, this is the result
   * of inspecting #TpChannelIface:handle.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE; until then, it may be
   * the empty string.
   *
   * Changed in 0.11.4: this property is never %NULL. Previously,
   * it was %NULL before an identifier was known, or when a channel
   * with no TargetID D-Bus property had TargetHandleType %TP_HANDLE_TYPE_NONE.
   */
  param_spec = g_param_spec_string ("identifier",
      "The identifier",
      "The identifier of the channel",
      "",
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_IDENTIFIER,
      param_spec);

  /**
   * TpChannel:channel-properties:
   *
   * The immutable D-Bus properties of this channel, represented by a
   * #GHashTable where the keys are D-Bus interface name + "." + property
   * name, and the values are #GValue instances.
   *
   * Read-only except during construction. If this is not provided
   * during construction, a reasonable (but possibly incomplete) version
   * will be made up from the values of individual properties; reading this
   * property repeatedly may yield progressively more complete values until
   * tp_proxy_prepare_async() has finished preparing %TP_CHANNEL_FEATURE_CORE.
   */
  param_spec = g_param_spec_boxed ("channel-properties",
      "Immutable D-Bus properties",
      "A map D-Bus interface + \".\" + property name => GValue",
      TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PROPERTIES,
      param_spec);

  /**
   * TpChannel:connection:
   *
   * The #TpConnection to which this #TpChannel belongs. Used for e.g.
   * handle manipulation.
   */
  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The connection to which this object belongs.", TP_TYPE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  /**
   * TpChannel:group-flags:
   *
   * If the %TP_CHANNEL_FEATURE_GROUP feature has been prepared successfully,
   * #TpChannelGroupFlags indicating the capabilities and behaviour of that
   * group.
   *
   * Otherwise, this may be 0.
   *
   * Change notification is via notify::group-flags or
   * TpChannel::group-flags-changed.
   *
   * Since: 0.99.1
   */
  param_spec = g_param_spec_uint ("group-flags", "Group.GroupFlags",
      "0 if not a group", 0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GROUP_FLAGS,
      param_spec);

  /**
   * TpChannel:requested:
   *
   * %TRUE if this channel was created in response to a local request, such
   * as a call to tp_account_channel_request_create_channel_async(). %FALSE
   * if this channel was initiated by a remote contact
   * (the #TpChannel:initiator-contact), or if it appeared as a side-effect
   * of some other action.
   *
   * For instance, this is %FALSE on incoming calls and file transfers,
   * remotely-initiated 1-1 text conversations, and invitations to chatrooms,
   * and %TRUE on outgoing calls and file transfers, locally-initiated 1-1
   * text conversations, and chatrooms joined by local user action.
   *
   * This is not guaranteed to be meaningful until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE; until then, it may return
   * %FALSE even if the channel was actually requested.
   *
   * Since: 0.11.15
   */
  param_spec = g_param_spec_boolean ("requested", "Requested",
      "TRUE if the channel has been requested",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED,
      param_spec);

  /**
   * TpChannel:password-needed:
   *
   * If %TRUE, tp_channel_provide_password_async() has to be called
   * to be able to join the channel.
   *
   * This is not guaranteed to be meaningful until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_PASSWORD; until then, it may return
   * %FALSE even if the channel is actually protected by a password.
   * Preparing %TP_CHANNEL_FEATURE_PASSWORD also ensures that the
   * notify::password-needed signal will be fired when this property changes.
   *
   * Since: 0.15.2
   */
  param_spec = g_param_spec_boolean ("password-needed",
      "Password needed",
      "Password neede to join the channel",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PASSWORD_NEEDED,
      param_spec);

  /**
   * TpChannel::group-flags-changed:
   * @self: a channel
   * @added: #TpChannelGroupFlags which are newly set
   * @removed: #TpChannelGroupFlags which are no longer set
   *
   * Emitted when the #TpChannel:group-flags property changes while the
   * channel is ready.
   *
   * Since: 0.7.12
   */
  signals[SIGNAL_GROUP_FLAGS_CHANGED] = g_signal_new ("group-flags-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * TpChannel:target-contact:
   *
   * If this channel is for communication with a single contact (that is,
   * #TpChannelIface:handle-type is %TP_HANDLE_TYPE_CONTACT), then a #TpContact
   * representing the remote contact. For chat rooms, contact search channels and
   * other channels without a single remote contact, %NULL.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE; until then, it may be
   * %NULL.
   *
   * The #TpContact object is guaranteed to have all of the features previously
   * passed to tp_client_factory_add_contact_features() prepared.
   *
   * Since: 0.15.6
   */
  param_spec = g_param_spec_object ("target-contact", "Target Contact",
      "The channel's target contact", TP_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET_CONTACT,
      param_spec);

  /**
   * TpChannel:initiator-contact:
   *
   * The #TpContact of the initiator of this channel, or %NULL if there is no
   * particular initiator.
   *
   * If the channel was initiated by a remote contact, this represents
   * that contact, and #TpChannel:requested will be %FALSE. For instance,
   * for an incoming call this property indicates the caller, and for a
   * chatroom invitation this property indicates who sent the invitation.
   *
   * If the channel was requested by the local user, #TpChannel:requested
   * will be %TRUE, and this property may be the #TpChannel:group-self-contact
   * or #TpConnection:self-contact.
   *
   * If the channel appeared for some other reason (for instance as a
   * side-effect of connecting to the server), this property may be %NULL.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE; until then, it may be
   * %NULL.
   *
   * The #TpContact object is guaranteed to have all of the features previously
   * passed to tp_client_factory_add_contact_features() prepared.
   *
   * Since: 0.15.6
   */
  param_spec = g_param_spec_object ("initiator-contact", "Initiator Contact",
      "Undefined if not a group", TP_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_CONTACT,
      param_spec);

  /**
   * TpChannel:group-self-contact:
   *
   * If this channel is a group and %TP_CHANNEL_FEATURE_GROUP has been
   * prepared, and the user is a member of the group, the #TpContact
   * representing them in this group.
   *
   * Otherwise, the result may be either a contact representing the user,
   * or %NULL.
   *
   * Change notification is via notify::group-self-contact.
   *
   * Since: 0.99.1
   */
  param_spec = g_param_spec_object ("group-self-contact", "Group.SelfHandle",
      "Undefined if not a group", TP_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GROUP_SELF_CONTACT,
      param_spec);

  /**
   * TpChannel::group-members-changed:
   * @self: a channel
   * @added: (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
   *  a #GPtrArray of #TpContact containing the full members added
   * @removed: (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
   *  a #GPtrArray of #TpContact containing the members (full, local-pending or
   *  remote-pending) removed
   * @local_pending: (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
   *  a #GPtrArray of #TpContact containing the local-pending members added
   * @remote_pending: (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
   *  a #GPtrArray of #TpContact containing the remote-pending members added
   * @actor: a #TpContact for the "actor" handle in @details
   * @details: (type GLib.HashTable) (element-type utf8 GObject.Value):
   *  a #GHashTable mapping (gchar *) to #GValue containing details
   *  about the change, as described in the specification of the
   *  MembersChanged signal.
   *
   * Emitted when the group members change in a Group channel.
   *
   * This is not guaranteed to be emitted until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_GROUP; until then, it may be
   * omitted.
   *
   * Since: 0.99.1
   */
  signals[SIGNAL_GROUP_MEMBERS_CHANGED] = g_signal_new (
      "group-members-changed", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE, 6,
      G_TYPE_PTR_ARRAY, G_TYPE_PTR_ARRAY, G_TYPE_PTR_ARRAY, G_TYPE_PTR_ARRAY,
      TP_TYPE_CONTACT, TP_HASH_TYPE_STRING_VARIANT_MAP);
}

TpChannel *
_tp_channel_new_with_factory (TpClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  TpChannel *ret = NULL;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  /* An unfortunate collision between the default value in
   * TpChannelIface (0), and the default we want (-1), means that
   * we have to pass TP_UNKNOWN_HANDLE_TYPE to the constructor
   * explicitly, even if providing channel-properties. */

  ret = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "connection", conn,
        "dbus-daemon", tp_proxy_get_dbus_daemon (conn),
        "bus-name", tp_proxy_get_bus_name (conn),
        "object-path", object_path,
        "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
        "channel-properties", immutable_properties,
        "factory", factory,
        NULL));

finally:
  return ret;
}

static gpointer
tp_channel_once (gpointer data G_GNUC_UNUSED)
{
  GType type = TP_TYPE_CHANNEL;

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (type,
      tp_cli_channel_add_signals);
  tp_proxy_subclass_add_error_mapping (type,
      TP_ERROR_PREFIX, TP_ERROR, TP_TYPE_ERROR);

  return NULL;
}

/**
 * tp_channel_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpChannel have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CHANNEL.
 *
 * Since: 0.7.6
 */
void
tp_channel_init_known_interfaces (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tp_channel_once, NULL);
}

/**
 * tp_channel_get_requested:
 * @self: a #TpChannel
 *
 * Return the #TpChannel:requested property
 *
 * Returns: the value of #TpChannel:requested
 *
 * Since: 0.11.15
 */
gboolean
tp_channel_get_requested (TpChannel *self)
{
  return tp_asv_get_boolean (self->priv->channel_properties,
      TP_PROP_CHANNEL_REQUESTED, NULL);
}

/* tp_cli callbacks can potentially be called in a re-entrant way,
 * so we can't necessarily complete @result without using an idle. */
static void
channel_join_cb (TpChannel *self,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("join failed: %s", error->message);
      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete_in_idle (result);
}

/**
 * tp_channel_join_async:
 * @self: a #TpChannel
 * @message: the join message
 * @callback: a callback to call when we joined the channel
 * @user_data: data to pass to @callback
 *
 * Join channel @self with @message as join message.
 *
 * When we joined the channel, @callback will be called.
 * You can then call tp_channel_join_finish() to get the result of
 * the operation.
 *
 * %TP_CHANNEL_FEATURE_GROUP feature must be prepared before calling this
 * function.
 *
 * Since: 0.99.1
 */
void
tp_channel_join_async (TpChannel *self,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpHandle self_handle;
  GArray *array;

  g_return_if_fail (TP_IS_CHANNEL (self));
  g_return_if_fail (tp_proxy_is_prepared (self, TP_CHANNEL_FEATURE_GROUP));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_channel_join_async);

  array = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
  self_handle = tp_contact_get_handle (self->priv->group_self_contact);
  g_array_append_val (array, self_handle);

  tp_cli_channel_interface_group_call_add_members (self, -1, array, message,
      channel_join_cb, result, g_object_unref, NULL);

  g_array_unref (array);
}

/**
 * tp_channel_join_finish:
 * @self: a #TpChannel
 * @result: a #GAsyncResult passed to the callback for tp_channel_join_async().
 * @error: a #GError to fill
 *
 * Completes a call to tp_channel_join_async().
 *
 * Returns: %TRUE if the channel was successfully joined; %FALSE otherwise
 *
 * Since: 0.99.1
 */
gboolean
tp_channel_join_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_channel_join_async);
}

/* tp_cli callbacks can potentially be called in a re-entrant way,
 * so we can't necessarily complete @result without using an idle. */
static void
channel_close_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Close() failed: %s", error->message);

      if (tp_proxy_get_invalidated (channel) == NULL)
        {
          g_simple_async_result_set_from_error (result, error);
        }
      else
        {
          DEBUG ("... but channel was already invalidated, so never mind");
        }
    }

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}

static void
channel_remove_self_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (tp_proxy_get_invalidated (channel) == NULL &&
      error != NULL)
    {
      DEBUG ("RemoveMembersWithDetails() with self handle failed; call Close()"
          " %s", error->message);

      tp_cli_channel_call_close (channel, -1, channel_close_cb, result,
          NULL, NULL);
      return;
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_channel_leave_async:
 * @self: a #TpChannel
 * @reason: the leave reason
 * @message: the leave message
 * @callback: a callback to call when we left the channel
 * @user_data: data to pass to @callback
 *
 * Leave channel @self with @reason as reason and @message as leave message.
 * If %TP_CHANNEL_FEATURE_GROUP feature is not prepared, we close it.
 *
 * When we left the channel, @callback will be called.
 * You can then call tp_channel_leave_finish() to get the result of
 * the operation.
 *
 * Since: 0.99.1
 */
void
tp_channel_leave_async (TpChannel *self,
    TpChannelGroupChangeReason reason,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  TpHandle self_handle;
  GArray *handles;

  g_return_if_fail (TP_IS_CHANNEL (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_channel_leave_async);

  if (!tp_proxy_is_prepared (self, TP_CHANNEL_FEATURE_GROUP))
    {
      DEBUG ("TP_CHANNEL_FEATURE_GROUP is not prepared; "
          "fallback to Close()");

      tp_cli_channel_call_close (self, -1, channel_close_cb, result,
          NULL, NULL);
      return;
    }

  handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
  self_handle = tp_contact_get_handle (self->priv->group_self_contact);
  g_array_append_val (handles, self_handle);

  tp_cli_channel_interface_group_call_remove_members (
      self, -1, handles, message, reason,
      channel_remove_self_cb, result, NULL, NULL);

  g_array_unref (handles);
}

/**
 * tp_channel_leave_finish:
 * @self: a #TpChannel
 * @result: a #GAsyncResult passed to the callback for tp_channel_leave_async().
 * @error: a #GError to fill
 *
 * Completes a call to tp_channel_leave_async().
 *
 * Returns: %TRUE if the channel has been left; %FALSE otherwise
 *
 * Since: 0.13.10
 */
gboolean
tp_channel_leave_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_channel_leave_async)
}

/**
 * tp_channel_close_async:
 * @self: a #TpChannel
 * @callback: a callback to call when we closed the channel, or %NULL
 *  to ignore any reply
 * @user_data: data to pass to @callback
 *
 * Close channel @self. In most cases, it's generally cleaner to use
 * tp_channel_leave_async() instead to properly leave and close the channel.
 *
 * When the channel has been closed, @callback will be called.
 * You can then call tp_channel_close_finish() to get the result of
 * the operation.
 *
 * Since: 0.13.10
 */
void
tp_channel_close_async (TpChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_CHANNEL (self));

  if (callback == NULL)
    {
      tp_cli_channel_call_close (self, -1, NULL, NULL, NULL, NULL);
      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_channel_close_async);
  tp_cli_channel_call_close (self, -1, channel_close_cb, result,
      NULL, NULL);
}

/**
 * tp_channel_close_finish:
 * @self: a #TpChannel
 * @result: a #GAsyncResult passed to the callback for tp_channel_close_async().
 * @error: a #GError to fill
 *
 * Finishes a call to tp_channel_leave_async().
 *
 * Returns: %TRUE if the channel has been closed; %FALSE otherwise
 *
 * Since: 0.13.10
 */
gboolean
tp_channel_close_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_channel_close_async)
}

static void
channel_destroy_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (tp_proxy_get_invalidated (channel) == NULL &&
      error != NULL)
    {
      DEBUG ("Destroy() failed; call Close(): %s", error->message);

      tp_cli_channel_call_close (channel, -1, channel_close_cb, result,
          NULL, NULL);
      return;
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_channel_destroy_async:
 * @self: a #TpChannel
 * @callback: a callback to call when we left the channel
 * @user_data: data to pass to @callback
 *
 * Destroy channel @self.
 * If @self doesn't implement #TP_IFACE_QUARK_CHANNEL_INTERFACE_DESTROYABLE
 * or if for any reason we can't destroy the channel, we close it.
 *
 * When the channel has been destroyed or closed, @callback will be called.
 * You can then call tp_channel_destroy_finish() to get the result of
 * the operation.
 *
 * Since: 0.15.2
 */
void
tp_channel_destroy_async (TpChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_CHANNEL (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_channel_destroy_async);

  if (tp_proxy_is_prepared (self, TP_CHANNEL_FEATURE_CORE) &&
      !tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_DESTROYABLE))
    {
      DEBUG ("Channel doesn't implement Destroy; fallback to Close()");

      tp_cli_channel_call_close (self, -1, channel_close_cb, result,
          NULL, NULL);
      return;
    }

  tp_cli_channel_interface_destroyable_call_destroy (self, -1,
      channel_destroy_cb, result, NULL, NULL);
}

/**
 * tp_channel_destroy_finish:
 * @self: a #TpChannel
 * @result: a #GAsyncResult passed to the callback for tp_channel_destroy_async().
 * @error: a #GError to fill
 *
 * Completes a call to tp_channel_destroy_async().
 *
 * Returns: %TRUE if the channel has been destroyed or closed; %FALSE otherwise
 *
 * Since: 0.15.2
 */
gboolean
tp_channel_destroy_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_channel_destroy_async)
}

/**
 * TP_CHANNEL_FEATURE_PASSWORD:
 *
 * Expands to a call to a function that returns a quark representing the
 * password feature on a #TpChannel.
 *
 * When this feature is prepared, tp_channel_password_needed() and the
 * #TpChannel:password-needed property become useful.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.15.2
 */

GQuark
tp_channel_get_feature_quark_password (void)
{
  return g_quark_from_static_string ("tp-channel-feature-password");
}

/**
 * tp_channel_password_needed:
 * @self: a #TpChannel
 *
 * Return the #TpChannel:password-needed property
 *
 * Returns: the value of #TpChannel:password-needed
 *
 * Since: 0.15.2
 */
gboolean
tp_channel_password_needed (TpChannel *self)
{
  return self->priv->password_flags & TP_CHANNEL_PASSWORD_FLAG_PROVIDE;
}

static void
provide_password_cb (TpChannel *self,
    gboolean correct,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
    }
  else if (!correct)
    {
      DEBUG ("Wrong password provided for %s", tp_proxy_get_object_path (self));

      g_simple_async_result_set_error (result, TP_ERROR,
          TP_ERROR_AUTHENTICATION_FAILED, "Password was not correct");
    }

  g_simple_async_result_complete (result);
}

/**
 * tp_channel_provide_password_async:
 * @self: a #TpChannel
 * @password: the password
 * @callback: a callback to call when @password has been provided
 * @user_data: data to pass to @callback
 *
 * Provide @password so that @self can be joined.
 * This function must be called with the correct password in order for
 * channel joining to proceed if the TpChannel:password-needed property
 * is set.
 *
 * Once the password has been provided, @callback will be
 * called. You can then call tp_channel_provide_password_finish()
 * to get the result of the operation.
 *
 * Since: 0.15.2
 */
void
tp_channel_provide_password_async (TpChannel *self,
    const gchar *password,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_CHANNEL (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_channel_provide_password_async);

  tp_cli_channel_interface_password_call_provide_password (self, -1, password,
      provide_password_cb, result, g_object_unref, G_OBJECT (self));
}

/**
 * tp_channel_provide_password_finish:
 * @self: a #TpChannel
 * @result: a #GAsyncResult passed to the callback for
 *  tp_channel_provide_password_async().
 * @error: a #GError to fill
 *
 * Completes a call to tp_channel_provide_password_async().
 * If the password was rejected, the operation
 * fails with #TP_ERROR_AUTHENTICATION_FAILED.
 *
 * Returns: %TRUE if the password has been provided and accepted,
 * %FALSE otherwise.
 *
 * Since: 0.15.2
 */
gboolean
tp_channel_provide_password_finish (TpChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_channel_provide_password_async);
}
