/*
 * channel.c - Source for TpmediaChannel
 * Copyright (C) 2006-2007 Collabora Ltd.
 * Copyright (C) 2006-2007 Nokia Corporation
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

#include <stdlib.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include <gst/farsight/fs-conference-iface.h>

#include "channel.h"
#include "session.h"
#include "stream.h"
#include "session-priv.h"
#include "stream-priv.h"
#include "tpmedia-signals-marshal.h"

G_DEFINE_TYPE (TpmediaChannel, tpmedia_channel, G_TYPE_OBJECT);

#define CHANNEL_PRIVATE(o) ((o)->priv)

struct _TpmediaChannelPrivate
{
  TpChannel *channel_proxy;
  DBusGProxy *media_signalling_proxy;

  TpmediaNatProperties nat_props;
  guint prop_id_nat_traversal;
  guint prop_id_stun_server;
  guint prop_id_stun_port;
  guint prop_id_gtalk_p2p_relay_token;

  /* sessions is NULL until we've had a reply from GetSessionHandlers */
  GPtrArray *sessions;
  GPtrArray *streams;

  gulong channel_invalidated_handler;
  gulong channel_ready_handler;
};

enum
{
  TP_PROP_NAT_TRAVERSAL = 0,
  TP_PROP_STUN_SERVER,
  TP_PROP_STUN_PORT,
  TP_PROP_GTALK_P2P_RELAY_TOKEN,
  NUM_TP_PROPERTIES
};

enum
{
  CLOSED,
  STREAM_CREATED,
  SESSION_CREATED,
  SESSION_INVALIDATED,
  HANDLER_RESULT,
  STREAM_GET_CODEC_CONFIG,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

enum
{
  PROP_CHANNEL = 1,
  PROP_OBJECT_PATH
};

static void
tpmedia_channel_init (TpmediaChannel *self)
{
  TpmediaChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPMEDIA_TYPE_CHANNEL, TpmediaChannelPrivate);

  self->priv = priv;

  priv->sessions = NULL;
  priv->streams = g_ptr_array_new ();
}

