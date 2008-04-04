/*
 * stream.c - Source for TpStreamEngineStream
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "stream.h"
#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineStream, tp_stream_engine_stream, G_TYPE_OBJECT);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (%s) %s: " format, \
    stream->stream_id, \
    (stream->priv->media_type == FARSIGHT_MEDIA_TYPE_AUDIO) ? "audio" \
                                                            : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define STREAM_PRIVATE(o) ((o)->priv)

struct _TpStreamEngineStreamPrivate
{
  FarsightSession *fs_session;
  TpMediaStreamType media_type;
  TpMediaStreamDirection direction;
  const TpStreamEngineNatProperties *nat_props;
  GstBin *pipeline;

  TpMediaStreamHandler *stream_handler_proxy;

  gboolean playing;
  FarsightStreamState state;
  FarsightStreamDirection dir;
};

enum
{
  CLOSED,
  ERROR,
  STATE_CHANGED,
  RECEIVING,
  LINKED,
  REQUEST_RESOURCE,
  FREE_RESOURCE,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

/* properties */
enum
{
  PROP_FARSIGHT_SESSION = 1,
  PROP_PROXY,
  PROP_STREAM_ID,
  PROP_MEDIA_TYPE,
  PROP_DIRECTION,
  PROP_NAT_PROPERTIES,
  PROP_PIPELINE,
  PROP_SOURCE,
  PROP_SINK
};

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

static GstElement *make_src (TpStreamEngineStream *stream, guint media_type);

static GstElement *make_sink (TpStreamEngineStream *stream, guint media_type);

static void cb_fs_stream_error (FarsightStream *stream,
    FarsightStreamError error, const gchar *debug, gpointer user_data);

static void cb_fs_new_active_candidate_pair (FarsightStream *stream,
    const gchar *native_candidate, const gchar *remote_candidate,
    gpointer user_data);

static void cb_fs_codec_changed (FarsightStream *stream, gint codec_id,
    gpointer user_data);

static void cb_fs_new_active_candidate_pair (FarsightStream *stream,
    const gchar *native_candidate, const gchar *remote_candidate,
    gpointer user_data);

static void cb_fs_native_candidates_prepared (FarsightStream *stream,
    gpointer user_data);

static void cb_fs_state_changed (FarsightStream *stream,
    FarsightStreamState state, FarsightStreamDirection dir,
    gpointer user_data);

static void cb_fs_new_native_candidate (FarsightStream *stream,
    gchar *candidate_id, gpointer user_data);

static void set_nat_properties (TpStreamEngineStream *self);

static void prepare_transports (TpStreamEngineStream *self);

static void stop_stream (TpStreamEngineStream *self);

static void invalidated_cb (TpMediaStreamHandler *proxy,
    guint domain, gint code, gchar *message, gpointer user_data);


static void
tp_stream_engine_stream_init (TpStreamEngineStream *self)
{
  TpStreamEngineStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_STREAM, TpStreamEngineStreamPrivate);

  self->priv = priv;
}

