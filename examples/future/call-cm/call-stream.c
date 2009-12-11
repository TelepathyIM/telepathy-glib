/*
 * call-stream.c - a stream in a call.
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

#include "call-stream.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/gtypes.h>

#include "extensions/extensions.h"

#include "call-channel.h"

static void stream_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (ExampleCallStream,
    example_call_stream,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (FUTURE_TYPE_SVC_CALL_STREAM, stream_iface_init))

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL,
  PROP_CONTENT,
  PROP_ID,
  PROP_HANDLE,
  PROP_TYPE,
  PROP_STATE,
  PROP_PENDING_SEND,
  PROP_DIRECTION,
  PROP_STREAM_INFO,
  PROP_SIMULATION_DELAY,
  PROP_LOCALLY_REQUESTED,
  N_PROPS
};

enum
{
  SIGNAL_REMOVED,
  SIGNAL_DIRECTION_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _ExampleCallStreamPrivate
{
  gchar *object_path;
  TpBaseConnection *conn;
  ExampleCallChannel *channel;
  guint id;
  TpHandle handle;
  TpMediaStreamType type;
  TpMediaStreamState state;
  TpMediaStreamDirection direction;
  TpMediaStreamPendingSend pending_send;

  guint simulation_delay;

  gulong call_terminated_id;

  guint connected_event_id;

  gboolean locally_requested;
  gboolean removed;
};

static void
example_call_stream_init (ExampleCallStream *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALL_STREAM,
      ExampleCallStreamPrivate);

  /* start off directionless */
  self->priv->direction = TP_MEDIA_STREAM_DIRECTION_NONE;
  self->priv->pending_send = 0;
  self->priv->state = TP_MEDIA_STREAM_STATE_DISCONNECTED;
}

static void
call_terminated_cb (ExampleCallChannel *channel,
    ExampleCallStream *self)
{
  g_signal_handler_disconnect (channel, self->priv->call_terminated_id);
  self->priv->call_terminated_id = 0;
  example_call_stream_close (self);
}

static void
constructed (GObject *object)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (object);
  TpDBusDaemon *dbus_daemon;
  void (*chain_up) (GObject *) =
      ((GObjectClass *) example_call_stream_parent_class)->constructed;

  dbus_daemon = tp_dbus_daemon_dup (NULL);
  g_return_if_fail (dbus_daemon != NULL);

  if (chain_up != NULL)
    chain_up (object);

  dbus_daemon = tp_dbus_daemon_dup (NULL);
  g_return_if_fail (dbus_daemon != NULL);

  dbus_g_connection_register_g_object (
      tp_proxy_get_dbus_connection (dbus_daemon),
      self->priv->object_path, object);

  g_object_unref (dbus_daemon);
  dbus_daemon = NULL;

  g_object_get (self->priv->channel,
      "connection", &self->priv->conn,
      NULL);
  self->priv->call_terminated_id = g_signal_connect (self->priv->channel,
      "call-terminated", G_CALLBACK (call_terminated_cb), self);

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
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, self->priv->object_path);
      break;

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

    case PROP_STREAM_INFO:
        {
          GValueArray *va = g_value_array_new (6);
          guint i;

          for (i = 0; i < 6; i++)
            {
              g_value_array_append (va, NULL);
              g_value_init (va->values + i, G_TYPE_UINT);
            }

          g_value_set_uint (va->values + 0, self->priv->id);
          g_value_set_uint (va->values + 1, self->priv->handle);
          g_value_set_uint (va->values + 2, self->priv->type);
          g_value_set_uint (va->values + 3, self->priv->state);
          g_value_set_uint (va->values + 4, self->priv->direction);
          g_value_set_uint (va->values + 5, self->priv->pending_send);

          g_value_take_boxed (value, va);
        }
      break;

    case PROP_SIMULATION_DELAY:
      g_value_set_uint (value, self->priv->simulation_delay);
      break;

    case PROP_LOCALLY_REQUESTED:
      g_value_set_boolean (value, self->priv->locally_requested);
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
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_assert (self->priv->object_path == NULL);   /* construct-only */
      self->priv->object_path = g_value_dup_string (value);
      break;

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

    case PROP_SIMULATION_DELAY:
      self->priv->simulation_delay = g_value_get_uint (value);
      break;

    case PROP_LOCALLY_REQUESTED:
      self->priv->locally_requested = g_value_get_boolean (value);

      if (self->priv->locally_requested)
        {
          example_call_stream_change_direction (self,
              TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL, NULL);
        }
      else
        {
          example_call_stream_receive_direction_request (self,
              TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
        }

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dispose (GObject *object)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (object);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
      self->priv->conn, TP_HANDLE_TYPE_CONTACT);

  example_call_stream_close (self);

  if (self->priv->handle != 0)
    {
      tp_handle_unref (contact_repo, self->priv->handle);
      self->priv->handle = 0;
    }

  if (self->priv->channel != NULL)
    {
      if (self->priv->call_terminated_id != 0)
        {
          g_signal_handler_disconnect (self->priv->channel,
              self->priv->call_terminated_id);
          self->priv->call_terminated_id = 0;
        }

      g_object_unref (self->priv->channel);
      self->priv->channel = NULL;
    }

  if (self->priv->conn != NULL)
    {
      g_object_unref (self->priv->conn);
      self->priv->conn = NULL;
    }

  ((GObjectClass *) example_call_stream_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) example_call_stream_parent_class)->finalize;

  g_free (self->priv->object_path);

  if (chain_up != NULL)
    chain_up (object);
}

