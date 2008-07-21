/*
 * stream.c - Source for TpmediaStream
 * Copyright (C) 2006-2008 Collabora Ltd.
 * Copyright (C) 2006-2008 Nokia Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <gst/interfaces/xoverlay.h>

#include <gst/farsight/fs-conference-iface.h>

#include "stream.h"
#include "stream-priv.h"
#include "channel.h"
#include "tpmedia-signals-marshal.h"

G_DEFINE_TYPE (TpmediaStream, tpmedia_stream, G_TYPE_OBJECT);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (%s) %s: " format, \
    stream->stream_id, \
    (stream->priv->media_type == FS_MEDIA_TYPE_AUDIO) ? "audio" \
                                                      : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define WARNING(stream, format, ...) \
  g_warning ("stream %d (%s) %s: " format, \
    stream->stream_id, \
    (stream->priv->media_type == FS_MEDIA_TYPE_AUDIO) ? "audio" \
                                                      : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define STREAM_PRIVATE(o) ((o)->priv)

struct _TpmediaStreamPrivate
{
  TpmediaChannel *channel;
  FsConference *fs_conference;
  FsParticipant *fs_participant;
  FsSession *fs_session;
  FsStream *fs_stream;
  TpMediaStreamType media_type;
  TpMediaStreamDirection direction;
  const TpStreamEngineNatProperties *nat_props;
  GList *local_preferences;

  GError *construction_error;

  TpMediaStreamHandler *stream_handler_proxy;

  FsStreamDirection desired_direction;
  gboolean held;
  TpMediaStreamDirection has_resource;


  gboolean send_local_codecs;
  gboolean send_supported_codecs;
};

enum
{
  CLOSED,
  ERROR,
  REQUEST_RESOURCE,
  FREE_RESOURCE,
  SRC_PAD_ADDED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

/* properties */
enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_FARSIGHT_CONFERENCE,
  PROP_FARSIGHT_PARTICIPANT,
  PROP_PROXY,
  PROP_STREAM_ID,
  PROP_MEDIA_TYPE,
  PROP_DIRECTION,
  PROP_NAT_PROPERTIES,
  PROP_SINK_PAD,
  PROP_LOCAL_PREFERENCES
};


static gboolean tpmedia_stream_request_resource (
    TpmediaStream *self,
    TpMediaStreamDirection dir);
static void tpmedia_stream_free_resource (TpmediaStream *self,
    TpMediaStreamDirection dir);

static void add_remote_candidate (TpMediaStreamHandler *proxy,
    const gchar *candidate, const GPtrArray *transports,
    gpointer user_data, GObject *object);

static void remove_remote_candidate (TpMediaStreamHandler *proxy,
    const gchar *candidate,
    gpointer user_data, GObject *object);

static void set_active_candidate_pair (TpMediaStreamHandler *proxy,
    const gchar *native_candidate, const gchar *remote_candidate,
    gpointer user_data, GObject *object);

static void set_remote_candidate_list (TpMediaStreamHandler *proxy,
    const GPtrArray *candidates, gpointer user_data, GObject *object);

static void set_remote_codecs (TpMediaStreamHandler *proxy,
    const GPtrArray *codecs, gpointer user_data, GObject *object);

static void set_stream_playing (TpMediaStreamHandler *proxy, gboolean play,
    gpointer user_data, GObject *object);
static void set_stream_held (TpMediaStreamHandler *proxy, gboolean held,
    gpointer user_data, GObject *object);

static void set_stream_sending (TpMediaStreamHandler *proxy, gboolean play,
    gpointer user_data, GObject *object);

static void start_telephony_event (TpMediaStreamHandler *proxy, guchar event,
    gpointer user_data, GObject *object);

static void stop_telephony_event (TpMediaStreamHandler *proxy,
    gpointer user_data, GObject *object);

static void close (TpMediaStreamHandler *proxy,
    gpointer user_data, GObject *object);

static void invalidated_cb (TpMediaStreamHandler *proxy,
    guint domain, gint code, gchar *message, gpointer user_data);

static FsMediaType tp_media_type_to_fs (TpMediaStreamType type);

static GPtrArray *fs_codecs_to_tp (TpmediaStream *stream,
    const GList *codecs);
static void async_method_callback (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data,
    GObject *weak_object);

static void cb_fs_stream_src_pad_added (FsStream *fsstream G_GNUC_UNUSED,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data);
static void
tpmedia_stream_try_sending_codecs (TpmediaStream *stream);


static void
tpmedia_stream_init (TpmediaStream *self)
{
  TpmediaStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPMEDIA_TYPE_STREAM, TpmediaStreamPrivate);

  self->priv = priv;
  priv->has_resource = TP_MEDIA_STREAM_DIRECTION_NONE;
}