static void
tpmedia_channel_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (object);
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_value_set_object (value, priv->channel_proxy);
      break;
    case PROP_OBJECT_PATH:
        {
          TpProxy *as_proxy = (TpProxy *) priv->channel_proxy;

          g_value_set_string (value, as_proxy->object_path);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tpmedia_channel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (object);
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);

  switch (property_id)
    {
    case PROP_CHANNEL:
      priv->channel_proxy = TP_CHANNEL (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void channel_invalidated (TpChannel *channel_proxy,
    guint domain, gint code, gchar *message, TpmediaChannel *self);

static void new_media_session_handler (TpChannel *channel_proxy,
    const gchar *session_handler_path, const gchar *type,
    gpointer user_data, GObject *weak_object);

static void get_session_handlers_reply (TpChannel *channel_proxy,
    const GPtrArray *session_handlers, const GError *error,
    gpointer user_data, GObject *weak_object);

static void
cb_properties_changed (TpProxy *proxy G_GNUC_UNUSED,
                       const GPtrArray *structs,
                       gpointer user_data G_GNUC_UNUSED,
                       GObject *object)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (object);
  guint i;

  for (i = 0; i < structs->len; i++)
    {
      GValueArray *pair = g_ptr_array_index (structs, i);
      guint id;
      GValue *value;

      id = g_value_get_uint (g_value_array_get_nth (pair, 0));
      value = g_value_get_boxed (g_value_array_get_nth (pair, 1));

      if (id == self->priv->prop_id_nat_traversal)
        {
          g_free (self->priv->nat_props.nat_traversal);
          self->priv->nat_props.nat_traversal = NULL;

          if (G_VALUE_HOLDS_STRING (value))
            self->priv->nat_props.nat_traversal = g_value_dup_string (value);
        }
      else if (id == self->priv->prop_id_stun_server)
        {
          g_free (self->priv->nat_props.stun_server);
          self->priv->nat_props.stun_server = NULL;

          if (G_VALUE_HOLDS_STRING (value))
            self->priv->nat_props.stun_server = g_value_dup_string (value);
        }
      else if (id == self->priv->prop_id_gtalk_p2p_relay_token)
        {
          g_free (self->priv->nat_props.relay_token);
          self->priv->nat_props.relay_token = NULL;

          if (G_VALUE_HOLDS_STRING (value))
            self->priv->nat_props.relay_token = g_value_dup_string (value);
        }
      else if (id == self->priv->prop_id_stun_port)
        {
          self->priv->nat_props.stun_port = 0;

          if (G_VALUE_HOLDS_UINT (value))
            self->priv->nat_props.stun_port = g_value_get_uint (value);
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
  TpmediaChannel *self = TPMEDIA_CHANNEL (object);
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
          self->priv->prop_id_nat_traversal = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "stun-server") && !tp_strdiff (type, "s"))
        {
          self->priv->prop_id_stun_server = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "gtalk-p2p-relay-token") &&
          !tp_strdiff (type, "s"))
        {
          self->priv->prop_id_gtalk_p2p_relay_token = id;
          want = TRUE;
        }
      else if (!tp_strdiff (name, "stun-port") &&
          (!tp_strdiff (type, "u") || !tp_strdiff (type, "q")))
        {
          self->priv->prop_id_stun_port = id;
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

static void
channel_ready (TpChannel *channel_proxy,
               GParamSpec *unused G_GNUC_UNUSED,
               TpmediaChannel *self)
{
  TpProxy *as_proxy = (TpProxy *) channel_proxy;

  g_signal_handler_disconnect (channel_proxy,
      self->priv->channel_ready_handler);
  self->priv->channel_ready_handler = 0;

  if (!tp_proxy_has_interface_by_id (as_proxy,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MEDIA_SIGNALLING))
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
        "Stream Engine was passed a channel that does not implement "
        TP_IFACE_CHANNEL_INTERFACE_MEDIA_SIGNALLING };

      g_message ("%s", e.message);
      g_signal_emit (self, signals[HANDLER_RESULT], 0, &e);
      return;
    }

  g_signal_emit (self, signals[HANDLER_RESULT], 0, NULL);

  if (!tp_proxy_has_interface_by_id (as_proxy,
        TP_IFACE_QUARK_PROPERTIES_INTERFACE))
    {
      /* no point doing properties manipulation on a channel with none */
      g_message ("Channel has no properties: %s", as_proxy->object_path);
    }
  else
    {
      /* FIXME: it'd be good to use the replacement for TpPropsIface, when it
       * exists */
      tp_cli_properties_interface_connect_to_properties_changed (channel_proxy,
          cb_properties_changed, NULL, NULL, (GObject *) self, NULL);
      tp_cli_properties_interface_call_list_properties (channel_proxy, -1,
          cb_properties_listed, NULL, NULL, (GObject *) self);
    }

  tp_cli_channel_interface_media_signalling_connect_to_new_session_handler
      (channel_proxy, new_media_session_handler, NULL, NULL, (GObject *) self,
       NULL);
  tp_cli_channel_interface_media_signalling_call_get_session_handlers
      (channel_proxy, -1, get_session_handlers_reply, NULL, NULL,
       (GObject *) self);
}

static GObject *
tpmedia_channel_constructor (GType type,
                                      guint n_props,
                                      GObjectConstructParam *props)
{
  GObject *obj;
  TpmediaChannel *self;
  TpmediaChannelPrivate *priv;

  obj = G_OBJECT_CLASS (tpmedia_channel_parent_class)->
           constructor (type, n_props, props);
  self = (TpmediaChannel *) obj;
  priv = CHANNEL_PRIVATE (self);

  priv->channel_ready_handler = g_signal_connect (priv->channel_proxy,
      "notify::channel-ready", G_CALLBACK (channel_ready), obj);

  priv->channel_invalidated_handler = g_signal_connect (priv->channel_proxy,
      "invalidated", G_CALLBACK (channel_invalidated), obj);

  return obj;
}

static void new_stream_cb (TpmediaSession *session, gchar *object_path,
    guint stream_id, TpMediaStreamType media_type,
    TpMediaStreamDirection direction, gpointer user_data);

static void stream_closed_cb (TpmediaStream *stream,
    gpointer user_data);