static void
example_call_stream_class_init (ExampleCallStreamClass *klass)
{
  /*
  static TpDBusPropertiesMixinPropImpl stream_props[] = {
      { "Senders", "senders", NULL },
      { NULL }
  };
  */
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      /*
      { FUTURE_IFACE_CALL_STREAM,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        stream_props,
      },
      */
      { NULL }
  };
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass,
      sizeof (ExampleCallStreamPrivate));

  object_class->constructed = constructed;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  param_spec = g_param_spec_string ("object-path", "D-Bus object path",
      "The D-Bus object path used for this object on the bus.", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, param_spec);

  param_spec = g_param_spec_object ("channel", "ExampleCallChannel",
      "Media channel that owns this stream",
      EXAMPLE_TYPE_CALL_CHANNEL,
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

  param_spec = g_param_spec_boxed ("stream-info", "Stream info",
      "6-entry GValueArray as returned by ListStreams and RequestStreams",
      TP_STRUCT_TYPE_MEDIA_STREAM_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STREAM_INFO, param_spec);

  param_spec = g_param_spec_uint ("simulation-delay", "Simulation delay",
      "Delay between simulated network events",
      0, G_MAXUINT32, 1000,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      param_spec);

  param_spec = g_param_spec_boolean ("locally-requested", "Locally requested?",
      "True if this channel was requested by the local user",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCALLY_REQUESTED,
      param_spec);

  signals[SIGNAL_REMOVED] = g_signal_new ("removed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_DIRECTION_CHANGED] = g_signal_new ("direction-changed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  klass->dbus_properties_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleCallStreamClass,
        dbus_properties_class));
}

void
example_call_stream_close (ExampleCallStream *self)
{
  if (!self->priv->removed)
    {
      self->priv->removed = TRUE;

      g_message ("Sending to server: Closing stream %u",
          self->priv->id);

      if (self->priv->connected_event_id != 0)
        {
          g_source_remove (self->priv->connected_event_id);
        }

      /* this has to come last, because the MediaChannel may unref us in
       * response to the removed signal */
      g_signal_emit (self, signals[SIGNAL_REMOVED], 0);
    }
}