static void
tp_stream_engine_stream_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  switch (property_id)
    {
    case PROP_FARSIGHT_SESSION:
      g_value_set_object (value, self->priv->fs_session);
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
    case PROP_PIPELINE:
      g_value_set_object (value,
          farsight_stream_get_pipeline (self->fs_stream));
      break;
    case PROP_SOURCE:
      g_value_set_object (value,
          farsight_stream_get_source (self->fs_stream));
      break;
    case PROP_SINK:
      g_value_set_object (value,
          farsight_stream_get_sink (self->fs_stream));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_stream_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  switch (property_id)
    {
    case PROP_FARSIGHT_SESSION:
      self->priv->fs_session = FARSIGHT_SESSION (g_value_dup_object (value));
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
    case PROP_PIPELINE:
      g_assert (self->priv->pipeline == NULL);
      self->priv->pipeline = (GstBin *) g_value_dup_object (value);
      break;
    case PROP_SOURCE:
      if (!farsight_stream_set_source (self->fs_stream,
              g_value_get_object (value)))
        g_warning ("Could not set source on farsight stream");
      break;
    case PROP_SINK:

      if (!farsight_stream_set_sink (self->fs_stream,
              g_value_get_object (value)))
        g_warning ("Could not set sink on farsight stream");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static GObject *
tp_stream_engine_stream_constructor (GType type,
                                     guint n_props,
                                     GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineStream *stream;
  TpStreamEngineStreamPrivate *priv;
  const gchar *conn_timeout_str;
  GstElement *src, *sink;
  TpStreamEngineStreamClass *klass = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->
            constructor (type, n_props, props);
  stream = (TpStreamEngineStream *) obj;
  priv = stream->priv;
  klass = TP_STREAM_ENGINE_STREAM_GET_CLASS(obj);

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

  stream->fs_stream = farsight_session_create_stream (priv->fs_session,
      priv->media_type, priv->direction);

  if (priv->pipeline != NULL)
    {
      farsight_stream_set_pipeline (stream->fs_stream,
          (GstElement *) priv->pipeline);
      g_object_unref ((GObject *) priv->pipeline);
      priv->pipeline = NULL;
    }

  conn_timeout_str = getenv ("FS_CONN_TIMEOUT");

  if (conn_timeout_str)
    {
      gint conn_timeout = (int) g_ascii_strtod (conn_timeout_str, NULL);
      DEBUG (stream, "setting connection timeout to %d", conn_timeout);
      g_object_set (G_OBJECT (stream->fs_stream),
          "conn_timeout", conn_timeout,
          NULL);
    }

  /* TODO Make this smarter, we should only create those sources and sinks if
   * they exist. */
  if (klass->make_src)
    src = klass->make_src (stream);
  else
    src = make_src (stream, priv->media_type);
  if (klass->make_sink)
    sink = klass->make_sink (stream);
  else
    sink = make_sink (stream, priv->media_type);

  if (src)
    {
      DEBUG (stream, "setting source on Farsight stream");

      if (!farsight_stream_set_source (stream->fs_stream, src))
        g_warning ("Could not set source on farsight stream");
    }
  else
    {
      DEBUG (stream, "not setting source on Farsight stream");
    }

  if (sink)
    {
      DEBUG (stream, "setting sink on Farsight stream");

      if (!farsight_stream_set_sink (stream->fs_stream, sink))
        g_warning ("Could not set sink on farsight stream");
    }
  else
    {
      DEBUG (stream, "not setting sink on Farsight stream");
    }

  g_signal_connect (G_OBJECT (stream->fs_stream), "error",
      G_CALLBACK (cb_fs_stream_error), obj);
  g_signal_connect (G_OBJECT (stream->fs_stream),
      "new-active-candidate-pair",
      G_CALLBACK (cb_fs_new_active_candidate_pair), obj);
  g_signal_connect (G_OBJECT (stream->fs_stream),
      "codec-changed", G_CALLBACK (cb_fs_codec_changed), obj);
  g_signal_connect (G_OBJECT (stream->fs_stream),
      "native-candidates-prepared",
      G_CALLBACK (cb_fs_native_candidates_prepared), obj);
  g_signal_connect (G_OBJECT (stream->fs_stream), "state-changed",
        G_CALLBACK (cb_fs_state_changed), obj);
  g_signal_connect (G_OBJECT (stream->fs_stream),
      "new-native-candidate",
      G_CALLBACK (cb_fs_new_native_candidate), obj);

  set_nat_properties (stream);

  prepare_transports (stream);

  return obj;
}

static void
tp_stream_engine_stream_dispose (GObject *object)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = stream->priv;

  g_assert (priv->pipeline == NULL);

  if (priv->fs_session)
    {
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = priv->stream_handler_proxy;

      g_signal_handlers_disconnect_by_func (
          priv->stream_handler_proxy, invalidated_cb, stream);

      priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  if (stream->fs_stream)
    {
      stop_stream (stream);

      /* Free resources used by this stream
       * If this stream was not the active one, then the signal
       * would be ignored by stream-engine
       */
      g_signal_emit (stream, signals[FREE_RESOURCE], 0);

      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_stream_error, stream);
      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_new_active_candidate_pair, stream);
      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_codec_changed, stream);
      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_native_candidates_prepared, stream);
      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_state_changed, stream);
      g_signal_handlers_disconnect_by_func (
          stream->fs_stream, cb_fs_new_native_candidate, stream);

      g_object_unref (stream->fs_stream);
      stream->fs_stream = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->dispose (object);
}