static void
tpmedia_stream_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_value_set_object (value, self->priv->channel);
      break;
    case PROP_FARSIGHT_CONFERENCE:
      g_value_set_object (value, self->priv->fs_conference);
      break;
    case PROP_FARSIGHT_PARTICIPANT:
      g_value_set_object (value, self->priv->fs_participant);
      break;
    case PROP_PROXY:
      g_value_set_object (value, self->priv->stream_handler_proxy);
      break;
    case PROP_STREAM_ID:
      g_value_set_uint (value, self->stream_id);
      break;
    case PROP_MEDIA_TYPE:
      g_value_set_uint (value, self->priv->media_type);
      break;
    case PROP_DIRECTION:
      g_value_set_uint (value, self->priv->direction);
      break;
    case PROP_NAT_PROPERTIES:
      g_value_set_pointer (value,
          (TpStreamEngineNatProperties *) self->priv->nat_props);
      break;
    case PROP_SINK_PAD:
      g_object_get_property (G_OBJECT (self->priv->fs_session),
          "sink-pad", value);
      break;
    case PROP_LOCAL_PREFERENCES:
      g_value_set_boxed (value, self->priv->local_preferences);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tpmedia_stream_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      self->priv->channel =
          TPMEDIA_CHANNEL (g_value_get_object (value));
      break;
    case PROP_FARSIGHT_CONFERENCE:
      self->priv->fs_conference =
          FS_CONFERENCE (g_value_dup_object (value));
      break;
    case PROP_FARSIGHT_PARTICIPANT:
      self->priv->fs_participant =
          FS_PARTICIPANT (g_value_dup_object (value));
      break;
    case PROP_PROXY:
      self->priv->stream_handler_proxy =
          TP_MEDIA_STREAM_HANDLER (g_value_dup_object (value));
      break;
    case PROP_STREAM_ID:
      self->stream_id = g_value_get_uint (value);
      break;
    case PROP_MEDIA_TYPE:
      self->priv->media_type = g_value_get_uint (value);
      break;
    case PROP_DIRECTION:
      self->priv->direction = g_value_get_uint (value);
      break;
    case PROP_NAT_PROPERTIES:
      self->priv->nat_props = g_value_get_pointer (value);
      break;
    case PROP_LOCAL_PREFERENCES:
      self->priv->local_preferences = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

#define MAX_STREAM_TRANS_PARAMS 6

