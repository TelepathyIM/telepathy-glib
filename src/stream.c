/*
 * stream.c - Source for TpStreamEngineStream
 * Copyright (C) 2006 Collabora Ltd.
 * Copyright (C) 2006 Nokia Corporation
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
    ((TpStreamEngineStreamPrivate *) STREAM_PRIVATE(stream))->stream_id, \
    ((TpStreamEngineStreamPrivate *) STREAM_PRIVATE(stream))->media_type \
      == FARSIGHT_MEDIA_TYPE_AUDIO ? "audio" : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define STREAM_PRIVATE(o) ((TpStreamEngineStreamPrivate *)((o)->priv))

typedef struct _TpStreamEngineStreamPrivate TpStreamEngineStreamPrivate;

struct _TpStreamEngineStreamPrivate
{
  FarsightSession *fs_session;
  guint stream_id;
  TpMediaStreamType media_type;
  TpMediaStreamDirection direction;
  const TpStreamEngineNatProperties *nat_props;
  GstBin *pipeline;

  TpMediaStreamHandler *stream_handler_proxy;

  FarsightStream *fs_stream;
  guint state_changed_handler_id;

  gboolean playing;
  FarsightStreamState state;
  FarsightStreamDirection dir;

  guint output_volume;
  gboolean output_mute;
  gboolean input_mute;
  guint output_window_id;
};

enum
{
  CLOSED,
  ERROR,
  STATE_CHANGED,
  RECEIVING,
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

static void add_remote_candidate (TpProxy *proxy, const gchar *candidate,
    const GPtrArray *transports, gpointer user_data, GObject *object);

static void remove_remote_candidate (TpProxy *proxy, const gchar *candidate,
    gpointer user_data, GObject *object);

static void set_active_candidate_pair (TpProxy *proxy,
    const gchar *native_candidate, const gchar *remote_candidate,
    gpointer user_data, GObject *object);

static void set_remote_candidate_list (TpProxy *proxy,
    const GPtrArray *candidates, gpointer user_data, GObject *object);

static void set_remote_codecs (TpProxy *proxy, const GPtrArray *codecs,
    gpointer user_data, GObject *object);

static void set_stream_playing (TpProxy *proxy, gboolean play,
    gpointer user_data, GObject *object);

static void set_stream_sending (TpProxy *proxy, gboolean play,
    gpointer user_data, GObject *object);

static void start_telephony_event (TpProxy *proxy, guchar event,
    gpointer user_data, GObject *object);

static void stop_telephony_event (TpProxy *proxy,
    gpointer user_data, GObject *object);

static void close (TpProxy *proxy, gpointer user_data, GObject *object);

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

static void destroy_cb (TpProxy *proxy, gpointer user_data);

static void
_remove_video_sink (TpStreamEngineStream *stream, GstElement *sink)
{
  TpStreamEngine *engine;
  GstElement *pipeline;
  gboolean retval;
  GstStateChangeReturn ret;

  DEBUG (stream, "removing video sink");

  if (sink == NULL)
    return;

  engine = tp_stream_engine_get ();
  pipeline = tp_stream_engine_get_pipeline (engine);

  gst_object_ref (sink);

  retval = gst_bin_remove (GST_BIN (pipeline), sink);
  g_assert (retval == TRUE);

  ret = gst_element_set_state (sink, GST_STATE_NULL);
  g_assert (ret != GST_STATE_CHANGE_FAILURE);

  if (ret == GST_STATE_CHANGE_ASYNC) {
    ret = gst_element_get_state (sink, NULL, NULL, 5*GST_SECOND);
    g_assert (ret != GST_STATE_CHANGE_FAILURE);
  }

  DEBUG (stream, "sink refcount: %d", GST_OBJECT_REFCOUNT_VALUE(sink));

  gst_object_unref (sink);
}

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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  switch (property_id)
    {
    case PROP_FARSIGHT_SESSION:
      g_value_set_object (value, priv->fs_session);
      break;
    case PROP_PROXY:
      g_value_set_object (value, priv->stream_handler_proxy);
      break;
    case PROP_STREAM_ID:
      g_value_set_uint (value, priv->stream_id);
      break;
    case PROP_MEDIA_TYPE:
      g_value_set_uint (value, priv->media_type);
      break;
    case PROP_DIRECTION:
      g_value_set_uint (value, priv->direction);
      break;
    case PROP_NAT_PROPERTIES:
      g_value_set_pointer (value,
          (TpStreamEngineNatProperties *) priv->nat_props);
      break;
    case PROP_PIPELINE:
      g_value_set_object (value,
          farsight_stream_get_pipeline (priv->fs_stream));
      break;
    case PROP_SOURCE:
      g_value_set_object (value,
          farsight_stream_get_source (priv->fs_stream));
      break;
    case PROP_SINK:
      g_value_set_object (value,
          farsight_stream_get_sink (priv->fs_stream));
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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  switch (property_id)
    {
    case PROP_FARSIGHT_SESSION:
      priv->fs_session = FARSIGHT_SESSION (g_value_dup_object (value));
      break;
    case PROP_PROXY:
      priv->stream_handler_proxy =
          TP_MEDIA_STREAM_HANDLER (g_value_dup_object (value));
      break;
    case PROP_STREAM_ID:
      priv->stream_id = g_value_get_uint (value);
      break;
    case PROP_MEDIA_TYPE:
      priv->media_type = g_value_get_uint (value);
      break;
    case PROP_DIRECTION:
      priv->direction = g_value_get_uint (value);
      break;
    case PROP_NAT_PROPERTIES:
      priv->nat_props = g_value_get_pointer (value);
      break;
    case PROP_PIPELINE:
      g_assert (priv->pipeline == NULL);
      priv->pipeline = (GstBin *) g_value_dup_object (value);
      break;
    case PROP_SOURCE:
      farsight_stream_set_source (priv->fs_stream,
          g_value_get_object (value));
      break;
    case PROP_SINK:
      farsight_stream_set_sink (priv->fs_stream,
          g_value_get_object (value));
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

  obj = G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->
            constructor (type, n_props, props);
  stream = (TpStreamEngineStream *) obj;
  priv = STREAM_PRIVATE (stream);

  g_signal_connect (priv->stream_handler_proxy, "destroyed",
      G_CALLBACK (destroy_cb), obj);

  tp_cli_media_stream_handler_connect_to_add_remote_candidate
      (priv->stream_handler_proxy, add_remote_candidate, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_remove_remote_candidate
      (priv->stream_handler_proxy, remove_remote_candidate, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_set_active_candidate_pair
      (priv->stream_handler_proxy, set_active_candidate_pair, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_set_remote_candidate_list
      (priv->stream_handler_proxy, set_remote_candidate_list, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_set_remote_codecs
      (priv->stream_handler_proxy, set_remote_codecs, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_set_stream_playing
      (priv->stream_handler_proxy, set_stream_playing, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_set_stream_sending
      (priv->stream_handler_proxy, set_stream_sending, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_start_telephony_event
      (priv->stream_handler_proxy, start_telephony_event, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_stop_telephony_event
      (priv->stream_handler_proxy, stop_telephony_event, NULL, NULL, obj);
  tp_cli_media_stream_handler_connect_to_close
      (priv->stream_handler_proxy, close, NULL, NULL, obj);

  priv->fs_stream = farsight_session_create_stream (priv->fs_session,
      priv->media_type, priv->direction);

  if (priv->pipeline != NULL)
    {
      farsight_stream_set_pipeline (priv->fs_stream,
          (GstElement *) priv->pipeline);
      g_object_unref ((GObject *) priv->pipeline);
      priv->pipeline = NULL;
    }

  conn_timeout_str = getenv ("FS_CONN_TIMEOUT");

  if (conn_timeout_str)
    {
      gint conn_timeout = (int) g_ascii_strtod (conn_timeout_str, NULL);
      DEBUG (stream, "setting connection timeout to %d", conn_timeout);
      g_object_set (G_OBJECT(priv->fs_stream), "conn_timeout", conn_timeout, NULL);
    }

  /* TODO Make this smarter, we should only create those sources and sinks if
   * they exist. */
  src = make_src (stream, priv->media_type);
  sink = make_sink (stream, priv->media_type);

  if (src)
    {
      DEBUG (stream, "setting source on Farsight stream");
      farsight_stream_set_source (priv->fs_stream, src);
    }
  else
    {
      DEBUG (stream, "not setting source on Farsight stream");
    }

  if (sink)
    {
      DEBUG (stream, "setting sink on Farsight stream");
      farsight_stream_set_sink (priv->fs_stream, sink);
      gst_object_unref (sink);
    }
  else
    {
      DEBUG (stream, "not setting sink on Farsight stream");
    }

  g_signal_connect (G_OBJECT (priv->fs_stream), "error",
      G_CALLBACK (cb_fs_stream_error), obj);
  g_signal_connect (G_OBJECT (priv->fs_stream), "new-active-candidate-pair",
      G_CALLBACK (cb_fs_new_active_candidate_pair), obj);
  g_signal_connect (G_OBJECT (priv->fs_stream), "codec-changed",
      G_CALLBACK (cb_fs_codec_changed), obj);
  g_signal_connect (G_OBJECT (priv->fs_stream), "native-candidates-prepared",
      G_CALLBACK (cb_fs_native_candidates_prepared), obj);
  priv->state_changed_handler_id =
    g_signal_connect (G_OBJECT (priv->fs_stream), "state-changed",
        G_CALLBACK (cb_fs_state_changed), obj);
  g_signal_connect (G_OBJECT (priv->fs_stream), "new-native-candidate",
      G_CALLBACK (cb_fs_new_native_candidate), obj);

  set_nat_properties (stream);

  prepare_transports (stream);

  return obj;
}

