/*
 * call-channel.c - Source for TfCallChannel
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
 * SECTION:call-channel

 * @short_description: Handle the Call interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.Call on a
 * channel using Farsight2.
 */


#include "call-channel.h"

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>

#include "stream.h"
#include "session-priv.h"
#include "stream-priv.h"
#include "tf-signals-marshal.h"


struct _TfCallChannel {
  GObject parent;

  TpChannel *channel_proxy;

  TfNatProperties nat_props;
  guint prop_id_nat_traversal;
  guint prop_id_stun_server;
  guint prop_id_stun_port;
  guint prop_id_gtalk_p2p_relay_token;

  /* sessions is NULL until we've had a reply from GetSessionHandlers */
  TfSession *session;
  gboolean got_sessions;
  GPtrArray *streams;
};

struct _TfCallChannelClass{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (TfCallChannel, tf_call_channel,
    G_TYPE_OBJECT);

enum
{
  SIGNAL_COUNT
};

// static guint signals[SIGNAL_COUNT] = {0};

static void tf_call_channel_dispose (GObject *object);



static void
tf_call_channel_class_init (TfCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_channel_dispose;
}


static void
tf_call_channel_init (TfCallChannel *self)
{
}



TfCallChannel *
tf_call_channel_new (TpChannel *channel)
{
  TfCallChannel *self = g_object_new (
      TF_TYPE_CALL_CHANNEL, NULL);

  return self;
}


static void
tf_call_channel_dispose (GObject *object)
{
  // TfCallChannel *self = TF_CALL_CHANNEL (object);

  g_debug (G_STRFUNC);

  if (G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose (object);
}

void
tf_call_channel_error (TfCallChannel *chan,
  TpMediaStreamError error,
  const gchar *message)
{
}

gboolean
tf_call_channel_bus_message (TfCallChannel *channel,
    GstMessage *message)
{
  return FALSE;
}