static GObject *
tpmedia_stream_constructor (GType type,
                                     guint n_props,
                                     GObjectConstructParam *props)
{
  GObject *obj;
  TpmediaStream *stream;
  TpmediaStreamPrivate *priv;
  TpmediaStreamClass *klass = NULL;
  gchar *transmitter;
  guint n_args = 0;
  GList *preferred_local_candidates = NULL;
  GParameter params[MAX_STREAM_TRANS_PARAMS];

  obj = G_OBJECT_CLASS (tpmedia_stream_parent_class)->
            constructor (type, n_props, props);
  stream = (TpmediaStream *) obj;
  priv = stream->priv;
  klass = TPMEDIA_STREAM_GET_CLASS(obj);

  g_signal_connect (priv->stream_handler_proxy, "invalidated",
      G_CALLBACK (invalidated_cb), obj);

  tp_cli_media_stream_handler_connect_to_add_remote_candidate
      (priv->stream_handler_proxy, add_remote_candidate, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_remove_remote_candidate
      (priv->stream_handler_proxy, remove_remote_candidate, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_set_active_candidate_pair
      (priv->stream_handler_proxy, set_active_candidate_pair, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_candidate_list
      (priv->stream_handler_proxy, set_remote_candidate_list, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_codecs
      (priv->stream_handler_proxy, set_remote_codecs, NULL, NULL, obj, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_playing
      (priv->stream_handler_proxy, set_stream_playing, NULL, NULL, obj, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_sending
      (priv->stream_handler_proxy, set_stream_sending, NULL, NULL, obj, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_held
      (priv->stream_handler_proxy, set_stream_held, NULL, NULL, obj, NULL);
  tp_cli_media_stream_handler_connect_to_start_telephony_event
      (priv->stream_handler_proxy, start_telephony_event, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_stop_telephony_event
      (priv->stream_handler_proxy, stop_telephony_event, NULL, NULL, obj,
       NULL);
  tp_cli_media_stream_handler_connect_to_close
      (priv->stream_handler_proxy, close, NULL, NULL, obj, NULL);

  memset (params, 0, sizeof(GParameter) * MAX_STREAM_TRANS_PARAMS);

  if (stream->priv->nat_props == NULL ||
      stream->priv->nat_props->nat_traversal == NULL ||
      !strcmp (stream->priv->nat_props->nat_traversal, "gtalk-p2p"))
    {
      transmitter = "nice";

      params[n_args].name = "compatibility-mode";
      g_value_init (&params[n_args].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_args].value, 1);
      n_args++;
    }
  else
    {
      transmitter = "rawudp";

      if (stream->priv->media_type == TP_MEDIA_STREAM_TYPE_AUDIO)
        preferred_local_candidates = g_list_prepend (NULL,
            fs_candidate_new (NULL, FS_COMPONENT_RTP, FS_CANDIDATE_TYPE_HOST,
                FS_NETWORK_PROTOCOL_UDP, NULL, 7078));
      else if (stream->priv->media_type == TP_MEDIA_STREAM_TYPE_VIDEO)
        preferred_local_candidates = g_list_prepend (NULL,
            fs_candidate_new (NULL, FS_COMPONENT_RTP, FS_CANDIDATE_TYPE_HOST,
                FS_NETWORK_PROTOCOL_UDP, NULL, 9078));
    }

  if (stream->priv->nat_props &&
      stream->priv->nat_props->stun_server &&
      stream->priv->nat_props->stun_port)
    {
      gchar *conn_timeout_str = NULL;

      params[n_args].name = "stun-ip";
      g_value_init (&params[n_args].value, G_TYPE_STRING);
      g_value_set_string (&params[n_args].value,
          stream->priv->nat_props->stun_server);
      n_args++;

      params[n_args].name = "stun-port";
      g_value_init (&params[n_args].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_args].value,
          stream->priv->nat_props->stun_port);
      n_args++;

      conn_timeout_str = getenv ("FS_CONN_TIMEOUT");

      if (conn_timeout_str)
        {
          gint conn_timeout = strtol (conn_timeout_str, NULL, 10);

          params[n_args].name = "stun-timeout";
          g_value_init (&params[n_args].value, G_TYPE_UINT);
          g_value_set_uint (&params[n_args].value, conn_timeout);
          n_args++;
        }
    }

  if (preferred_local_candidates)
    {
      params[n_args].name = "preferred-local-candidates";
      g_value_init (&params[n_args].value, FS_TYPE_CANDIDATE_LIST);
      g_value_take_boxed (&params[n_args].value,
          preferred_local_candidates);
      n_args++;
    }

  stream->priv->fs_session = fs_conference_new_session (
      stream->priv->fs_conference,
      tp_media_type_to_fs (stream->priv->media_type),
      &stream->priv->construction_error);

  if (!stream->priv->fs_session)
    return obj;

  stream->priv->fs_stream = fs_session_new_stream (stream->priv->fs_session,
      stream->priv->fs_participant,
      FS_DIRECTION_NONE,
      transmitter,
      n_args,
      params,
      &stream->priv->construction_error);

  if (!stream->priv->fs_stream)
    return obj;

  if (stream->priv->local_preferences)
    if (!fs_session_set_codec_preferences (stream->priv->fs_session,
            stream->priv->local_preferences,
            &stream->priv->construction_error))
      return obj;


  if (g_object_class_find_property (
          G_OBJECT_GET_CLASS (stream->priv->fs_session),
          "no-rtcp-timeout"))
    g_object_set (stream->priv->fs_session, "no-rtcp-timeout", 0, NULL);

  g_signal_connect (stream->priv->fs_stream, "src-pad-added",
      G_CALLBACK (cb_fs_stream_src_pad_added), stream);

  stream->priv->send_local_codecs = TRUE;
  tpmedia_stream_try_sending_codecs (stream);

  return obj;
}

static void
tpmedia_stream_dispose (GObject *object)
{
  TpmediaStream *stream = TPMEDIA_STREAM (object);
  TpmediaStreamPrivate *priv = stream->priv;

  if (priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = priv->stream_handler_proxy;

      g_signal_handlers_disconnect_by_func (
          priv->stream_handler_proxy, invalidated_cb, stream);

      priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  if (priv->fs_stream)
    {
      tpmedia_stream_free_resource (stream,
          TP_MEDIA_STREAM_DIRECTION_SEND);

      g_object_unref (priv->fs_stream);

      tpmedia_stream_free_resource (stream,
          TP_MEDIA_STREAM_DIRECTION_RECEIVE);

      priv->fs_stream = NULL;
    }

  if (priv->fs_session)
    {
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (priv->local_preferences)
    {
      fs_codec_list_destroy (priv->local_preferences);
      priv->local_preferences = NULL;
    }

  if (G_OBJECT_CLASS (tpmedia_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tpmedia_stream_parent_class)->dispose (object);
}

static void
tpmedia_stream_class_init (TpmediaStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpmediaStreamPrivate));

  object_class->set_property = tpmedia_stream_set_property;
  object_class->get_property = tpmedia_stream_get_property;
  object_class->constructor = tpmedia_stream_constructor;
  object_class->dispose = tpmedia_stream_dispose;

  param_spec = g_param_spec_object ("channel",
                                    "Telepathy channel",
                                    "The TpmediaChannel this stream"
                                    " is in",
                                    TPMEDIA_TYPE_CHANNEL,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CHANNEL,
      param_spec);

  param_spec = g_param_spec_object ("farsight-conference",
                                    "Farsight conference",
                                    "The Farsight conference this stream will "
                                    "create streams within.",
                                    FS_TYPE_CONFERENCE,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_FARSIGHT_CONFERENCE,
      param_spec);

  param_spec = g_param_spec_object ("farsight-participant",
                                    "Farsight participant",
                                    "The Farsight participant this stream will "
                                    "create streams for.",
                                    FS_TYPE_PARTICIPANT,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_FARSIGHT_PARTICIPANT,
      param_spec);

  param_spec = g_param_spec_object ("proxy", "TpMediaStreamHandler proxy",
      "The stream handler proxy which this stream interacts with.",
      TP_TYPE_MEDIA_STREAM_HANDLER,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PROXY, param_spec);

  param_spec = g_param_spec_uint ("stream-id",
                                  "stream ID",
                                  "A number identifying this stream within "
                                  "its channel.",
                                  0, G_MAXUINT, 0,
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_READWRITE |
                                  G_PARAM_STATIC_NICK |
                                  G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STREAM_ID, param_spec);

  param_spec = g_param_spec_uint ("media-type",
                                  "stream media type",
                                  "The Telepathy stream media type (ie audio "
                                  "or video)",
                                  TP_MEDIA_STREAM_TYPE_AUDIO,
                                  TP_MEDIA_STREAM_TYPE_VIDEO,
                                  TP_MEDIA_STREAM_TYPE_AUDIO,
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_READWRITE |
                                  G_PARAM_STATIC_NICK |
                                  G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_MEDIA_TYPE, param_spec);

  param_spec = g_param_spec_uint ("direction",
                                  "stream direction",
                                  "The Telepathy stream direction",
                                  TP_MEDIA_STREAM_DIRECTION_NONE,
                                  TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
                                  TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_READWRITE |
                                  G_PARAM_STATIC_NICK |
                                  G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_DIRECTION, param_spec);

  param_spec = g_param_spec_pointer ("nat-properties",
                                     "NAT properties",
                                     "A pointer to a "
                                     "TpStreamEngineNatProperties structure "
                                     "detailing which NAT traversal method "
                                     "and parameters to use for this stream.",
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_NICK |
                                     G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_NAT_PROPERTIES,
      param_spec);

  param_spec = g_param_spec_object ("sink-pad",
                                    "Sink pad for this stream",
                                    "This sink pad that data has to be sent",
                                     GST_TYPE_PAD,
                                     G_PARAM_READABLE |
                                     G_PARAM_STATIC_NICK |
                                     G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SINK_PAD,
      param_spec);


  param_spec = g_param_spec_boxed ("codec-preferences",
                                   "Local codec preferences",
                                   "A GList of FsCodec representing preferences to be passed to the fs_session_set_local_preferences() function",
                                    FS_TYPE_CODEC_LIST,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_LOCAL_PREFERENCES,
      param_spec);


  signals[CLOSED] =
    g_signal_new ("closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ERROR] =
    g_signal_new ("error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[REQUEST_RESOURCE] =
    g_signal_new ("request-resource",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  g_signal_accumulator_true_handled, NULL,
                  _tpmedia_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1, G_TYPE_UINT);

  signals[FREE_RESOURCE] =
    g_signal_new ("free-resource",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[SRC_PAD_ADDED] =
    g_signal_new ("src-pad-added",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _tpmedia_marshal_VOID__OBJECT_BOXED,
                  G_TYPE_NONE, 2, GST_TYPE_PAD, FS_TYPE_CODEC);
}

/* dummy callback handler for async calling calls with no return values */
static void
async_method_callback (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                       const GError *error,
                       gpointer user_data,
                       GObject *weak_object)
{
  TpmediaStream *self = TPMEDIA_STREAM (weak_object);

  if (error != NULL)
    {
      g_warning ("Error calling %s: %s", (gchar *) user_data, error->message);
      g_signal_emit (self, signals[ERROR], 0);
    }
}

static void
cb_fs_new_local_candidate (TpmediaStream *self,
    FsCandidate *candidate)
{
  GPtrArray *transports;
  GValue transport = { 0, };
  TpMediaStreamBaseProto proto;
  TpMediaStreamTransportType type;

  transports = g_ptr_array_new ();

  g_value_init (&transport, TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT);
  g_value_set_static_boxed (&transport,
      dbus_g_type_specialized_construct (TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT));

  switch (candidate->proto) {
  case FS_NETWORK_PROTOCOL_UDP:
    proto = TP_MEDIA_STREAM_BASE_PROTO_UDP;
    break;
  case FS_NETWORK_PROTOCOL_TCP:
    proto = TP_MEDIA_STREAM_BASE_PROTO_TCP;
    break;
  default:
    g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
        G_STRFUNC);
    return;
  }

  switch (candidate->type) {
  case FS_CANDIDATE_TYPE_HOST:
    type = TP_MEDIA_STREAM_TRANSPORT_TYPE_LOCAL;
    break;
  case FS_CANDIDATE_TYPE_SRFLX:
  case FS_CANDIDATE_TYPE_PRFLX:
    type = TP_MEDIA_STREAM_TRANSPORT_TYPE_DERIVED;
    break;
  case FS_CANDIDATE_TYPE_RELAY:
    type = TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY;
    break;
  default:
    g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
        G_STRFUNC);
    return;
  }

  DEBUG (self, "candidate->ip = '%s'", candidate->ip);

  dbus_g_type_struct_set (&transport,
      0, candidate->component_id,
      1, candidate->ip,
      2, candidate->port,
      3, proto,
      4, "RTP",
      5, "AVP",
      6, (double) candidate->priority / 65536.0,
      7, type,
      8, candidate->username,
      9, candidate->password,
      G_MAXUINT);

      g_ptr_array_add (transports, g_value_get_boxed (&transport));

  tp_cli_media_stream_handler_call_new_native_candidate (
      self->priv->stream_handler_proxy, -1, candidate->foundation, transports,
      async_method_callback, "Media.StreamHandler::NativeCandidatesPrepared",
      NULL, (GObject *) self);
}

/*
 * small helper function to help converting a
 * telepathy dbus candidate to a list of FarsightTransportInfos
 * nothing is copied, so always keep the usage of this within a function
 * if you need to do multiple candidates, call this repeatedly and
 * g_list_join them together.
 * Free the list using free_fs_transports
 */
static GList *
tp_transports_to_fs (const gchar* foundation, const GPtrArray *transports)
{
  GList *fs_trans_list = NULL;
  GValueArray *transport;
  FsCandidate *fs_candidate;
  guint i;

  for (i=0; i< transports->len; i++)
    {
      transport = g_ptr_array_index (transports, i);
      FsNetworkProtocol proto;
      FsCandidateType type;

      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 0)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 1)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 2)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 3)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 4)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 5)));
      g_assert(G_VALUE_HOLDS_DOUBLE (g_value_array_get_nth (transport, 6)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 7)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 8)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 9)));

      switch (g_value_get_uint (g_value_array_get_nth (transport, 7)))
        {
        case TP_MEDIA_STREAM_TRANSPORT_TYPE_LOCAL:
          type = FS_CANDIDATE_TYPE_HOST;
          break;
        case TP_MEDIA_STREAM_TRANSPORT_TYPE_DERIVED:
          type = FS_CANDIDATE_TYPE_SRFLX;
          /* or S_CANDIDATE_TYPE_PRFLX .. if can't know */
          break;
        case TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY:
          type = FS_CANDIDATE_TYPE_RELAY;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          type = FS_CANDIDATE_TYPE_HOST;
        }

      switch (g_value_get_uint (g_value_array_get_nth (transport, 3)))
        {
        case TP_MEDIA_STREAM_BASE_PROTO_UDP:
          proto = FS_NETWORK_PROTOCOL_UDP;
          break;
        case TP_MEDIA_STREAM_BASE_PROTO_TCP:
          proto = FS_NETWORK_PROTOCOL_TCP;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          proto = FS_NETWORK_PROTOCOL_UDP;
        }

      fs_candidate = fs_candidate_new (foundation,
          g_value_get_uint (g_value_array_get_nth (transport, 0)), /*component*/
          type, proto,

          g_value_dup_string (g_value_array_get_nth (transport, 1)), /* ip */
          g_value_get_uint (g_value_array_get_nth (transport, 2))); /* port */

      fs_candidate->priority = (gint)
          g_value_get_double (g_value_array_get_nth (transport, 6)) * 65536.0;
      fs_candidate->username =
          g_value_dup_string (g_value_array_get_nth (transport, 8));
      fs_candidate->password =
          g_value_dup_string (g_value_array_get_nth (transport, 9));

      fs_trans_list = g_list_prepend (fs_trans_list, fs_candidate);
    }
  fs_trans_list = g_list_reverse (fs_trans_list);

  return fs_trans_list;
}