static void
tpmedia_channel_dispose (GObject *object)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (object);
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_debug (G_STRFUNC);

  if (priv->sessions != NULL)
    {
      guint i;

      for (i = 0; i < priv->sessions->len; i++)
        {
          GObject *obj = g_ptr_array_index (priv->sessions, i);

          g_signal_handlers_disconnect_by_func (obj, new_stream_cb, self);

          g_object_unref (g_ptr_array_index (priv->sessions, i));
        }

      g_ptr_array_free (priv->sessions, TRUE);
      priv->sessions = NULL;
    }

  if (priv->streams)
    {
      guint i;

      for (i = 0; i < priv->streams->len; i++)
        {
          GObject *obj = g_ptr_array_index (priv->streams, i);

          if (obj != NULL)
            {
              /* this first one covers both error and closed */
              g_signal_handlers_disconnect_by_func (obj,
                  stream_closed_cb, self);

              g_object_unref (obj);
            }
        }

      g_ptr_array_free (priv->streams, TRUE);
      priv->streams = NULL;
    }

  if (priv->channel_proxy)
    {
      TpChannel *tmp;

      if (priv->channel_ready_handler != 0)
        g_signal_handler_disconnect (priv->channel_proxy,
            priv->channel_ready_handler);

      if (priv->channel_invalidated_handler != 0)
        g_signal_handler_disconnect (priv->channel_proxy,
            priv->channel_invalidated_handler);

      tmp = priv->channel_proxy;
      priv->channel_proxy = NULL;
      g_object_unref (tmp);
    }

  g_free (priv->nat_props.nat_traversal);
  priv->nat_props.nat_traversal = NULL;

  g_free (priv->nat_props.stun_server);
  priv->nat_props.stun_server = NULL;

  g_free (priv->nat_props.relay_token);
  priv->nat_props.relay_token = NULL;

  if (G_OBJECT_CLASS (tpmedia_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tpmedia_channel_parent_class)->dispose (object);
}

static void
tpmedia_channel_class_init (TpmediaChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpmediaChannelPrivate));

  object_class->set_property = tpmedia_channel_set_property;
  object_class->get_property = tpmedia_channel_get_property;

  object_class->constructor = tpmedia_channel_constructor;

  object_class->dispose = tpmedia_channel_dispose;

  param_spec = g_param_spec_object ("channel", "TpChannel object",
      "Telepathy channel object which this media channel should operate on.",
      TP_TYPE_CHANNEL,
      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_CHANNEL, param_spec);

  param_spec = g_param_spec_string ("object-path", "channel object path",
      "D-Bus object path of the Telepathy channel which this channel operates "
      "on.",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, param_spec);

  signals[HANDLER_RESULT] = g_signal_new ("handler-result",
      G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0, NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
      1, G_TYPE_POINTER);

  /**
   * TpmediaChannel::closed:
   *
   * This function is called after a channel is closed, either because
   * it has been closed by the connection manager or because we had a locally
   * generated error.
   */

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * TpmediaChannel::stream-created:
   * @tpmediachannel: the #TpmediaChannel which has a new stream
   * @stream: The new #TpmediaStream
   *
   * This signal is emitted when a new stream has been created in the connection
   * manager and a local proxy has been generated.
   */

  signals[STREAM_CREATED] =
    g_signal_new ("stream-created",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, TPMEDIA_TYPE_STREAM);

  /**
   * TpmediaChannel::session-created:
   * @tpmediachannel: the #TpmediaChannel which has a new stream
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
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  _tpmedia_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2, FS_TYPE_CONFERENCE, FS_TYPE_PARTICIPANT);

  /**
   * TpmediaChannel::session-invalidated:
   * @tpmediachannel: the #TpmediaChannel which has a new stream
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
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  _tpmedia_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2, FS_TYPE_CONFERENCE, FS_TYPE_PARTICIPANT);

  /**
   * TpmediaChannel::stream-get-codec-config:
   * @tpmediachannel: the #TpmediaChannel
   * @stream_id: The ID of the stream which is requestiing new codec config
   * @media_type: The #TpMediaStreamType of the stream
   * @direction: The #TpMediaStreamDirection of the stream
   *
   * This is emitted when a new stream is created and allows the caller to
   * specify his codec preferences.
   *
   * Returns: a #GList of #FsCodec
   */

  signals[STREAM_GET_CODEC_CONFIG] =
    g_signal_new ("stream-get-codec-config",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  _tpmedia_marshal_BOXED__UINT_UINT_UINT,
                  FS_TYPE_CODEC_LIST, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

static void
stream_closed_cb (TpmediaStream *stream,
                  gpointer user_data)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (user_data);
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);
  guint stream_id;

  g_object_get (stream, "stream-id", &stream_id, NULL);

  g_assert (stream == g_ptr_array_index (priv->streams, stream_id));

  g_object_unref (stream);
  g_ptr_array_index (priv->streams, stream_id) = NULL;
}

