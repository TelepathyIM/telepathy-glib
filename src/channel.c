/*
 * channel.c - Source for TpStreamEngineChannel
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
#include "tp-stream-engine-signals-marshal.h"

G_DEFINE_TYPE (TpStreamEngineChannel, tp_stream_engine_channel, G_TYPE_OBJECT);

#define CHANNEL_PRIVATE(o) ((o)->priv)

struct _TpStreamEngineChannelPrivate
{
  TpChannel *channel_proxy;
  DBusGProxy *media_signalling_proxy;
  GType audio_stream_gtype;
  GType video_stream_gtype;

  TpStreamEngineNatProperties nat_props;
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
  STREAM_STATE_CHANGED,
  STREAM_RECEIVING,
  HANDLER_RESULT,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

enum
{
  PROP_CHANNEL = 1,
  PROP_OBJECT_PATH,
  PROP_AUDIO_STREAM_GTYPE,
  PROP_VIDEO_STREAM_GTYPE
};

static void
tp_stream_engine_channel_init (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannelPrivate);

  self->priv = priv;

  priv->audio_stream_gtype = TP_STREAM_ENGINE_TYPE_STREAM;
  priv->video_stream_gtype = TP_STREAM_ENGINE_TYPE_STREAM;

  priv->sessions = NULL;
  priv->streams = g_ptr_array_new ();
}

static void
tp_stream_engine_channel_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

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
    case PROP_AUDIO_STREAM_GTYPE:
      g_value_set_gtype (value, priv->audio_stream_gtype);
      break;
    case PROP_VIDEO_STREAM_GTYPE:
      g_value_set_gtype (value, priv->video_stream_gtype);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_channel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  switch (property_id)
    {
    case PROP_CHANNEL:
      priv->channel_proxy = TP_CHANNEL (g_value_dup_object (value));
      break;
    case PROP_AUDIO_STREAM_GTYPE:
      priv->audio_stream_gtype = g_value_get_gtype (value);
      break;
    case PROP_VIDEO_STREAM_GTYPE:
      priv->video_stream_gtype = g_value_get_gtype (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void channel_invalidated (TpChannel *channel_proxy,
    guint domain, gint code, gchar *message, TpStreamEngineChannel *self);

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
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
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
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
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
      gboolean want;

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
               TpStreamEngineChannel *self)
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
tp_stream_engine_channel_constructor (GType type,
                                      guint n_props,
                                      GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineChannel *self;
  TpStreamEngineChannelPrivate *priv;

  obj = G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->
           constructor (type, n_props, props);
  self = (TpStreamEngineChannel *) obj;
  priv = CHANNEL_PRIVATE (self);

  priv->channel_ready_handler = g_signal_connect (priv->channel_proxy,
      "notify::channel-ready", G_CALLBACK (channel_ready), obj);

  priv->channel_invalidated_handler = g_signal_connect (priv->channel_proxy,
      "invalidated", G_CALLBACK (channel_invalidated), obj);

  return obj;
}

static void new_stream_cb (TpStreamEngineSession *session, gchar *object_path,
    guint stream_id, TpMediaStreamType media_type,
    TpMediaStreamDirection direction, gpointer user_data);

static void stream_state_changed_cb (TpStreamEngineStream *stream,
    TpMediaStreamState state, TpMediaStreamDirection direction,
    gpointer user_data);

static void stream_receiving_cb (TpStreamEngineStream *stream,
    gboolean receiving, gpointer user_data);

static void stream_closed_cb (TpStreamEngineStream *stream,
    gpointer user_data);

static void
tp_stream_engine_channel_dispose (GObject *object)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

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
              g_signal_handlers_disconnect_by_func (obj,
                  stream_state_changed_cb, self);
              g_signal_handlers_disconnect_by_func (obj,
                  stream_receiving_cb, self);

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

  if (G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose (object);
}

static void
tp_stream_engine_channel_class_init (TpStreamEngineChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineChannelPrivate));

  object_class->set_property = tp_stream_engine_channel_set_property;
  object_class->get_property = tp_stream_engine_channel_get_property;

  object_class->constructor = tp_stream_engine_channel_constructor;

  object_class->dispose = tp_stream_engine_channel_dispose;

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

  param_spec = g_param_spec_gtype ("audio-stream-gtype",
      "GType of audio streams",
      "GType which will be instantiated for audio streams.",
      TP_STREAM_ENGINE_TYPE_STREAM,
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_AUDIO_STREAM_GTYPE,
      param_spec);

  param_spec = g_param_spec_gtype ("video-stream-gtype",
      "GType of video streams",
      "GType which will be instantiated for video streams.",
      TP_STREAM_ENGINE_TYPE_STREAM,
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_VIDEO_STREAM_GTYPE,
      param_spec);

  signals[HANDLER_RESULT] = g_signal_new ("handler-result",
      G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0, NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
      1, G_TYPE_POINTER);

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[STREAM_CREATED] =
    g_signal_new ("stream-created",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, TP_STREAM_ENGINE_TYPE_STREAM);

  signals[STREAM_STATE_CHANGED] =
    g_signal_new ("stream-state-changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  tp_stream_engine_marshal_VOID__UINT_UINT_UINT,
                  G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  signals[STREAM_RECEIVING] =
    g_signal_new ("stream-receiving",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  tp_stream_engine_marshal_VOID__UINT_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_BOOLEAN);
}

static void
stream_closed_cb (TpStreamEngineStream *stream,
                  gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  guint stream_id;

  g_object_get (stream, "stream-id", &stream_id, NULL);

  g_assert (stream == g_ptr_array_index (priv->streams, stream_id));

  g_object_unref (stream);
  g_ptr_array_index (priv->streams, stream_id) = NULL;
}

static void
stream_state_changed_cb (TpStreamEngineStream *stream,
                         TpMediaStreamState state,
                         TpMediaStreamDirection direction,
                         gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  guint stream_id;

  g_object_get (stream, "stream-id", &stream_id, NULL);

  g_signal_emit (self, signals[STREAM_STATE_CHANGED], 0, stream_id, state,
      direction);
}

static void
stream_receiving_cb (TpStreamEngineStream *stream,
                     gboolean receiving,
                     gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  guint stream_id;

  g_object_get (stream, "stream-id", &stream_id, NULL);

  g_signal_emit (self, signals[STREAM_RECEIVING], 0, stream_id, receiving);
}

static void
new_stream_cb (TpStreamEngineSession *session,
               gchar *object_path,
               guint stream_id,
               TpMediaStreamType media_type,
               TpMediaStreamDirection direction,
               gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  TpStreamEngineStream *stream;
  FsConference *fs_conference;
  FsParticipant *fs_participant;
  GType stream_gtype;
  TpProxy *channel_as_proxy = (TpProxy *) priv->channel_proxy;
  TpMediaStreamHandler *proxy;

  proxy = tp_media_stream_handler_new (channel_as_proxy->dbus_daemon,
      channel_as_proxy->bus_name, object_path, NULL);

  if (proxy == NULL)
    {
      g_warning ("failed to construct TpMediaStreamHandler: bad object path "
          "'%s'?", object_path);
      return;
    }

  g_object_get (session,
      "farsight-conference", &fs_conference,
      "farsight-participant", &fs_participant,
      NULL);

  if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO)
      stream_gtype = priv->video_stream_gtype;
  else
      stream_gtype = priv->audio_stream_gtype;

  stream = g_object_new (stream_gtype,
      "farsight-conference", fs_conference,
      "farsight-participant", fs_participant,
      "proxy", proxy,
      "stream-id", stream_id,
      "media-type", media_type,
      "direction", direction,
      "nat-properties", &(priv->nat_props),
      NULL);

  g_object_unref (proxy);
  g_object_unref (fs_conference);
  g_object_unref (fs_participant);

  if (priv->streams->len <= stream_id)
    g_ptr_array_set_size (priv->streams, stream_id + 1);

  if (g_ptr_array_index (priv->streams, stream_id) != NULL)
    {
      g_warning ("connection manager gave us a new stream with existing id "
          "%u, sending error!", stream_id);

      tp_stream_engine_stream_error (stream, 0,
          "already have a stream with this ID");

      g_object_unref (stream);

      return;
    }

  g_ptr_array_index (priv->streams, stream_id) = stream;
  g_signal_connect (stream, "error", G_CALLBACK (stream_closed_cb),
      self);
  g_signal_connect (stream, "closed", G_CALLBACK (stream_closed_cb),
      self);
  g_signal_connect (stream, "state-changed",
      G_CALLBACK (stream_state_changed_cb), self);
  g_signal_connect (stream, "receiving",
      G_CALLBACK (stream_receiving_cb), self);

  g_signal_emit (self, signals[STREAM_CREATED], 0, stream);
}

static void
add_session (TpStreamEngineChannel *self,
             const gchar *object_path,
             const gchar *session_type)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  TpStreamEngineSession *session;
  GError *error = NULL;
  TpProxy *channel_as_proxy = (TpProxy *) priv->channel_proxy;
  TpMediaSessionHandler *proxy;

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

  session = tp_stream_engine_session_new (proxy, session_type, &error);

  if (session == NULL)
    {
      g_warning ("failed to create session: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (session, "new-stream", G_CALLBACK (new_stream_cb), self);

  g_ptr_array_add (priv->sessions, session);
}

static void
new_media_session_handler (TpChannel *channel_proxy G_GNUC_UNUSED,
                           const gchar *session_handler_path,
                           const gchar *type,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *weak_object)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (weak_object);

  /* Ignore NewMediaSessionHandler until we've had a reply to
   * GetSessionHandlers; otherwise, if the two cross over in mid-flight,
   * we think the CM is asking us to add the same session twice, and get
   * very confused
   */
  if (self->priv->sessions != NULL)
    add_session (self, session_handler_path, type);
}