static void
tp_stream_engine_stream_class_init (TpStreamEngineStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineStreamPrivate));

  object_class->set_property = tp_stream_engine_stream_set_property;
  object_class->get_property = tp_stream_engine_stream_get_property;

  object_class->constructor = tp_stream_engine_stream_constructor;

  object_class->dispose = tp_stream_engine_stream_dispose;

  param_spec = g_param_spec_object ("farsight-session",
                                    "Farsight session",
                                    "The Farsight session this stream will "
                                    "create streams within.",
                                    FARSIGHT_TYPE_SESSION,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_FARSIGHT_SESSION,
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

  param_spec = g_param_spec_object ("pipeline",
                                    "GStreamer pipeline",
                                    "The GStreamer pipeline this stream will "
                                    "use.",
                                    GST_TYPE_BIN,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PIPELINE, param_spec);

  param_spec = g_param_spec_object ("source",
                                    "GStreamer source",
                                    "The GStreamer source element this stream "
                                    "will use.",
                                    GST_TYPE_ELEMENT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SOURCE, param_spec);

  param_spec = g_param_spec_object ("sink",
                                    "GStreamer sink",
                                    "The GStreamer sink element this stream "
                                    "will use.",
                                    GST_TYPE_ELEMENT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SINK, param_spec);

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

  signals[STATE_CHANGED] =
    g_signal_new ("state-changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  tp_stream_engine_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  signals[RECEIVING] =
    g_signal_new ("receiving",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[LINKED] =
    g_signal_new ("linked",
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
                  tp_stream_engine_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  signals[FREE_RESOURCE] =
    g_signal_new ("free-resource",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/* dummy callback handler for async calling calls with no return values */
static void
async_method_callback (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                       const GError *error,
                       gpointer user_data,
                       GObject *weak_object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (weak_object);

  if (error != NULL)
    {
      g_warning ("Error calling %s: %s", (gchar *) user_data, error->message);
      g_signal_emit (self, signals[ERROR], 0);
    }
}

static void
cb_fs_state_changed (FarsightStream *stream,
                     FarsightStreamState state,
                     FarsightStreamDirection dir,
                     gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  const gchar *state_str = "invalid!", *dir_str = "invalid!";

  switch (state)
    {
    case FARSIGHT_STREAM_STATE_DISCONNECTED:
      state_str = "disconnected";
      break;
    case FARSIGHT_STREAM_STATE_CONNECTING:
      state_str = "connecting";
      break;
    case FARSIGHT_STREAM_STATE_CONNECTED:
      state_str = "connected";
      break;
    }

  switch (dir)
    {
    case FARSIGHT_STREAM_DIRECTION_NONE:
      dir_str = "none";
      break;
    case FARSIGHT_STREAM_DIRECTION_SENDONLY:
      dir_str = "send";
      break;
    case FARSIGHT_STREAM_DIRECTION_RECEIVEONLY:
      dir_str = "receive";
      break;
    case FARSIGHT_STREAM_DIRECTION_BOTH:
      dir_str = "both";
      break;
    case FARSIGHT_STREAM_DIRECTION_LAST:
      break;
    }

  DEBUG (self, "stream %p, state: %s, direction: %s", stream, state_str,
      dir_str);

  if (self->priv->state != state || self->priv->dir != dir)
    {
      g_signal_emit (self, signals[STATE_CHANGED], 0, state, dir);
    }

  if (self->priv->state != state)
    {
      if (self->priv->stream_handler_proxy)
        {
          tp_cli_media_stream_handler_call_stream_state (
            self->priv->stream_handler_proxy, -1, state,
            async_method_callback, "Media.StreamHandler::StreamState", NULL,
            (GObject *) self);
        }

      self->priv->state = state;
    }

  if (self->priv->dir != dir)
    {
      if ((self->priv->dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY) !=
          (dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY))
        {
          gboolean receiving =
            ((dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY) ==
             FARSIGHT_STREAM_DIRECTION_RECEIVEONLY);
          g_signal_emit (self, signals[RECEIVING], 0, receiving);
        }

      self->priv->dir = dir;
    }
}

static void
cb_fs_new_native_candidate (FarsightStream *stream,
                            gchar *candidate_id,
                            gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  const GList *fs_candidates, *lp;
  GPtrArray *transports;

  fs_candidates = farsight_stream_get_native_candidate (stream, candidate_id);
  transports = g_ptr_array_new ();

  for (lp = fs_candidates; lp; lp = lp->next)
    {
      FarsightTransportInfo *fs_transport = lp->data;
      GValue transport = { 0, };
      TpMediaStreamBaseProto proto;
      TpMediaStreamTransportType type;

      g_value_init (&transport, TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT);
      g_value_set_static_boxed (&transport,
          dbus_g_type_specialized_construct (TP_STRUCT_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT));

      switch (fs_transport->proto) {
        case FARSIGHT_NETWORK_PROTOCOL_UDP:
          proto = TP_MEDIA_STREAM_BASE_PROTO_UDP;
          break;
        case FARSIGHT_NETWORK_PROTOCOL_TCP:
          proto = TP_MEDIA_STREAM_BASE_PROTO_TCP;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          return;
      }

      switch (fs_transport->type) {
        case FARSIGHT_CANDIDATE_TYPE_LOCAL:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_LOCAL;
          break;
        case FARSIGHT_CANDIDATE_TYPE_DERIVED:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_DERIVED;
          break;
        case FARSIGHT_CANDIDATE_TYPE_RELAY:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          return;
      }

      DEBUG (self, "fs_transport->ip = '%s'", fs_transport->ip);

      dbus_g_type_struct_set (&transport,
          0, fs_transport->component,
          1, fs_transport->ip,
          2, fs_transport->port,
          3, proto,
          4, fs_transport->proto_subtype,
          5, fs_transport->proto_profile,
          6, (double) fs_transport->preference,
          7, type,
          8, fs_transport->username,
          9, fs_transport->password,
          G_MAXUINT);

      g_ptr_array_add (transports, g_value_get_boxed (&transport));
    }

  tp_cli_media_stream_handler_call_new_native_candidate (
      self->priv->stream_handler_proxy, -1, candidate_id, transports,
      async_method_callback, "Media.StreamHandler::NativeCandidatesPrepared",
      NULL, (GObject *) self);
}

/**
 * small helper function to help converting a
 * telepathy dbus candidate to a list of FarsightTransportInfos
 * nothing is copied, so always keep the usage of this within a function
 * if you need to do multiple candidates, call this repeatedly and
 * g_list_join them together.
 * Free the list using free_fs_transports
 */
static GList *
tp_transports_to_fs (const gchar* candidate, const GPtrArray *transports)
{
  GList *fs_trans_list = NULL;
  GValueArray *transport;
  FarsightTransportInfo *fs_transport;
  guint i;

  for (i=0; i< transports->len; i++)
    {
      transport = g_ptr_array_index (transports, i);
      fs_transport = g_new0 (FarsightTransportInfo, 1);

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

      fs_transport->candidate_id = candidate;
      fs_transport->component =
        g_value_get_uint (g_value_array_get_nth (transport, 0));
      fs_transport->ip =
        g_value_get_string (g_value_array_get_nth (transport, 1));
      fs_transport->port =
        (guint16) g_value_get_uint (g_value_array_get_nth (transport, 2));
      fs_transport->proto =
        g_value_get_uint (g_value_array_get_nth (transport, 3));
      fs_transport->proto_subtype =
        g_value_get_string (g_value_array_get_nth (transport, 4));
      fs_transport->proto_profile =
        g_value_get_string (g_value_array_get_nth (transport, 5));
      fs_transport->preference =
        (float) g_value_get_double (g_value_array_get_nth (transport, 6));
      fs_transport->type =
        g_value_get_uint (g_value_array_get_nth (transport, 7));
      fs_transport->username =
        g_value_get_string (g_value_array_get_nth (transport, 8));
      fs_transport->password =
        g_value_get_string (g_value_array_get_nth (transport, 9));

      fs_trans_list = g_list_prepend (fs_trans_list, fs_transport);
    }
  fs_trans_list = g_list_reverse (fs_trans_list);

  return fs_trans_list;
}

static void
free_fs_transports (GList *fs_trans_list)
{
  GList *lp;
  for (lp = g_list_first (fs_trans_list); lp; lp = g_list_next (lp))
    {
      g_free(lp->data);
    }
  g_list_free (fs_trans_list);
}

/**
 * Small helper function to help converting a list of FarsightCodecs
 * to a Telepathy codec list.
 */
static GPtrArray *
fs_codecs_to_tp (TpStreamEngineStream *stream,
                 const GList *codecs)
{
  GPtrArray *tp_codecs;
  const GList *el;

  tp_codecs = g_ptr_array_new ();

  for (el = codecs; el; el = g_list_next (el))
    {
      FarsightCodec *fsc = el->data;
      GValue codec = { 0, };
      TpMediaStreamType type;
      GHashTable *params;
      GList *cur;

      switch (fsc->media_type) {
        case FARSIGHT_MEDIA_TYPE_AUDIO:
          type = TP_MEDIA_STREAM_TYPE_AUDIO;
          break;
        case FARSIGHT_MEDIA_TYPE_VIDEO:
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
          FarsightCodecParameter *param = (FarsightCodecParameter *) cur->data;

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
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  GList *fs_transports;

  fs_transports = tp_transports_to_fs (candidate, transports);

  DEBUG (self, "adding remote candidate %s", candidate);
  farsight_stream_add_remote_candidate (self->fs_stream, fs_transports);

  free_fs_transports (fs_transports);
}

static void
remove_remote_candidate (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                         const gchar *candidate,
                         gpointer user_data G_GNUC_UNUSED,
                         GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  DEBUG (self, "removing remote candidate %s", candidate);
  farsight_stream_remove_remote_candidate (self->fs_stream, candidate);
}

static void
set_active_candidate_pair (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                           const gchar *native_candidate,
                           const gchar *remote_candidate,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  farsight_stream_set_active_candidate_pair (self->fs_stream,
                                             native_candidate,
                                             remote_candidate);
}

static void
set_remote_candidate_list (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                           const GPtrArray *candidates,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  GList *fs_transports = NULL;
  GValueArray *candidate = NULL;
  GPtrArray *transports = NULL;
  gchar *candidate_id = NULL;
  guint i;

  for (i = 0; i < candidates->len; i++)
    {
      candidate = g_ptr_array_index (candidates, i);
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (candidate,0)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (candidate, 1)) ==
                               TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT_LIST);

      /* TODO: mmm, candidate_id should be const in Farsight API */
      candidate_id =
        (gchar*) g_value_get_string (g_value_array_get_nth (candidate, 0));
      transports =
        g_value_get_boxed (g_value_array_get_nth (candidate, 1));

      fs_transports = g_list_concat(fs_transports,
                        tp_transports_to_fs (candidate_id, transports));
    }

  farsight_stream_set_remote_candidate_list (self->fs_stream,
      fs_transports);
  free_fs_transports (fs_transports);
}

static void
fill_fs_params (gpointer key, gpointer value, gpointer user_data)
{
  GList **fs_params = (GList **) user_data;
  FarsightCodecParameter *param = g_new0(FarsightCodecParameter,1);
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
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  GList *fs_codecs =NULL, *lp, *lp2;
  GValueArray *codec;
  GHashTable *params = NULL;
  FarsightCodec *fs_codec;
  GList *fs_params = NULL;
  guint i;
  GPtrArray *supp_codecs;

  DEBUG (self, "called");

  for (i = 0; i < codecs->len; i++)
    {
      codec = g_ptr_array_index (codecs, i);
      fs_codec = g_new0(FarsightCodec,1);

      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,0)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (codec,1)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,2)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,3)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,4)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (codec, 5)) ==
                               DBUS_TYPE_G_STRING_STRING_HASHTABLE);

      fs_codec->id =
        g_value_get_uint (g_value_array_get_nth (codec, 0));
      /* TODO: Farsight API should take const strings */
      fs_codec->encoding_name =
        (gchar*)g_value_get_string (g_value_array_get_nth (codec, 1));
      fs_codec->media_type =
        g_value_get_uint (g_value_array_get_nth (codec, 2));
      fs_codec->clock_rate =
        g_value_get_uint (g_value_array_get_nth (codec, 3));
      fs_codec->channels =
        g_value_get_uint (g_value_array_get_nth (codec, 4));

      params = g_value_get_boxed (g_value_array_get_nth (codec, 5));
      fs_params = NULL;
      g_hash_table_foreach (params, fill_fs_params, &fs_params);

      fs_codec->optional_params = fs_params;

      g_message ("%s: adding remote codec %s [%d]",
          G_STRFUNC, fs_codec->encoding_name, fs_codec->id);

      fs_codecs = g_list_prepend (fs_codecs, fs_codec);
  }
  fs_codecs = g_list_reverse (fs_codecs);

  if (!farsight_stream_set_remote_codecs (self->fs_stream, fs_codecs)) {
    /*
     * Call the error method with the proper thing here
     */
    g_warning("Negotiation failed");
    tp_stream_engine_stream_error (self, 0, "Codec negotiation failed");
    return;
  }

  supp_codecs = fs_codecs_to_tp (self,
      farsight_stream_get_codec_intersection (self->fs_stream));

  tp_cli_media_stream_handler_call_supported_codecs
    (self->priv->stream_handler_proxy, -1, supp_codecs, async_method_callback,
     "Media.StreamHandler::SupportedCodecs", NULL, (GObject *) self);

  for (lp = g_list_first (fs_codecs); lp; lp = g_list_next (lp))
    {
      /*free the optional parameters lists*/
      fs_codec = (FarsightCodec*) lp->data;
      fs_params = fs_codec->optional_params;
      for (lp2 = g_list_first (fs_params); lp2; lp2 = g_list_next (lp2))
      {
        g_free(lp2->data);
      }
      g_list_free(fs_params);
      g_free(lp->data);
    }
  g_list_free (fs_codecs);
}