void
example_call_stream_accept_proposed_direction (ExampleCallStream *self)
{
  if (self->priv->removed ||
      !(self->priv->pending_send & TP_MEDIA_STREAM_PENDING_LOCAL_SEND))
    return;

  g_message ("SIGNALLING: send: OK, I'll send you media on stream %u",
      self->priv->id);

  self->priv->direction |= TP_MEDIA_STREAM_DIRECTION_SEND;
  self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_LOCAL_SEND;

  g_signal_emit (self, signals[SIGNAL_DIRECTION_CHANGED], 0);
}

void
example_call_stream_simulate_contact_agreed_to_send (ExampleCallStream *self)
{
  if (self->priv->removed ||
      !(self->priv->pending_send & TP_MEDIA_STREAM_PENDING_REMOTE_SEND))
    return;

  g_message ("SIGNALLING: receive: OK, I'll send you media on stream %u",
      self->priv->id);

  self->priv->direction |= TP_MEDIA_STREAM_DIRECTION_RECEIVE;
  self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_REMOTE_SEND;

  g_signal_emit (self, signals[SIGNAL_DIRECTION_CHANGED], 0);
}

static gboolean
simulate_contact_agreed_to_send_cb (gpointer p)
{
  example_call_stream_simulate_contact_agreed_to_send (p);
  return FALSE;
}

gboolean
example_call_stream_change_direction (ExampleCallStream *self,
    TpMediaStreamDirection direction,
    GError **error)
{
  gboolean sending =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_SEND) != 0);
  gboolean receiving =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean want_to_send =
    ((direction & TP_MEDIA_STREAM_DIRECTION_SEND) != 0);
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
          g_timeout_add_full (G_PRIORITY_DEFAULT, self->priv->simulation_delay,
              simulate_contact_agreed_to_send_cb, g_object_ref (self),
              g_object_unref);
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

  if (changed)
    g_signal_emit (self, signals[SIGNAL_DIRECTION_CHANGED], 0);

  return TRUE;
}

static gboolean
simulate_stream_connected_cb (gpointer p)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (p);

  g_message ("MEDIA: stream connected");
  self->priv->state = TP_MEDIA_STREAM_STATE_CONNECTED;
  g_object_notify ((GObject *) self, "state");

  return FALSE;
}

void
example_call_stream_connect (ExampleCallStream *self)
{
  /* if already trying to connect, do nothing */
  if (self->priv->connected_event_id != 0)
    return;

  /* simulate it taking a short time to connect */
  self->priv->connected_event_id = g_timeout_add (self->priv->simulation_delay,
      simulate_stream_connected_cb, self);
}

