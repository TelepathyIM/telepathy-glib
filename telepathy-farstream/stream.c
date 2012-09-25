/*
 * stream.c - Source for TfStream
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

/**
 * SECTION:stream
 * @short_description: Handles a media Stream
 *
 * These objects handle media streams and wrap the appropriate Farstream
 * objects. It is used to interact on a stream level with the other parts
 * of the media pipeline and the proper UI.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <telepathy-glib/telepathy-glib.h>

#include <farstream/fs-conference.h>
#include <farstream/fs-rtp.h>
#include <farstream/fs-utils.h>

#include "stream.h"
#include "media-signalling-channel.h"
#include "utils.h"

G_DEFINE_TYPE (TfStream, tf_stream, G_TYPE_OBJECT);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d %p (%s) %s: " format, \
    stream->stream_id, stream,                                   \
    (stream->priv->media_type == TP_MEDIA_STREAM_TYPE_AUDIO) ? "audio"  \
                                                      : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define WARNING(stream, format, ...) \
  g_warning ("stream %d %p (%s) %s: " format, \
    stream->stream_id, stream,                                   \
    (stream->priv->media_type == TP_MEDIA_STREAM_TYPE_AUDIO) ? "audio"  \
                                                      : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define STREAM_PRIVATE(o) ((o)->priv)

#define TF_STREAM_LOCK(o)   (g_static_mutex_lock (&(o)->priv->mutex))
#define TF_STREAM_UNLOCK(o) (g_static_mutex_unlock (&(o)->priv->mutex))

static TpMediaStreamError fserrorno_to_tperrorno (FsError fserror);

struct DtmfEvent {
  gint codec_id;
  guint event_id;
};

struct _TfStreamPrivate
{
  TfMediaSignallingChannel *channel;
  FsConference *fs_conference;
  FsParticipant *fs_participant;
  FsSession *fs_session;
  FsStream *fs_stream;
  TpMediaStreamType media_type;
  TpMediaStreamDirection direction;
  const TfNatProperties *nat_props;
  GList *local_preferences;

  TpMediaStreamHandler *stream_handler_proxy;

  FsStreamDirection desired_direction;
  gboolean held;
  TpMediaStreamDirection has_resource;

  GList *local_candidates;

  GList *last_sent_codecs;

  gboolean send_local_codecs;
  gboolean send_supported_codecs;

  guint tos;

  GHashTable *feedback_messages;
  GPtrArray *header_extensions;

  GStaticMutex mutex;
  guint idle_connected_id; /* Protected by mutex */
  gboolean disposed; /* Protected by mutex */

  TpMediaStreamState current_state;

  NewStreamCreatedCb *new_stream_created_cb;

  GQueue events_to_send;

  gboolean sending_telephony_event;
};

