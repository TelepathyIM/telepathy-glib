/*
 * base-password-channel.c - Source for TpBasePasswordChannel
 * Copyright (C) 2010 Collabora Ltd.
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
 * SECTION:base-password-channel
 * @title: TpBasePasswordChannel
 * @short_description: a simple X-TELEPATHY-PASSWORD channel
 *
 * This class implements a SASL Authentication channel with the
 * X-TELEPATHY-PASSWORD SASL mechanism.  Most of the time, you should
 * not use or instantiate this class directly.  It is used by
 * #TpSimplePasswordManager behind the scenes.  In some special
 * circumstances (e.g. when the authentication channel needs to
 * implement additional interfaces), it may be necessary to create
 * your own custom authentication channels instead of letting
 * #TpSimplePasswordManager create them automatically.  In this case,
 * you should derive your channel from this class and then pass the
 * channel as an argument to
 * tp_simple_password_manager_prompt_for_channel_async().
 *
 * Since: 0.13.15
 */

/**
 * TpBasePasswordChannel:
 *
 * Data structure representing a channel implementing a SASL Authentication
 * channel with the X-TELEPATHY-PASSWORD SASL mechanism.
 *
 * Since: 0.13.15
 */

/**
 * TpBasePasswordChannelClass:
 *
 * The class of a #TpBasePasswordChannel.
 */

#include "config.h"

#include "telepathy-glib/base-password-channel.h"

#include <telepathy-glib/asv.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-interface.h>

#define DEBUG_FLAG TP_DEBUG_SASL
#include "telepathy-glib/debug-internal.h"

static void sasl_auth_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpBasePasswordChannel, tp_base_password_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_SERVER_AUTHENTICATION1,
        NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_SASL_AUTHENTICATION1,
        sasl_auth_iface_init));

static const gchar *tp_base_password_channel_available_mechanisms[] = {
  "X-TELEPATHY-PASSWORD",
  NULL
};

/* properties */
enum
{
  PROP_AUTHENTICATION_METHOD = 1,

  PROP_AVAILABLE_MECHANISMS,
  PROP_HAS_INITIAL_DATA,
  PROP_CAN_TRY_AGAIN,
  PROP_SASL_STATUS,
  PROP_SASL_ERROR,
  PROP_SASL_ERROR_DETAILS,
  PROP_AUTHORIZATION_IDENTITY,
  PROP_DEFAULT_USERNAME,
  PROP_DEFAULT_REALM,
  PROP_MAY_SAVE_RESPONSE,

  LAST_PROPERTY,
};

/* signals */
enum
{
  FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _TpBasePasswordChannelPrivate
{
  TpSASLStatus sasl_status;
  gchar *sasl_error;
  /* a{sv} */
  GHashTable *sasl_error_details;

  gchar *authorization_identity;
  gchar *default_username;
  gchar *default_realm;

  GString *password;

  gboolean may_save_response;
};

static void
tp_base_password_channel_init (TpBasePasswordChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_PASSWORD_CHANNEL, TpBasePasswordChannelPrivate);
}