void
example_call_stream_receive_direction_request (ExampleCallStream *self,
    TpMediaStreamDirection direction)
{
  /* The remote user wants to change the direction of this stream to
   * @direction. Shall we let him? */
  gboolean sending =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_SEND) != 0);
  gboolean receiving =
    ((self->priv->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean send_requested =
    ((direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean receive_requested =
    ((direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE) != 0);
  gboolean pending_remote_send =
    ((self->priv->pending_send & TP_MEDIA_STREAM_PENDING_REMOTE_SEND) != 0);
  gboolean pending_local_send =
    ((self->priv->pending_send & TP_MEDIA_STREAM_PENDING_LOCAL_SEND) != 0);
  gboolean changed = FALSE;

  /* In some protocols, streams cannot be neither sending nor receiving, so
   * if a stream is set to TP_MEDIA_STREAM_DIRECTION_NONE, this is equivalent
   * to removing it. (This is true in XMPP, for instance.)
   *
   * However, for this example we'll emulate a protocol where streams can be
   * directionless.
   */

  if (send_requested)
    {
      g_message ("SIGNALLING: receive: Please start sending me stream %u",
          self->priv->id);

      if (!sending)
        {
          /* ask the user for permission */
          self->priv->pending_send |= TP_MEDIA_STREAM_PENDING_LOCAL_SEND;
          changed = TRUE;
        }
      else
        {
          /* nothing to do, we're already sending on that stream */
        }
    }
  else
    {
      g_message ("SIGNALLING: receive: Please stop sending me stream %u",
          self->priv->id);
      g_message ("SIGNALLING: send: OK, not sending stream %u",
          self->priv->id);

      if (sending)
        {
          g_message ("MEDIA: No longer sending media to peer for stream %u",
              self->priv->id);
          self->priv->direction &= ~TP_MEDIA_STREAM_DIRECTION_SEND;
          changed = TRUE;
        }
      else if (pending_local_send)
        {
          self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_LOCAL_SEND;
          changed = TRUE;
        }
      else
        {
          /* nothing to do, we're not sending on that stream anyway */
        }
    }

  if (receive_requested)
    {
      g_message ("SIGNALLING: receive: I will now send you media on stream %u",
          self->priv->id);

      if (!receiving)
        {
          self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_REMOTE_SEND;
          self->priv->direction |= TP_MEDIA_STREAM_DIRECTION_RECEIVE;
          changed = TRUE;
        }
    }
  else
    {
      if (pending_remote_send)
        {
          g_message ("SIGNALLING: receive: No, I refuse to send you media on "
              "stream %u", self->priv->id);
          self->priv->pending_send &= ~TP_MEDIA_STREAM_PENDING_REMOTE_SEND;
          changed = TRUE;
        }
      else if (receiving)
        {
          g_message ("SIGNALLING: receive: I will no longer send you media on "
              "stream %u", self->priv->id);
          self->priv->direction &= ~TP_MEDIA_STREAM_DIRECTION_RECEIVE;
          changed = TRUE;
        }
    }

  if (changed)
    g_signal_emit (self, signals[SIGNAL_DIRECTION_CHANGED], 0);
}

static void
stream_set_sending (FutureSvcCallStream *iface G_GNUC_UNUSED,
    gboolean sending G_GNUC_UNUSED,
    DBusGMethodInvocation *context)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (iface);
  TpMediaStreamDirection new_direction = self->priv->direction;

  if (sending)
    {
      new_direction |= TP_MEDIA_STREAM_DIRECTION_SEND;
    }
  else
    {
      new_direction &= ~TP_MEDIA_STREAM_DIRECTION_SEND;
    }

  /* this always returns TRUE in practice */
  example_call_stream_change_direction (self, new_direction, NULL);

  future_svc_call_stream_return_from_set_sending (context);
}

static void
stream_request_receiving (FutureSvcCallStream *iface,
    TpHandle contact,
    gboolean receive,
    DBusGMethodInvocation *context)
{
  ExampleCallStream *self = EXAMPLE_CALL_STREAM (iface);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles
      (self->priv->conn, TP_HANDLE_TYPE_CONTACT);
  TpMediaStreamDirection new_direction = self->priv->direction;
  GError *error = NULL;

  if (!tp_handle_is_valid (contact_repo, contact, &error))
    {
      goto finally;
    }

  if (contact != self->priv->handle)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Can't receive from contact #%u: this stream only contains #%u",
          contact, self->priv->handle);
      goto finally;
    }

  if (receive)
    {
      new_direction |= TP_MEDIA_STREAM_DIRECTION_RECEIVE;
    }
  else
    {
      new_direction &= ~TP_MEDIA_STREAM_DIRECTION_RECEIVE;
    }

  /* this always returns TRUE in practice */
  example_call_stream_change_direction (self, new_direction, NULL);

finally:
  if (error == NULL)
    {
      future_svc_call_stream_return_from_request_receiving (context);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }
}

static void
stream_iface_init (gpointer iface,
    gpointer data)
{
  FutureSvcCallStreamClass *klass = iface;

#define IMPLEMENT(x) \
  future_svc_call_stream_implement_##x (klass, stream_##x)
  IMPLEMENT (set_sending);
  IMPLEMENT (request_receiving);
#undef IMPLEMENT
}