static void
tp_stream_engine_stream_dispose (GObject *object)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

  g_assert (priv->pipeline == NULL);

  if (priv->fs_session)
    {
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = priv->stream_handler_proxy;

      priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  if (priv->fs_stream)
    {
      stop_stream (stream);

      g_signal_handler_disconnect (priv->fs_stream,
          priv->state_changed_handler_id);
      g_object_unref (priv->fs_stream);
      priv->fs_stream = NULL;
    }

  if (priv->output_window_id)
    {
      gboolean ret;
      TpStreamEngine *engine = tp_stream_engine_get ();
      ret = tp_stream_engine_remove_output_window (engine,
          priv->output_window_id);
      g_assert (ret);
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
}

typedef struct _method_call_ctx method_call_ctx;

struct _method_call_ctx
{
  TpStreamEngineStream *stream;
  const gchar *method;
};

static void
method_call_ctx_free (gpointer user_data)
{
  g_slice_free (method_call_ctx, user_data);
}

/* dummy callback handler for async calling calls with no return values */
static void
async_method_callback (TpProxy *proxy,
                       const GError *error,
                       gpointer user_data,
                       GObject *weak_object)
{
  method_call_ctx *ctx = (method_call_ctx *) user_data;

  if (error != NULL)
    {
      g_warning ("Error calling %s: %s", ctx->method, error->message);
      g_signal_emit (ctx->stream, signals[ERROR], 0);
    }
}

static void
cb_fs_state_changed (FarsightStream *stream,
                     FarsightStreamState state,
                     FarsightStreamDirection dir,
                     gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
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

  if (priv->state != state || priv->dir != dir)
    {
      g_signal_emit (self, signals[STATE_CHANGED], 0, state, dir);
    }

  if (priv->state != state)
    {
      if (priv->stream_handler_proxy)
        {
          method_call_ctx *ctx = g_slice_new0 (method_call_ctx);

          ctx->stream = self;
          ctx->method = "Media.StreamHandler::StreamState";

          tp_cli_media_stream_handler_call_stream_state (
            priv->stream_handler_proxy, -1, state, async_method_callback, ctx,
            method_call_ctx_free, (GObject *) self);
        }

      priv->state = state;
    }

  if (priv->dir != dir)
    {
      if ((priv->dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY) !=
          (dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY))
        {
          gboolean receiving =
            ((dir & FARSIGHT_STREAM_DIRECTION_RECEIVEONLY) ==
             FARSIGHT_STREAM_DIRECTION_RECEIVEONLY);
          g_signal_emit (self, signals[RECEIVING], 0, receiving);
        }

      priv->dir = dir;
    }
}

static void
cb_fs_new_native_candidate (FarsightStream *stream,
                            gchar *candidate_id,
                            gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  const GList *fs_candidates, *lp;
  GPtrArray *transports;
  method_call_ctx *ctx;

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

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::NativeCandidatesPrepared";

  tp_cli_media_stream_handler_call_new_native_candidate (
      priv->stream_handler_proxy, -1, candidate_id, transports,
      async_method_callback, ctx, method_call_ctx_free, (GObject *) self);
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
fs_codecs_to_tp (const GList *codecs)
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

      g_debug ("%s: adding codec %s [%d]",
          G_STRFUNC, fsc->encoding_name, fsc->id);

      g_ptr_array_add (tp_codecs, g_value_get_boxed (&codec));
    }

  return tp_codecs;
}