static void
tp_base_password_channel_constructed (GObject *obj)
{
  TpBasePasswordChannel *chan = TP_BASE_PASSWORD_CHANNEL (obj);
  TpBasePasswordChannelPrivate *priv = chan->priv;
  TpBaseConnection *base_conn = tp_base_channel_get_connection (
      TP_BASE_CHANNEL (obj));
  TpHandleRepoIface *contact_handles = tp_base_connection_get_handles (
      base_conn, TP_ENTITY_TYPE_CONTACT);
  GDBusObjectSkeleton *skel = G_DBUS_OBJECT_SKELETON (chan);
  GDBusInterfaceSkeleton *iface;

  if (((GObjectClass *) tp_base_password_channel_parent_class)->constructed != NULL)
    ((GObjectClass *) tp_base_password_channel_parent_class)->constructed (obj);

  priv->sasl_error = g_strdup ("");
  priv->sasl_error_details = tp_asv_new (NULL, NULL);

  priv->authorization_identity = g_strdup (
      tp_handle_inspect (contact_handles,
        tp_base_connection_get_self_handle (base_conn)));
  priv->default_username = g_strdup (priv->authorization_identity);
  priv->default_realm = g_strdup ("");

  iface = tp_svc_interface_skeleton_new (skel,
      TP_TYPE_SVC_CHANNEL_TYPE_SERVER_AUTHENTICATION1);
  g_dbus_object_skeleton_add_interface (skel, iface);
  g_object_unref (iface);

  iface = tp_svc_interface_skeleton_new (skel,
      TP_TYPE_SVC_CHANNEL_INTERFACE_SASL_AUTHENTICATION1);
  g_dbus_object_skeleton_add_interface (skel, iface);
  g_object_unref (iface);
}

