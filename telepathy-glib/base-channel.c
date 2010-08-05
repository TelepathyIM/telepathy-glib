/*
 * base-channel.c - base class for Channel implementations
 *
 * Copyright (C) 2009 Collabora Ltd.
 * Copyright (C) 2009 Nokia Corporation
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
 * SECTION:base-channel
 * @title: TpBaseChannel
 * @short_description: base class for #TpExportableChannel implementations
 * @see_also: #TpSvcChannel
 *
 * This base class makes it easier to write #TpExportableChannel
 * implementations by implementing some of its properties, and defining other
 * relevant properties.
 *
 * Subclasses must implement the close() virtual function and either call
 * tp_base_channel_destroyed() or tp_base_channel_reopened() to indicate that
 * the channel will be re-spawned (NOTE: channels that support re-spawning
 * must also implement #TpSvcChannelInterfaceDestroyable). The default
 * implementation for #TpExportableChannel:channel-properties just includes the
 * immutable properties from the Channel interface; subclasses will almost
 * certainly want to override this to include other immutable properties.
 *
 * Subclasses should fill in #TpBaseChannelClass:channel_type,
 * #TpBaseChannelClass:target_type and #TpBaseChannelClass:interfaces;
 * if some instances of the channel need a different set of interfaces, they
 * may set #TpBaseChannel:interfaces to override the default set for the
 * class.
 *
 * Subclasses should ensure that #TpBaseChannel:object_path is not %NULL by
 * the time construction is finished (if it is not set by the object's creator,
 * they must fill it in themself); #TpBaseChannel will take care of freeing
 * it.
 *
 * Since: 0.11.12
 */

#include "config.h"
#include "base-channel.h"

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/debug-internal.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL

#include "debug.h"

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_TARGET_ID,
  PROP_REQUESTED,
  PROP_CONNECTION,
  PROP_INTERFACES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_PROPERTIES,
  LAST_PROPERTY
};

struct _TpBaseChannelPrivate
{
  TpBaseConnection *conn;

  char *object_path;
  const gchar **interfaces;

  TpHandle target;
  TpHandle initiator;

  gboolean requested;
  gboolean destroyed;

  gboolean dispose_has_run;
};

static void channel_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpBaseChannel, tp_base_channel,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_EXPORTABLE_CHANNEL, NULL);
    )

/**
 * tp_base_channel_register:
 * @chan: a channel
 *
 * Make the channel appear on the bus.  #TpExportableChannel:object-path must have been set
 * to a valid path, which must not already be in use as another object's path.
 */
void
tp_base_channel_register (TpBaseChannel *chan)
{
  TpDBusDaemon *bus = tp_base_connection_get_dbus_daemon (chan->priv->conn);

  g_assert (chan->priv->object_path != NULL);

  tp_dbus_daemon_register_object (bus, chan->priv->object_path, chan);
}

/**
 * tp_base_channel_destroyed:
 * @chan: a channel
 *
 * Called by subclasses to indicate that this channel was destroyed and can be
 * removed from the bus.  The "Closed" signal will be emitted and the
 * #TpExportableChannel:channel-destroyed property will be set.
 */
void
tp_base_channel_destroyed (TpBaseChannel *chan)
{
  TpDBusDaemon *bus = tp_base_connection_get_dbus_daemon (chan->priv->conn);

  chan->priv->destroyed = TRUE;
  tp_svc_channel_emit_closed (chan);

  tp_dbus_daemon_unregister_object (bus, chan);
}

/**
 * tp_base_channel_reopened:
 * @chan: a channel
 *
 * Called by subclasses to indicate that this channel was closed but was
 * re-opened due to pending messages.  The "Closed" signal will be emitted, but
 * the #TpExportableChannel:channel-destroyed property will not be set.  The
 * channel's #TpBaseChannel:initiator-handle property will be set to
 * @initiator.
 */
void
tp_base_channel_reopened (TpBaseChannel *chan)
{
  tp_svc_channel_emit_closed (chan);
}

static void
tp_base_channel_init (TpBaseChannel *self)
{
  TpBaseChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CHANNEL, TpBaseChannelPrivate);

  self->priv = priv;

}

static void
tp_base_channel_constructed (GObject *object)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (object);
  GObjectClass *parent_class = tp_base_channel_parent_class;
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseConnection *conn = (TpBaseConnection *) chan->priv->conn;
  TpHandleRepoIface *handles;

  if (parent_class->constructed != NULL)
    parent_class->constructed (object);

  if (chan->priv->interfaces == NULL)
    chan->priv->interfaces = klass->interfaces;

  if (klass->target_type != TP_HANDLE_TYPE_NONE)
    {
      handles = tp_base_connection_get_handles (conn, klass->target_type);
      g_assert (handles != NULL);
      g_assert (chan->priv->target != 0);
      tp_handle_ref (handles, chan->priv->target);
    }

  if (chan->priv->initiator != 0)
    {
      handles = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);
      g_assert (handles != NULL);
      tp_handle_ref (handles, chan->priv->initiator);
    }
}

