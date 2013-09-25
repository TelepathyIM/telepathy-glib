/*
 * media-signalling-channel.c - Source for TfMediaSignallingChannel
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2011 Nokia Corporation
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
 * SECTION:media-signalling-channel
 * @short_description: Handle the MediaSignalling interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.MediaSignalling on a
 * channel using Farstream.
 */

#include "config.h"

#include "media-signalling-channel.h"

#include <telepathy-glib/telepathy-glib.h>

#include "stream.h"
#include "session-priv.h"



G_DEFINE_TYPE (TfMediaSignallingChannel, tf_media_signalling_channel,
    G_TYPE_OBJECT);

enum
{
  STREAM_CREATED,
  SESSION_CREATED,
  SESSION_INVALIDATED,
  GET_CODEC_CONFIG,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void tf_media_signalling_channel_dispose (GObject *object);

static void new_media_session_handler (TpChannel *channel_proxy,
    const gchar *session_handler_path, const gchar *type,
    gpointer user_data, GObject *weak_object);

static void get_session_handlers_reply (TpChannel *channel_proxy,
    const GPtrArray *session_handlers, const GError *error,
    gpointer user_data, GObject *weak_object);


static void new_stream_cb (TfSession *session, gchar *object_path,
    guint stream_id, TpMediaStreamType media_type,
    TpMediaStreamDirection direction, gpointer user_data);

static void stream_closed_cb (TfStream *stream,
    gpointer user_data);

static void
tf_media_signalling_channel_error (TfMediaSignallingChannel *chan,
    TpMediaStreamError error,
    const gchar *message);



static void
tf_media_signalling_channel_class_init (TfMediaSignallingChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_media_signalling_channel_dispose;

  /**
   * TfMediaSignallingChannel::stream-created:
   * @channel: the #TfMediaSignallingChannel which has a new stream
   * @stream: The new #TfStream
   *
   * This signal is emitted when a new stream has been created in the connection
   * manager and a local proxy has been generated.
   */

  signals[STREAM_CREATED] =
      g_signal_new ("stream-created",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 1, TF_TYPE_STREAM);

  /**
   * TfMediaSignallingChannel::session-created:
   * @channel: the #TfMediaSignallingChannel which has a new stream
   * @conference: the #FsConference of the new session
   * @participant: the #FsParticipant of the new session
   *
   * This signal is emitted when a new session has been created in the
   * connection manager. The user should add the new #FsConference to a pipeline
   * and set it to playing. The user should also set any property he wants to
   * set.
   */

  signals[SESSION_CREATED] =
      g_signal_new ("session-created",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 1, FS_TYPE_CONFERENCE);

  /**
   * TfMediaSignallingChannel::session-invalidated:
   * @channel: the #TfMediaSignallingChannel which has a new stream
   * @conference: the #FsConference of the new session
   * @participant: the #FsParticipant of the new session
   *
   * This signal is emitted when a session has been invalidated.
   * The #FsConference and #FsParticipant for this session are returned.
   * The #FsConference should be removed from the pipeline.
   */

  signals[SESSION_INVALIDATED] =
      g_signal_new ("session-invalidated",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 2, FS_TYPE_CONFERENCE, FS_TYPE_PARTICIPANT);


  signals[GET_CODEC_CONFIG] =
      g_signal_new ("get-codec-config",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          FS_TYPE_CODEC_LIST, 1, G_TYPE_UINT);
}

static void
tf_media_signalling_channel_init (TfMediaSignallingChannel *self)
{
  self->streams = g_ptr_array_new ();
}

static void
cb_properties_changed (TpProxy *proxy G_GNUC_UNUSED,
    const GPtrArray *structs,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (object);
  guint i;

  for (i = 0; i < structs->len; i++)
    {
      GValueArray *pair = g_ptr_array_index (structs, i);
      guint id;
      GValue *value;

      id = g_value_get_uint (g_value_array_get_nth (pair, 0));
      value = g_value_get_boxed (g_value_array_get_nth (pair, 1));

      if (id == self->prop_id_nat_traversal)
        {
          g_free (self->nat_props.nat_traversal);
          self->nat_props.nat_traversal = NULL;

          if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value)[0])
            self->nat_props.nat_traversal = g_value_dup_string (value);
        }
      else if (id == self->prop_id_stun_server)
        {
          g_free (self->nat_props.stun_server);
          self->nat_props.stun_server = NULL;

          if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value)[0])
            self->nat_props.stun_server = g_value_dup_string (value);
        }
      else if (id == self->prop_id_gtalk_p2p_relay_token)
        {
          g_free (self->nat_props.relay_token);
          self->nat_props.relay_token = NULL;

          if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value)[0])
            self->nat_props.relay_token = g_value_dup_string (value);
        }
      else if (id == self->prop_id_stun_port)
        {
          self->nat_props.stun_port = 0;

          if (G_VALUE_HOLDS_UINT (value))
            self->nat_props.stun_port = g_value_get_uint (value);
        }
    }
}