static void
tp_base_password_channel_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBasePasswordChannel *chan = TP_BASE_PASSWORD_CHANNEL (object);
  TpBasePasswordChannelPrivate *priv = chan->priv;

  switch (property_id)
    {
    case PROP_MAY_SAVE_RESPONSE:
      priv->may_save_response = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

enum {
    DBUSPROP_0,
    DBUSPROP_AUTHENTICATION_METHOD,
    DBUSPROP_AVAILABLE_MECHANISMS,
    DBUSPROP_HAS_INITIAL_DATA,
    DBUSPROP_CAN_TRY_AGAIN,
    DBUSPROP_SASL_STATUS,
    DBUSPROP_SASL_ERROR,
    DBUSPROP_SASL_ERROR_DETAILS,
    DBUSPROP_AUTHORIZATION_IDENTITY,
    DBUSPROP_DEFAULT_USERNAME,
    DBUSPROP_DEFAULT_REALM,
    DBUSPROP_MAY_SAVE_RESPONSE,
    N_DBUSPROPS
};

static void
tp_base_password_channel_get_sasl_property (GObject *object,
    GQuark iface,
    GQuark name,
    GValue *value,
    gpointer getter_data)
{
  TpBasePasswordChannel *chan = TP_BASE_PASSWORD_CHANNEL (object);
  TpBasePasswordChannelPrivate *priv = chan->priv;

  switch (GPOINTER_TO_UINT (getter_data))
    {
    case DBUSPROP_AUTHENTICATION_METHOD:
      g_value_set_static_string (value,
          TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1);
      break;
    case DBUSPROP_AVAILABLE_MECHANISMS:
      g_value_set_boxed (value,
          tp_base_password_channel_available_mechanisms);
      break;
    case DBUSPROP_HAS_INITIAL_DATA:
      g_value_set_boolean (value, TRUE);
      break;
    case DBUSPROP_CAN_TRY_AGAIN:
      g_value_set_boolean (value, FALSE);
      break;
    case DBUSPROP_SASL_STATUS:
      g_value_set_uint (value, priv->sasl_status);
      break;
    case DBUSPROP_SASL_ERROR:
      g_value_set_string (value, priv->sasl_error);
      break;
    case DBUSPROP_SASL_ERROR_DETAILS:
      g_value_set_boxed (value, priv->sasl_error_details);
      break;
    case DBUSPROP_AUTHORIZATION_IDENTITY:
      g_value_set_string (value, priv->authorization_identity);
      break;
    case DBUSPROP_DEFAULT_USERNAME:
      g_value_set_string (value, priv->default_username);
      break;
    case DBUSPROP_DEFAULT_REALM:
      g_value_set_string (value, priv->default_realm);
      break;
    case DBUSPROP_MAY_SAVE_RESPONSE:
      g_value_set_boolean (value, priv->may_save_response);
      break;
    default:
      g_return_if_reached ();
      break;
  }
}

static void
tp_base_password_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBasePasswordChannel *chan = TP_BASE_PASSWORD_CHANNEL (object);
  TpBasePasswordChannelPrivate *priv = chan->priv;

  switch (property_id)
    {
    case PROP_MAY_SAVE_RESPONSE:
      g_value_set_boolean (value, priv->may_save_response);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void tp_base_password_channel_finalize (GObject *object);
static void tp_base_password_channel_close (TpBaseChannel *base);
static void tp_base_password_channel_fill_immutable_properties (TpBaseChannel *chan,
    GHashTable *properties);

static void
tp_base_password_channel_class_init (TpBasePasswordChannelClass *tp_base_password_channel_class)
{
  TpBaseChannelClass *chan_class = TP_BASE_CHANNEL_CLASS (
      tp_base_password_channel_class);

  static TpDBusPropertiesMixinPropImpl server_base_password_props[] = {
    { "AuthenticationMethod",
      GUINT_TO_POINTER (DBUSPROP_AUTHENTICATION_METHOD) },
    { NULL }
  };
  static TpDBusPropertiesMixinPropImpl sasl_auth_props[] = {
    { "AvailableMechanisms", GUINT_TO_POINTER (DBUSPROP_AVAILABLE_MECHANISMS) },
    { "HasInitialData", GUINT_TO_POINTER (DBUSPROP_HAS_INITIAL_DATA) },
    { "CanTryAgain", GUINT_TO_POINTER (DBUSPROP_CAN_TRY_AGAIN) },
    { "SASLStatus", GUINT_TO_POINTER (DBUSPROP_SASL_STATUS) },
    { "SASLError", GUINT_TO_POINTER (DBUSPROP_SASL_ERROR) },
    { "SASLErrorDetails", GUINT_TO_POINTER (DBUSPROP_SASL_ERROR_DETAILS) },
    { "AuthorizationIdentity",
      GUINT_TO_POINTER (DBUSPROP_AUTHORIZATION_IDENTITY) },
    { "DefaultUsername", GUINT_TO_POINTER (DBUSPROP_DEFAULT_USERNAME) },
    { "DefaultRealm", GUINT_TO_POINTER (DBUSPROP_DEFAULT_REALM) },
    { "MaySaveResponse", GUINT_TO_POINTER (DBUSPROP_MAY_SAVE_RESPONSE) },
    { NULL }
  };
  GObjectClass *object_class = G_OBJECT_CLASS (tp_base_password_channel_class);
  GParamSpec *param_spec;

  g_type_class_add_private (tp_base_password_channel_class,
      sizeof (TpBasePasswordChannelPrivate));

  object_class->constructed = tp_base_password_channel_constructed;
  object_class->set_property = tp_base_password_channel_set_property;
  object_class->get_property = tp_base_password_channel_get_property;
  object_class->finalize = tp_base_password_channel_finalize;

  chan_class->channel_type = TP_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION1;
  chan_class->target_entity_type = TP_ENTITY_TYPE_NONE;
  chan_class->close = tp_base_password_channel_close;
  chan_class->fill_immutable_properties =
    tp_base_password_channel_fill_immutable_properties;

  param_spec = g_param_spec_boolean ("may-save-response",
      "Whether the client may save the authentication response",
      "Whether the client may save the authentication response",
      TRUE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MAY_SAVE_RESPONSE,
      param_spec);

  /**
   * TpBasePasswordChannel::finished:
   * @password: the password provided by the user, or %NULL if the
   * authentication has been aborted
   * @domain: domain of a #GError indicating why the authentication has been
   * aborted, or 0
   * @code: error code of a GError indicating why the authentication has been
   * aborted, or 0
   * @message: a message associated with the error, or %NULL
   *
   * Emitted when either the password has been provided by the user or the
   * authentication has been aborted.
   *
   * Since: 0.13.15
   */
  signals[FINISHED] = g_signal_new ("finished",
      G_TYPE_FROM_CLASS (object_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE, 4,
      G_TYPE_GSTRING, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_TYPE_SERVER_AUTHENTICATION1,
      /* this only has one property so we recycle the getter function from
       * the SASL interface */
      tp_base_password_channel_get_sasl_property, NULL,
      server_base_password_props);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_INTERFACE_SASL_AUTHENTICATION1,
      tp_base_password_channel_get_sasl_property, NULL, sasl_auth_props);
}

static void
tp_base_password_channel_finalize (GObject *object)
{
  TpBasePasswordChannel *self = TP_BASE_PASSWORD_CHANNEL (object);
  TpBasePasswordChannelPrivate *priv = self->priv;

  tp_clear_pointer (&priv->sasl_error_details, g_hash_table_unref);
  tp_clear_pointer (&priv->sasl_error, g_free);
  tp_clear_pointer (&priv->authorization_identity, g_free);
  tp_clear_pointer (&priv->default_username, g_free);
  tp_clear_pointer (&priv->default_realm, g_free);

  if (priv->password != NULL)
    {
      g_string_free (priv->password, TRUE);
      priv->password = NULL;
    }

  if (G_OBJECT_CLASS (tp_base_password_channel_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (tp_base_password_channel_parent_class)->finalize (object);
}

static void
tp_base_password_channel_change_status (TpBasePasswordChannel *channel,
    TpSASLStatus new_status,
    const gchar *new_sasl_error)
{
  TpBasePasswordChannelPrivate *priv = channel->priv;

  priv->sasl_status = new_status;

  g_free (priv->sasl_error);
  priv->sasl_error = g_strdup (new_sasl_error);

  tp_svc_channel_interface_sasl_authentication1_emit_sasl_status_changed (
      channel, priv->sasl_status, priv->sasl_error, priv->sasl_error_details);
}

static void
tp_base_password_channel_close (TpBaseChannel *base)
{
  TpBasePasswordChannel *self = TP_BASE_PASSWORD_CHANNEL (base);
  TpBasePasswordChannelPrivate *priv = self->priv;

  DEBUG ("Called on %p", base);

  if (tp_base_channel_is_destroyed (base))
    return;

  if (priv->sasl_status != TP_SASL_STATUS_SUCCEEDED
      && priv->sasl_status != TP_SASL_STATUS_SERVER_FAILED
      && priv->sasl_status != TP_SASL_STATUS_CLIENT_FAILED)
    {
      tp_base_password_channel_change_status (self,
          TP_SASL_STATUS_CLIENT_FAILED, TP_ERROR_STR_CANCELLED);

      g_signal_emit (self, signals[FINISHED], 0, NULL, TP_ERROR,
          TP_ERROR_CANCELLED, "BasePassword channel was closed");
    }

  DEBUG ("Closing channel");
  tp_base_channel_destroyed (base);
}

static void
tp_base_password_channel_fill_immutable_properties (TpBaseChannel *chan,
    GHashTable *properties)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (
      tp_base_password_channel_parent_class);

  klass->fill_immutable_properties (chan, properties);

  tp_dbus_properties_mixin_fill_properties_hash (
      G_OBJECT (chan), properties,
      TP_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION1, "AuthenticationMethod",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "AvailableMechanisms",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "HasInitialData",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "CanTryAgain",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "AuthorizationIdentity",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "DefaultUsername",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "DefaultRealm",
      TP_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION1, "MaySaveResponse",
      NULL);
}

static void
tp_base_password_channel_start_mechanism_with_data (
    TpSvcChannelInterfaceSASLAuthentication1 *self,
    const gchar *mechanism,
    const GArray *initial_data,
    GDBusMethodInvocation *context)
{
  TpBasePasswordChannel *channel = TP_BASE_PASSWORD_CHANNEL (self);
  TpBasePasswordChannelPrivate *priv = channel->priv;
  GError *error = NULL;

  if (!tp_strv_contains (
          tp_base_password_channel_available_mechanisms, mechanism))
    {
      error = g_error_new (TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "The mechanism %s is not implemented", mechanism);
      goto error;
    }

  if (priv->sasl_status != TP_SASL_STATUS_NOT_STARTED)
    {
      error = g_error_new (TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "StartMechanismWithData cannot be called in state %u",
          priv->sasl_status);
      goto error;
    }

  if (initial_data->len == 0)
    {
      error = g_error_new (TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "No initial data given");
      goto error;
    }

  tp_base_password_channel_change_status (channel,
      TP_SASL_STATUS_IN_PROGRESS, "");

  priv->password = g_string_new_len (
      initial_data->data, initial_data->len);

  tp_base_password_channel_change_status (channel,
      TP_SASL_STATUS_SERVER_SUCCEEDED, "");

  tp_svc_channel_interface_sasl_authentication1_return_from_start_mechanism_with_data (
      context);

  return;

error:
  DEBUG ("%s", error->message);
  g_dbus_method_invocation_return_gerror (context, error);
  g_error_free (error);
}

static void
tp_base_password_channel_accept_sasl (
    TpSvcChannelInterfaceSASLAuthentication1 *self,
    GDBusMethodInvocation *context)
{
  TpBasePasswordChannel *channel = TP_BASE_PASSWORD_CHANNEL (self);
  TpBasePasswordChannelPrivate *priv = channel->priv;

  if (priv->sasl_status != TP_SASL_STATUS_SERVER_SUCCEEDED)
    {
      GError *error = g_error_new (TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "AcceptSASL cannot be called in state %u", priv->sasl_status);
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
      return;
    }

  tp_base_password_channel_change_status (channel,
      TP_SASL_STATUS_SUCCEEDED, "");

  g_signal_emit (channel, signals[FINISHED], 0, priv->password, 0, 0, NULL);

  tp_svc_channel_interface_sasl_authentication1_return_from_accept_sasl (
      context);
}

static void
tp_base_password_channel_abort_sasl (
    TpSvcChannelInterfaceSASLAuthentication1 *self,
    TpSASLAbortReason reason,
    const gchar *debug_message,
    GDBusMethodInvocation *context)
{
  TpBasePasswordChannel *channel = TP_BASE_PASSWORD_CHANNEL (self);
  TpBasePasswordChannelPrivate *priv = channel->priv;

  if (priv->sasl_status == TP_SASL_STATUS_SERVER_SUCCEEDED
      || priv->sasl_status == TP_SASL_STATUS_CLIENT_ACCEPTED)
    {
      GError *error = g_error_new (TP_ERROR, TP_ERROR_NOT_AVAILABLE,
          "AbortSASL cannot be called in state %u", priv->sasl_status);
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
      return;
    }
  if (priv->sasl_status != TP_SASL_STATUS_CLIENT_FAILED
      && priv->sasl_status != TP_SASL_STATUS_SERVER_FAILED)
    {
      DEBUG ("Aborting SASL because: %s", debug_message);

      /* we don't care about the reason; it'll always be User_Abort
       * anyway */

      tp_asv_set_string (priv->sasl_error_details, "debug-message",
          debug_message);

      tp_base_password_channel_change_status (channel,
          TP_SASL_STATUS_CLIENT_FAILED, TP_ERROR_STR_CANCELLED);

      g_signal_emit (channel, signals[FINISHED], 0, NULL, TP_ERROR,
          TP_ERROR_CANCELLED, "AbortSASL was called");
    }

  tp_svc_channel_interface_sasl_authentication1_return_from_abort_sasl (
      context);
}

static void
sasl_auth_iface_init (gpointer g_iface,
    gpointer iface_data)
{
#define IMPLEMENT(x) tp_svc_channel_interface_sasl_authentication1_implement_##x (\
    g_iface, tp_base_password_channel_##x)
  IMPLEMENT(start_mechanism_with_data);
  IMPLEMENT(accept_sasl);
  IMPLEMENT(abort_sasl);
#undef IMPLEMENT
}