static void
tp_base_channel_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, chan->priv->object_path);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value, klass->channel_type);
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, klass->target_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, chan->priv->target);
      break;
    case PROP_TARGET_ID:
      if (chan->priv->target != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, klass->target_type);

          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->target));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_INITIATOR_HANDLE:
      g_value_set_uint (value, chan->priv->initiator);
      break;
    case PROP_INITIATOR_ID:
      if (chan->priv->initiator != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, TP_HANDLE_TYPE_CONTACT);

          g_assert (chan->priv->initiator != 0);
          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->initiator));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_REQUESTED:
      g_value_set_boolean (value, (chan->priv->requested));
      break;
    case PROP_CONNECTION:
      g_value_set_object (value, chan->priv->conn);
      break;
    case PROP_INTERFACES:
      g_value_set_boxed (value, chan->priv->interfaces);
      break;
    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean (value, chan->priv->destroyed);
      break;
    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed (value,
          tp_dbus_properties_mixin_make_properties_hash (
              object,
              TP_IFACE_CHANNEL, "TargetHandle",
              TP_IFACE_CHANNEL, "TargetHandleType",
              TP_IFACE_CHANNEL, "ChannelType",
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
tp_base_channel_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_free (chan->priv->object_path);
      chan->priv->object_path = g_value_dup_string (value);
      break;
    case PROP_HANDLE:
      /* we don't ref it here because we don't necessarily have access to the
       * contact repo yet - instead we ref it in constructed.
       */
      chan->priv->target = g_value_get_uint (value);
      break;
    case PROP_INITIATOR_HANDLE:
      /* similarly we can't ref this yet */
      chan->priv->initiator = g_value_get_uint (value);
      break;
    case PROP_HANDLE_TYPE:
    case PROP_CHANNEL_TYPE:
      /* these properties are writable in the interface, but not actually
       * meaningfully changeable on this channel, so we do nothing */
      break;
    case PROP_CONNECTION:
      chan->priv->conn = g_value_dup_object (value);
      break;
    case PROP_REQUESTED:
      chan->priv->requested = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_channel_dispose (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);
  TpBaseChannelPrivate *priv = chan->priv;
  TpHandleRepoIface *handles;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (!priv->destroyed)
    {
      tp_base_channel_destroyed (chan);
    }

  if (priv->target != 0)
    {
      handles = tp_base_connection_get_handles (priv->conn,
                                                klass->target_type);
      tp_handle_unref (handles, priv->target);
      priv->target = 0;
    }

  if (priv->initiator != 0)
    {
      handles = tp_base_connection_get_handles (priv->conn,
                                                TP_HANDLE_TYPE_CONTACT);
      tp_handle_unref (handles, priv->initiator);
      priv->initiator = 0;
    }

  if (priv->conn != NULL)
    {
      g_object_unref (priv->conn);
      priv->conn = NULL;
    }

  if (G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose (object);
}

static void
tp_base_channel_finalize (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  g_free (chan->priv->object_path);

  G_OBJECT_CLASS (tp_base_channel_parent_class)->finalize (object);
}

static void
tp_base_channel_class_init (TpBaseChannelClass *tp_base_channel_class)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "TargetHandleType", "handle-type", NULL },
      { "TargetHandle", "handle", NULL },
      { "TargetID", "target-id", NULL },
      { "ChannelType", "channel-type", NULL },
      { "Interfaces", "interfaces", NULL },
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
  GObjectClass *object_class = G_OBJECT_CLASS (tp_base_channel_class);
  GParamSpec *param_spec;

  g_type_class_add_private (tp_base_channel_class,
      sizeof (TpBaseChannelPrivate));

  object_class->constructed = tp_base_channel_constructed;

  object_class->get_property = tp_base_channel_get_property;
  object_class->set_property = tp_base_channel_set_property;

  object_class->dispose = tp_base_channel_dispose;
  object_class->finalize = tp_base_channel_finalize;

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
      "Connection object that owns this channel.",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "Additional Channel.Interface.* interfaces",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("target-id", "Target's identifier",
      "The string obtained by inspecting the target handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET_ID, param_spec);

  param_spec = g_param_spec_boolean ("requested", "Requested?",
      "True if this channel was requested by the local user",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED, param_spec);

  param_spec = g_param_spec_uint ("initiator-handle", "Initiator's handle",
      "The contact who initiated the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
      param_spec);

  param_spec = g_param_spec_string ("initiator-id", "Initiator's bare JID",
      "The string obtained by inspecting the initiator-handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_ID,
      param_spec);

  tp_base_channel_class->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseChannelClass, dbus_props_class));
}

static void
tp_base_channel_get_channel_type (TpSvcChannel *iface,
                                  DBusGMethodInvocation *context)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (iface);

  tp_svc_channel_return_from_get_channel_type (context, klass->channel_type);
}

static void
tp_base_channel_get_handle (TpSvcChannel *iface,
                            DBusGMethodInvocation *context)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (iface);
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);

  tp_svc_channel_return_from_get_handle (context, klass->target_type,
      chan->priv->target);
}

static void
tp_base_channel_get_interfaces (TpSvcChannel *iface,
                                DBusGMethodInvocation *context)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);

  tp_svc_channel_return_from_get_interfaces (context, chan->priv->interfaces);
}

static void
tp_base_channel_close (TpSvcChannel *iface,
                       DBusGMethodInvocation *context)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  klass->close (chan);

  tp_svc_channel_return_from_close (context);
}

static void
channel_iface_init (gpointer g_iface,
                    gpointer iface_data)
{
  TpSvcChannelClass *klass = (TpSvcChannelClass *) g_iface;

#define IMPLEMENT(x) tp_svc_channel_implement_##x (\
    klass, tp_base_channel_##x)
  IMPLEMENT(get_channel_type);
  IMPLEMENT(get_handle);
  IMPLEMENT(get_interfaces);
  IMPLEMENT(close);
#undef IMPLEMENT
}