static void
add_remote_candidate (TpProxy *proxy,
                      const gchar *candidate,
                      const GPtrArray *transports,
                      gpointer user_data,
                      GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GList *fs_transports;

  fs_transports = tp_transports_to_fs (candidate, transports);

  DEBUG (self, "adding remote candidate %s", candidate);
  farsight_stream_add_remote_candidate (priv->fs_stream, fs_transports);

  free_fs_transports (fs_transports);
}

static void
remove_remote_candidate (TpProxy *proxy,
                         const gchar *candidate,
                         gpointer user_data,
                         GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  DEBUG (self, "removing remote candidate %s", candidate);
  farsight_stream_remove_remote_candidate (priv->fs_stream, candidate);
}

static void
set_active_candidate_pair (TpProxy *proxy,
                           const gchar *native_candidate,
                           const gchar *remote_candidate,
                           gpointer user_data,
                           GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  farsight_stream_set_active_candidate_pair (priv->fs_stream,
                                             native_candidate,
                                             remote_candidate);
}

static void
set_remote_candidate_list (TpProxy *proxy,
                           const GPtrArray *candidates,
                           gpointer user_data,
                           GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
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

  farsight_stream_set_remote_candidate_list (priv->fs_stream, fs_transports);
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
set_remote_codecs (TpProxy *proxy,
                   const GPtrArray *codecs,
                   gpointer user_data,
                   GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GList *fs_codecs =NULL, *lp, *lp2;
  GValueArray *codec;
  GHashTable *params = NULL;
  FarsightCodec *fs_codec;
  GList *fs_params = NULL;
  guint i;
  GPtrArray *supp_codecs;
  method_call_ctx *ctx;

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

  if (!farsight_stream_set_remote_codecs (priv->fs_stream, fs_codecs)) {
    /*
     * Call the error method with the proper thing here
     */
    g_warning("Negotiation failed");
    tp_stream_engine_stream_error (self, 0, "Codec negotiation failed");
    return;
  }

  tp_stream_engine_stream_mute_input (self, priv->input_mute, NULL);

  supp_codecs = fs_codecs_to_tp (
      farsight_stream_get_codec_intersection (priv->fs_stream));

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::SupportedCodecs";

  tp_cli_media_stream_handler_call_supported_codecs
    (priv->stream_handler_proxy, -1, supp_codecs, async_method_callback, ctx,
     method_call_ctx_free, (GObject *) self);

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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GstElement *sink = NULL;

  if (!priv->fs_stream)
    return;

  DEBUG (self, "calling stop on farsight stream %p", priv->fs_stream);

  if (priv->media_type == FARSIGHT_MEDIA_TYPE_VIDEO)
    sink = farsight_stream_get_sink (priv->fs_stream);

  farsight_stream_stop (priv->fs_stream);

  if (sink)
    _remove_video_sink (self, sink);
}

static void
set_stream_playing (TpProxy *proxy,
                    gboolean play,
                    gpointer user_data,
                    GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_assert (priv->fs_stream != NULL);

  DEBUG (self, "%d", play);

  if (play)
    {
      priv->playing = TRUE;
      farsight_stream_start (priv->fs_stream);
    }
  else if (priv->playing)
    {
      stop_stream (self);
    }
}

static void
set_stream_sending (TpProxy *proxy,
                    gboolean send,
                    gpointer user_data,
                    GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_assert (priv->fs_stream != NULL);

  DEBUG (self, "%d", send);

  farsight_stream_set_sending (priv->fs_stream, send);
}

static void
start_telephony_event (TpProxy *proxy,
                       guchar event,
                       gpointer user_data,
                       GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_assert (priv->fs_stream != NULL);

  DEBUG (self, "called with event %u", event);

  /* this week, volume is 8, for the sake of argument... */
  if (!farsight_stream_start_telephony_event (priv->fs_stream, event, 8))
    DEBUG (self, "sending event %u failed", event);
}

static void
stop_telephony_event (TpProxy *proxy,
                      gpointer user_data,
                      GObject *object)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_assert (priv->fs_stream != NULL);

  DEBUG (self, "called");

  if (!farsight_stream_stop_telephony_event (priv->fs_stream))
    DEBUG (self, "stopping event failed");
}

