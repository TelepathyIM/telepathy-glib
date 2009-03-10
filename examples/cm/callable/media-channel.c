/*
 * media-channel.c - an example 1-1 streamed media call.
 *
 * For simplicity, this channel emulates a device with its own
 * audio/video user interface, like a video-equipped form of the phones
 * manipulated by telepathy-snom or gnome-phone-manager.
 *
 * As a result, this channel does not have the MediaSignalling interface, and
 * clients should not attempt to do their own streaming using
 * telepathy-farsight, telepathy-stream-engine or maemo-stream-engine.
 *
 * In practice, nearly all connection managers also have the MediaSignalling
 * interface on their streamed media channels. Usage for those CMs is the
 * same, except that whichever client is the primary handler for the channel
 * should also hand the channel over to telepathy-farsight or
 * telepathy-stream-engine to implement the actual streaming.
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "media-channel.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

static void media_iface_init (gpointer iface, gpointer data);
static void channel_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ExampleCallableMediaChannel,
    example_callable_media_channel,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_STREAMED_MEDIA,
      media_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
      tp_group_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_EXPORTABLE_CHANNEL, NULL))

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_TARGET_ID,
  PROP_REQUESTED,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_CONNECTION,
  PROP_INTERFACES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_PROPERTIES,
  N_PROPS
};

struct _ExampleCallableMediaChannelPrivate
{
  TpBaseConnection *conn;
  gchar *object_path;
  TpHandle handle;
  TpHandle initiator;

  guint next_stream_id;

  /* These are really booleans, but gboolean is signed. Thanks, GLib */
  unsigned locally_requested:1;
  unsigned closed:1;
  unsigned disposed:1;
};

static const char * example_callable_media_channel_interfaces[] = {
    TP_IFACE_CHANNEL_INTERFACE_GROUP,
    NULL
};

static void
example_callable_media_channel_init (ExampleCallableMediaChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALLABLE_MEDIA_CHANNEL,
      ExampleCallableMediaChannelPrivate);

  self->priv->next_stream_id = 1;
}

static void
constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      ((GObjectClass *) example_callable_media_channel_parent_class)->constructed;
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (object);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles
      (self->priv->conn, TP_HANDLE_TYPE_CONTACT);
  DBusGConnection *bus;

  if (chain_up != NULL)
    chain_up (object);

  tp_handle_ref (contact_repo, self->priv->handle);
  tp_handle_ref (contact_repo, self->priv->initiator);

  bus = tp_get_bus ();
  dbus_g_connection_register_g_object (bus, self->priv->object_path, object);

  tp_group_mixin_init (object,
      G_STRUCT_OFFSET (ExampleCallableMediaChannel, group),
      contact_repo, self->priv->conn->self_handle);

  /* FIXME: correct flags */
  tp_group_mixin_change_flags (object,
      TP_CHANNEL_GROUP_FLAG_CHANNEL_SPECIFIC_HANDLES |
      TP_CHANNEL_GROUP_FLAG_PROPERTIES,
      0);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->priv->object_path);
      break;

    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);
      break;

    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
      break;

    case PROP_HANDLE:
      g_value_set_uint (value, self->priv->handle);
      break;

    case PROP_TARGET_ID:
        {
          TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
              self->priv->conn, TP_HANDLE_TYPE_CONTACT);

          g_value_set_string (value,
              tp_handle_inspect (contact_repo, self->priv->handle));
        }
      break;

    case PROP_REQUESTED:
      g_value_set_boolean (value, self->priv->locally_requested);
      break;

    case PROP_INITIATOR_HANDLE:
      g_value_set_uint (value, self->priv->initiator);
      break;

    case PROP_INITIATOR_ID:
        {
          TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
              self->priv->conn, TP_HANDLE_TYPE_CONTACT);

          g_value_set_string (value,
              tp_handle_inspect (contact_repo, self->priv->initiator));
        }
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->conn);
      break;

    case PROP_INTERFACES:
      g_value_set_boxed (value, example_callable_media_channel_interfaces);
      break;

    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean (value, self->priv->closed);
      break;

    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed (value,
          tp_dbus_properties_mixin_make_properties_hash (object,
              TP_IFACE_CHANNEL, "ChannelType",
              TP_IFACE_CHANNEL, "TargetHandleType",
              TP_IFACE_CHANNEL, "TargetHandle",
              TP_IFACE_CHANNEL, "TargetID",
              TP_IFACE_CHANNEL, "InitiatorHandle",
              TP_IFACE_CHANNEL, "InitiatorID",
              TP_IFACE_CHANNEL, "Requested",
              TP_IFACE_CHANNEL, "Interfaces",
              NULL));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *pspec)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_assert (self->priv->object_path == NULL);
      self->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_HANDLE:
      /* we don't ref it here because we don't necessarily have access to the
       * contact repo yet - instead we ref it in the constructor.
       */
      self->priv->handle = g_value_get_uint (value);
      break;

    case PROP_INITIATOR_HANDLE:
      /* likewise */
      self->priv->initiator = g_value_get_uint (value);
      break;

    case PROP_REQUESTED:
      self->priv->locally_requested = g_value_get_boolean (value);
      break;

    case PROP_HANDLE_TYPE:
    case PROP_CHANNEL_TYPE:
      /* these properties are writable in the interface, but not actually
       * meaningfully changable on this channel, so we do nothing */
      break;

    case PROP_CONNECTION:
      self->priv->conn = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
