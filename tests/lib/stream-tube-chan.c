/*
 * stream-tube-chan.c - Simple stream tube channel
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "stream-tube-chan.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/svc-channel.h>

#include <gio/gunixsocketaddress.h>
#include <gio/gunixconnection.h>

#include <stdio.h>

enum
{
  PROP_SERVICE = 1,
  PROP_SUPPORTED_SOCKET_TYPES,
  PROP_PARAMETERS,
  PROP_STATE,
};

enum
{
  SIG_INCOMING_CONNECTION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };


struct _TpTestsStreamTubeChannelPrivate {
    TpTubeChannelState state;

    /* Accepting side */
    GSocketListener *listener;

    /* Offering side */
    TpSocketAddressType address_type;
    GValue *address;
    TpSocketAccessControl access_control;
};

static void
destroy_socket_control_list (gpointer data)
{
  GArray *tab = data;
  g_array_free (tab, TRUE);
}

static GHashTable *
get_supported_socket_types (void)
{
  GHashTable *ret;
  TpSocketAccessControl access_control;
  GArray *unix_tab;

  ret = g_hash_table_new_full (NULL, NULL, NULL, destroy_socket_control_list);

  /* Socket_Address_Type_Unix */
  unix_tab = g_array_sized_new (FALSE, FALSE, sizeof (TpSocketAccessControl),
      1);
  access_control = TP_SOCKET_ACCESS_CONTROL_LOCALHOST;
  g_array_append_val (unix_tab, access_control);

  g_hash_table_insert (ret, GUINT_TO_POINTER (TP_SOCKET_ADDRESS_TYPE_UNIX),
      unix_tab);

  return ret;
}

static void
tp_tests_stream_tube_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTestsStreamTubeChannel *self = (TpTestsStreamTubeChannel *) object;

  switch (property_id)
    {
      case PROP_SERVICE:
        g_value_set_string (value, "test-service");
        break;

      case PROP_SUPPORTED_SOCKET_TYPES:
        g_value_take_boxed (value,
            get_supported_socket_types ());
        break;

      case PROP_PARAMETERS:
        g_value_take_boxed (value, tp_asv_new (
              "badger", G_TYPE_UINT, 42,
              NULL));
        break;

      case PROP_STATE:
        g_value_set_uint (value, self->priv->state);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void stream_tube_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (TpTestsStreamTubeChannel,
    tp_tests_stream_tube_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_STREAM_TUBE,
      stream_tube_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_TUBE,
      NULL);
    )

/* type definition stuff */

static const char * tp_tests_stream_tube_channel_interfaces[] = {
    TP_IFACE_CHANNEL_INTERFACE_TUBE,
    NULL
};

static void
tp_tests_stream_tube_channel_init (TpTestsStreamTubeChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
      TP_TESTS_TYPE_STREAM_TUBE_CHANNEL, TpTestsStreamTubeChannelPrivate);
}

static GObject *
constructor (GType type,
             guint n_props,
             GObjectConstructParam *props)
{
  GObject *object =
      G_OBJECT_CLASS (tp_tests_stream_tube_channel_parent_class)->constructor (
          type, n_props, props);
  TpTestsStreamTubeChannel *self = TP_TESTS_STREAM_TUBE_CHANNEL (object);

  if (tp_base_channel_is_requested (TP_BASE_CHANNEL (self)))
    self->priv->state = TP_TUBE_CHANNEL_STATE_NOT_OFFERED;
  else
    self->priv->state = TP_TUBE_CHANNEL_STATE_LOCAL_PENDING;

  tp_base_channel_register (TP_BASE_CHANNEL (self));

  return object;
}

static void
dispose (GObject *object)
{
  TpTestsStreamTubeChannel *self = (TpTestsStreamTubeChannel *) object;

  tp_clear_object (&self->priv->listener);
  tp_clear_pointer (&self->priv->address, tp_g_value_slice_free);

  ((GObjectClass *) tp_tests_stream_tube_channel_parent_class)->dispose (
    object);
}

static void
channel_close (TpBaseChannel *channel)
{
  tp_base_channel_destroyed (channel);
}

static void
fill_immutable_properties (TpBaseChannel *chan,
    GHashTable *properties)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (
      tp_tests_stream_tube_channel_parent_class);

  klass->fill_immutable_properties (chan, properties);

  tp_dbus_properties_mixin_fill_properties_hash (
      G_OBJECT (chan), properties,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE, "Service",
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE, "SupportedSocketTypes",
      NULL);

  if (!tp_base_channel_is_requested (chan))
    {
      /* Parameters is immutable only for incoming tubes */
      tp_dbus_properties_mixin_fill_properties_hash (
          G_OBJECT (chan), properties,
          TP_IFACE_CHANNEL_INTERFACE_TUBE, "Parameters",
          NULL);
    }
}

