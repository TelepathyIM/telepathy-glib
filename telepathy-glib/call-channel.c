/*
 * call-channel.h - high level API for Call channels
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:call-channel
 * @title: TpCallChannel
 * @short_description: proxy object for a call channel
 *
 * #TpCallChannel is a sub-class of #TpChannel providing convenient API
 * to make calls
 */

/**
 * TpCallChannel:
 *
 * Data structure representing a #TpCallChannel.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpCallChannelClass:
 *
 * The class of a #TpCallChannel.
 *
 * Since: 0.UNRELEASED
 */

#include <config.h>

#include "telepathy-glib/call-channel.h"
#include "telepathy-glib/call-misc.h"

#define DEBUG_FLAG TP_DEBUG_CALL
#include "telepathy-glib/debug-internal.h"

G_DEFINE_TYPE (TpCallChannel, tp_call_channel, TP_TYPE_CHANNEL)

struct _TpCallChannelPrivate
{
  gpointer dummy;
};

static void
tp_call_channel_constructed (GObject *obj)
{
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_call_channel_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (obj);
}

static void
tp_call_channel_class_init (TpCallChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = tp_call_channel_constructed;

  g_type_class_add_private (gobject_class, sizeof (TpCallChannelPrivate));
  tp_call_mute_init_known_interfaces ();
}

static void
tp_call_channel_init (TpCallChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_CALL_CHANNEL,
      TpCallChannelPrivate);
}