static void
stop_stream (TpStreamEngineStream *self)
{
  TpStreamEngineStreamClass *klass = TP_STREAM_ENGINE_STREAM_GET_CLASS (self);

  if (klass->stop_stream)
    {
      klass->stop_stream (self);
      return;
    }

  if (!self->fs_stream)
    return;

  DEBUG (self, "calling stop on farsight stream %p", self->fs_stream);

  farsight_stream_stop (self->fs_stream);

  farsight_stream_set_source (self->fs_stream, NULL);
}

static void
set_stream_playing (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                    gboolean play,
                    gpointer user_data G_GNUC_UNUSED,
                    GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  gboolean resource_available = FALSE;

  g_assert (self->fs_stream != NULL);

  DEBUG (self, "%d", play);

  if (play)
    {
      g_signal_emit (self, signals[REQUEST_RESOURCE], 0, &resource_available);
      if (resource_available)
        {
          self->priv->playing = TRUE;
          farsight_stream_start (self->fs_stream);
        }
      else
        {
          stop_stream (self);
          tp_stream_engine_stream_error (self, 0, "Resource Unavailable");
        }
    }
  else if (self->priv->playing)
    {
      stop_stream (self);
      g_signal_emit (self, signals[FREE_RESOURCE], 0);
    }
}

