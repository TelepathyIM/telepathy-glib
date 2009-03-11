/*
 * media-stream.c - a stream in a streamed media call.
 *
 * In connection managers with MediaSignalling, this object would be a D-Bus
 * object in its own right. In this CM, MediaSignalling is not used, and this
 * object just represents internal state of the MediaChannel.
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

#include "media-stream.h"

#include <telepathy-glib/base-connection.h>

#include "media-channel.h"

G_DEFINE_TYPE (ExampleCallableMediaStream,
    example_callable_media_stream,
    G_TYPE_OBJECT)

enum
{
  PROP_CHANNEL = 1,
  PROP_ID,
  PROP_HANDLE,
  PROP_TYPE,
  PROP_STATE,
  PROP_PENDING_SEND,
  PROP_DIRECTION,
  N_PROPS
};

enum
{
  SIGNAL_REMOVED,
  SIGNAL_DIRECTION_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _ExampleCallableMediaStreamPrivate
{
  TpBaseConnection *conn;
  ExampleCallableMediaChannel *channel;
  guint id;
  TpHandle handle;
  TpMediaStreamType type;
  TpMediaStreamState state;
  TpMediaStreamDirection direction;
  TpMediaStreamPendingSend pending_send;
};

static void
example_callable_media_stream_init (ExampleCallableMediaStream *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALLABLE_MEDIA_STREAM,
      ExampleCallableMediaStreamPrivate);
}

static void
call_terminated_cb (ExampleCallableMediaChannel *channel,
                    ExampleCallableMediaStream *self)
{
  example_callable_media_stream_close (self);
}

static void
constructed (GObject *object)
{
  ExampleCallableMediaStream *self = EXAMPLE_CALLABLE_MEDIA_STREAM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) example_callable_media_stream_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_object_get (self->priv->channel,
      "connection", &self->priv->conn,
      NULL);
  g_signal_connect (self->priv->channel, "call-terminated",
      G_CALLBACK (call_terminated_cb), self);

  if (self->priv->handle != 0)
    {
      TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
          self->priv->conn, TP_HANDLE_TYPE_CONTACT);

      tp_handle_ref (contact_repo, self->priv->handle);
    }
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleCallableMediaStream *self = EXAMPLE_CALLABLE_MEDIA_STREAM (object);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_uint (value, self->priv->id);
      break;

    case PROP_HANDLE:
      g_value_set_uint (value, self->priv->handle);
      break;

    case PROP_TYPE:
      g_value_set_uint (value, self->priv->type);
      break;

    case PROP_STATE:
      g_value_set_uint (value, self->priv->state);
      break;

    case PROP_PENDING_SEND:
      g_value_set_uint (value, self->priv->pending_send);
      break;

    case PROP_DIRECTION:
      g_value_set_uint (value, self->priv->direction);
      break;

    case PROP_CHANNEL:
      g_value_set_object (value, self->priv->channel);
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
  ExampleCallableMediaStream *self = EXAMPLE_CALLABLE_MEDIA_STREAM (object);

  switch (property_id)
    {
    case PROP_ID:
      self->priv->id = g_value_get_uint (value);
      break;

    case PROP_HANDLE:
      self->priv->handle = g_value_get_uint (value);
      break;

    case PROP_TYPE:
      self->priv->type = g_value_get_uint (value);
      break;

    case PROP_CHANNEL:
      g_assert (self->priv->channel == NULL);
      self->priv->channel = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dispose (GObject *object)
{
  ExampleCallableMediaStream *self = EXAMPLE_CALLABLE_MEDIA_STREAM (object);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
      self->priv->conn, TP_HANDLE_TYPE_CONTACT);

  if (self->priv->handle != 0)
    {
      tp_handle_unref (contact_repo, self->priv->handle);
      self->priv->handle = 0;
    }

  if (self->priv->channel != NULL)
    {
      g_object_unref (self->priv->channel);
      self->priv->channel = NULL;
    }

  if (self->priv->conn != NULL)
    {
      g_object_unref (self->priv->conn);
      self->priv->conn = NULL;
    }

  ((GObjectClass *) example_callable_media_stream_parent_class)->dispose (object);
}

static void
example_callable_media_stream_class_init (ExampleCallableMediaStreamClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass,
      sizeof (ExampleCallableMediaStreamPrivate));

  object_class->constructed = constructed;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  param_spec = g_param_spec_object ("channel", "ExampleCallableMediaChannel",
      "Media channel that owns this stream",
      EXAMPLE_TYPE_CALLABLE_MEDIA_CHANNEL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL, param_spec);

  param_spec = g_param_spec_uint ("id", "Stream ID",
      "ID of this stream",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ID, param_spec);

  param_spec = g_param_spec_uint ("handle", "Peer's TpHandle",
      "The handle with which this stream communicates or 0 if not applicable",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HANDLE, param_spec);

  param_spec = g_param_spec_uint ("type", "TpMediaStreamType",
      "Media stream type",
      0, NUM_TP_MEDIA_STREAM_TYPES - 1, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TYPE, param_spec);

  param_spec = g_param_spec_uint ("state", "TpMediaStreamState",
      "Media stream connection state",
      0, NUM_TP_MEDIA_STREAM_STATES - 1, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STATE, param_spec);

  param_spec = g_param_spec_uint ("direction", "TpMediaStreamDirection",
      "Media stream direction",
      0, NUM_TP_MEDIA_STREAM_DIRECTIONS - 1, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  param_spec = g_param_spec_uint ("pending-send", "TpMediaStreamPendingSend",
      "Requested media stream directions pending approval",
      0,
      TP_MEDIA_STREAM_PENDING_LOCAL_SEND | TP_MEDIA_STREAM_PENDING_REMOTE_SEND,
      0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PENDING_SEND, param_spec);

  signals[SIGNAL_REMOVED] = g_signal_new ("removed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_DIRECTION_CHANGED] = g_signal_new ("direction-changed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

void
example_callable_media_stream_close (ExampleCallableMediaStream *self)
{
  g_message ("Sending to server: Closing stream %u",
      self->priv->id);

  g_signal_emit (self, signals[SIGNAL_REMOVED], 0);
}

gboolean
example_callable_media_stream_change_direction (
    ExampleCallableMediaStream *self,
    TpMediaStreamDirection direction,
    GError **error)
{
  gboolean sending =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_SEND) != 0);
  gboolean receiving =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean want_to_send =
    ((direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean want_to_receive =
    ((direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean pending_remote_send =
    ((self->priv->pending_send & TP_MEDIA_STREAM_PENDING_REMOTE_SEND) != 0);
  gboolean pending_local_send =
    ((self->priv->pending_send & TP_MEDIA_STREAM_PENDING_LOCAL_SEND) != 0);
  gboolean changed = FALSE;

  if (want_to_send)
    {
      if (!sending)
        {
          if (pending_local_send)
            {
              g_message ("SIGNALLING: send: I will now send you media on "
                  "stream %u", self->priv->id);
            }

          g_message ("MEDIA: Sending media to peer for stream %u",
              self->priv->id);
          changed = TRUE;
          self->priv->direction |= TP_MEDIA_STREAM_DIRECTION_SEND;
        }
    }
  else
    {
      if (sending)
        {
          g_message ("SIGNALLING: send: I will no longer send you media on "
              "stream %u", self->priv->id);
          g_message ("MEDIA: No longer sending media to peer for stream %u",
              self->priv->id);
          changed = TRUE;
          self->priv->direction &= ~TP_MEDIA_STREAM_DIRECTION_SEND;
        }
      else if (pending_local_send)
        {
          g_message ("SIGNALLING: send: No, I refuse to send you media on "
              "stream %u", self->priv->id);
          changed = TRUE;
          self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_LOCAL_SEND;
        }
    }

  if (want_to_receive)
    {
      if (!receiving && !pending_remote_send)
        {
          g_message ("SIGNALLING: send: Please start sending me stream %u",
              self->priv->id);
          changed = TRUE;
          self->priv->pending_send |= TP_MEDIA_STREAM_PENDING_REMOTE_SEND;
          /* FIXME: schedule a timeout, after which the sender will accept
           * our request */
        }
    }
  else
    {
      if (receiving)
        {
          g_message ("SIGNALLING: send: Please stop sending me stream %u",
              self->priv->id);
          g_message ("MEDIA: Suppressing output of stream %u",
              self->priv->id);
          changed = TRUE;
          self->priv->direction &= ~TP_MEDIA_STREAM_DIRECTION_RECEIVE;
        }
    }

  return TRUE;
}
