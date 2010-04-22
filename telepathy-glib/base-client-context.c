/*
 * Context objects for TpBaseClient calls
 *
 * Copyright © 2010 Collabora Ltd.
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

#include "telepathy-glib/base-client-context.h"
#include "telepathy-glib/base-client-context-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

struct _TpObserveChannelsContextClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _TpObserveChannelsContext {
    /*<private>*/
    GObject parent;
    TpObserveChannelsContextPrivate *priv;
};

G_DEFINE_TYPE(TpObserveChannelsContext, tp_observe_channels_context,
    G_TYPE_OBJECT)

enum {
    PROP_DBUS_CONTEXT = 1,
    PROP_OBSERVER_INFO,
    N_PROPS
};

struct _TpObserveChannelsContextPrivate
{
  DBusGMethodInvocation *dbus_context;
  GHashTable *observer_info;
  TpBaseClientContextState state;
};

static void
tp_observe_channels_context_init (TpObserveChannelsContext *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_OBSERVE_CHANNELS_CONTEXT, TpObserveChannelsContextPrivate);

  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_NONE;
}

static void
tp_observe_channels_context_dispose (GObject *object)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_observe_channels_context_parent_class)->dispose;

  if (self->priv->observer_info != NULL)
    {
      g_hash_table_unref (self->priv->observer_info);
      self->priv->observer_info = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_observe_channels_context_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);

  switch (property_id)
    {
      case PROP_DBUS_CONTEXT:
        g_value_set_pointer (value, self->priv->dbus_context);
        break;
      case PROP_OBSERVER_INFO:
        g_value_set_boxed (value, self->priv->observer_info);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channels_context_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpObserveChannelsContext *self = TP_OBSERVE_CHANNELS_CONTEXT (object);

  switch (property_id)
    {
      case PROP_DBUS_CONTEXT:
        self->priv->dbus_context = g_value_get_pointer (value);
        break;
      case PROP_OBSERVER_INFO:
        self->priv->observer_info = g_value_dup_boxed (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_observe_channels_context_class_init (TpObserveChannelsContextClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpObserveChannelsContextPrivate));

  object_class->get_property = tp_observe_channels_context_get_property;
  object_class->set_property = tp_observe_channels_context_set_property;
  object_class->dispose = tp_observe_channels_context_dispose;

  param_spec = g_param_spec_pointer ("dbus-context", "D-Bus context",
      "The DBusGMethodInvocation associated with the ObserveChannels call",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_CONTEXT,
      param_spec);

  param_spec = g_param_spec_boxed ("observer-info", "Observer info",
      "The Observer_Info that has been passed to ObserveChannels",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBSERVER_INFO,
      param_spec);
}

TpObserveChannelsContext *
_tp_observe_channels_context_new (
    DBusGMethodInvocation *dbus_context,
    GHashTable *observer_info)
{
  return g_object_new (TP_TYPE_OBSERVE_CHANNELS_CONTEXT,
      "dbus-context", dbus_context,
      "observer-info", observer_info,
      NULL);
}

void
tp_observe_channels_context_accept (TpObserveChannelsContext *self)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_DONE;
  dbus_g_method_return (self->priv->dbus_context);
}

void
tp_observe_channels_context_fail (TpObserveChannelsContext *self,
    const GError *error)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_FAILED;
  dbus_g_method_return_error (self->priv->dbus_context, error);
}

void
tp_observe_channels_context_delay (TpObserveChannelsContext *self)
{
  self->priv->state = TP_BASE_CLIENT_CONTEXT_STATE_DELAYED;
}

gboolean
tp_observe_channels_context_get_recovering (TpObserveChannelsContext *self)
{
  /* tp_asv_get_boolean returns FALSE if the key is not set which is what we
   * want */
  return tp_asv_get_boolean (self->priv->observer_info, "recovering", NULL);
}

TpBaseClientContextState
_tp_observe_channels_context_get_state (
    TpObserveChannelsContext *self)
{
  return self->priv->state;
}