static void
new_stream_cb (TpmediaSession *session,
               gchar *object_path,
               guint stream_id,
               TpMediaStreamType media_type,
               TpMediaStreamDirection direction,
               gpointer user_data)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (user_data);
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);
  TpmediaStream *stream;
  FsConference *fs_conference;
  FsParticipant *fs_participant;
  TpProxy *channel_as_proxy = (TpProxy *) priv->channel_proxy;
  TpMediaStreamHandler *proxy;
  GError *error = NULL;
  GList *local_codec_config = NULL;

  proxy = tp_media_stream_handler_new (channel_as_proxy->dbus_daemon,
      channel_as_proxy->bus_name, object_path, NULL);

  if (proxy == NULL)
    {
      g_warning ("failed to construct TpMediaStreamHandler: bad object path "
          "'%s'?", object_path);
      return;
    }

  g_signal_emit (self, signals[STREAM_GET_CODEC_CONFIG], 0,
      stream_id,
      media_type,
      direction,
      &local_codec_config);

  g_object_get (session,
      "farsight-conference", &fs_conference,
      "farsight-participant", &fs_participant,
      NULL);

  stream = _tpmedia_stream_new ((gpointer) self, fs_conference,
      fs_participant, proxy, stream_id, media_type, direction,
      &priv->nat_props, local_codec_config, &error);

  fs_codec_list_destroy (local_codec_config);

  if (!stream)
    {
      g_warning ("Error creating stream: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_object_unref (proxy);
  g_object_unref (fs_conference);
  g_object_unref (fs_participant);

  if (priv->streams->len <= stream_id)
    g_ptr_array_set_size (priv->streams, stream_id + 1);

  if (g_ptr_array_index (priv->streams, stream_id) != NULL)
    {
      g_warning ("connection manager gave us a new stream with existing id "
          "%u, sending error!", stream_id);

      tpmedia_stream_error (stream, 0,
          "already have a stream with this ID");

      g_object_unref (stream);

      return;
    }

  g_ptr_array_index (priv->streams, stream_id) = stream;
  g_signal_connect (stream, "closed", G_CALLBACK (stream_closed_cb),
      self);

  g_signal_emit (self, signals[STREAM_CREATED], 0, stream);
}

static void
session_invalidated_cb (TpmediaSession *session, gpointer user_data)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (user_data);
  FsConference *conf = NULL;
  FsParticipant *part = NULL;

  g_object_get (session, "conference", &conf, "participant", &part, NULL);

  g_signal_emit (self, signals[SESSION_INVALIDATED], 0, conf, part);

  g_object_unref (conf);
  g_object_unref (part);
}

static void
add_session (TpmediaChannel *self,
             const gchar *object_path,
             const gchar *session_type)
{
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);
  TpmediaSession *session;
  GError *error = NULL;
  TpProxy *channel_as_proxy = (TpProxy *) priv->channel_proxy;
  TpMediaSessionHandler *proxy;
  FsConference *conf = NULL;
  FsParticipant *part = NULL;

  g_debug ("adding session handler %s, type %s", object_path, session_type);

  g_assert (self->priv->sessions != NULL);

  proxy = tp_media_session_handler_new (channel_as_proxy->dbus_daemon,
      channel_as_proxy->bus_name, object_path, &error);

  if (proxy == NULL)
    {
      g_warning ("failed to construct TpMediaSessionHandler: %s",
          error->message);
      g_error_free (error);
      return;
    }

  session = _tpmedia_session_new (proxy, session_type, &error);

  if (session == NULL)
    {
      g_warning ("failed to create session: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (session, "new-stream", G_CALLBACK (new_stream_cb), self);
  g_signal_connect (session, "invalidated",
      G_CALLBACK (session_invalidated_cb), self);

  g_ptr_array_add (priv->sessions, session);

  g_object_get (session, "conference", &conf, "participant", &part, NULL);

  g_signal_emit (self, signals[SESSION_CREATED], 0, conf, part);
  g_object_unref (conf);
  g_object_unref (part);
}

static void
new_media_session_handler (TpChannel *channel_proxy G_GNUC_UNUSED,
                           const gchar *session_handler_path,
                           const gchar *type,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *weak_object)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (weak_object);

  /* Ignore NewMediaSessionHandler until we've had a reply to
   * GetSessionHandlers; otherwise, if the two cross over in mid-flight,
   * we think the CM is asking us to add the same session twice, and get
   * very confused
   */
  if (self->priv->sessions != NULL)
    add_session (self, session_handler_path, type);
}