static void
cb_properties_got (TpProxy *proxy,
                   const GPtrArray *structs,
                   const GError *error,
                   gpointer user_data,
                   GObject *object)
{
  if (error != NULL)
    {
      g_warning ("GetProperties(): %s", error->message);
      return;
    }
  else
    {
      cb_properties_changed (proxy, structs, user_data, object);
    }
}

static void
cb_properties_listed (TpProxy *proxy,
    const GPtrArray *structs,
    const GError *error,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (object);
  guint i;
  GArray *get_properties;

  if (error != NULL)
    {
      g_warning ("ListProperties(): %s", error->message);
      return;
    }

  get_properties = g_array_sized_new (FALSE, FALSE, sizeof (guint), 4);

  for (i = 0; i < structs->len; i++)
    {
      GValueArray *spec = g_ptr_array_index (structs, i);
      guint id, flags;
      const gchar *name, *type;
      gboolean want = FALSE;

      id = g_value_get_uint (g_value_array_get_nth (spec, 0));
      name = g_value_get_string (g_value_array_get_nth (spec, 1));
      type = g_value_get_string (g_value_array_get_nth (spec, 2));
      flags = g_value_get_uint (g_value_array_get_nth (spec, 3));

      if (!tp_strdiff (name, "nat-traversal") && !tp_strdiff (type, "s"))
        {
          self->prop_id_nat_traversal = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "stun-server") && !tp_strdiff (type, "s"))
        {
          self->prop_id_stun_server = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "gtalk-p2p-relay-token") &&
          !tp_strdiff (type, "s"))
        {
          self->prop_id_gtalk_p2p_relay_token = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "stun-port") &&
          (!tp_strdiff (type, "u") || !tp_strdiff (type, "q")))
        {
          self->prop_id_stun_port = id;
          want = TRUE;
        }
      else
        {
          g_debug ("Ignoring unrecognised property %s of type %s", name, type);
        }

      if (want && (flags & TP_PROPERTY_FLAG_READ))
        g_array_append_val (get_properties, id);
    }

  if (get_properties->len > 0)
    tp_cli_properties_interface_call_get_properties (proxy, -1,
        get_properties, cb_properties_got, NULL, NULL, object);

  g_array_free (get_properties, TRUE);
}


TfMediaSignallingChannel *
tf_media_signalling_channel_new (TpChannel *channel)
{
  TfMediaSignallingChannel *self = g_object_new (
      TF_TYPE_MEDIA_SIGNALLING_CHANNEL, NULL);

  self->channel_proxy = channel;

  if (!tp_proxy_has_interface_by_id (TP_PROXY (channel),
        TP_IFACE_QUARK_PROPERTIES_INTERFACE))
    {
      /* no point doing properties manipulation on a channel with none */
      g_message ("Channel has no properties: %s",
          tp_proxy_get_object_path (TP_PROXY (channel)));
    }
  else
    {
      /* FIXME: it'd be good to use the replacement for TpPropsIface, when it
       * exists */
      tp_cli_properties_interface_connect_to_properties_changed (channel,
          cb_properties_changed, NULL, NULL, (GObject *) self, NULL);
      tp_cli_properties_interface_call_list_properties (channel, -1,
          cb_properties_listed, NULL, NULL, (GObject *) self);
    }

  tp_cli_channel_interface_media_signalling_connect_to_new_session_handler
      (channel, new_media_session_handler, NULL, NULL, (GObject *) self,
       NULL);
  tp_cli_channel_interface_media_signalling_call_get_session_handlers
      (channel, -1, get_session_handlers_reply, NULL, NULL,
       (GObject *) self);

  return self;
}