static FsMediaType
tp_media_type_to_fs (TpMediaStreamType type)
{
  switch (type)
    {
    case TP_MEDIA_STREAM_TYPE_AUDIO:
      return FS_MEDIA_TYPE_AUDIO;
    case TP_MEDIA_STREAM_TYPE_VIDEO:
      return FS_MEDIA_TYPE_VIDEO;
    default:
      return FS_MEDIA_TYPE_APPLICATION;
    }
}

/*
 * Small helper function to help converting a list of FarsightCodecs
 * to a Telepathy codec list.
 */
static GPtrArray *
fs_codecs_to_tp (TpmediaStream *stream,
                 const GList *codecs)
{
  GPtrArray *tp_codecs;
  const GList *el;

  tp_codecs = g_ptr_array_new ();

  for (el = codecs; el; el = g_list_next (el))
    {
      FsCodec *fsc = el->data;
      GValue codec = { 0, };
      TpMediaStreamType type;
      GHashTable *params;
      GList *cur;

      switch (fsc->media_type) {
        case FS_MEDIA_TYPE_AUDIO:
          type = TP_MEDIA_STREAM_TYPE_AUDIO;
          break;
        case FS_MEDIA_TYPE_VIDEO:
          type = TP_MEDIA_STREAM_TYPE_VIDEO;
          break;
        default:
          g_critical ("%s: FarsightCodec [%d, %s]'s media_type has an invalid value",
              G_STRFUNC, fsc->id, fsc->encoding_name);
          return NULL;
      }

      /* fill in optional parameters */
      params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      for (cur = fsc->optional_params; cur != NULL; cur = cur->next)
        {
          FsCodecParameter *param = (FsCodecParameter *) cur->data;

          g_hash_table_insert (params, g_strdup (param->name),
                               g_strdup (param->value));
        }

      g_value_init (&codec, TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_CODEC);
      g_value_set_static_boxed (&codec,
          dbus_g_type_specialized_construct (TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_CODEC));

      dbus_g_type_struct_set (&codec,
          0, fsc->id,
          1, fsc->encoding_name,
          2, type,
          3, fsc->clock_rate,
          4, fsc->channels,
          5, params,
          G_MAXUINT);

      g_hash_table_destroy (params);

      DEBUG (stream, "adding codec %s [%d]", fsc->encoding_name, fsc->id);

      g_ptr_array_add (tp_codecs, g_value_get_boxed (&codec));
    }

  return tp_codecs;
}