static void
set_stream_sending (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                    gboolean send,
                    gpointer user_data G_GNUC_UNUSED,
                    GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  g_assert (self->fs_stream != NULL);

  DEBUG (self, "%d", send);

  farsight_stream_set_sending (self->fs_stream, send);
}


static void
set_stream_held (TpMediaStreamHandler *proxy,
                    gboolean held,
                    gpointer user_data,
                    GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  gboolean resource_available = FALSE;

  g_assert (self->fs_stream != NULL);

  DEBUG (self, "Holding : %d", held);

  if (held)
    {
      /* Hold the stream */
      if (farsight_stream_hold (self->fs_stream))
        {
          g_signal_emit (self, signals[FREE_RESOURCE], 0);
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
          stop_stream (self);
          tp_stream_engine_stream_error (self, 0, "Error holding stream");
          g_signal_emit (self, signals[FREE_RESOURCE], 0);
        }
    }
  else
    {
      g_signal_emit (self, signals[REQUEST_RESOURCE], 0, &resource_available);
      /* Make sure we have access to the resource */
      if (resource_available)
        {
          /* Unhold the stream */
          if (farsight_stream_unhold (self->fs_stream))
            {
              /* Send success message */
              if (self->priv->stream_handler_proxy)
                {
                  tp_cli_media_stream_handler_call_hold_state (
                      self->priv->stream_handler_proxy, -1, FALSE,
                      async_method_callback, "Media.StreamHandler::HoldState",
                      NULL, (GObject *) self);
                }
            }
          else
            {
              stop_stream (self);
              tp_stream_engine_stream_error (self, 0, "Error unholding stream");
              g_signal_emit (self, signals[FREE_RESOURCE], 0);
            }
        }
      else
        {
          /* Send failure message */
          if (self->priv->stream_handler_proxy)
            {
              tp_cli_media_stream_handler_call_unhold_failure (
                  self->priv->stream_handler_proxy, -1,
                  async_method_callback, "Media.StreamHandler::UnholdFailure",
                  NULL, (GObject *) self);
            }
        }
    }
}

