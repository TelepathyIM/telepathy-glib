/*
 * base-media-call-channel.c - Source for TpBaseMediaCallChannel
 * Copyright © 2011 Collabora Ltd.
 * @author Olivier Crete <olivier.crete@collabora.com>
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
 * SECTION:base-media-call-channel
 * @title: TpBaseMediaCallChannel
 * @short_description: base class for #TpSvcChannelTypeCall RTP media implementations
 * @see_also: #TpBaseCallChannel, #TpBaseMediaCallContent, #TpBaseMediaCallStream
 *
 * This is a base class for connection managers that use standard RTP media.
 *
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallChannel:
 *
 * A base class for call channel implementations with standard RTP
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallChannelClass:
 *
 * The class structure for #TpBaseMediaCallChannel
 *
 * Since: 0.UNRELEASED
 */


#include "config.h"
#include "base-media-call-channel.h"

#define DEBUG_FLAG TP_DEBUG_CALL

#include "telepathy-glib/base-call-content.h"
#include "telepathy-glib/base-call-internal.h"
#include "telepathy-glib/base-media-call-stream.h"
#include "telepathy-glib/base-connection.h"
#include "telepathy-glib/channel-iface.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/dbus.h"
#include "telepathy-glib/enums.h"
#include "telepathy-glib/gtypes.h"
#include "telepathy-glib/interfaces.h"
#include "telepathy-glib/svc-call.h"
#include "telepathy-glib/svc-channel.h"
#include "telepathy-glib/svc-properties-interface.h"
#include "telepathy-glib/util.h"

static void hold_iface_init (gpointer g_iface, gpointer iface_data);
static void mute_iface_init (gpointer g_iface, gpointer iface_data);


G_DEFINE_TYPE_WITH_CODE(TpBaseMediaCallChannel, tp_base_media_call_channel,
  TP_TYPE_BASE_CALL_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_HOLD,
        hold_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CALL_INTERFACE_MUTE,
        mute_iface_init);
);

/* private structure */
struct _TpBaseMediaCallChannelPrivate
{
  gboolean local_mute_state;

  TpLocalHoldState hold_state;
  TpLocalHoldStateReason hold_state_reason;
};

static const gchar *tp_base_media_call_channel_interfaces[] = {
  TP_IFACE_CHANNEL_INTERFACE_HOLD,
  TP_IFACE_CALL_INTERFACE_MUTE,
  NULL
};


/* properties */
enum
{
  PROP_LOCAL_MUTE_STATE = 1,

  LAST_PROPERTY
};


