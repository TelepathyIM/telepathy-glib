/*
 * channel-view.c - a view for the TpChannel proxy
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "telepathy-glib/channel-view.h"

#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#define GET_PRIV(o) (((TpChannelView *)o)->priv)

G_DEFINE_ABSTRACT_TYPE (TpChannelView, tp_channel_view, G_TYPE_OBJECT);

struct _TpChannelViewPrivate
{
  TpChannel *channel;
};


enum /* properties */
{
  PROP_0,
  PROP_CHANNEL
};


static void
tp_channel_view_get_property (GObject *self,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpChannelViewPrivate *priv = GET_PRIV (self);

  switch (prop_id)
    {
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}


static void
tp_channel_view_set_property (GObject *self,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpChannelViewPrivate *priv = GET_PRIV (self);

  switch (prop_id)
    {
      case PROP_CHANNEL:
        priv->channel = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}


static void
tp_channel_view_finalize (GObject *self)
{
  TpChannelViewPrivate *priv = GET_PRIV (self);

  tp_clear_object (&priv->channel);

  G_OBJECT_CLASS (tp_channel_view_parent_class)->finalize (self);
}


static void
tp_channel_view_class_init (TpChannelViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = tp_channel_view_get_property;
  gobject_class->set_property = tp_channel_view_set_property;
  gobject_class->finalize = tp_channel_view_finalize;

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_object ("channel",
        "Channel",
        "The #TpChannel we are viewing",
        TP_TYPE_CHANNEL,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (gobject_class, sizeof (TpChannelViewPrivate));
}


static void
tp_channel_view_init (TpChannelView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_CHANNEL_VIEW,
      TpChannelViewPrivate);
}


/**
 * tp_channel_view_borrow_channel:
 * @self:
 *
 * Returns: (transfer none):
 */
TpChannel *
tp_channel_view_borrow_channel (TpChannelView *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL_VIEW (self), NULL);

  return self->priv->channel;
}