static void
add_remote_candidate (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                      const gchar *candidate,
                      const GPtrArray *transports,
                      gpointer user_data G_GNUC_UNUSED,
                      GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  GError *error = NULL;
  GList *fscandidates;

  DEBUG (self, "adding remote candidate %s", candidate);

  fscandidates = tp_transports_to_fs (candidate, transports);

  if (!fs_stream_set_remote_candidates (self->priv->fs_stream,
          fscandidates, &error))
      tpmedia_stream_error (self, 0, error->message);

  fs_candidate_list_destroy (fscandidates);
  g_clear_error (&error);
}

static void
remove_remote_candidate (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                         const gchar *candidate G_GNUC_UNUSED,
                         gpointer user_data G_GNUC_UNUSED,
                         GObject *object G_GNUC_UNUSED)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  tpmedia_stream_error (self, 0,
      "RemoveRemoteCandidate is NOT implemented by plugin");
}

static void
set_active_candidate_pair (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                           const gchar *native_candidate,
                           const gchar *remote_candidate,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  GError *error = NULL;

  if (!fs_stream_select_candidate_pair (self->priv->fs_stream,
          native_candidate,
          remote_candidate,
          &error))
    {
      if (error->domain == FS_ERROR && error->code == FS_ERROR_NOT_IMPLEMENTED)
        DEBUG (self, "Called not implemented SetActiveCandidatePair");
      else
        tpmedia_stream_error (self, 0, error->message);
    }

  g_clear_error (&error);
}

static void
set_remote_candidate_list (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                           const GPtrArray *candidates,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  guint i;
  GList *fs_candidates = NULL;
  GError *error = NULL;

  for (i = 0; i < candidates->len; i++)
    {
      GPtrArray *transports = NULL;
      gchar *foundation;
      GValueArray *candidate;

      candidate = g_ptr_array_index (candidates, i);

      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (candidate,0)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (candidate, 1)) ==
          TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT_LIST);

      foundation =
        (gchar*) g_value_get_string (g_value_array_get_nth (candidate, 0));
      transports =
          g_value_get_boxed (g_value_array_get_nth (candidate, 1));

      fs_candidates = g_list_concat (fs_candidates,
          tp_transports_to_fs (foundation, transports));
    }

  if (!fs_stream_set_remote_candidates (self->priv->fs_stream,
                  fs_candidates, &error))
    tpmedia_stream_error (self, 0, error->message);

  g_clear_error (&error);
  fs_candidate_list_destroy (fs_candidates);
}

static void
fill_fs_params (gpointer key, gpointer value, gpointer user_data)
{
  GList **fs_params = (GList **) user_data;
  FsCodecParameter *param = g_new0(FsCodecParameter,1);
  param->name = key;
  param->value = value;
  *fs_params = g_list_prepend (*fs_params, param);
}