static void
tp_base_media_call_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseMediaCallChannel *self = TP_BASE_MEDIA_CALL_CHANNEL (object);

  switch (property_id)
    {
    case PROP_LOCAL_MUTE_STATE:
      g_value_set_boolean (value, self->priv->local_mute_state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_media_call_channel_class_init (TpBaseMediaCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseChannelClass *base_channel_class = TP_BASE_CHANNEL_CLASS (klass);
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl call_mute_props[] = {
      { "LocalMuteState", "local-mute-state", NULL },
      { NULL }
  };

  g_type_class_add_private (klass, sizeof (TpBaseMediaCallChannelPrivate));

  object_class->get_property = tp_base_media_call_channel_get_property;

  base_channel_class->interfaces = tp_base_media_call_channel_interfaces;

  /**
   * TpBaseMediaCallChannel:local-mute-state:
   *
   * If set to %TRUE, the call is locally muted
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boolean ("local-mute-state", "LocalMuteState",
      "Whether the channel is locally mutted",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCAL_MUTE_STATE,
      param_spec);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CALL_INTERFACE_MUTE,
      tp_dbus_properties_mixin_getter_gobject_properties,
      NULL,
      call_mute_props);
}


static void
tp_base_media_call_channel_init (TpBaseMediaCallChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_MEDIA_CALL_CHANNEL, TpBaseMediaCallChannelPrivate);
}

void
tp_base_media_call_channel_set_hold_state (TpBaseMediaCallChannel *self,
    TpLocalHoldState hold_state, TpLocalHoldStateReason hold_state_reason)
{
  gboolean changed;

  g_return_if_fail (TP_IS_BASE_CALL_CHANNEL (self));
  g_return_if_fail (hold_state == TP_LOCAL_HOLD_STATE_HELD ||
      hold_state == TP_LOCAL_HOLD_STATE_UNHELD);
  g_return_if_fail (hold_state_reason < NUM_TP_LOCAL_HOLD_STATE_REASONS);

  changed = (self->priv->hold_state == hold_state);

  self->priv->hold_state = hold_state;
  self->priv->hold_state_reason = hold_state_reason;

  if (changed)
    tp_svc_channel_interface_hold_emit_hold_state_changed (self, hold_state,
        hold_state_reason);
}

static void
tp_base_media_call_channel_get_hold_state (
    TpSvcChannelInterfaceHold *hold_iface,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallChannel *self = TP_BASE_MEDIA_CALL_CHANNEL (hold_iface);

  tp_svc_channel_interface_hold_return_from_get_hold_state (context,
      self->priv->hold_state, self->priv->hold_state_reason);
}

static void
tp_base_media_call_channel_request_hold (
    TpSvcChannelInterfaceHold *hold_iface,
    gboolean in_Hold,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallChannel *self = TP_BASE_MEDIA_CALL_CHANNEL (hold_iface);
  TpBaseMediaCallChannelClass *klass =
      TP_BASE_MEDIA_CALL_CHANNEL_GET_CLASS (self);

  if ((in_Hold && (self->priv->hold_state == TP_LOCAL_HOLD_STATE_HELD ||
              self->priv->hold_state == TP_LOCAL_HOLD_STATE_PENDING_HOLD)) ||
      (!in_Hold && (self->priv->hold_state == TP_LOCAL_HOLD_STATE_UNHELD ||
          self->priv->hold_state == TP_LOCAL_HOLD_STATE_PENDING_UNHOLD)))
    {
      self->priv->hold_state_reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;
      goto out;
    }

  self->priv->hold_state_reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;

  if (in_Hold)
    self->priv->hold_state = TP_LOCAL_HOLD_STATE_PENDING_HOLD;
  else
    self->priv->hold_state = TP_LOCAL_HOLD_STATE_PENDING_UNHOLD;


  tp_svc_channel_interface_hold_emit_hold_state_changed (self,
      self->priv->hold_state, self->priv->hold_state_reason);

  if (klass->hold_state_changed)
    klass->hold_state_changed (self,
        self->priv->hold_state, self->priv->hold_state_reason);

 out:
  tp_svc_channel_interface_hold_return_from_request_hold (context);
}


static void
tp_base_media_call_channel_request_muted (TpSvcCallInterfaceMute *mute_iface,
    gboolean in_Muted, DBusGMethodInvocation *context)
{
  TpBaseMediaCallChannel *self = TP_BASE_MEDIA_CALL_CHANNEL (mute_iface);
  gboolean changed = FALSE;

  if (in_Muted != self->priv->local_mute_state)
    changed = TRUE;

  self->priv->local_mute_state = in_Muted;

  if (changed)
    {
      tp_svc_call_interface_mute_emit_mute_state_changed (mute_iface, in_Muted);
      g_object_notify (G_OBJECT (self), "local-mute-state");
      _tp_base_call_channel_set_locally_muted (TP_BASE_CALL_CHANNEL (self),
          in_Muted);
    }

  tp_svc_call_interface_mute_return_from_request_muted (context);
}


static void
hold_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceHoldClass *klass =
      (TpSvcChannelInterfaceHoldClass *) g_iface;

#define IMPLEMENT(x, suffix) tp_svc_channel_interface_hold_implement_##x (\
    klass, tp_base_media_call_channel_##x##suffix)
  IMPLEMENT(get_hold_state,);
  IMPLEMENT(request_hold,);
#undef IMPLEMENT
}


static void
mute_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcCallInterfaceMuteClass *klass =
    (TpSvcCallInterfaceMuteClass *) g_iface;

#define IMPLEMENT(x, suffix) tp_svc_call_interface_mute_implement_##x (\
    klass, tp_base_media_call_channel_##x##suffix)
  IMPLEMENT(request_muted,);
#undef IMPLEMENT
}