example_callable_media_channel_close (ExampleCallableMediaChannel *self)
{
  if (!self->priv->closed)
    {
      self->priv->closed = TRUE;
      tp_svc_channel_emit_closed (self);
    }
}

static void
dispose (GObject *object)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (object);

  if (self->priv->disposed)
    return;

  self->priv->disposed = TRUE;

  example_callable_media_channel_close (self);

  ((GObjectClass *) example_callable_media_channel_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (object);
  TpHandleRepoIface *contact_handles = tp_base_connection_get_handles
      (self->priv->conn, TP_HANDLE_TYPE_CONTACT);

  tp_handle_unref (contact_handles, self->priv->handle);
  tp_handle_unref (contact_handles, self->priv->initiator);

  g_free (self->priv->object_path);

  tp_group_mixin_finalize (object);

  ((GObjectClass *) example_callable_media_channel_parent_class)->finalize (object);
}

static gboolean
add_member (GObject *object,
            TpHandle member,
            const gchar *message,
            GError **error)
{
  return TRUE;
}

static gboolean
remove_member_with_reason (GObject *object,
                           TpHandle member,
                           const gchar *message,
                           guint reason,
                           GError **error)
{
  return TRUE;
}

static void
example_callable_media_channel_class_init (ExampleCallableMediaChannelClass *klass)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "TargetHandleType", "handle-type", NULL },
      { "TargetHandle", "handle", NULL },
      { "ChannelType", "channel-type", NULL },
      { "Interfaces", "interfaces", NULL },
      { "TargetID", "target-id", NULL },
      { "Requested", "requested", NULL },
      { "InitiatorHandle", "initiator-handle", NULL },
      { "InitiatorID", "initiator-id", NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { TP_IFACE_CHANNEL,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        channel_props,
      },
      { NULL }
  };
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass,
      sizeof (ExampleCallableMediaChannelPrivate));

  object_class->constructed = constructed;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  g_object_class_override_property (object_class, PROP_OBJECT_PATH,
      "object-path");
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");
  g_object_class_override_property (object_class, PROP_HANDLE, "handle");

  g_object_class_override_property (object_class, PROP_CHANNEL_DESTROYED,
      "channel-destroyed");
  g_object_class_override_property (object_class, PROP_CHANNEL_PROPERTIES,
      "channel-properties");

  param_spec = g_param_spec_object ("connection", "TpBaseConnection object",
      "Connection object that owns this channel",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "Additional Channel.Interface.* interfaces",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("target-id", "Peer's ID",
      "The string obtained by inspecting the target handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET_ID, param_spec);

  param_spec = g_param_spec_uint ("initiator-handle", "Initiator's handle",
      "The contact who initiated the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
      param_spec);

  param_spec = g_param_spec_string ("initiator-id", "Initiator's ID",
      "The string obtained by inspecting the initiator-handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_ID,
      param_spec);

  param_spec = g_param_spec_boolean ("requested", "Requested?",
      "True if this channel was requested by the local user",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED, param_spec);

  klass->dbus_properties_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleCallableMediaChannelClass,
        dbus_properties_class));

  tp_group_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleCallableMediaChannelClass, group_class),
      add_member,
      NULL);
  tp_group_mixin_class_set_remove_with_reason_func (object_class,
      remove_member_with_reason);
  tp_group_mixin_init_dbus_properties (object_class);
}