static void
set_remote_codecs (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                   const GPtrArray *codecs,
                   gpointer user_data G_GNUC_UNUSED,
                   GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  GValueArray *codec;
  GHashTable *params = NULL;
  GList *fs_params = NULL;
  GList *fs_remote_codecs = NULL;
  guint i;
  GError *error = NULL;

  DEBUG (self, "called");

  for (i = 0; i < codecs->len; i++)
    {
      FsCodec *fs_codec = NULL;


      codec = g_ptr_array_index (codecs, i);

      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,0)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (codec,1)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,2)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,3)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,4)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (codec, 5)) ==
                               DBUS_TYPE_G_STRING_STRING_HASHTABLE);


      fs_codec = fs_codec_new (
          g_value_get_uint (g_value_array_get_nth (codec, 0)), /* id */
          g_value_get_string (g_value_array_get_nth (codec, 1)), /* encoding_name */
          g_value_get_uint (g_value_array_get_nth (codec, 2)), /* media_type */
          g_value_get_uint (g_value_array_get_nth (codec, 3))); /* clock_rate */

      fs_codec->channels =
          g_value_get_uint (g_value_array_get_nth (codec, 4));

      params = g_value_get_boxed (g_value_array_get_nth (codec, 5));
      fs_params = NULL;
      g_hash_table_foreach (params, fill_fs_params, &fs_params);

      fs_codec->optional_params = fs_params;

      DEBUG (self, "adding remote codec %s [%d]",
          fs_codec->encoding_name, fs_codec->id);

      fs_remote_codecs = g_list_prepend (fs_remote_codecs, fs_codec);
  }
  fs_remote_codecs = g_list_reverse (fs_remote_codecs);

  if (!fs_stream_set_remote_codecs (self->priv->fs_stream, fs_remote_codecs,
          &error)) {
    /*
     * Call the error method with the proper thing here
     */
    gchar *str = g_strdup_printf ("Codec negotiation failed: %s",
        error->message);
    tpmedia_stream_error (self, 0, str);
    g_free (str);
    return;
  }

  self->priv->send_supported_codecs = TRUE;
  tpmedia_stream_try_sending_codecs (self);
}

static void
set_stream_playing (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                    gboolean play,
                    gpointer user_data G_GNUC_UNUSED,
                    GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  FsStreamDirection current_direction;
  gboolean playing;

  g_assert (self->priv->fs_stream != NULL);

  DEBUG (self, "%d", play);

  g_object_get (self->priv->fs_stream, "direction", &current_direction, NULL);

  playing = (current_direction & FS_DIRECTION_RECV) != 0;

  /* We're already in the right state */
  if (play == playing)
    return;

  if (play)
    {
      if (!self->priv->held) {
        if (tpmedia_stream_request_resource (self,
                TP_MEDIA_STREAM_DIRECTION_RECEIVE))
          {
            g_object_set (self->priv->fs_stream,
                "direction", current_direction | FS_DIRECTION_RECV,
                NULL);
          }
        else
          {
            tpmedia_stream_error (self, 0, "Resource Unavailable");
          }
      }
      self->priv->desired_direction |= FS_DIRECTION_RECV;
    }
  else
    {
      if (!self->priv->held) {
        tpmedia_stream_free_resource (self,
            TP_MEDIA_STREAM_DIRECTION_RECEIVE);

        g_object_set (self->priv->fs_stream,
            "direction", current_direction & ~(FS_DIRECTION_RECV),
            NULL);
      }
      self->priv->desired_direction &= ~(FS_DIRECTION_RECV);
    }
}

static void
set_stream_sending (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                    gboolean send,
                    gpointer user_data G_GNUC_UNUSED,
                    GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);
  FsStreamDirection current_direction;
  gboolean sending;

  g_assert (self->priv->fs_stream != NULL);

  DEBUG (self, "%d", send);

  g_object_get (self->priv->fs_stream, "direction", &current_direction, NULL);


  sending = (current_direction & FS_DIRECTION_SEND) != 0;

  /* We're already in the right state */
  if (send == sending)
    return;

  if (send)
    {
      if (!self->priv->held) {
        if (tpmedia_stream_request_resource (self,
                TP_MEDIA_STREAM_DIRECTION_SEND))
          {
            g_object_set (self->priv->fs_stream,
                "direction", current_direction | FS_DIRECTION_SEND,
                NULL);
          }
        else
          {
            tpmedia_stream_error (self, 0, "Resource Unavailable");
          }
      }
      self->priv->desired_direction |= FS_DIRECTION_SEND;
    }
  else
    {
      if (!self->priv->held) {
        tpmedia_stream_free_resource (self,
            TP_MEDIA_STREAM_DIRECTION_RECEIVE);

        g_object_set (self->priv->fs_stream,
            "direction", current_direction & ~(FS_DIRECTION_SEND),
            NULL);
      }
      self->priv->desired_direction &= ~(FS_DIRECTION_SEND);
    }
}