static void
close (TpProxy *proxy,
       gpointer user_data,
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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  const TpStreamEngineNatProperties *props = priv->nat_props;
  FarsightStream *stream = priv->fs_stream;
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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GPtrArray *codecs;
  method_call_ctx *ctx;

  farsight_stream_prepare_transports (priv->fs_stream);

  codecs = fs_codecs_to_tp (
             farsight_stream_get_local_codecs (priv->fs_stream));

  DEBUG (self, "calling MediaStreamHandler::Ready");

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::Ready";

  tp_cli_media_stream_handler_call_ready (priv->stream_handler_proxy,
      -1, codecs, async_method_callback, ctx, method_call_ctx_free,
      (GObject *) self);
}

static void
cb_fs_codec_changed (FarsightStream *stream,
                     gint codec_id,
                     gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  method_call_ctx *ctx;

  if (priv->media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      tp_stream_engine_stream_mute_output (self, priv->output_mute, NULL);
      tp_stream_engine_stream_mute_input (self, priv->input_mute, NULL);
      tp_stream_engine_stream_set_output_volume (self, priv->output_volume,
        NULL);
    }

  DEBUG (self, "codec_id=%d, stream=%p", codec_id, stream);

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::CodecChoice";

  tp_cli_media_stream_handler_call_codec_choice (priv->stream_handler_proxy,
      -1, codec_id, async_method_callback, ctx, method_call_ctx_free,
      (GObject *) self);
}