static void
tp_tests_stream_tube_channel_class_init (TpTestsStreamTubeChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseChannelClass *base_class = TP_BASE_CHANNEL_CLASS (klass);
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl stream_tube_props[] = {
      { "Service", "service", NULL, },
      { "SupportedSocketTypes", "supported-socket-types", NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinPropImpl tube_props[] = {
      { "Parameters", "parameters", NULL, },
      { "State", "state", NULL, },
      { NULL }
  };

  object_class->constructor = constructor;
  object_class->get_property = tp_tests_stream_tube_channel_get_property;
  object_class->dispose = dispose;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_STREAM_TUBE;
  base_class->target_handle_type = TP_HANDLE_TYPE_CONTACT;
  base_class->interfaces = tp_tests_stream_tube_channel_interfaces;
  base_class->close = channel_close;
  base_class->fill_immutable_properties = fill_immutable_properties;

  tp_text_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpTestsStreamTubeChannelClass, text_class));

  param_spec = g_param_spec_string ("service", "service name",
      "the service associated with this tube object.",
       "",
       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SERVICE, param_spec);

  param_spec = g_param_spec_boxed (
      "supported-socket-types", "Supported socket types",
      "GHashTable containing supported socket types.",
      TP_HASH_TYPE_SUPPORTED_SOCKET_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SUPPORTED_SOCKET_TYPES,
      param_spec);

  param_spec = g_param_spec_boxed (
      "parameters", "Parameters",
      "parameters of the tube",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PARAMETERS,
      param_spec);

  param_spec = g_param_spec_uint (
      "state", "TpTubeState",
      "state of the tube",
      0, NUM_TP_TUBE_CHANNEL_STATES - 1, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STATE,
      param_spec);

  signals[SIG_INCOMING_CONNECTION] = g_signal_new ("incoming-connection",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, G_TYPE_IO_STREAM);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_TYPE_STREAM_TUBE,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      stream_tube_props);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_INTERFACE_TUBE,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      tube_props);

  g_type_class_add_private (object_class,
      sizeof (TpTestsStreamTubeChannelPrivate));
}

static void
change_state (TpTestsStreamTubeChannel *self,
  TpTubeChannelState state)
{
  self->priv->state = state;

  tp_svc_channel_interface_tube_emit_tube_channel_state_changed (self, state);
}

static void
stream_tube_offer (TpSvcChannelTypeStreamTube *iface,
    guint address_type,
    const GValue *address,
    guint access_control,
    GHashTable *parameters,
    DBusGMethodInvocation *context)
{
  TpTestsStreamTubeChannel *self = (TpTestsStreamTubeChannel *) iface;
  GError *error = NULL;

  if (self->priv->state != TP_TUBE_CHANNEL_STATE_NOT_OFFERED)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Tube is not in the not offered state");
      goto fail;
    }

  self->priv->address_type = address_type;
  self->priv->address = tp_g_value_slice_dup (address);
  self->priv->access_control = access_control;

  change_state (self, TP_TUBE_CHANNEL_STATE_REMOTE_PENDING);

  tp_svc_channel_type_stream_tube_return_from_offer (context);
  return;

fail:
  dbus_g_method_return_error (context, error);
  g_error_free (error);
}

static void
listener_accept_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpTestsStreamTubeChannel *self = user_data;
  GError *error = NULL;
  GSocketConnection *connection;

  connection = g_socket_listener_accept_finish (G_SOCKET_LISTENER (source),
      result, NULL, &error);
  g_assert_no_error (error);

  g_signal_emit (self, signals[SIG_INCOMING_CONNECTION], 0, connection);

  g_object_unref (connection);
}

static GValue *
create_local_socket (TpTestsStreamTubeChannel *self,
    TpSocketAddressType address_type,
    TpSocketAccessControl access_control,
    const GValue *access_control_param,
    GError **error)
{
  gboolean success;
  GSocketAddress *address;
  GValue *address_gvalue;

  if (address_type != TP_SOCKET_ADDRESS_TYPE_UNIX)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Unsupported address type");
      return NULL;
    }

  if (access_control != TP_SOCKET_ACCESS_CONTROL_LOCALHOST)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Unsupported access control");
      return NULL;
    }

  self->priv->listener = g_socket_listener_new ();
  address = g_unix_socket_address_new (tmpnam (NULL));

  success = g_socket_listener_add_address (self->priv->listener,
      address, G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_DEFAULT,
      NULL, NULL, NULL);
  g_assert (success);

  g_socket_listener_accept_async (self->priv->listener, NULL,
      listener_accept_cb, self);

  address_gvalue =  tp_g_value_slice_new_bytes (
      g_unix_socket_address_get_path_len (G_UNIX_SOCKET_ADDRESS (address)),
      g_unix_socket_address_get_path (G_UNIX_SOCKET_ADDRESS (address)));

  g_object_unref (address);
  return address_gvalue;
}

static void
stream_tube_accept (TpSvcChannelTypeStreamTube *iface,
    TpSocketAddressType address_type,
    TpSocketAccessControl access_control,
    const GValue *access_control_param,
    DBusGMethodInvocation *context)
{
  TpTestsStreamTubeChannel *self = (TpTestsStreamTubeChannel *) iface;
  GError *error = NULL;
  GValue *address;

  if (self->priv->state != TP_TUBE_CHANNEL_STATE_LOCAL_PENDING)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Tube is not in the local pending state");
      goto fail;
    }

  address = create_local_socket (self, address_type, access_control,
      access_control_param, &error);
  if (address == NULL)
    goto fail;

  change_state (self, TP_TUBE_CHANNEL_STATE_OPEN);

  tp_svc_channel_type_stream_tube_return_from_accept (context, address);

  tp_g_value_slice_free (address);
  return;

fail:
  dbus_g_method_return_error (context, error);
  g_error_free (error);
}

static void
stream_tube_iface_init (gpointer iface,
    gpointer data)
{
  TpSvcChannelTypeStreamTubeClass *klass = iface;

#define IMPLEMENT(x) tp_svc_channel_type_stream_tube_implement_##x (klass, stream_tube_##x)
  IMPLEMENT(offer);
  IMPLEMENT(accept);
#undef IMPLEMENT
}