static void
shutdown_channel (TpmediaChannel *self)
{
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (self);

  if (priv->channel_proxy != NULL)
    {
      /* I've ensured that this is true everywhere this function is called */
      g_assert (priv->channel_ready_handler == 0);

      if (priv->channel_invalidated_handler)
        {
          g_signal_handler_disconnect (
            priv->channel_proxy, priv->channel_invalidated_handler);
          priv->channel_invalidated_handler = 0;
        }
    }

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
channel_invalidated (TpChannel *channel_proxy,
                     guint domain,
                     gint code,
                     gchar *message,
                     TpmediaChannel *self)
{
  GError e = { domain, code, message };

  if (self->priv->channel_ready_handler != 0)
    {
      /* we haven't yet decided whether to handle this channel - do it now */
      g_signal_handler_disconnect (channel_proxy,
          self->priv->channel_ready_handler);
      self->priv->channel_ready_handler = 0;

      g_signal_emit (self, signals[HANDLER_RESULT], 0, &e);
    }

  shutdown_channel (self);
}

static void
get_session_handlers_reply (TpChannel *channel_proxy G_GNUC_UNUSED,
                            const GPtrArray *session_handlers,
                            const GError *error,
                            gpointer user_data G_GNUC_UNUSED,
                            GObject *weak_object)
{
  TpmediaChannel *self = TPMEDIA_CHANNEL (weak_object);
  guint i;

  self->priv->sessions = g_ptr_array_sized_new (session_handlers->len);

  if (error)
    {
      g_critical ("Error calling GetSessionHandlers: %s", error->message);
      return;
    }

  if (session_handlers->len == 0)
    {
      g_debug ("GetSessionHandlers returned 0 sessions");
    }
  else
    {
      g_debug ("GetSessionHandlers replied: ");

      for (i = 0; i < session_handlers->len; i++)
        {
          GValueArray *session = g_ptr_array_index (session_handlers, i);
          GValue *obj = g_value_array_get_nth (session, 0);
          GValue *type = g_value_array_get_nth (session, 1);

          g_assert (G_VALUE_TYPE (obj) == DBUS_TYPE_G_OBJECT_PATH);
          g_assert (G_VALUE_HOLDS_STRING (type));

          g_debug ("  - session %s", (char *)g_value_get_boxed (obj));
          g_debug ("    type %s", g_value_get_string (type));

          add_session (self,
              g_value_get_boxed (obj), g_value_get_string (type));
        }
    }
}

/**
 * tpmedia_channel_new_from_proxy:
 * @channel_proxy: a #TpChannel proxy
 *
 * Creates a new #TpmediaChannel from an existing channel proxy
 *
 * Returns: a new #TpmediaChannel
 */

TpmediaChannel *
tpmedia_channel_new_from_proxy (TpChannel *channel_proxy)
{
  return g_object_new (TPMEDIA_TYPE_CHANNEL,
      "channel", channel_proxy,
      NULL);
}

/**
 * tpmedia_channel_new:
 * @dbus_daemon: a #TpDBusDaemon
 * @bus_name: the name of the bus to connect to
 * @connection_path: the connection path to connect to
 * @channel_path: the path of the channel to connect to
 * @handle_type: the type of handle
 * @handle: the handle
 *
 * Creates a new #TpmediaChannel by connecting to the D-Bus bus and finding
 * an already existing channel object. This API would normally be used with the
 * HandleChannel method.
 *
 * Returns: a new #TpmediaChannel
 */

TpmediaChannel *
tpmedia_channel_new (TpDBusDaemon *dbus_daemon,
                              const gchar *bus_name,
                              const gchar *connection_path,
                              const gchar *channel_path,
                              guint handle_type,
                              guint handle,
                              GError **error)
{
  TpConnection *connection;
  TpChannel *channel_proxy;
  TpmediaChannel *ret;

  g_return_val_if_fail (bus_name != NULL, NULL);
  g_return_val_if_fail (connection_path != NULL, NULL);
  g_return_val_if_fail (channel_path != NULL, NULL);

  connection = tp_connection_new (dbus_daemon,
      bus_name, connection_path, error);

  if (connection == NULL)
    return NULL;

  channel_proxy = tp_channel_new (connection, channel_path,
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, handle_type, handle, error);

  if (channel_proxy == NULL)
    return NULL;

  g_object_unref (connection);

  ret = g_object_new (TPMEDIA_TYPE_CHANNEL,
      "channel", channel_proxy,
      NULL);

  g_object_unref (channel_proxy);

  return ret;
}

/**
 * tpmedia_channel_error:
 * @chan: a #TpmediaChannel
 * @error: the error number of type #TpMediaStreamError
 * @message: the error message
 *
 * Stops the channel and all stream related to it and sends an error to the
 * connection manager.
 */

void
tpmedia_channel_error (TpmediaChannel *chan,
                                guint error,
                                const gchar *message)
{
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (chan);
  guint i;

  for (i = 0; i < priv->streams->len; i++)
    if (g_ptr_array_index (priv->streams, i) != NULL)
      tpmedia_stream_error (g_ptr_array_index (priv->streams, i),
          error, message);

  if (chan->priv->channel_ready_handler != 0)
    {
      /* we haven't yet decided whether we're handling this channel. This
       * seems an unlikely situation at this point, but for the sake of
       * returning *something* from HandleChannel, let's claim we are */

      g_signal_emit (chan, signals[HANDLER_RESULT], 0, NULL);

      /* if the channel becomes ready, we no longer want to know */
      g_signal_handler_disconnect (chan->priv->channel_proxy,
          chan->priv->channel_ready_handler);
      chan->priv->channel_ready_handler = 0;
    }

  shutdown_channel (chan);
}

/**
 * tpmedia_channel_lookup_stream:
 * @chan: a #TpmediaChannel
 * @stream_id: the stream id to look for
 *
 * Finds the stream with the specified id if it exists.
 *
 * Returns: a #TpmediaStream or %NULL
 */

TpmediaStream *
tpmedia_channel_lookup_stream (TpmediaChannel *chan,
                                        guint stream_id)
{
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (chan);

  if (stream_id >= priv->streams->len)
    return NULL;

  return g_ptr_array_index (priv->streams, stream_id);
}

/**
 * tpmedia_channel_foreach_stream:
 * @chan: a #TpmediaChannel
 * @func: the function to call on every stream in this channel
 * @user_data: data that will be passed to the function
 *
 * Calls the function func on every stream inside this channel.
 */
void
tpmedia_channel_foreach_stream (TpmediaChannel *chan,
                                         TpmediaChannelStreamFunc func,
                                         gpointer user_data)
{
  TpmediaChannelPrivate *priv = CHANNEL_PRIVATE (chan);
  guint i;

  for (i = 0; i < priv->streams->len; i++)
    {
      TpmediaStream *stream = g_ptr_array_index (priv->streams, i);

      if (stream != NULL)
        func (chan, i, stream, user_data);
    }
}


/**
 * tpmedia_channel_bus_message:
 * @channel: A #TpmediaChannel
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
tpmedia_channel_bus_message (TpmediaChannel *channel,
    GstMessage *message)
{
  guint i;
  gboolean ret = FALSE;

  if (channel->priv->sessions == NULL)
    return FALSE;

  for (i = 0; i < channel->priv->sessions->len; i++)
    {
      TpmediaSession *session = g_ptr_array_index (
          channel->priv->sessions, i);

      if (session != NULL)
        if (_tpmedia_session_bus_message (session, message))
          ret = TRUE;
    }

  for (i = 0; i < channel->priv->streams->len; i++)
    {
      TpmediaStream *stream = g_ptr_array_index (
          channel->priv->streams, i);

      if (stream != NULL)
        if (_tpmedia_stream_bus_message (stream, message))
          ret = TRUE;
    }

  return ret;
}