static void
tf_media_signalling_channel_dispose (GObject *object)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (object);

  g_debug (G_STRFUNC);


  if (self->streams)
    {
      guint i;

      for (i = 0; i < self->streams->len; i++)
        {
          GObject *obj = g_ptr_array_index (self->streams, i);

          if (obj != NULL)
            {
              tf_stream_error (TF_STREAM (obj),
                  TP_MEDIA_STREAM_ERROR_UNKNOWN,
                  "UI stopped channel");

              /* this first one covers both error and closed */
              g_signal_handlers_disconnect_by_func (obj,
                  stream_closed_cb, self);

              g_object_unref (obj);
            }
        }

      g_ptr_array_free (self->streams, TRUE);
      self->streams = NULL;
    }

  if (self->session)
    {
      g_signal_handlers_disconnect_by_func (self->session, new_stream_cb, self);

      g_object_unref (self->session);
      self->session = NULL;
    }

  g_free (self->nat_props.nat_traversal);
  self->nat_props.nat_traversal = NULL;

  g_free (self->nat_props.stun_server);
  self->nat_props.stun_server = NULL;

  g_free (self->nat_props.relay_token);
  self->nat_props.relay_token = NULL;

  if (G_OBJECT_CLASS (tf_media_signalling_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tf_media_signalling_channel_parent_class)->dispose (object);
}


static void
stream_closed_cb (TfStream *stream,
                  gpointer user_data)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (user_data);
  guint stream_id;

  g_object_get (stream, "stream-id", &stream_id, NULL);

  g_assert (stream == g_ptr_array_index (self->streams, stream_id));

  g_object_unref (stream);
  g_ptr_array_index (self->streams, stream_id) = NULL;
}

static void
stream_created_cb (TfStream *stream, gpointer user_data)
{
  TfMediaSignallingChannel *self = user_data;

  g_signal_emit (self, signals[STREAM_CREATED], 0, stream);

  _tf_stream_try_sending_codecs (stream);
}

static void
new_stream_cb (TfSession *session,
    gchar *object_path,
    guint stream_id,
    TpMediaStreamType media_type,
    TpMediaStreamDirection direction,
    gpointer user_data)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (user_data);
  TfStream *stream;
  FsConference *fs_conference;
  FsParticipant *fs_participant;
  TpMediaStreamHandler *proxy;
  GList *local_codec_config = NULL;

  proxy = tp_media_stream_handler_new (
      tp_proxy_get_dbus_daemon (self->channel_proxy),
      tp_proxy_get_bus_name (self->channel_proxy), object_path, NULL);

  if (proxy == NULL)
    {
      gchar *str = g_strdup_printf ("failed to construct TpMediaStreamHandler:"
          " bad object path '%s'?", object_path);
      g_warning ("%s", str);
      tf_media_signalling_channel_error (self, TP_MEDIA_STREAM_ERROR_UNKNOWN,
        str);
      g_free (str);
      return;
    }

  g_signal_emit (self, signals[GET_CODEC_CONFIG], 0,
      media_type,
      &local_codec_config);

  g_object_get (session,
      "farstream-conference", &fs_conference,
      "farstream-participant", &fs_participant,
      NULL);

  stream = _tf_stream_new ((gpointer) self, fs_conference,
      fs_participant, proxy, stream_id, media_type, direction,
      &self->nat_props, local_codec_config,
      stream_created_cb);

  fs_codec_list_destroy (local_codec_config);

  g_object_unref (proxy);
  g_object_unref (fs_conference);
  g_object_unref (fs_participant);

  if (self->streams->len <= stream_id)
    g_ptr_array_set_size (self->streams, stream_id + 1);

  if (g_ptr_array_index (self->streams, stream_id) != NULL)
    {
      g_warning ("connection manager gave us a new stream with existing id "
          "%u, sending error!", stream_id);

      tf_stream_error (stream, TP_MEDIA_STREAM_ERROR_INVALID_CM_BEHAVIOR,
          "already have a stream with this ID");

      g_object_unref (stream);

      return;
    }

  g_ptr_array_index (self->streams, stream_id) = stream;
  g_signal_connect (stream, "closed", G_CALLBACK (stream_closed_cb),
      self);
}

static void
session_invalidated_cb (TfSession *session, gpointer user_data)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (user_data);

  g_assert (session == self->session);

  g_signal_handlers_disconnect_by_func (session, new_stream_cb, self);

  g_object_unref (session);
  self->session = NULL;
}