static void
channel_close (TpSvcChannel *iface,
               DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);

  example_callable_media_channel_close (self);
  tp_svc_channel_return_from_close (context);
}

static void
channel_get_channel_type (TpSvcChannel *iface G_GNUC_UNUSED,
                          DBusGMethodInvocation *context)
{
  tp_svc_channel_return_from_get_channel_type (context,
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);
}

static void
channel_get_handle (TpSvcChannel *iface,
                    DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);

  tp_svc_channel_return_from_get_handle (context, TP_HANDLE_TYPE_CONTACT,
      self->priv->handle);
}

static void
channel_get_interfaces (TpSvcChannel *iface G_GNUC_UNUSED,
                        DBusGMethodInvocation *context)
{
  tp_svc_channel_return_from_get_interfaces (context,
      example_callable_media_channel_interfaces);
}

static void
channel_iface_init (gpointer iface,
                    gpointer data)
{
  TpSvcChannelClass *klass = iface;

#define IMPLEMENT(x) tp_svc_channel_implement_##x (klass, channel_##x)
  IMPLEMENT (close);
  IMPLEMENT (get_channel_type);
  IMPLEMENT (get_handle);
  IMPLEMENT (get_interfaces);
#undef IMPLEMENT
}

static void
media_list_streams (TpSvcChannelTypeStreamedMedia *iface,
                    DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);
  GPtrArray *array = g_ptr_array_sized_new (0);

  /* FIXME */
  (void) self;

  tp_svc_channel_type_streamed_media_return_from_list_streams (context,
      array);
  g_ptr_array_free (array, TRUE);
}

static void
media_remove_streams (TpSvcChannelTypeStreamedMedia *iface,
                      const GArray *stream_ids,
                      DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);

  /* FIXME */
  (void) self;

  tp_svc_channel_type_streamed_media_return_from_remove_streams (context);
}

static void
media_request_stream_direction (TpSvcChannelTypeStreamedMedia *iface,
                                guint stream_id,
                                guint stream_direction,
                                DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);

  /* FIXME */
  (void) self;

  tp_svc_channel_type_streamed_media_return_from_request_stream_direction (
      context);
}

static void
media_request_streams (TpSvcChannelTypeStreamedMedia *iface,
                       guint contact_handle,
                       const GArray *media_types,
                       DBusGMethodInvocation *context)
{
  ExampleCallableMediaChannel *self = EXAMPLE_CALLABLE_MEDIA_CHANNEL (iface);
  GPtrArray *array = g_ptr_array_sized_new (0);

  /* FIXME */
  (void) self;

  tp_svc_channel_type_streamed_media_return_from_request_streams (context,
      array);
  g_ptr_array_free (array, TRUE);
}

static void
media_iface_init (gpointer iface,
                  gpointer data)
{
  TpSvcChannelTypeStreamedMediaClass *klass = iface;

#define IMPLEMENT(x) \
  tp_svc_channel_type_streamed_media_implement_##x (klass, media_##x)
  IMPLEMENT (list_streams);
  IMPLEMENT (remove_streams);
  IMPLEMENT (request_stream_direction);
  IMPLEMENT (request_streams);
#undef IMPLEMENT
}