static gboolean
tpmedia_stream_request_resource (TpmediaStream *self,
                                          TpMediaStreamDirection dir)
{
  gboolean resource_available = FALSE;

  if ((self->priv->has_resource & dir) == dir)
    return TRUE;

  g_signal_emit (self, signals[REQUEST_RESOURCE], 0,
      dir & ~self->priv->has_resource,
      &resource_available);

  /* Make sure we have access to the resource */
  if (resource_available)
    {
      self->priv->has_resource |= dir;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
tpmedia_stream_free_resource (TpmediaStream *self,
    TpMediaStreamDirection dir)
{
  if ((self->priv->has_resource & dir) == 0)
    return;

  g_signal_emit (self, signals[FREE_RESOURCE], 0,
      self->priv->has_resource & dir);
  self->priv->has_resource &= ~dir;
}

static void
set_stream_held (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                 gboolean held,
                 gpointer user_data G_GNUC_UNUSED,
                 GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  DEBUG (self, "Holding : %d", held);

  if (held == self->priv->held)
    return;

  if (held)
    {
       g_object_set (self->priv->fs_stream,
            "direction", FS_DIRECTION_NONE,
            NULL);

       tpmedia_stream_free_resource (self,
                TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
       /* Send success message */
       if (self->priv->stream_handler_proxy)
         {
           tp_cli_media_stream_handler_call_hold_state (
             self->priv->stream_handler_proxy, -1, TRUE,
             async_method_callback, "Media.StreamHandler::HoldState",
             NULL, (GObject *) self);
         }
    }
  else
    {
      if (tpmedia_stream_request_resource (self,
              self->priv->desired_direction))
        {
           g_object_set (self->priv->fs_stream,
               "direction", self->priv->desired_direction,
               NULL);
           tp_cli_media_stream_handler_call_hold_state (
             self->priv->stream_handler_proxy, -1, FALSE,
             async_method_callback, "Media.StreamHandler::HoldState",
             NULL, (GObject *) self);
        }
      else
        {
          tpmedia_stream_error (self, 0, "Error unholding stream");
        }
    }
}

static void
start_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                       guchar event,
                       gpointer user_data G_GNUC_UNUSED,
                       GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  g_assert (self->priv->fs_session != NULL);

  DEBUG (self, "called with event %u", event);

  /* this week, volume is 8, for the sake of argument... */

  if (!fs_session_start_telephony_event (self->priv->fs_session, event, 8,
          FS_DTMF_METHOD_AUTO))
    WARNING (self, "sending event %u failed", event);
}

static void
stop_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                      gpointer user_data G_GNUC_UNUSED,
                      GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  g_assert (self->priv->fs_session  != NULL);

  DEBUG (self, "called");

  if (!fs_session_stop_telephony_event (self->priv->fs_session,
          FS_DTMF_METHOD_AUTO))
    WARNING (self, "stopping event failed");
}

static void
close (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
       gpointer user_data G_GNUC_UNUSED,
       GObject *object)
{
  TpmediaStream *self = TPMEDIA_STREAM (object);

  DEBUG (self, "close requested by connection manager");

  g_object_set (self->priv->fs_stream,
            "direction", FS_DIRECTION_NONE,
            NULL);
  tpmedia_stream_free_resource (self,
      TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
cb_fs_recv_codecs_changed (TpmediaStream *self,
    GList *codecs)
{
  guint id;
  GList *item;

  for (item = codecs;
       item;
       item = g_list_next (item))
    {
      gchar *str = fs_codec_to_string (item->data);

      DEBUG (self, "receiving codec: %s", str);
      g_free (str);
    }

  id = ((FsCodec*)codecs->data)->id;

  tp_cli_media_stream_handler_call_codec_choice
      (self->priv->stream_handler_proxy, -1, id,
          async_method_callback, "Media.StreamHandler::CodecChoice",
          NULL, (GObject *) self);
}

static void
cb_fs_new_active_candidate_pair (TpmediaStream *self,
    FsCandidate *local_candidate,
    FsCandidate *remote_candidate)
{
  DEBUG (self, "called");

  tp_cli_media_stream_handler_call_new_active_candidate_pair (
    self->priv->stream_handler_proxy, -1, local_candidate->foundation,
    remote_candidate->foundation,
    async_method_callback, "Media.StreamHandler::NewActiveCandidatePair",
    NULL, (GObject *) self);

  tp_cli_media_stream_handler_call_stream_state (
    self->priv->stream_handler_proxy, -1, TP_MEDIA_STREAM_STATE_CONNECTED,
    async_method_callback, "Media.StreamHandler::SetStreamState",
    NULL, (GObject *) self);
}

static void
cb_fs_local_candidates_prepared (TpmediaStream *self)
{
  DEBUG (self, "called");

  tp_cli_media_stream_handler_call_native_candidates_prepared (
    self->priv->stream_handler_proxy, -1, async_method_callback,
    "Media.StreamHandler::NativeCandidatesPrepared",
    NULL, (GObject *) self);
}

static void
invalidated_cb (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                guint domain G_GNUC_UNUSED,
                gint code G_GNUC_UNUSED,
                gchar *message G_GNUC_UNUSED,
                gpointer user_data)
{
  TpmediaStream *stream = TPMEDIA_STREAM (user_data);

  if (stream->priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = stream->priv->stream_handler_proxy;

      stream->priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }
}

void
tpmedia_stream_error (TpmediaStream *self,
                               guint error,
                               const gchar *message)
{
  g_message ("%s: stream error errorno=%d error=%s", G_STRFUNC, error, message);

  tp_cli_media_stream_handler_call_error (self->priv->stream_handler_proxy,
      -1, error, message, NULL, NULL, NULL, NULL);

  g_signal_emit (self, signals[ERROR], 0);
}


/**
 * tpmedia_stream_bus_message:
 * @stream: A #TpmediaStream
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
tpmedia_stream_bus_message (TpmediaStream *stream,
    GstMessage *message)
{
  const gchar *debug = NULL;
  const GstStructure *s = gst_message_get_structure (message);
  const GValue *value = NULL;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return FALSE;

  if (gst_structure_has_name (s, "farsight-error"))
    {
      GObject *object;

      value = gst_structure_get_value (s, "src-object");
      object = g_value_get_object (value);

      if (object == (GObject*) stream->priv->fs_session ||
          object == (GObject*) stream->priv->fs_stream)
        {
          const gchar *msg;
          FsError errorno;
          GEnumClass *enumclass;
          GEnumValue *enumvalue;

          value = gst_structure_get_value (s, "error-no");
          errorno = g_value_get_enum (value);
          msg = gst_structure_get_string (s, "error-msg");
          debug = gst_structure_get_string (s, "debug-msg");

          /*
           * We ignore the Unknown Cname error because current signalling
           * does no provide us with a cname
           */
          if (errorno == FS_ERROR_UNKNOWN_CNAME)
            return TRUE;


          enumclass = g_type_class_ref (FS_TYPE_ERROR);
          enumvalue = g_enum_get_value (enumclass, errorno);
          WARNING (stream, "error (%s (%d)): %s : %s",
              enumvalue->value_nick, errorno, msg, debug);
          g_type_class_unref (enumclass);

          tpmedia_stream_error (stream, 0, msg);
          return TRUE;
        }
    }
  else if (gst_structure_has_name (s, "farsight-new-local-candidate"))
    {
      FsStream *fsstream;
      FsCandidate *candidate;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      value = gst_structure_get_value (s, "candidate");
      candidate = g_value_get_boxed (value);

      cb_fs_new_local_candidate (stream, candidate);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farsight-local-candidates-prepared"))
    {
      FsStream *fsstream;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      cb_fs_local_candidates_prepared (stream);

      return TRUE;
    }
  else if (gst_structure_has_name (s, "farsight-new-active-candidate-pair"))
    {
      FsStream *fsstream;
      FsCandidate *local_candidate;
      FsCandidate *remote_candidate;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      value = gst_structure_get_value (s, "local-candidate");
      local_candidate = g_value_get_boxed (value);

      value = gst_structure_get_value (s, "remote-candidate");
      remote_candidate = g_value_get_boxed (value);

      cb_fs_new_active_candidate_pair (stream, local_candidate, remote_candidate);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farsight-current-recv-codecs-changed"))
    {
      FsStream *fsstream;
      GList *codecs;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      value = gst_structure_get_value (s, "codecs");
      codecs = g_value_get_boxed (value);

      cb_fs_recv_codecs_changed (stream, codecs);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farsight-codecs-changed"))
    {
      FsSession *fssession;
      const GValue *value;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != stream->priv->fs_session)
        return FALSE;

      tpmedia_stream_try_sending_codecs (stream);
    }
  else if (gst_structure_has_name (s, "farsight-send-codec-changed"))
    {
      FsSession *fssession;
      const GValue *value;
      FsCodec *codec = NULL;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != stream->priv->fs_session)
        return FALSE;

      g_object_get (fssession, "current-send-codec", &codec, NULL);

      if (codec)
        {
          DEBUG (stream, "Send codec changed: " FS_CODEC_FORMAT,
              FS_CODEC_ARGS (codec));

          fs_codec_destroy (codec);
        }
    }

  return FALSE;
}

static void
cb_fs_stream_src_pad_added (FsStream *fsstream G_GNUC_UNUSED,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data)
{
  TpmediaStream *self = TPMEDIA_STREAM (user_data);

  DEBUG (self, "New pad");

  g_signal_emit (self, signals[SRC_PAD_ADDED], 0, pad, codec);
}

TpmediaStream *
tpmedia_stream_new (gpointer channel,
    FsConference *conference,
    FsParticipant *participant,
    TpMediaStreamHandler *proxy,
    guint stream_id,
    TpMediaStreamType media_type,
    TpMediaStreamDirection direction,
    TpStreamEngineNatProperties *nat_props,
    GList *local_preferences,
    GError **error)

{
  TpmediaStream *self = NULL;

  self = g_object_new (TPMEDIA_TYPE_STREAM,
      "channel", channel,
      "farsight-conference", conference,
      "farsight-participant", participant,
      "proxy", proxy,
      "stream-id", stream_id,
      "media-type", media_type,
      "direction", direction,
      "nat-properties", nat_props,
      "codec-preferences", local_preferences,
      NULL);

  if (self->priv->construction_error)
    {
      tpmedia_stream_error (self, 0, self->priv->construction_error->message);
      g_propagate_error (error, self->priv->construction_error);
      g_object_unref (self);
      return NULL;
    }

  return self;
}

static void
tpmedia_stream_try_sending_codecs (TpmediaStream *stream)
{
  gboolean ready = FALSE;
  GList *fscodecs = NULL;
  GList *item = NULL;

  if (!stream->priv->send_supported_codecs && !stream->priv->send_local_codecs)
    return;

  g_object_get (stream->priv->fs_session, "codecs-ready", &ready, NULL);

  if (!ready)
    return;

  g_object_get (stream->priv->fs_session, "codecs", &fscodecs, NULL);

  for(item = fscodecs; item; item = g_list_next (item))
    {
      gchar *tmp = fs_codec_to_string (item->data);
      DEBUG (stream, "%s", tmp);
      g_free (tmp);
    }

  if (stream->priv->send_local_codecs)
    {
      GPtrArray *tpcodecs = fs_codecs_to_tp (stream, fscodecs);

      DEBUG (stream, "calling MediaStreamHandler::Ready");
      tp_cli_media_stream_handler_call_ready (
          stream->priv->stream_handler_proxy,
          -1, tpcodecs, async_method_callback, "Media.StreamHandler::Ready",
          NULL, (GObject *) stream);
      stream->priv->send_local_codecs = FALSE;
    }


  if (stream->priv->send_supported_codecs)
    {
      GPtrArray *tpcodecs = fs_codecs_to_tp (stream, fscodecs);

      DEBUG (stream, "calling MediaStreamHandler::SupportedCodecs");
      tp_cli_media_stream_handler_call_supported_codecs (
          stream->priv->stream_handler_proxy,
          -1, tpcodecs, async_method_callback,
          "Media.StreamHandler::SupportedCodecs", NULL, (GObject *) stream);
      stream->priv->send_supported_codecs = FALSE;
    }

  fs_codec_list_destroy (fscodecs);
}