static void
start_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                       guchar event,
                       gpointer user_data G_GNUC_UNUSED,
                       GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  g_assert (self->fs_stream != NULL);

  DEBUG (self, "called with event %u", event);

  /* this week, volume is 8, for the sake of argument... */
  if (!farsight_stream_start_telephony_event (self->fs_stream, event, 8))
    DEBUG (self, "sending event %u failed", event);
}

static void
stop_telephony_event (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                      gpointer user_data G_GNUC_UNUSED,
                      GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  g_assert (self->fs_stream != NULL);

  DEBUG (self, "called");

  if (!farsight_stream_stop_telephony_event (self->fs_stream))
    DEBUG (self, "stopping event failed");
}

static void
close (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
       gpointer user_data G_GNUC_UNUSED,
       GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);

  DEBUG (self, "close requested by connection manager");

  stop_stream (self);
  g_signal_emit (self, signals[CLOSED], 0);
}

static void
set_nat_properties (TpStreamEngineStream *self)
{
  const TpStreamEngineNatProperties *props = self->priv->nat_props;
  FarsightStream *stream = self->fs_stream;
  const gchar *transmitter = "rawudp";
  GObject *xmit = NULL;

  if (props == NULL ||
      props->nat_traversal == NULL ||
      !strcmp (props->nat_traversal, "gtalk-p2p"))
    {
      transmitter = "libjingle";
    }

  if (g_object_has_property ((GObject *) stream, "transmitter"))
    {
      DEBUG (self, "setting farsight transmitter to %s", transmitter);
      g_object_set (stream, "transmitter", transmitter, NULL);
    }

  if (props == NULL)
    {
      return;
    }

  /* transmitter should have been created as a result of setting transmitter-name */
  g_object_get (stream, "transmitter-object", &xmit, NULL);
  g_return_if_fail (xmit != NULL);

  if ((props->stun_server != NULL) && g_object_has_property (xmit, "stun-ip"))
    {
      DEBUG (self, "setting farsight stun-ip to %s", props->stun_server);
      g_object_set (xmit, "stun-ip", props->stun_server, NULL);

      if (props->stun_port != 0)
        {
          DEBUG (self, "setting farsight stun-port to %u", props->stun_port);
          g_object_set (xmit, "stun-port", props->stun_port, NULL);
        }
    }

  if ((props->relay_token != NULL) && g_object_has_property (xmit, "relay-token"))
    {
      DEBUG (self, "setting farsight relay-token to %s", props->relay_token);
      g_object_set (xmit, "relay-token", props->relay_token, NULL);
    }

  g_object_unref (xmit);
}