enum
{
  CLOSED,
  ERROR_SIGNAL,
  REQUEST_RESOURCE,
  FREE_RESOURCE,
  SRC_PAD_ADDED,
  RESTART_SOURCE,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

/* properties */
enum
{
  PROP_CHANNEL = 1,
  PROP_FARSTREAM_CONFERENCE,
  PROP_FARSTREAM_SESSION,
  PROP_FARSTREAM_STREAM,
  PROP_FARSTREAM_PARTICIPANT,
  PROP_PROXY,
  PROP_STREAM_ID,
  PROP_MEDIA_TYPE,
  PROP_DIRECTION,
  PROP_NAT_PROPERTIES,
  PROP_SINK_PAD,
  PROP_LOCAL_PREFERENCES,
  PROP_TOS,
  PROP_RESOURCES
};

static void get_all_properties_cb (TpProxy *proxy,
    GHashTable *out_Properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object);
static gboolean tf_stream_request_resource (
    TfStream *self,
    TpMediaStreamDirection dir);
static void tf_stream_free_resource (TfStream *self,
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

static void start_named_telephony_event (TpMediaStreamHandler *proxy,
    guchar event, guint codecid, gpointer user_data, GObject *object);

static void start_sound_telephony_event (TpMediaStreamHandler *proxy,
    guchar event, gpointer user_data, GObject *object);

static void stop_telephony_event (TpMediaStreamHandler *proxy,
    gpointer user_data, GObject *object);

static void stream_close (TpMediaStreamHandler *proxy,
    gpointer user_data, GObject *object);

static void set_remote_feedback_messages (TpMediaStreamHandler *proxy,
    GHashTable *messages, gpointer user_data, GObject *object);

static void set_remote_header_extensions (TpMediaStreamHandler *proxy,
    const GPtrArray *header_extensions, gpointer user_data, GObject *object);

static void invalidated_cb (TpMediaStreamHandler *proxy,
    guint domain, gint code, gchar *message, gpointer user_data);

static TpMediaStreamBaseProto fs_network_proto_to_tp (FsNetworkProtocol proto,
    gboolean *valid);
static TpMediaStreamTransportType fs_candidate_type_to_tp (FsCandidateType type,
    gboolean *valid);
static GValueArray *fs_candidate_to_tp_array (const FsCandidate *candidate);

static GPtrArray *fs_codecs_to_tp (TfStream *stream,
    const GList *codecs);
static void async_method_callback (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data,
    GObject *weak_object);
static void tf_stream_shutdown (TfStream *self);


static void cb_fs_stream_src_pad_added (FsStream *fsstream G_GNUC_UNUSED,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data);

static void cb_fs_component_state_changed (TfStream *self,
    guint component,
    FsStreamState fsstate);


static void
tf_stream_init (TfStream *self)
{
  TfStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TF_TYPE_STREAM, TfStreamPrivate);

  self->priv = priv;
  g_static_mutex_init (&priv->mutex);
  priv->has_resource = TP_MEDIA_STREAM_DIRECTION_NONE;
  priv->current_state = TP_MEDIA_STREAM_STATE_DISCONNECTED;
  priv->sending_telephony_event = FALSE;

  g_queue_init (&priv->events_to_send);
}

static void
tf_stream_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  TfStream *self = TF_STREAM (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      g_value_set_object (value, self->priv->channel);
      break;
    case PROP_FARSTREAM_CONFERENCE:
      g_value_set_object (value, self->priv->fs_conference);
      break;
    case PROP_FARSTREAM_PARTICIPANT:
      g_value_set_object (value, self->priv->fs_participant);
      break;
    case PROP_FARSTREAM_SESSION:
      g_value_set_object (value, self->priv->fs_session);
      break;
    case PROP_FARSTREAM_STREAM:
      g_value_set_object (value, self->priv->fs_stream);
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
          (TfNatProperties *) self->priv->nat_props);
      break;
    case PROP_SINK_PAD:
      g_object_get_property (G_OBJECT (self->priv->fs_session),
          "sink-pad", value);
      break;
    case PROP_LOCAL_PREFERENCES:
      g_value_set_boxed (value, self->priv->local_preferences);
      break;
    case PROP_TOS:
      if (self->priv->fs_session)
        g_object_get_property (G_OBJECT (self->priv->fs_session), "tos", value);
      else
        g_value_set_uint (value, self->priv->tos);
      break;
    case PROP_RESOURCES:
      g_value_set_uint (value, self->priv->has_resource);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tf_stream_set_property (GObject      *object,
    guint         property_id,
    const GValue *value,
    GParamSpec   *pspec)
{
  TfStream *self = TF_STREAM (object);

  switch (property_id)
    {
    case PROP_CHANNEL:
      self->priv->channel =
          TF_MEDIA_SIGNALLING_CHANNEL (g_value_get_object (value));
      break;
    case PROP_FARSTREAM_CONFERENCE:
      self->priv->fs_conference =
          FS_CONFERENCE (g_value_dup_object (value));
      break;
    case PROP_FARSTREAM_PARTICIPANT:
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
    case PROP_TOS:
      self->priv->tos = g_value_get_uint (value);
      if (self->priv->fs_session)
        g_object_set_property (G_OBJECT (self->priv->fs_session), "tos", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

#define MAX_STREAM_TRANS_PARAMS 7

static GObject *
tf_stream_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TfStream *stream;


  obj = G_OBJECT_CLASS (tf_stream_parent_class)->
      constructor (type, n_props, props);

  stream = TF_STREAM (obj);

  g_signal_connect (stream->priv->stream_handler_proxy, "invalidated",
      G_CALLBACK (invalidated_cb), obj);

  tp_cli_dbus_properties_call_get_all (stream->priv->stream_handler_proxy,
      -1, "org.freedesktop.Telepathy.Media.StreamHandler",
      get_all_properties_cb, NULL, NULL, obj);

  return obj;
}

static void
tf_stream_dispose (GObject *object)
{
  TfStream *stream = TF_STREAM (object);
  TfStreamPrivate *priv = stream->priv;
  gpointer data;

  TF_STREAM_LOCK (stream);
  if (stream->priv->idle_connected_id)
    g_source_remove (stream->priv->idle_connected_id);
  stream->priv->idle_connected_id = 0;

  stream->priv->disposed = TRUE;
  TF_STREAM_UNLOCK (stream);


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
      tf_stream_free_resource (stream,
          TP_MEDIA_STREAM_DIRECTION_RECEIVE);

      fs_stream_destroy (priv->fs_stream);
      g_object_unref (priv->fs_stream);

      tf_stream_free_resource (stream,
          TP_MEDIA_STREAM_DIRECTION_SEND);

      priv->fs_stream = NULL;
    }

  if (priv->fs_session)
    {
      fs_session_destroy (priv->fs_session);
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (priv->fs_participant)
    {
      g_object_unref (priv->fs_participant);
      priv->fs_participant = NULL;
    }

  if (priv->fs_conference)
    {
      g_object_unref (priv->fs_conference);
      priv->fs_conference = NULL;
    }

  if (priv->local_preferences)
    {
      fs_codec_list_destroy (priv->local_preferences);
      priv->local_preferences = NULL;
    }

  if (priv->last_sent_codecs)
    {
      fs_codec_list_destroy (priv->last_sent_codecs);
      priv->last_sent_codecs = NULL;
    }

  if (priv->feedback_messages)
    g_boxed_free (TP_HASH_TYPE_RTCP_FEEDBACK_MESSAGE_MAP,
        priv->feedback_messages);
  priv->feedback_messages = NULL;

  if (priv->header_extensions)
    g_boxed_free (TP_ARRAY_TYPE_RTP_HEADER_EXTENSIONS_LIST,
        priv->header_extensions);
  priv->header_extensions = NULL;

  while ((data = g_queue_pop_head (&priv->events_to_send)))
    g_slice_free (struct DtmfEvent, data);

  fs_candidate_list_destroy (priv->local_candidates);
  priv->local_candidates = NULL;

  if (G_OBJECT_CLASS (tf_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tf_stream_parent_class)->dispose (object);
}

static void
tf_stream_class_init (TfStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TfStreamPrivate));

  object_class->set_property = tf_stream_set_property;
  object_class->get_property = tf_stream_get_property;
  object_class->constructor = tf_stream_constructor;
  object_class->dispose = tf_stream_dispose;

  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_object ("channel",
          "Telepathy channel",
          "The TfChannel this stream is in",
          TF_TYPE_MEDIA_SIGNALLING_CHANNEL,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FARSTREAM_CONFERENCE,
      g_param_spec_object ("farstream-conference",
          "Farstream conference",
          "The Farstream conference this stream will "
          "create streams within.",
          FS_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FARSTREAM_PARTICIPANT,
      g_param_spec_object ("farstream-participant",
          "Farstream participant",
          "The Farstream participant this stream will "
          "create streams for.",
          FS_TYPE_PARTICIPANT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FARSTREAM_SESSION,
      g_param_spec_object ("farstream-session",
          "Farstream session",
          "The Farstream session",
          FS_TYPE_SESSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FARSTREAM_STREAM,
      g_param_spec_object ("farstream-stream",
          "Farstream stream",
          "The Farstream stream",
          FS_TYPE_STREAM,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PROXY,
      g_param_spec_object ("proxy",
          "TpMediaStreamHandler proxy",
          "The stream handler proxy which this stream interacts with.",
          TP_TYPE_MEDIA_STREAM_HANDLER,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_STREAM_ID,
      g_param_spec_uint ("stream-id",
          "stream ID",
          "A number identifying this stream within "
          "its channel.",
          0, G_MAXUINT, 0,
          G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MEDIA_TYPE,
      g_param_spec_uint ("media-type",
          "stream media type",
          "The Telepathy stream media type"
          " (as a TpStreamMediaType)",
          TP_MEDIA_STREAM_TYPE_AUDIO,
          TP_MEDIA_STREAM_TYPE_VIDEO,
          TP_MEDIA_STREAM_TYPE_AUDIO,
          G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_DIRECTION,
      g_param_spec_uint ("direction",
          "stream direction",
          "The Telepathy stream direction"
          " (a TpMediaStreamDirection)",
          TP_MEDIA_STREAM_DIRECTION_NONE,
          TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
          TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
          G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_NAT_PROPERTIES,
      g_param_spec_pointer ("nat-properties",
          "NAT properties",
          "A pointer to a "
          "TfNatProperties structure "
          "detailing which NAT traversal method "
          "and parameters to use for this stream",
          G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_WRITABLE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_SINK_PAD,
      g_param_spec_object ("sink-pad",
          "Sink pad for this stream",
          "This sink pad that data has to be sent",
          GST_TYPE_PAD,
          G_PARAM_READABLE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_LOCAL_PREFERENCES,
      g_param_spec_boxed ("codec-preferences",
          "Local codec preferences",
          "A GList of FsCodec representing preferences"
          " to be passed to the"
          " fs_session_set_local_preferences()"
          " function",
          FS_TYPE_CODEC_LIST,
          G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_TOS,
      g_param_spec_uint ("tos",
          "IP Type of Service",
          "The IP Type of Service to set on sent packets",
          0, 255, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_RESOURCES,
      g_param_spec_uint ("resources",
          "Resources held by the stream",
          "The resources held by a TpMediaStreamDirection",
          TP_MEDIA_STREAM_DIRECTION_NONE,
          TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
          TP_MEDIA_STREAM_DIRECTION_NONE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * TfStream::closed:
   * @stream: the stream that has been closed
   *
   * This signal is emitted when the Close() signal is received from the
   * connection manager.
   */

  signals[CLOSED] =
      g_signal_new ("closed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 0);

  /**
   * TfStream::error:
   * @stream: the stream that has been errored
   *
   * This signal is emitted when there is an error on this stream
   */

  signals[ERROR_SIGNAL] =
      g_signal_new ("error",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 0);

  /**
   * TfStream::request-resource:
   * @stream: the stream requesting the resources
   * @direction: The direction for which this resource is requested
   *  (as a #TpMediaDirection
   *
   * This signal is emitted when the connection manager ask to send or receive
   * media. For example, this can be used allocated an X window or open a
   * camera. The resouces can later be freed on #TfStream::free-resource
   *
   * Returns: %TRUE if the resources requested could be allocated or %FALSE
   * otherwise
   */

  signals[REQUEST_RESOURCE] =
      g_signal_new ("request-resource",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          g_signal_accumulator_true_handled, NULL, NULL,
          G_TYPE_BOOLEAN, 1, G_TYPE_UINT);

  /**
   * TfStream::free-resource:
   * @stream: the stream for which resources can be freed
   * @direction: The direction for which this resource is freed
   *  (as a #TpMediaDirection
   *
   * Emitted when the stream no longer needs a resource allocated
   * from #TfStream::request-resource and it can be freed.
   */

  signals[FREE_RESOURCE] =
      g_signal_new ("free-resource",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 1, G_TYPE_UINT);

  /**
   * TfStream::src-pad-added:
   * @stream: the stream which has a new pad
   * @pad: The new src pad
   * @codec: the codec for which data is coming out
   *
   * This is emitted when a new src pad comes out. The user must connect
   * this pad to his pipeline.
   */

  signals[SRC_PAD_ADDED] =
      g_signal_new ("src-pad-added",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 2, GST_TYPE_PAD, FS_TYPE_CODEC);


  /**
   * TfStream::restart-source:
   * @stream: the stream
   *
   * This is emitted when there is a caps change and the source should be
   * restarted to take this into account.
   */

  signals[RESTART_SOURCE] =
      g_signal_new ("restart-source",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          NULL, NULL, NULL,
          G_TYPE_NONE, 0);
}

static void
get_all_properties_cb (TpProxy *proxy,
    GHashTable *out_Properties,
    const GError *dbus_error,
    gpointer user_data,
    GObject *weak_object)
{
  TfStream *stream = TF_STREAM (weak_object);
  GError *myerror = NULL;
  gchar *transmitter;
  guint n_args = 0;
  GList *preferred_local_candidates = NULL;
  GParameter params[MAX_STREAM_TRANS_PARAMS];
  const gchar *nat_traversal = NULL;
  GPtrArray *stun_servers = NULL;
  gboolean got_stun = FALSE;
  GPtrArray *dbus_relay_info = NULL;
  gboolean created_locally = TRUE;
  gboolean valid = FALSE;
  guint i;
  gboolean do_controlling = FALSE;
  GList *rtp_header_extensions;
  gboolean res = FALSE;

  if (dbus_error &&
      !(dbus_error->domain == DBUS_GERROR &&
          dbus_error->code == DBUS_GERROR_UNKNOWN_METHOD))
    {
      tf_stream_error (stream, TP_MEDIA_STREAM_ERROR_INVALID_CM_BEHAVIOR,
          dbus_error->message);
      return;
    }

  tp_cli_media_stream_handler_connect_to_add_remote_candidate
      (stream->priv->stream_handler_proxy, add_remote_candidate, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_remove_remote_candidate
      (stream->priv->stream_handler_proxy, remove_remote_candidate, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_active_candidate_pair
      (stream->priv->stream_handler_proxy, set_active_candidate_pair, NULL,
          NULL, (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_candidate_list
      (stream->priv->stream_handler_proxy, set_remote_candidate_list, NULL,
          NULL, (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_codecs
      (stream->priv->stream_handler_proxy, set_remote_codecs, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_playing
      (stream->priv->stream_handler_proxy, set_stream_playing, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_sending
      (stream->priv->stream_handler_proxy, set_stream_sending, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_stream_held
      (stream->priv->stream_handler_proxy, set_stream_held, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_start_telephony_event
      (stream->priv->stream_handler_proxy, start_telephony_event, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_start_named_telephony_event
      (stream->priv->stream_handler_proxy, start_named_telephony_event, NULL,
          NULL, (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_start_sound_telephony_event
      (stream->priv->stream_handler_proxy, start_sound_telephony_event, NULL,
          NULL, (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_stop_telephony_event
      (stream->priv->stream_handler_proxy, stop_telephony_event, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_close
      (stream->priv->stream_handler_proxy, stream_close, NULL, NULL,
          (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_feedback_messages
      (stream->priv->stream_handler_proxy, set_remote_feedback_messages, NULL,
          NULL, (GObject*) stream, NULL);
  tp_cli_media_stream_handler_connect_to_set_remote_header_extensions
      (stream->priv->stream_handler_proxy, set_remote_header_extensions, NULL,
          NULL, (GObject*) stream, NULL);

  memset (params, 0, sizeof(GParameter) * MAX_STREAM_TRANS_PARAMS);

  if (out_Properties)
    nat_traversal = tp_asv_get_string (out_Properties, "NATTraversal");
  if (!nat_traversal && stream->priv->nat_props)
    nat_traversal = stream->priv->nat_props->nat_traversal;

  if (!nat_traversal || !strcmp (nat_traversal, "gtalk-p2p"))
    {
      transmitter = "nice";
      do_controlling = TRUE;

      params[n_args].name = "compatibility-mode";
      g_value_init (&params[n_args].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_args].value, 1);
      n_args++;
    }
  else if (!strcmp (nat_traversal, "ice-udp"))
    {
      transmitter = "nice";
      do_controlling = TRUE;
    }
  else if (!strcmp (nat_traversal, "wlm-8.5"))
    {
      transmitter = "nice";
      do_controlling = TRUE;

      params[n_args].name = "compatibility-mode";
      g_value_init (&params[n_args].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_args].value, 2);
      n_args++;
    }
  else if (!strcmp (nat_traversal, "wlm-2009"))
    {
      transmitter = "nice";
      do_controlling = TRUE;

      params[n_args].name = "compatibility-mode";
      g_value_init (&params[n_args].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_args].value, 3);
      n_args++;
    }
  else if (!strcmp (nat_traversal, "shm"))
    {
      transmitter = "shm";
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

  /* FIXME: use correct macro when available */
  if (out_Properties)
    stun_servers = tp_asv_get_boxed (out_Properties, "STUNServers",
        tp_type_dbus_array_su ());

  if (stun_servers && stun_servers->len)
    {
      GValueArray *stun_server = g_ptr_array_index (stun_servers, 0);

      if (stun_server && stun_server->n_values == 2)
        {
          GValue *stun_ip = g_value_array_get_nth (stun_server, 0);
          GValue *stun_port = g_value_array_get_nth (stun_server, 1);

          DEBUG (stream, "Adding STUN server %s:%u",
              g_value_get_string (stun_ip),
              g_value_get_uint (stun_port));

          params[n_args].name = "stun-ip";
          g_value_init (&params[n_args].value, G_TYPE_STRING);
          g_value_copy (stun_ip, &params[n_args].value);
          n_args++;

          params[n_args].name = "stun-port";
          g_value_init (&params[n_args].value, G_TYPE_UINT);
          g_value_copy (stun_port, &params[n_args].value);
          n_args++;

          got_stun = TRUE;
        }
    }

  if (!got_stun && stream->priv->nat_props &&
      stream->priv->nat_props->stun_server &&
      stream->priv->nat_props->stun_port)
    {
      DEBUG (stream, "Adding STUN server (old API) %s:%u",
          stream->priv->nat_props->stun_server,
          stream->priv->nat_props->stun_port);
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

      got_stun = TRUE;
    }

  if (got_stun)
    {
      gchar *conn_timeout_str = NULL;

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

  if (out_Properties)
    dbus_relay_info = tp_asv_get_boxed (out_Properties, "RelayInfo",
        TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST);

  if (dbus_relay_info && dbus_relay_info->len)
    {
      GPtrArray *fs_relay_info = NULL;

      for (i = 0; i < dbus_relay_info->len; i++)
        {
          GHashTable *one_relay = g_ptr_array_index(dbus_relay_info, i);
          const gchar *type;
          const gchar *ip;
          guint32 port;
          const gchar *username;
          const gchar *password;
          guint component;
          GstStructure *s;

          ip = tp_asv_get_string (one_relay, "ip");
          port = tp_asv_get_uint32 (one_relay, "port", NULL);
          type = tp_asv_get_string (one_relay, "type");
          username = tp_asv_get_string (one_relay, "username");
          password = tp_asv_get_string (one_relay, "password");
          component = tp_asv_get_uint32 (one_relay, "component", NULL);

          if (!ip || !port || !username || !password)
              continue;

          if (!fs_relay_info)
            fs_relay_info = g_ptr_array_new_with_free_func (
              (GDestroyNotify) gst_structure_free);

          s = gst_structure_new ("relay-info",
              "ip", G_TYPE_STRING, ip,
              "port", G_TYPE_UINT, port,
              "username", G_TYPE_STRING, username,
              "password", G_TYPE_STRING, password,
              NULL);

          if (type)
            gst_structure_set (s, "relay-type", G_TYPE_STRING, type, NULL);

          if (component)
            gst_structure_set (s, "component", G_TYPE_UINT, component, NULL);

          if (!type)
            type = "udp";

          DEBUG (stream, "Adding relay (%s) %s:%u %s:%s %u",
              type, ip, port, username, password, component);

          g_ptr_array_add (fs_relay_info, s);
        }

      if (fs_relay_info)
        {
          params[n_args].name = "relay-info";
          g_value_init (&params[n_args].value, G_TYPE_PTR_ARRAY);
          g_value_take_boxed (&params[n_args].value, fs_relay_info);
          n_args++;
        }
    }

  if (out_Properties && do_controlling)
    {
      created_locally = tp_asv_get_boolean (out_Properties, "CreatedLocally",
          &valid);
      if (valid)
        {
          params[n_args].name = "controlling-mode";
          g_value_init (&params[n_args].value, G_TYPE_BOOLEAN);
          g_value_set_boolean (&params[n_args].value, created_locally);
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
      &myerror);

  if (!stream->priv->fs_session)
    {
      tf_stream_error (stream, fserror_to_tperror (myerror), myerror->message);
      WARNING (stream, "Error creating session: %s", myerror->message);
      g_clear_error (&myerror);
      return;
    }

  if (stream->priv->tos)
    g_object_set (stream->priv->fs_session, "tos", stream->priv->tos, NULL);

  stream->priv->fs_stream = fs_session_new_stream (stream->priv->fs_session,
      stream->priv->fs_participant,
      FS_DIRECTION_NONE,
      &myerror);

  if (stream->priv->fs_stream)
    res = fs_stream_set_transmitter (stream->priv->fs_stream,
        transmitter, params, n_args, &myerror);

  for (i = 0; i < n_args; i++)
    g_value_unset (&params[i].value);

  if (!stream->priv->fs_stream)
    {
      tf_stream_error (stream, fserror_to_tperror (myerror), myerror->message);
      WARNING (stream, "Error creating stream: %s", myerror->message);
      g_clear_error (&myerror);
      return;
    }

  if (!res)
    {
      tf_stream_error (stream, fserror_to_tperror (myerror), myerror->message);
      WARNING (stream, "Could not set transmitter for stream: %s",
          myerror->message);
      g_clear_error (&myerror);
      return;
    }

  rtp_header_extensions =
      fs_utils_get_default_rtp_header_extension_preferences (
          GST_ELEMENT (stream->priv->fs_conference),
          tp_media_type_to_fs (stream->priv->media_type));

  if (rtp_header_extensions)
    {
      g_object_set (stream->priv->fs_session,
          "rtp-header-extension-preferences", rtp_header_extensions, NULL);
      fs_rtp_header_extension_list_destroy (rtp_header_extensions);
    }

  if (!stream->priv->local_preferences)
    stream->priv->local_preferences = fs_utils_get_default_codec_preferences (
        GST_ELEMENT (stream->priv->fs_conference));

  if (stream->priv->local_preferences)
    if (!fs_session_set_codec_preferences (stream->priv->fs_session,
            stream->priv->local_preferences,
            &myerror))
      {
        if (!(myerror->domain == FS_ERROR &&
                myerror->code == FS_ERROR_NOT_IMPLEMENTED))
          {
            tf_stream_error (stream, fserror_to_tperror (myerror),
                myerror->message);
            WARNING (stream, "Error setting codec preferences: %s",
                myerror->message);
            g_clear_error (&myerror);
            return;
          }
        g_clear_error (&myerror);
      }

  if (g_object_class_find_property (
          G_OBJECT_GET_CLASS (stream->priv->fs_session),
          "no-rtcp-timeout"))
    g_object_set (stream->priv->fs_session, "no-rtcp-timeout", 0, NULL);

  g_signal_connect_object (stream->priv->fs_stream, "src-pad-added",
      G_CALLBACK (cb_fs_stream_src_pad_added), stream, 0);

  stream->priv->send_local_codecs = TRUE;

  stream->priv->new_stream_created_cb (stream, stream->priv->channel);
}

/* dummy callback handler for async calling calls with no return values */
static void
async_method_callback (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TfStream *self = TF_STREAM (weak_object);

  if (error != NULL)
    {
      g_warning ("Error calling %s: %s", (gchar *) user_data, error->message);
      g_signal_emit (self, signals[ERROR_SIGNAL], 0);
    }
}


/* dummy callback handler for async calling calls with no return values
 * and whose implementation is optional */
static void
async_method_callback_optional (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  if (error == NULL ||
      g_error_matches (error, DBUS_GERROR, G_DBUS_ERROR_UNKNOWN_METHOD) ||
      g_error_matches (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED))
    return;

  async_method_callback (proxy, error, user_data, weak_object);
}

static void
cb_fs_new_local_candidate (TfStream *self,
    FsCandidate *candidate)
{
  DEBUG (self, "called");

  self->priv->local_candidates = g_list_append (self->priv->local_candidates,
      fs_candidate_copy (candidate));
}

static void
cb_fs_local_candidates_prepared (TfStream *self)
{
  DEBUG (self, "called");

  while (self->priv->local_candidates)
    {
      GPtrArray *transports = g_ptr_array_new ();
      FsCandidate *candidate =
          g_list_first (self->priv->local_candidates)->data;
      gchar *foundation = g_strdup (candidate->foundation);

      while (candidate)
        {
          GValue transport = { 0, };
          TpMediaStreamBaseProto proto;
          TpMediaStreamTransportType type;
          gboolean valid = TRUE;
          GList *item = NULL;

          g_value_init (&transport,
              TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT);
          g_value_take_boxed (&transport,
              dbus_g_type_specialized_construct (
                  TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT));

          proto = fs_network_proto_to_tp (candidate->proto, &valid);
          if (valid == FALSE)
            return;
          type = fs_candidate_type_to_tp (candidate->type, &valid);
          if (valid == FALSE)
            return;

          DEBUG (self, "ip = '%s port = %u component = %u'", candidate->ip,
              candidate->port, candidate->component_id);

          dbus_g_type_struct_set (&transport,
              0, candidate->component_id,
              1, candidate->ip,
              2, candidate->port,
              3, proto,
              4, "RTP",
              5, "AVP",
              6, (double) (candidate->priority / 65536.0),
              7, type,
              8, candidate->username,
              9, candidate->password,
              G_MAXUINT);

          g_ptr_array_add (transports, g_value_get_boxed (&transport));

          self->priv->local_candidates = g_list_remove (
              self->priv->local_candidates, candidate);

          fs_candidate_destroy (candidate);

          for (item = self->priv->local_candidates;
               item;
               item = g_list_next (item))
            {
              FsCandidate *tmpcand = item->data;
              if (!strcmp (tmpcand->foundation, foundation))
                break;
            }
          if (item)
            candidate = item->data;
          else
            candidate = NULL;
        }

      tp_cli_media_stream_handler_call_new_native_candidate (
          self->priv->stream_handler_proxy, -1, foundation, transports,
          async_method_callback,
          "Media.StreamHandler::NewNativeCandidate",
          NULL, (GObject *) self);

      g_boxed_free (TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT_LIST,
          transports);
      g_free (foundation);
    }

  tp_cli_media_stream_handler_call_native_candidates_prepared (
    self->priv->stream_handler_proxy, -1, async_method_callback,
    "Media.StreamHandler::NativeCandidatesPrepared",
    NULL, (GObject *) self);
}



/*
 * small helper function to help converting a
 * telepathy dbus candidate to a list of FsCandidate.
 * nothing is copied, so always keep the usage of this within a function
 * Free the result with fs_candidate_list_destroy()
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
          /* or FS_CANDIDATE_TYPE_PRFLX .. the TP spec doesn't differentiate */
          break;
        case TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY:
          type = FS_CANDIDATE_TYPE_RELAY;
          break;
        default:
          g_critical ("%s: FarstreamTransportInfo.proto has an invalid value",
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
          g_critical ("%s: FarstreamTransportInfo.proto has an invalid value",
              G_STRFUNC);
          proto = FS_NETWORK_PROTOCOL_UDP;
        }

      fs_candidate = fs_candidate_new (foundation,
          g_value_get_uint (g_value_array_get_nth (transport, 0)), /*component*/
          type, proto,

          g_value_get_string (g_value_array_get_nth (transport, 1)), /* ip */
          g_value_get_uint (g_value_array_get_nth (transport, 2))); /* port */

      fs_candidate->priority = (gint)
          (g_value_get_double (g_value_array_get_nth (transport, 6)) * 65536.0);
      fs_candidate->username =
          g_value_dup_string (g_value_array_get_nth (transport, 8));
      fs_candidate->password =
          g_value_dup_string (g_value_array_get_nth (transport, 9));

      fs_trans_list = g_list_prepend (fs_trans_list, fs_candidate);
    }
  fs_trans_list = g_list_reverse (fs_trans_list);

  return fs_trans_list;
}

static TpMediaStreamBaseProto
fs_network_proto_to_tp (FsNetworkProtocol proto, gboolean *valid)
{
  if (valid != NULL)
    *valid = TRUE;

  switch (proto) {
  case FS_NETWORK_PROTOCOL_UDP:
    return TP_MEDIA_STREAM_BASE_PROTO_UDP;
  case FS_NETWORK_PROTOCOL_TCP:
    return TP_MEDIA_STREAM_BASE_PROTO_TCP;
  default:
    g_critical ("%s: FarstreamTransportInfo.proto has an invalid value",
        G_STRFUNC);
    if (valid != NULL)
      *valid = FALSE;
    g_return_val_if_reached(0);
  }
}

static TpMediaStreamTransportType
fs_candidate_type_to_tp (FsCandidateType type, gboolean *valid)
{
  if (valid != NULL)
    *valid = TRUE;

  switch (type) {
  case FS_CANDIDATE_TYPE_HOST:
    return TP_MEDIA_STREAM_TRANSPORT_TYPE_LOCAL;
  case FS_CANDIDATE_TYPE_SRFLX:
  case FS_CANDIDATE_TYPE_PRFLX:
    return TP_MEDIA_STREAM_TRANSPORT_TYPE_DERIVED;
  case FS_CANDIDATE_TYPE_RELAY:
    return TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY;
  default:
    g_critical ("%s: FarstreamTransportInfo.proto has an invalid value",
        G_STRFUNC);
    if (valid != NULL)
      *valid = FALSE;
    g_return_val_if_reached(0);
  }
}

static GValueArray *
fs_candidate_to_tp_array (const FsCandidate *candidate)
{
  GValueArray *transport = NULL;
  TpMediaStreamBaseProto proto;
  TpMediaStreamTransportType type;
  gboolean valid = TRUE;

  proto = fs_network_proto_to_tp (candidate->proto, &valid);
  if (valid == FALSE)
    return NULL;
  type = fs_candidate_type_to_tp (candidate->type, &valid);
  if (valid == FALSE)
    return NULL;

  transport = tp_value_array_build (10,
      G_TYPE_UINT, candidate->component_id,
      G_TYPE_STRING, candidate->ip,
      G_TYPE_UINT, candidate->port,
      G_TYPE_UINT, proto,
      G_TYPE_STRING, "RTP",
      G_TYPE_STRING, "AVP",
      G_TYPE_DOUBLE, (double) (candidate->priority / 65536.0),
      G_TYPE_UINT, type,
      G_TYPE_STRING, candidate->username,
      G_TYPE_STRING, candidate->password,
      G_TYPE_INVALID);

  return transport;
}

/*
 * Small helper function to help converting a list of FarstreamCodecs
 * to a Telepathy codec list.
 */
static GPtrArray *
fs_codecs_to_tp (TfStream *stream,
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
          g_critical ("%s: FarstreamCodec [%d, %s]'s media_type has an invalid value",
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
      g_value_take_boxed (&codec,
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

      DEBUG (stream, "adding codec " FS_CODEC_FORMAT, FS_CODEC_ARGS (fsc));

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
  TfStream *self = TF_STREAM (object);
  GError *error = NULL;
  GList *fscandidates;

  DEBUG (self, "adding remote candidate %s", candidate);

  fscandidates = tp_transports_to_fs (candidate, transports);

  if (!fs_stream_add_remote_candidates (self->priv->fs_stream,
          fscandidates, &error))
    tf_stream_error (self, fserror_to_tperror (error), error->message);

  fs_candidate_list_destroy (fscandidates);
  g_clear_error (&error);
}

static void
remove_remote_candidate (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const gchar *candidate G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object G_GNUC_UNUSED)
{
  TfStream *self = TF_STREAM (object);

  tf_stream_error (self, TP_MEDIA_STREAM_ERROR_INVALID_CM_BEHAVIOR,
      "RemoveRemoteCandidate is DEPRECATED");
}

static void
set_active_candidate_pair (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const gchar *native_candidate,
    const gchar *remote_candidate,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  /*
  TfStream *self = TF_STREAM (object);
  GError *error = NULL;

  if (!fs_stream_select_candidate_pair (self->priv->fs_stream,
          native_candidate,
          remote_candidate,
          &error))
    {
      if (error->domain == FS_ERROR && error->code == FS_ERROR_NOT_IMPLEMENTED)
        DEBUG (self, "Called not implemented SetActiveCandidatePair");
      else
        tf_stream_error (self, 0, error->message);
    }

  g_clear_error (&error);
  */
}

static void
set_remote_candidate_list (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GPtrArray *candidates,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);
  guint i;
  GList *fs_candidates = NULL;
  GError *error = NULL;
  gboolean ret;

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

  ret = fs_stream_add_remote_candidates (self->priv->fs_stream,
      fs_candidates, &error);
  if (!ret && error &&
      error->domain == FS_ERROR && error->code == FS_ERROR_NOT_IMPLEMENTED)
  {
    g_clear_error (&error);
    ret = fs_stream_force_remote_candidates (self->priv->fs_stream,
        fs_candidates, &error);
  }
  if (!ret)
    tf_stream_error (self, fserror_to_tperror (error),  error->message);

  g_clear_error (&error);
  fs_candidate_list_destroy (fs_candidates);
}

static void
fill_fs_params (gpointer key, gpointer value, gpointer user_data)
{
  FsCodec *codec = user_data;

  fs_codec_add_optional_parameter (codec, key, value);
}

static void
set_remote_codecs (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    const GPtrArray *codecs,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);
  GValueArray *codec;
  GHashTable *params = NULL;
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
      g_hash_table_foreach (params, fill_fs_params, fs_codec);

      if (self->priv->feedback_messages)
        {
          GValueArray *message_props;

          message_props = g_hash_table_lookup (self->priv->feedback_messages,
              GUINT_TO_POINTER (fs_codec->id));

          if (message_props)
            {
              GValue *val;
              GPtrArray *messages;
              guint j;

              g_assert (G_VALUE_HOLDS_UINT (
                      g_value_array_get_nth (message_props, 0)));
              g_assert (G_VALUE_TYPE (
                      g_value_array_get_nth (message_props, 1)) ==
                  TP_ARRAY_TYPE_RTCP_FEEDBACK_MESSAGE_LIST);

              val = g_value_array_get_nth (message_props, 0);
              fs_codec->minimum_reporting_interval =
                  g_value_get_uint (val);

              val = g_value_array_get_nth (message_props, 1);
              messages = g_value_get_boxed (val);

              for (j = 0; j < messages->len; j++)
                {
                  GValueArray *msg = g_ptr_array_index (messages, j);

                  g_assert (G_VALUE_HOLDS_STRING (
                          g_value_array_get_nth (msg, 0)));
                  g_assert (G_VALUE_HOLDS_STRING (
                           g_value_array_get_nth (msg, 1)));
                  g_assert (G_VALUE_HOLDS_STRING (
                          g_value_array_get_nth (msg, 2)));

                  fs_codec_add_feedback_parameter (fs_codec,
                      g_value_get_string (g_value_array_get_nth (msg, 0)),
                      g_value_get_string (g_value_array_get_nth (msg, 1)),
                      g_value_get_string (g_value_array_get_nth (msg, 2)));
                }
            }
        }


      DEBUG (self, "adding remote codec %s [%d]",
          fs_codec->encoding_name, fs_codec->id);

      fs_remote_codecs = g_list_prepend (fs_remote_codecs, fs_codec);
  }
  fs_remote_codecs = g_list_reverse (fs_remote_codecs);

  if (self->priv->feedback_messages)
    {
      g_boxed_free (TP_HASH_TYPE_RTCP_FEEDBACK_MESSAGE_MAP,
          self->priv->feedback_messages);
      self->priv->feedback_messages = NULL;
    }


  if (self->priv->header_extensions)
    {
      if (g_object_class_find_property (
              G_OBJECT_GET_CLASS (self->priv->fs_stream),
              "rtp-header-extensions"))
        {
          GList *hdrexts = NULL;

          for (i = 0; i < self->priv->header_extensions->len; i++)
            {
              GValueArray *extension =
                  g_ptr_array_index (self->priv->header_extensions, i);
              FsRtpHeaderExtension *hdrext;

              g_assert (extension->n_values >= 3);
              g_assert (G_VALUE_HOLDS_UINT (
                      g_value_array_get_nth (extension, 0)));
              g_assert (G_VALUE_HOLDS_UINT (
                      g_value_array_get_nth (extension, 1)));
              g_assert (G_VALUE_HOLDS_STRING (
                      g_value_array_get_nth (extension, 2)));

              hdrext = fs_rtp_header_extension_new (
                  g_value_get_uint (g_value_array_get_nth (extension, 0)),
                  tpdirection_to_fsdirection (
                      g_value_get_uint (g_value_array_get_nth (extension, 1))),
                  g_value_get_string (g_value_array_get_nth (extension, 2)));

              hdrexts = g_list_append (hdrexts, hdrext);
            }

          g_object_set (self->priv->fs_stream, "rtp-header-extensions",
              hdrexts, NULL);

          fs_rtp_header_extension_list_destroy (hdrexts);
        }
      g_boxed_free (TP_ARRAY_TYPE_RTP_HEADER_EXTENSIONS_LIST,
          self->priv->header_extensions);
      self->priv->header_extensions = NULL;
    }

  if (!fs_stream_set_remote_codecs (self->priv->fs_stream, fs_remote_codecs,
          &error)) {
    /*
     * Call the error method with the proper thing here
     */
    g_prefix_error (&error, "Codec negotiation failed: ");
    tf_stream_error (self, fserror_to_tperror (error), error->message);
    g_clear_error (&error);
    fs_codec_list_destroy (fs_remote_codecs);
    return;
  }

  fs_codec_list_destroy (fs_remote_codecs);

  self->priv->send_supported_codecs = TRUE;
  _tf_stream_try_sending_codecs (self);
}

static void
set_stream_playing (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    gboolean play,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);
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
        if (tf_stream_request_resource (self,
                TP_MEDIA_STREAM_DIRECTION_RECEIVE))
          {
            g_object_set (self->priv->fs_stream,
                "direction", current_direction | FS_DIRECTION_RECV,
                NULL);
          }
        else
          {
            tf_stream_error (self, TP_MEDIA_STREAM_ERROR_MEDIA_ERROR,
                "Resource Unavailable");
          }
      }
      self->priv->desired_direction |= FS_DIRECTION_RECV;
    }
  else
    {
      if (!self->priv->held) {
        tf_stream_free_resource (self,
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
  TfStream *self = TF_STREAM (object);
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
        if (tf_stream_request_resource (self,
                TP_MEDIA_STREAM_DIRECTION_SEND))
          {
            g_object_set (self->priv->fs_stream,
                "direction", current_direction | FS_DIRECTION_SEND,
                NULL);
          }
        else
          {
            tf_stream_error (self, TP_MEDIA_STREAM_ERROR_MEDIA_ERROR,
                "Resource Unavailable");
          }
      }
      self->priv->desired_direction |= FS_DIRECTION_SEND;
    }
  else
    {
      g_object_set (self->priv->fs_stream,
          "direction", current_direction & ~(FS_DIRECTION_SEND),
          NULL);

      tf_stream_free_resource (self, FS_DIRECTION_SEND);

      self->priv->desired_direction &= ~(FS_DIRECTION_SEND);
    }
}

static gboolean
tf_stream_request_resource (TfStream *self,
    TpMediaStreamDirection dir)
{
  gboolean resource_available;
  GValue instance_and_arg[2];
  GValue resource_avail_val = {0,};

  if ((self->priv->has_resource & dir) == dir)
    return TRUE;

  memset (instance_and_arg, 0, sizeof(GValue) * 2);

  g_value_init (&resource_avail_val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&resource_avail_val, TRUE);

  g_value_init (&instance_and_arg[0], TF_TYPE_STREAM);
  g_value_set_object (&instance_and_arg[0], self);
  g_value_init (&instance_and_arg[1], G_TYPE_UINT);
  g_value_set_uint (&instance_and_arg[1], dir & ~self->priv->has_resource);

  DEBUG (self, "Requesting resource for direction %d", dir);

  g_signal_emitv (instance_and_arg, signals[REQUEST_RESOURCE], 0,
      &resource_avail_val);
  resource_available = g_value_get_boolean (&resource_avail_val);

  g_value_unset (&instance_and_arg[0]);
  g_value_unset (&instance_and_arg[1]);
  g_value_unset (&resource_avail_val);

  DEBUG (self, "Requesting resource for direction %d returned %d", dir,
      resource_available);

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
tf_stream_free_resource (TfStream *self,
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
  TfStream *self = TF_STREAM (object);

  if (held == self->priv->held)
    return;

  DEBUG (self, "Holding : %d", held);

  if (held)
    {
       g_object_set (self->priv->fs_stream,
            "direction", FS_DIRECTION_NONE,
            NULL);

       tf_stream_free_resource (self,
                TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);
       /* Send success message */
       if (self->priv->stream_handler_proxy)
         {
           tp_cli_media_stream_handler_call_hold_state (
             self->priv->stream_handler_proxy, -1, TRUE,
             async_method_callback, "Media.StreamHandler::HoldState TRUE",
             NULL, (GObject *) self);
         }
       self->priv->held = TRUE;
    }
  else
    {
      FsStreamDirection desired_direction = self->priv->desired_direction;

      if (tf_stream_request_resource (self, desired_direction))
        {
           g_object_set (self->priv->fs_stream,
               "direction", self->priv->desired_direction,
               NULL);
           tp_cli_media_stream_handler_call_hold_state (
             self->priv->stream_handler_proxy, -1, FALSE,
             async_method_callback, "Media.StreamHandler::HoldState FALSE",
             NULL, (GObject *) self);

           self->priv->held = FALSE;
        }
      else
        {
          tf_stream_error (self, TP_MEDIA_STREAM_ERROR_MEDIA_ERROR,
              "Error unholding stream");
        }
    }
}

static void
start_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    guchar event,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);

  g_assert (self->priv->fs_session != NULL);

  DEBUG (self, "called with event %u", event);

  if (self->priv->sending_telephony_event)
    {
      WARNING (self, "start new telephony event without stopping the"
          " previous one first");
      if (!fs_session_stop_telephony_event (self->priv->fs_session))
        WARNING (self, "stopping event failed");
    }

  /* this week, volume is 8, for the sake of argument... */

  if (!fs_session_start_telephony_event (self->priv->fs_session, event, 8))
    WARNING (self, "sending event %u failed", event);
  self->priv->sending_telephony_event = TRUE;
}

static gboolean
check_codecs_for_telephone_event (TfStream *self, GList **codecs,
    FsCodec *send_codec, gint codecid)
{
  GList *item = NULL;
  GError *error = NULL;
  gboolean changed = FALSE;

  for (item = *codecs; item; item = item->next)
    {
      FsCodec *codec = item->data;

      if (!g_ascii_strcasecmp (codec->encoding_name, "telephone-event") &&
          send_codec->clock_rate == codec->clock_rate)
        {
          if (codecid < 0 || codecid == codec->id)
            return TRUE;
          else
            codec->id = codecid;
          changed = TRUE;
          break;
        }
    }

  if (codecid < 0)
    return FALSE;

  if (!changed)
    {
      FsCodec *codec = fs_codec_new (codecid, "telephone-event",
          FS_MEDIA_TYPE_AUDIO, send_codec->clock_rate);

      *codecs = g_list_append (*codecs, codec);
    }
  if (!fs_stream_set_remote_codecs (self->priv->fs_stream, *codecs, &error))
    {
      /*
       * Call the error method with the proper thing here
       */
      g_prefix_error (&error, "Codec negotiation failed for DTMF: ");
      tf_stream_error (self, fserror_to_tperror (error), error->message);
      g_clear_error (&error);
    }

  return FALSE;
}

static void
start_named_telephony_event (TpMediaStreamHandler *proxy,
    guchar event,
    guint codecid,
    gpointer user_data,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);
  FsCodec *send_codec = NULL;
  GList *codecs = NULL;
  struct DtmfEvent *dtmfevent;

  g_object_get (self->priv->fs_session,
      "current-send-codec", &send_codec,
      "codecs", &codecs,
      NULL);

  if (send_codec == NULL)
    goto out;

  if (check_codecs_for_telephone_event (self, &codecs, send_codec, codecid))
    {
      if (self->priv->sending_telephony_event)
        {
          WARNING (self, "start new telephony event without stopping the"
              " previous one first");
          if (!fs_session_stop_telephony_event (self->priv->fs_session))
            WARNING (self, "stopping event failed");
        }


      DEBUG (self, "Sending named telephony event %d with pt %d",
          event, codecid);
      if (!fs_session_start_telephony_event (self->priv->fs_session,
              event, 8))
        WARNING (self, "sending event %u failed", event);
      self->priv->sending_telephony_event = TRUE;
    }
  else
    {
      DEBUG (self, "Queing named telephony event %d with pt %d",
          event, codecid);
      dtmfevent = g_slice_new (struct DtmfEvent);
      dtmfevent->codec_id = codecid;
      dtmfevent->event_id = event;
      g_queue_push_tail (&self->priv->events_to_send, dtmfevent);
    }

out:

  fs_codec_destroy (send_codec);
  fs_codec_list_destroy (codecs);
}

static void
start_sound_telephony_event (TpMediaStreamHandler *proxy, guchar event,
    gpointer user_data, GObject *object)
{
  TfStream *self = TF_STREAM (object);
  FsCodec *send_codec = NULL;
  GList *codecs = NULL;

  g_assert (self->priv->fs_session != NULL);

  DEBUG (self, "called with event %u", event);

  g_object_get (self->priv->fs_session,
      "current-send-codec", &send_codec,
      "codecs", &codecs,
      NULL);

  if (send_codec == NULL)
    goto out;

  if (check_codecs_for_telephone_event (self, &codecs, send_codec, -1))
    {
      WARNING (self, "Tried to do sound event while telephone-event is set,"
          " ignoring");
      goto out;
    }

  if (self->priv->sending_telephony_event)
    {
      WARNING (self, "start new telephony event without stopping the"
          " previous one first");
      if (!fs_session_stop_telephony_event (self->priv->fs_session))
        WARNING (self, "stopping event failed");
    }

  /* this week, volume is 8, for the sake of argument... */

  if (!fs_session_start_telephony_event (self->priv->fs_session, event, 8))
    WARNING (self, "sending sound event %u failed", event);
  self->priv->sending_telephony_event = TRUE;

 out:
  fs_codec_destroy (send_codec);
  fs_codec_list_destroy (codecs);
}


static void
stop_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED,
    GObject *object)
{
  TfStream *self = TF_STREAM (object);

  g_assert (self->priv->fs_session  != NULL);

  DEBUG (self, "called");

  if (!self->priv->sending_telephony_event)
    WARNING (self, "Trying to stop telephony event without having started one");
  self->priv->sending_telephony_event = FALSE;

  if (!fs_session_stop_telephony_event (self->priv->fs_session))
    WARNING (self, "stopping event failed");
}


static void
tf_stream_shutdown (TfStream *self)
{
  if (self->priv->fs_stream)
    g_object_set (self->priv->fs_stream,
        "direction", FS_DIRECTION_NONE,
        NULL);
  tf_stream_free_resource (self,
      TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL);

  g_signal_emit (self, signals[CLOSED], 0);
}


static void
stream_close (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
       gpointer user_data G_GNUC_UNUSED,
       GObject *object)
{
  TfStream *self = TF_STREAM (object);

  DEBUG (self, "close requested by connection manager");

  tf_stream_shutdown (self);
}


static void
set_remote_feedback_messages (TpMediaStreamHandler *proxy,
    GHashTable *messages, gpointer user_data, GObject *object)
{
  TfStream *self = TF_STREAM (object);

  if (self->priv->feedback_messages)
    g_boxed_free (TP_HASH_TYPE_RTCP_FEEDBACK_MESSAGE_MAP,
        self->priv->feedback_messages);

  self->priv->feedback_messages =
      g_boxed_copy (TP_HASH_TYPE_RTCP_FEEDBACK_MESSAGE_MAP, messages);
}


static void
set_remote_header_extensions (TpMediaStreamHandler *proxy,
    const GPtrArray *header_extensions, gpointer user_data, GObject *object)
{
  TfStream *self = TF_STREAM (object);

  if (self->priv->header_extensions)
    g_boxed_free (TP_ARRAY_TYPE_RTP_HEADER_EXTENSIONS_LIST,
        self->priv->header_extensions);

  self->priv->header_extensions =
      g_boxed_copy (TP_ARRAY_TYPE_RTP_HEADER_EXTENSIONS_LIST,
          header_extensions);
}


static void
cb_fs_recv_codecs_changed (TfStream *self,
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
          async_method_callback_optional,
          "Media.StreamHandler::CodecChoice", NULL, (GObject *) self);
}

static void
cb_fs_new_active_candidate_pair (TfStream *self,
    FsCandidate *local_candidate,
    FsCandidate *remote_candidate)
{
  GValueArray *local_transport = NULL;
  GValueArray *remote_transport = NULL;

  DEBUG (self, "called: c:%d local: %s %s:%u  remote: %s %s:%u",
      local_candidate->component_id,
      local_candidate->foundation, local_candidate->ip, local_candidate->port,
      remote_candidate->foundation, remote_candidate->ip,
      remote_candidate->port);

  local_transport = fs_candidate_to_tp_array (local_candidate);
  if (!local_transport)
    return;

  remote_transport = fs_candidate_to_tp_array (remote_candidate);
  if (!remote_transport)
    {
      g_value_array_free (local_transport);
      return;
    }

  tp_cli_media_stream_handler_call_new_active_transport_pair (
    self->priv->stream_handler_proxy, -1, local_candidate->foundation,
    local_transport, remote_candidate->foundation, remote_transport,
    async_method_callback_optional,
    "Media.StreamHandler::NewActiveTransportPair",
    NULL, (GObject *) self);

  tp_cli_media_stream_handler_call_new_active_candidate_pair (
    self->priv->stream_handler_proxy, -1, local_candidate->foundation,
    remote_candidate->foundation,
    async_method_callback_optional,
    "Media.StreamHandler::NewActiveCandidatePair",
    NULL, (GObject *) self);

  if (self->priv->current_state == TP_MEDIA_STREAM_STATE_DISCONNECTED)
  {
    tp_cli_media_stream_handler_call_stream_state (
        self->priv->stream_handler_proxy, -1, TP_MEDIA_STREAM_STATE_CONNECTED,
        async_method_callback, "Media.StreamHandler::StreamState",
        NULL, (GObject *) self);
    self->priv->current_state = TP_MEDIA_STREAM_STATE_CONNECTED;
  }

  g_value_array_free (local_transport);
  g_value_array_free (remote_transport);
}

static void
invalidated_cb (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
    guint domain G_GNUC_UNUSED,
    gint code G_GNUC_UNUSED,
    gchar *message G_GNUC_UNUSED,
    gpointer user_data)
{
  TfStream *stream = TF_STREAM (user_data);

  DEBUG (stream, "proxy invalidated");

  if (stream->priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = stream->priv->stream_handler_proxy;

      stream->priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  tf_stream_shutdown (stream);
}

static void
cb_fs_send_codec_changed (TfStream *self,
    FsCodec *send_codec,
    GList *secondary_codecs)
{
  GList *item;
  gint last_event_id = -1;
  struct DtmfEvent *dtmfevent;

  while ((dtmfevent = g_queue_peek_head (&self->priv->events_to_send)))
    {
      if (dtmfevent->codec_id != last_event_id)
        {
          last_event_id = -1;
          for (item = secondary_codecs; item; item = item->next)
            {
              FsCodec *codec = item->data;

              if (!g_ascii_strcasecmp (codec->encoding_name, "telephone-event")
                  && codec->id == dtmfevent->codec_id)
                {
                  last_event_id = codec->id;
                  goto have_id;
                }
            }
          if (dtmfevent->codec_id != last_event_id)
            {
              GList *codecs = NULL;

              g_object_get (self->priv->fs_session, "codecs", &codecs, NULL);

              DEBUG (self, "Still do not have the right PT for telephony"
                  " events, trying to force it again");
              if (check_codecs_for_telephone_event (self, &codecs, send_codec,
                      dtmfevent->codec_id))
                WARNING (self, "Did not have the right pt in the secondary"
                    " codecs, but it was in the codec list. Ignoring for now");
              fs_codec_list_destroy (codecs);
              return;
            }
        }

    have_id:
      DEBUG (self, "Sending queued event %d with pt %d", dtmfevent->event_id,
          dtmfevent->codec_id);
      dtmfevent = g_queue_pop_head (&self->priv->events_to_send);
      if (self->priv->sending_telephony_event)
        {
          WARNING (self, "start new telephony event without stopping the"
              " previous one first");
          if (!fs_session_stop_telephony_event (self->priv->fs_session))
            WARNING (self, "stopping event failed");
        }
      self->priv->sending_telephony_event = FALSE;

      if (!fs_session_start_telephony_event (self->priv->fs_session,
              dtmfevent->event_id, 8))
        WARNING (self, "sending event %u failed", dtmfevent->event_id);
      fs_session_stop_telephony_event (self->priv->fs_session);

      g_slice_free (struct DtmfEvent, dtmfevent);
    }
}

/**
 * tf_stream_error:
 * @self: a #TfStream
 * @error: the error number as a #TpMediaStreamError
 * @message: the message for this error
 *
 * This function can be used to tell the connection manager that an error
 * has happened on a specific stream.
 */

void
tf_stream_error (TfStream *self,
    TpMediaStreamError error,
    const gchar *message)
{
  g_message ("%s: stream error errorno=%d error=%s", G_STRFUNC, error, message);

  tp_cli_media_stream_handler_call_error (self->priv->stream_handler_proxy,
      -1, error, message, NULL, NULL, NULL, NULL);

  g_signal_emit (self, signals[ERROR_SIGNAL], 0);
}


/**
 * _tf_stream_bus_message:
 * @stream: A #TfStream
 * @message: A #GstMessage received from the bus
 *
 * You must call this function on call messages received on the async bus.
 * #GstMessages are not modified.
 *
 * Returns: %TRUE if the message has been handled, %FALSE otherwise
 */

gboolean
_tf_stream_bus_message (TfStream *stream,
    GstMessage *message)
{
  const GstStructure *s = gst_message_get_structure (message);

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return FALSE;

  if (!stream->priv->fs_stream || !stream->priv->fs_session)
    return FALSE;

  if (gst_structure_has_name (s, "farstream-error"))
    {
      GObject *object;
      const GValue *value = NULL;

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

          enumclass = g_type_class_ref (FS_TYPE_ERROR);
          enumvalue = g_enum_get_value (enumclass, errorno);
          WARNING (stream, "error (%s (%d)): %s",
              enumvalue->value_nick, errorno, msg);
          g_type_class_unref (enumclass);

          tf_stream_error (stream, fserrorno_to_tperrorno (errorno), msg);
          return TRUE;
        }
    }
  else if (gst_structure_has_name (s, "farstream-new-local-candidate"))
    {
      FsStream *fsstream;
      FsCandidate *candidate;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      g_debug ("new local fs: %p s:%p", stream->priv->fs_stream, stream);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      value = gst_structure_get_value (s, "candidate");
      candidate = g_value_get_boxed (value);

      g_debug ("NEW LOCAL CAND");

      cb_fs_new_local_candidate (stream, candidate);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farstream-local-candidates-prepared"))
    {
      FsStream *fsstream;
      const GValue *value;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      g_debug ("local cand prep fs: %p s:%p", stream->priv->fs_stream, stream);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      g_debug ("LOCAL CAND PREP");

      cb_fs_local_candidates_prepared (stream);

      return TRUE;
    }
  else if (gst_structure_has_name (s, "farstream-new-active-candidate-pair"))
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
  else if (gst_structure_has_name (s, "farstream-current-recv-codecs-changed"))
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
  else if (gst_structure_has_name (s, "farstream-codecs-changed"))
    {
      FsSession *fssession;
      const GValue *value;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != stream->priv->fs_session)
        return FALSE;

      DEBUG (stream, "Codecs changed");

      _tf_stream_try_sending_codecs (stream);

      return TRUE;
    }
  else if (gst_structure_has_name (s, "farstream-send-codec-changed"))
    {
      FsSession *fssession;
      const GValue *value;
      FsCodec *codec = NULL;
      GList *secondary_codecs = NULL;
      FsCodec *objcodec = NULL;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != stream->priv->fs_session)
        return FALSE;

      value = gst_structure_get_value (s, "codec");
      codec = g_value_get_boxed (value);
      g_object_get (fssession, "current-send-codec", &objcodec, NULL);

      if (!fs_codec_are_equal (objcodec, codec))
        {
          fs_codec_destroy (objcodec);
          return TRUE;
        }

      value = gst_structure_get_value (s, "secondary-codecs");
      secondary_codecs = g_value_get_boxed (value);


      if (codec)
        DEBUG (stream, "Send codec changed: " FS_CODEC_FORMAT,
            FS_CODEC_ARGS (codec));

      cb_fs_send_codec_changed (stream, codec, secondary_codecs);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farstream-component-state-changed"))
    {
      FsStream *fsstream;
      const GValue *value;
      guint component;
      FsStreamState fsstate;

      value = gst_structure_get_value (s, "stream");
      fsstream = g_value_get_object (value);

      if (fsstream != stream->priv->fs_stream)
        return FALSE;

      if (!gst_structure_get_uint (s, "component", &component) ||
          !gst_structure_get_enum (s, "state", FS_TYPE_STREAM_STATE,
              (gint*) &fsstate))
        return TRUE;

      cb_fs_component_state_changed (stream, component, fsstate);
      return TRUE;
    }
  else if (gst_structure_has_name (s, "farstream-renegotiate"))
    {
      FsSession *fssession;
      const GValue *value;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != stream->priv->fs_session)
        return FALSE;

      g_signal_emit (stream, signals[RESTART_SOURCE], 0);

      return TRUE;
    }

  return FALSE;
}

static gboolean
emit_connected (gpointer data)
{
  TfStream *self = TF_STREAM (data);

  TF_STREAM_LOCK (self);
  self->priv->idle_connected_id = 0;
  if (self->priv->disposed)
  {
    TF_STREAM_UNLOCK (self);
    return FALSE;
  }
  TF_STREAM_UNLOCK (self);

  tp_cli_media_stream_handler_call_stream_state (
      self->priv->stream_handler_proxy, -1, TP_MEDIA_STREAM_STATE_CONNECTED,
      async_method_callback, "Media.StreamHandler::StreamState",
      NULL, (GObject *) self);

  return FALSE;
}

static void
cb_fs_stream_src_pad_added (FsStream *fsstream G_GNUC_UNUSED,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data)
{
  TfStream *self = TF_STREAM (user_data);
  gchar *padname = gst_pad_get_name (pad);

  DEBUG (self, "New pad %s: " FS_CODEC_FORMAT, padname, FS_CODEC_ARGS (codec));
  g_free (padname);

  TF_STREAM_LOCK (self);
  if (self->priv->disposed)
  {
    TF_STREAM_UNLOCK (self);
    return;
  }

  if (!self->priv->idle_connected_id)
    self->priv->idle_connected_id = g_idle_add (emit_connected, self);
  TF_STREAM_UNLOCK (self);

  g_signal_emit (self, signals[SRC_PAD_ADDED], 0, pad, codec);
}

TfStream *
_tf_stream_new (gpointer channel,
    FsConference *conference,
    FsParticipant *participant,
    TpMediaStreamHandler *proxy,
    guint stream_id,
    TpMediaStreamType media_type,
    TpMediaStreamDirection direction,
    TfNatProperties *nat_props,
    GList *local_preferences,
    NewStreamCreatedCb new_stream_created_cb)
{
  TfStream *self = NULL;

  self = g_object_new (TF_TYPE_STREAM,
      "channel", channel,
      "farstream-conference", conference,
      "farstream-participant", participant,
      "proxy", proxy,
      "stream-id", stream_id,
      "media-type", media_type,
      "direction", direction,
      "nat-properties", nat_props,
      "codec-preferences", local_preferences,
      NULL);

  self->priv->new_stream_created_cb = new_stream_created_cb;

  return self;
}

static GHashTable *
fs_codecs_to_feedback_messages (GList *fscodecs)
{
  GList *item;
  GHashTable *feedback_messages = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, (GDestroyNotify) g_value_array_free);

  for (item = fscodecs; item; item = item->next)
    {
      FsCodec *fs_codec = item->data;

      if (fs_codec->minimum_reporting_interval != G_MAXUINT ||
          fs_codec->feedback_params)
        {
          GValueArray *codec = g_value_array_new (2);
          GPtrArray *messages = g_ptr_array_new ();
          GValue *val;
          GList *item2;

          for (item2 = fs_codec->feedback_params;
               item2;
               item2 = item2->next)
            {
              FsFeedbackParameter *p = item2->data;
              GValueArray *message = g_value_array_new (3);
              GValue *val2;

              g_value_array_insert (message, 0, NULL);
              val2 = g_value_array_get_nth (message, 0);
              g_value_init (val2, G_TYPE_STRING);
              g_value_set_string (val2, p->type);

              g_value_array_insert (message, 1, NULL);
              val2 = g_value_array_get_nth (message, 1);
              g_value_init (val2, G_TYPE_STRING);
              g_value_set_string (val2, p->subtype);

              g_value_array_insert (message, 2, NULL);
              val2 = g_value_array_get_nth (message, 2);
              g_value_init (val2, G_TYPE_STRING);
              g_value_set_string (val2, p->extra_params);

              g_ptr_array_add (messages, message);
            }

          g_value_array_insert (codec, 0, NULL);
          val = g_value_array_get_nth (codec, 0);
          g_value_init (val, G_TYPE_UINT);
          g_value_set_uint (val, fs_codec->minimum_reporting_interval);

          g_value_array_insert (codec, 1, NULL);
          val = g_value_array_get_nth (codec, 1);
          g_value_init (val, TP_ARRAY_TYPE_RTCP_FEEDBACK_MESSAGE_LIST);
          g_value_take_boxed (val, messages);

          g_hash_table_insert (feedback_messages,
              GUINT_TO_POINTER (fs_codec->id), codec);
        }
    }

  return feedback_messages;
}



static GPtrArray *
_tf_stream_get_header_extensions (TfStream *stream)
{
  GPtrArray *extensions = g_ptr_array_new ();
  GList *hdrexts;
  GList *item;

  if (!g_object_class_find_property (
          G_OBJECT_GET_CLASS (stream->priv->fs_session),
          "rtp-header-extensions"))
    return extensions;

  g_object_get (stream->priv->fs_session,
      "rtp-header-extensions", &hdrexts, NULL);

  for (item = hdrexts; item; item = item->next)
    {
      FsRtpHeaderExtension *hdrext = item->data;

      g_ptr_array_add (extensions,
          tp_value_array_build (4,
              G_TYPE_UINT, hdrext->id,
              G_TYPE_UINT, fsdirection_to_tpdirection (hdrext->direction),
              G_TYPE_STRING, hdrext->uri,
              G_TYPE_STRING, "",
              G_TYPE_INVALID));
    }

  return extensions;
}

void
_tf_stream_try_sending_codecs (TfStream *stream)
{
  GList *fscodecs = NULL;
  GList *item = NULL;
  GPtrArray *tpcodecs = NULL;
  GHashTable *feedback_messages = NULL;
  GPtrArray *header_extensions = NULL;
  gboolean sent = FALSE;
  GList *resend_codecs = NULL;
  const gchar *codecs_prop = NULL;

  DEBUG (stream, "called (send_local:%d send_supported:%d)",
      stream->priv->send_local_codecs, stream->priv->send_supported_codecs);


  if (stream->priv->has_resource & TP_MEDIA_STREAM_DIRECTION_SEND)
    codecs_prop = "codecs";
  else
    codecs_prop = "codecs-without-config";

  g_object_get (stream->priv->fs_session, codecs_prop, &fscodecs, NULL);

  if (!fscodecs)
    {
      DEBUG (stream, "Ignoring new codecs because we're sending,"
          " but we're not ready");
      return;
    }

  for(item = fscodecs; item; item = g_list_next (item))
    {
      gchar *tmp = fs_codec_to_string (item->data);
      DEBUG (stream, "%s", tmp);
      g_free (tmp);
    }

  if (stream->priv->send_local_codecs)
    {
      tpcodecs = fs_codecs_to_tp (stream, fscodecs);
      feedback_messages = fs_codecs_to_feedback_messages (fscodecs);
      header_extensions = _tf_stream_get_header_extensions (stream);

      DEBUG (stream, "calling MediaStreamHandler::Ready");
      tp_cli_media_stream_handler_call_supported_feedback_messages (
          stream->priv->stream_handler_proxy,
          -1, feedback_messages, async_method_callback_optional,
          "Media.StreamHandler::SupportedFeedbackMessages for Ready",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_supported_header_extensions (
          stream->priv->stream_handler_proxy,
          -1, header_extensions, async_method_callback_optional,
          "Media.StreamHandler::SupportedHeaderExtensions for Ready",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_ready (
          stream->priv->stream_handler_proxy,
          -1, tpcodecs, async_method_callback, "Media.StreamHandler::Ready",
          NULL, (GObject *) stream);
      stream->priv->send_local_codecs = FALSE;
      sent = TRUE;
      goto out;
    }

  if (stream->priv->send_supported_codecs)
    {
      tpcodecs = fs_codecs_to_tp (stream, fscodecs);
      feedback_messages = fs_codecs_to_feedback_messages (fscodecs);
      header_extensions = _tf_stream_get_header_extensions (stream);

      DEBUG (stream, "calling MediaStreamHandler::SupportedCodecs");
      tp_cli_media_stream_handler_call_supported_feedback_messages (
          stream->priv->stream_handler_proxy,
          -1, feedback_messages, async_method_callback_optional,
          "Media.StreamHandler::SupportedFeedbackMessages for SupportedCodecs",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_supported_header_extensions (
          stream->priv->stream_handler_proxy,
          -1, header_extensions, async_method_callback_optional,
          "Media.StreamHandler::SupportedHeaderExtensions for SupportedCodecs",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_supported_codecs (
          stream->priv->stream_handler_proxy,
          -1, tpcodecs, async_method_callback,
          "Media.StreamHandler::SupportedCodecs", NULL, (GObject *) stream);
      stream->priv->send_supported_codecs = FALSE;
      sent = TRUE;

      /* Fallthrough to potentially call CodecsUpdated as CMs assume
       * SupportedCodecs will only give the intersection of the already sent
       * (if any) local codecs, not any updates */
    }


  /* Only send updates if there was something to update (iotw we sent codecs
   * before) or our list changed */
  if (stream->priv->last_sent_codecs != NULL
      && (resend_codecs =
          fs_session_codecs_need_resend (stream->priv->fs_session,
              stream->priv->last_sent_codecs, fscodecs)) != NULL)
    {
      fs_codec_list_destroy (resend_codecs);

      if (!tpcodecs)
        tpcodecs = fs_codecs_to_tp (stream, fscodecs);
      if (!feedback_messages)
        feedback_messages = fs_codecs_to_feedback_messages (fscodecs);
      if (!header_extensions)
        header_extensions = _tf_stream_get_header_extensions (stream);


      DEBUG (stream, "calling MediaStreamHandler::CodecsUpdated");
      tp_cli_media_stream_handler_call_supported_feedback_messages (
          stream->priv->stream_handler_proxy,
          -1, feedback_messages, async_method_callback_optional,
          "Media.StreamHandler::SupportedFeedbackMessages for CodecsUpdated",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_supported_header_extensions (
          stream->priv->stream_handler_proxy,
          -1, header_extensions, async_method_callback_optional,
          "Media.StreamHandler::SupportedHeaderExtensions for CodecsUpdated",
          NULL, (GObject *) stream);
      tp_cli_media_stream_handler_call_codecs_updated (
          stream->priv->stream_handler_proxy,
          -1, tpcodecs, async_method_callback,
          "Media.StreamHandler::CodecsUpdated", NULL, (GObject *) stream);
      sent = TRUE;
    }

out:
  if (tpcodecs)
    g_boxed_free (TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_CODEC_LIST, tpcodecs);
  if (feedback_messages)
    g_boxed_free (TP_HASH_TYPE_RTCP_FEEDBACK_MESSAGE_MAP, feedback_messages);
  if (header_extensions)
    g_boxed_free (TP_ARRAY_TYPE_RTP_HEADER_EXTENSIONS_LIST, header_extensions);
  if (sent)
    {
      fs_codec_list_destroy (stream->priv->last_sent_codecs);
      stream->priv->last_sent_codecs = fscodecs;
    }
}

/**
 * tf_stream_get_id
 * @stream: A #TfStream
 *
 * Quick getter for the stream id
 *
 * Returns: the stream's id
 */

guint
tf_stream_get_id (TfStream *stream)
{
  g_return_val_if_fail (TF_IS_STREAM (stream), 0);

  return stream->stream_id;
}

static TpMediaStreamError
fserrorno_to_tperrorno (FsError fserror)
{
  TpMediaStreamError tperror;

  switch (fserror)
  {
    case FS_ERROR_NETWORK:
      tperror = TP_MEDIA_STREAM_ERROR_NETWORK_ERROR;
      break;
    case FS_ERROR_CONNECTION_FAILED:
      tperror = TP_MEDIA_STREAM_ERROR_CONNECTION_FAILED;
      break;
    case FS_ERROR_NO_CODECS:
      tperror = TP_MEDIA_STREAM_ERROR_NO_CODECS;
      break;
    case FS_ERROR_NEGOTIATION_FAILED:
      tperror = TP_MEDIA_STREAM_ERROR_CODEC_NEGOTIATION_FAILED;
      break;
    case FS_ERROR_INVALID_ARGUMENTS:
      tperror = TP_MEDIA_STREAM_ERROR_INVALID_CM_BEHAVIOR;
      break;
    case FS_ERROR_NO_CODECS_LEFT:
    case FS_ERROR_CONSTRUCTION:
    case FS_ERROR_INTERNAL:
    case FS_ERROR_NOT_IMPLEMENTED: /* Not really a real error */
    case FS_ERROR_DISPOSED: /* Not really a real error */
    default:
      tperror = TP_MEDIA_STREAM_ERROR_MEDIA_ERROR;
  }

  return tperror;
}

TpMediaStreamError
fserror_to_tperror (GError *error)
{
  if (!error || error->domain != FS_ERROR)
    return TP_MEDIA_STREAM_ERROR_UNKNOWN;

  return fserrorno_to_tperrorno (error->code);
}

static void
cb_fs_component_state_changed (TfStream *self,
    guint component,
    FsStreamState fsstate)
{
  TpMediaStreamState state;
  const gchar *state_str = "";

  if (component != 1)
    return;

  switch (fsstate)
  {
    case FS_STREAM_STATE_FAILED:
    case FS_STREAM_STATE_DISCONNECTED:
      state = TP_MEDIA_STREAM_STATE_DISCONNECTED;
      state_str = "disconnected";
      break;
    case FS_STREAM_STATE_GATHERING:
    case FS_STREAM_STATE_CONNECTING:
      state = TP_MEDIA_STREAM_STATE_CONNECTING;
      state_str = "connecting";
      break;
    case FS_STREAM_STATE_CONNECTED:
    default:
      state_str = "connected";
      state = TP_MEDIA_STREAM_STATE_CONNECTED;
      break;
  }

  DEBUG (self, "calling MediaStreamHandler::StreamState (%u: %s)", state,
         state_str);

  self->priv->current_state = state;

  tp_cli_media_stream_handler_call_stream_state (
      self->priv->stream_handler_proxy, -1, state,
      async_method_callback, "Media.StreamHandler::StreamState",
      NULL, (GObject *) self);
}