static void
shutdown_channel (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

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
                     TpStreamEngineChannel *self)
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
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (weak_object);
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

TpStreamEngineChannel *
tp_stream_engine_channel_new (TpDBusDaemon *dbus_daemon,
                              const gchar *bus_name,
                              const gchar *connection_path,
                              const gchar *channel_path,
                              guint handle_type,
                              guint handle,
                              GError **error)
{
  TpConnection *connection;
  TpChannel *channel_proxy;
  TpStreamEngineChannel *ret;

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

  ret = g_object_new (TP_STREAM_ENGINE_TYPE_CHANNEL,
      "channel", channel_proxy,
      NULL);

  g_object_unref (channel_proxy);

  return ret;
}

void
tp_stream_engine_channel_error (TpStreamEngineChannel *self,
                                guint error,
                                const gchar *message)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  guint i;

  for (i = 0; i < priv->streams->len; i++)
    if (g_ptr_array_index (priv->streams, i) != NULL)
      tp_stream_engine_stream_error (g_ptr_array_index (priv->streams, i),
          error, message);

  if (self->priv->channel_ready_handler != 0)
    {
      /* we haven't yet decided whether we're handling this channel. This
       * seems an unlikely situation at this point, but for the sake of
       * returning *something* from HandleChannel, let's claim we are */

      g_signal_emit (self, signals[HANDLER_RESULT], 0, NULL);

      /* if the channel becomes ready, we no longer want to know */
      g_signal_handler_disconnect (self->priv->channel_proxy,
          self->priv->channel_ready_handler);
      self->priv->channel_ready_handler = 0;
    }

  shutdown_channel (self);
}