static void
prepare_transports (TpStreamEngineStream *self)
{
  GPtrArray *codecs;

  farsight_stream_prepare_transports (self->fs_stream);

  codecs = fs_codecs_to_tp (self,
      farsight_stream_get_local_codecs (self->fs_stream));

  DEBUG (self, "calling MediaStreamHandler::Ready");

  tp_cli_media_stream_handler_call_ready (self->priv->stream_handler_proxy,
      -1, codecs, async_method_callback, "Media.StreamHandler::Ready",
      NULL, (GObject *) self);
}

static void
cb_fs_codec_changed (FarsightStream *stream,
                     gint codec_id,
                     gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);

  DEBUG (self, "codec_id=%d, stream=%p", codec_id, stream);

  tp_cli_media_stream_handler_call_codec_choice
      (self->priv->stream_handler_proxy, -1, codec_id,
       async_method_callback, "Media.StreamHandler::CodecChoice",
       NULL, (GObject *) self);
}

static void
cb_fs_stream_error (FarsightStream *stream G_GNUC_UNUSED,
                    FarsightStreamError error G_GNUC_UNUSED,
                    const gchar *debug,
                    gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);

  /* FIXME: map Farsight errors to Telepathy errors */
  tp_stream_engine_stream_error (self, 0, debug);
}