static void
cb_fs_stream_error (FarsightStream *stream,
                    FarsightStreamError error,
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
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  method_call_ctx *ctx;

  DEBUG (self, "stream=%p", stream);

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::NewActiveCandidatePair";

  tp_cli_media_stream_handler_call_new_active_candidate_pair (
    priv->stream_handler_proxy, -1, native_candidate, remote_candidate,
    async_method_callback, ctx, method_call_ctx_free, (GObject *) self);
}

static void
cb_fs_native_candidates_prepared (FarsightStream *stream,
                                  gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  const GList *transport_candidates, *lp;
  FarsightTransportInfo *info;
  method_call_ctx *ctx;

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

  ctx = g_slice_new0 (method_call_ctx);
  ctx->stream = self;
  ctx->method = "Media.StreamHandler::NativeCandidatesPrepared";

  tp_cli_media_stream_handler_call_native_candidates_prepared (
    priv->stream_handler_proxy, -1, async_method_callback, ctx,
    method_call_ctx_free, (GObject *) self);
}

static GstElement *
make_src (TpStreamEngineStream *stream, guint media_type)
{
  const gchar *elem;
  GstElement *src = NULL;

  if (media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      if ((elem = getenv ("FS_AUDIO_SRC")) || (elem = getenv ("FS_AUDIOSRC")))
        {
          DEBUG (stream, "making audio src with pipeline \"%s\"", elem);
          src = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (src);
        }
      else
        {
          DEBUG (stream, "making audio src with alsasrc element");
          src = gst_element_factory_make ("alsasrc", NULL);

          if (src)
            {
              g_object_set(G_OBJECT(src), "blocksize", 320, NULL);
              g_object_set(G_OBJECT(src), "latency-time",
                G_GINT64_CONSTANT (20000), NULL);
            }
        }

      if (src && g_object_has_property (G_OBJECT (src), "is-live"))
        g_object_set(G_OBJECT(src), "is-live", TRUE, NULL);
    }
  else
    {
      TpStreamEngine *engine = tp_stream_engine_get ();
      GstElement *pipeline = tp_stream_engine_get_pipeline (engine);
      GstElement *tee = gst_bin_get_by_name (GST_BIN (pipeline), "tee");

#ifndef MAEMO_OSSO_SUPPORT
      GstElement *queue = gst_element_factory_make ("queue", NULL);

      if (!queue)
        g_error("Could not create queue element");

      g_object_set(G_OBJECT(queue), "leaky", 2,
          "max-size-time", 50*GST_MSECOND, NULL);

      gst_bin_add(GST_BIN(pipeline), queue);

      gst_element_set_state(queue, GST_STATE_PLAYING);

      gst_element_link(tee, queue);
      src = queue;
#else
      src = tee;
#endif
      gst_object_unref (tee);
    }

    return src;
}