TpStreamEngineStream *
tp_stream_engine_channel_lookup_stream (TpStreamEngineChannel *self,
                                        guint stream_id)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  if (stream_id >= priv->streams->len)
    return NULL;

  return g_ptr_array_index (priv->streams, stream_id);
}


void
tp_stream_engine_channel_foreach_stream (TpStreamEngineChannel *self,
                                         TpStreamEngineChannelStreamFunc func,
                                         gpointer user_data)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  guint i;

  for (i = 0; i < priv->streams->len; i++)
    {
      TpStreamEngineStream *stream = g_ptr_array_index (priv->streams, i);

      if (stream != NULL)
        func (self, i, stream, user_data);
    }
}


/**
 * tp_stream_engine_channel_bus_message:
 * @channel: A #TpStreamEngineChannel
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
tp_stream_engine_channel_bus_message (TpStreamEngineChannel *channel,
    GstMessage *message)
{
  guint i;
  gboolean ret = FALSE;

  for (i = 0; i < channel->priv->sessions->len; i++)
    {
      TpStreamEngineSession *session = g_ptr_array_index (
          channel->priv->sessions, i);

      if (session != NULL)
        if (tp_stream_engine_session_bus_message (session, message))
          ret = TRUE;
    }

  for (i = 0; i < channel->priv->streams->len; i++)
    {
      TpStreamEngineStream *stream = g_ptr_array_index (
          channel->priv->streams, i);

      if (stream != NULL)
        if (tp_stream_engine_stream_bus_message (stream, message))
          ret = TRUE;
    }

  return ret;
}