static void
cb_fs_new_active_candidate_pair (FarsightStream *stream,
                                 const gchar* native_candidate,
                                 const gchar *remote_candidate,
                                 gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);

  DEBUG (self, "stream=%p", stream);

  tp_cli_media_stream_handler_call_new_active_candidate_pair (
    self->priv->stream_handler_proxy, -1, native_candidate, remote_candidate,
    async_method_callback, "Media.StreamHandler::NewActiveCandidatePair",
    NULL, (GObject *) self);
}

static void
cb_fs_native_candidates_prepared (FarsightStream *stream,
                                  gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  const GList *transport_candidates, *lp;
  FarsightTransportInfo *info;

  DEBUG (self, "stream=%p", stream);

  transport_candidates = farsight_stream_get_native_candidate_list (stream);
  for (lp = transport_candidates; lp; lp = g_list_next (lp))
  {
    info = (FarsightTransportInfo*)lp->data;
    DEBUG (self, "local transport candidate: %s %d %s %s %s:%d, pref %f",
        info->candidate_id, info->component,
        (info->proto == FARSIGHT_NETWORK_PROTOCOL_TCP) ? "TCP" : "UDP",
        info->proto_subtype, info->ip, info->port, (double) info->preference);
  }

  tp_cli_media_stream_handler_call_native_candidates_prepared (
    self->priv->stream_handler_proxy, -1, async_method_callback,
    "Media.StreamHandler::NativeCandidatesPrepared",
    NULL, (GObject *) self);
}

static GstElement *
make_src (TpStreamEngineStream *stream,
          guint media_type)
{
  g_assert_not_reached ();

  return NULL;
}

static GstElement *
make_sink (TpStreamEngineStream *stream, guint media_type)
{
  g_assert_not_reached ();

  return NULL;
}

static void
invalidated_cb (TpMediaStreamHandler *proxy G_GNUC_UNUSED,
                guint domain G_GNUC_UNUSED,
                gint code G_GNUC_UNUSED,
                gchar *message G_GNUC_UNUSED,
                gpointer user_data)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (user_data);

  if (stream->priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = stream->priv->stream_handler_proxy;

      stream->priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }
}

void
tp_stream_engine_stream_error (TpStreamEngineStream *self,
                               guint error,
                               const gchar *message)
{
  g_message ("%s: stream errorno=%d error=%s", G_STRFUNC, error, message);

  tp_cli_media_stream_handler_call_error (self->priv->stream_handler_proxy,
      -1, error, message, NULL, NULL, NULL, NULL);

  g_signal_emit (self, signals[ERROR], 0);
}