static GstElement *
make_sink (TpStreamEngineStream *stream, guint media_type)
{
  const gchar *elem;
  GstElement *sink = NULL;

  if (media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      if ((elem = getenv ("FS_AUDIO_SINK")) || (elem = getenv("FS_AUDIOSINK")))
        {
          DEBUG (stream, "making audio sink with pipeline \"%s\"", elem);
          sink = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (sink);
        }
      else
        {
          DEBUG (stream, "making audio sink with alsasink element");
          sink = gst_element_factory_make ("alsasink", NULL);
        }
    }
  else
    {
      if ((elem = getenv ("STREAM_VIDEO_SINK")) ||
          (elem = getenv ("FS_VIDEO_SINK")) ||
          (elem = getenv ("FS_VIDEOSINK")))
        {
          TpStreamEngine *engine;
          DEBUG (stream, "making video sink with pipeline \"%s\"", elem);
          sink = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (GST_IS_BIN (sink));
          engine = tp_stream_engine_get ();
          gst_bin_add (GST_BIN (tp_stream_engine_get_pipeline (engine)), sink);
          gst_element_set_state (sink, GST_STATE_PLAYING);
          g_assert (sink);
        }
      else
        {
          /* do nothing: we set a sink when we get a window ID to send video
           * to */

          DEBUG (stream, "not making a video sink");
        }
    }

  if (sink && g_object_has_property (G_OBJECT (sink), "sync"))
    g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);

  return sink;
}

static void
destroy_cb (TpProxy *proxy,
            gpointer user_data)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

  if (priv->stream_handler_proxy)
    {
      TpMediaStreamHandler *tmp = priv->stream_handler_proxy;

      priv->stream_handler_proxy = NULL;
      g_object_unref (tmp);
    }
}

TpStreamEngineStream *
tp_stream_engine_stream_new (FarsightSession *fs_session,
                             TpMediaStreamHandler *proxy,
                             guint stream_id,
                             TpMediaStreamType media_type,
                             TpMediaStreamDirection direction,
                             const TpStreamEngineNatProperties *nat_props)
{
  TpStreamEngineStream *ret;

  g_return_val_if_fail (fs_session != NULL, NULL);
  g_return_val_if_fail (FARSIGHT_IS_SESSION (fs_session), NULL);
  g_return_val_if_fail (proxy != NULL, NULL);
  g_return_val_if_fail (media_type <= TP_MEDIA_STREAM_TYPE_VIDEO, NULL);
  g_return_val_if_fail (direction <= TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
      NULL);

  ret = g_object_new (TP_STREAM_ENGINE_TYPE_STREAM,
      "farsight-session", fs_session,
      "proxy", proxy,
      "stream-id", stream_id,
      "media-type", media_type,
      "direction", direction,
      "nat-properties", nat_props,
      NULL);

  return ret;
}