static void
add_session (TfMediaSignallingChannel *self,
    const gchar *object_path,
    const gchar *session_type)
{
  GError *error = NULL;
  TpMediaSessionHandler *proxy;
  FsConference *conf = NULL;

  g_debug ("adding session handler %s, type %s", object_path, session_type);

  g_assert (self->session == NULL);

  proxy = tp_media_session_handler_new (
      tp_proxy_get_dbus_daemon (self->channel_proxy),
      tp_proxy_get_bus_name (self->channel_proxy), object_path, &error);

  if (proxy == NULL)
    {
      g_prefix_error (&error,"failed to construct TpMediaSessionHandler: ");
      g_warning ("%s", error->message);
      tf_media_signalling_channel_error (self, TP_MEDIA_STREAM_ERROR_UNKNOWN,
          error->message);
      g_error_free (error);
      return;
    }

  self->session = _tf_session_new (proxy, session_type, &error);

  if (self->session == NULL)
    {
      g_prefix_error (&error, "failed to create session: ");
      g_warning ("%s", error->message);
      tf_media_signalling_channel_error (self, fserror_to_tperror (error), error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (self->session, "new-stream", G_CALLBACK (new_stream_cb),
      self);
  g_signal_connect (self->session, "invalidated",
      G_CALLBACK (session_invalidated_cb), self);

  g_object_get (self->session,
      "farstream-conference", &conf,
      NULL);

  g_signal_emit (self, signals[SESSION_CREATED], 0, conf);
  g_object_unref (conf);
}

static void
new_media_session_handler (TpChannel *channel_proxy G_GNUC_UNUSED,
    const gchar *session_handler_path,
    const gchar *type,
    gpointer user_data G_GNUC_UNUSED,
    GObject *weak_object)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (weak_object);

  /* Ignore NewMediaSessionHandler until we've had a reply to
   * GetSessionHandlers; otherwise, if the two cross over in mid-flight,
   * we think the CM is asking us to add the same session twice, and get
   * very confused
   */

  if (!self->got_sessions)
    return;

  add_session (self, session_handler_path, type);
}


static void
get_session_handlers_reply (TpChannel *channel_proxy G_GNUC_UNUSED,
    const GPtrArray *session_handlers,
    const GError *error,
    gpointer user_data G_GNUC_UNUSED,
    GObject *weak_object)
{
  TfMediaSignallingChannel *self = TF_MEDIA_SIGNALLING_CHANNEL (weak_object);

  if (error)
    {
      g_critical ("Error calling GetSessionHandlers: %s", error->message);
      return;
    }

  if (session_handlers->len == 0)
    {
      g_debug ("GetSessionHandlers returned 0 sessions");
    }
  else if (session_handlers->len == 1)
    {
      GValueArray *session;
      GValue *obj;
      GValue *type;

      g_debug ("GetSessionHandlers replied: ");

      session = g_ptr_array_index (session_handlers, 0);
      obj = g_value_array_get_nth (session, 0);
      type = g_value_array_get_nth (session, 1);

      g_assert (G_VALUE_TYPE (obj) == DBUS_TYPE_G_OBJECT_PATH);
      g_assert (G_VALUE_HOLDS_STRING (type));

      g_debug ("  - session %s", (char *)g_value_get_boxed (obj));
      g_debug ("    type %s", g_value_get_string (type));

      add_session (self,
          g_value_get_boxed (obj), g_value_get_string (type));
    }
  else
    {
      g_error ("Got more than one session");
    }

  self->got_sessions = TRUE;
}


/**
 * tf_media_signalling_channel_bus_message:
 * @channel: A #TfMediaSignallingChannel
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
tf_media_signalling_channel_bus_message (TfMediaSignallingChannel *channel,
    GstMessage *message)
{
  guint i;
  gboolean ret = FALSE;

  if (channel->session == NULL)
    return FALSE;

  if (_tf_session_bus_message (channel->session, message))
    ret = TRUE;

  for (i = 0; i < channel->streams->len; i++)
    {
      TfStream *stream = g_ptr_array_index (
          channel->streams, i);

      if (stream != NULL)
        if (_tf_stream_bus_message (stream, message))
          ret = TRUE;
    }

  return ret;
}

/**
 * tf_media_signalling_channel_lookup_stream:
 * @chan: a #TfMediaSignallingChannel
 * @stream_id: the stream id to look for
 *
 * Finds the stream with the specified id if it exists.
 *
 * Returns: a #TfStream or %NULL
 */

TfStream *
tf_media_signalling__channel_lookup_stream (TfMediaSignallingChannel *chan,
    guint stream_id)
{
  if (stream_id >= chan->streams->len)
    return NULL;

  return g_ptr_array_index (chan->streams, stream_id);
}



/**
 * tf_media_signalling_channel_error:
 * @chan: a #TfMediaSignallingChannel
 * @error: the error number of type #TpMediaStreamError
 * @message: the error message
 *
 * Stops the channel and all stream related to it and sends an error to the
 * connection manager.
 */

static void
tf_media_signalling_channel_error (TfMediaSignallingChannel *chan,
    TpMediaStreamError error,
    const gchar *message)
{
  guint i;

  for (i = 0; i < chan->streams->len; i++)
    if (g_ptr_array_index (chan->streams, i) != NULL)
      tf_stream_error (g_ptr_array_index (chan->streams, i),
          error, message);
}