gboolean tp_stream_engine_stream_mute_output (
  TpStreamEngineStream *stream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *sink;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteInput can only be called on audio streams");
      return FALSE;
    }

  priv->output_mute = mute_state;
  sink = farsight_stream_get_sink (priv->fs_stream);

  if (!sink)
    return TRUE;

  g_message ("%s: output mute set to %s", G_STRFUNC,
    mute_state ? "on" : "off");

  if (sink && g_object_has_property (G_OBJECT(sink), "mute"))
    g_object_set (G_OBJECT (sink), "mute", mute_state, NULL);

  return TRUE;
}

gboolean tp_stream_engine_stream_set_output_volume (
  TpStreamEngineStream *stream,
  guint volume,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *sink;
  guint scaled_volume;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "SetOutputVolume can only be called on audio streams");
      return FALSE;
    }

  if (volume > 100)
    volume = 100;

  priv->output_volume = volume;
  scaled_volume = (volume * 65535)/100;
  DEBUG (stream, "setting output volume to %d", priv->output_volume);
  sink = farsight_stream_get_sink (priv->fs_stream);

  if (!sink)
    return TRUE;

  if (sink && g_object_has_property (G_OBJECT (sink), "volume"))
    g_object_set (G_OBJECT (sink), "volume", scaled_volume, NULL);

  return TRUE;
}

gboolean tp_stream_engine_stream_mute_input (
  TpStreamEngineStream *stream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *source;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteInput can only be called on audio streams");
      return FALSE;
    }

  priv->input_mute = mute_state;
  source = farsight_stream_get_source (priv->fs_stream);

  if (!source)
    return TRUE;

  g_message ("%s: input mute set to %s", G_STRFUNC,
    mute_state ? " on" : "off");

  if (source && g_object_has_property (G_OBJECT (source), "mute"))
    g_object_set (G_OBJECT (source), "mute", mute_state, NULL);

  return TRUE;
}

gboolean
tp_stream_engine_stream_set_output_window (
  TpStreamEngineStream *stream,
  guint window_id,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  TpStreamEngine *engine;
  GstElement *sink;

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_VIDEO)
    {
      DEBUG (stream, "can only be called on video streams");
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "SetOutputWindow can only be called on video streams");
      return FALSE;
    }

  if (priv->output_window_id == window_id)
    {
      DEBUG (stream, "not doing anything, output window is already set to "
          "window ID %u", window_id);
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "not doing "
          "anything, output window is already set window ID %u", window_id);
      return FALSE;
    }

  engine = tp_stream_engine_get ();

  if (priv->output_window_id != 0)
    {
      tp_stream_engine_remove_output_window (engine, priv->output_window_id);
    }

  priv->output_window_id = window_id;

  if (priv->output_window_id == 0)
    {
      GstElement *stream_sink = farsight_stream_get_sink (priv->fs_stream);
      farsight_stream_set_sink (priv->fs_stream, NULL);
      _remove_video_sink (stream, stream_sink);

      return TRUE;
    }

  sink = tp_stream_engine_make_video_sink (engine, FALSE);

  if (sink == NULL)
    {
      DEBUG (stream, "failed to make video sink, no output for window %d :(",
          window_id);
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "failed to make a "
          "video sink");
      return FALSE;
    }

  DEBUG (stream, "putting video output in window %d", window_id);

  tp_stream_engine_add_output_window (engine, stream, sink, window_id);
  farsight_stream_set_sink (priv->fs_stream, sink);
  gst_object_unref (sink);

  return TRUE;
}

void
tp_stream_engine_stream_error (TpStreamEngineStream *self,
                               guint error,
                               const gchar *message)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_message ("%s: stream errorno=%d error=%s", G_STRFUNC, error, message);

  tp_cli_media_stream_handler_call_error (priv->stream_handler_proxy,
      -1, error, message, NULL, NULL, NULL, NULL);
  g_signal_emit (self, signals[ERROR], 0);
}

