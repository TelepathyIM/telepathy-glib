/*
 * tp-stream-engine.c - Source for TpStreamEngine
 * Copyright (C) 2005-2007 Collabora Ltd.
 * Copyright (C) 2005-2007 Nokia Corporation
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

#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <string.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/gtypes.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"

#include <gst/farsight/fs-element-added-notifier.h>

#include "api/api.h"
#include "channel.h"
#include "session.h"
#include "stream.h"
#include "audiostream.h"
#include "videosink.h"
#include "videostream.h"
#include "videopreview.h"
#include "util.h"

#define BUS_NAME        "org.freedesktop.Telepathy.StreamEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/StreamEngine"

static void
_create_pipeline (TpStreamEngine *self);

static void
register_dbus_signal_marshallers()
{
  /*register a marshaller for the NewMediaStreamHandler signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__BOXED_UINT_UINT_UINT, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
     G_TYPE_INVALID);

  /*register a marshaller for the NewMediaSessionHandler signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__BOXED_STRING, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the AddRemoteCandidate signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__STRING_BOXED, G_TYPE_NONE,
     G_TYPE_STRING, TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_TRANSPORT_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetActiveCandidatePair signal*/
  dbus_g_object_register_marshaller
    (tp_stream_engine_marshal_VOID__STRING_STRING, G_TYPE_NONE,
     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCandidateList signal*/
  dbus_g_object_register_marshaller
    (g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_CANDIDATE_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCodecs signal*/
  dbus_g_object_register_marshaller
    (g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_ARRAY_TYPE_MEDIA_STREAM_HANDLER_CODEC_LIST, G_TYPE_INVALID);
}

static void ch_iface_init (gpointer, gpointer);
static void se_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TpStreamEngine, tp_stream_engine, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (STREAM_ENGINE_TYPE_SVC_CHANNEL_HANDLER,
      ch_iface_init);
    G_IMPLEMENT_INTERFACE (STREAM_ENGINE_TYPE_SVC_STREAM_ENGINE,
      se_iface_init))

/* properties */
enum
{
  PROP_0,
  PROP_PIPELINE
};

/* signal enum */
enum
{
  HANDLING_CHANNEL,
  NO_MORE_CHANNELS,
  SHUTDOWN_REQUESTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
struct _TpStreamEnginePrivate
{
  gboolean dispose_has_run;

  TpDBusDaemon *dbus_daemon;

  GstElement *pipeline;
  GstElement *videosrc;
  GstElement *tee;

  GPtrArray *channels;
  GHashTable *channels_by_path;

  GMutex *mutex;

  FsElementAddedNotifier *notifier;

  /* Protected by the mutex */
  GList *output_sinks;
  GList *preview_sinks;

  guint bus_async_source_id;

  gboolean force_fakesrc;

  gint source_use_count;
};

static void
set_element_props (FsElementAddedNotifier *notifier,
    GstBin *bin,
    GstElement *element,
    gpointer user_data)
{
  if (g_object_has_property ((GObject *) element, "min-ptime"))
    g_object_set ((GObject *) element, "min-ptime", GST_MSECOND * 20, NULL);

  if (g_object_has_property ((GObject *) element, "max-ptime"))
    g_object_set ((GObject *) element, "max-ptime", GST_MSECOND * 50, NULL);
}

static void
tp_stream_engine_init (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate);

  self->priv = priv;

  priv->channels = g_ptr_array_new ();

  priv->channels_by_path = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  priv->mutex = g_mutex_new ();

  priv->notifier = fs_element_added_notifier_new ();
  g_signal_connect (priv->notifier, "element-added",
      G_CALLBACK (set_element_props), self);

  _create_pipeline (self);
}

static void tp_stream_engine_dispose (GObject *object);
static void tp_stream_engine_finalize (GObject *object);



static void
tp_stream_engine_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);

  switch (property_id)
    {
    case PROP_PIPELINE:
      g_value_set_object (value, self->priv->pipeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}



static void
tp_stream_engine_class_init (TpStreamEngineClass *tp_stream_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_stream_engine_class);
  GParamSpec *param_spec;

  g_type_class_add_private (tp_stream_engine_class,
      sizeof (TpStreamEnginePrivate));


  object_class->get_property = tp_stream_engine_get_property;
  object_class->dispose = tp_stream_engine_dispose;
  object_class->finalize = tp_stream_engine_finalize;

  /**
   * TpStreamEngine::handling-channel:
   *
   * Emitted whenever this object starts handling a channel
   */
  signals[HANDLING_CHANNEL] =
  g_signal_new ("handling-channel",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * TpStreamEngine::no-more-channels:
   *
   * Emitted whenever this object is handling no channels
   */
  signals[NO_MORE_CHANNELS] =
  g_signal_new ("no-more-channels",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * TpStreamEngine::shutdown:
   *
   * Emitted whenever stream engine needs to be shutdown
   */
  signals[SHUTDOWN_REQUESTED] =
    g_signal_new ("shutdown-requested",
        G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  param_spec = g_param_spec_object ("pipeline",
      "The GstPipeline",
      "The GstPipeline that all the objects here live in",
      GST_TYPE_PIPELINE,
      G_PARAM_READABLE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PIPELINE, param_spec);

}

static void
tp_stream_engine_dispose (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  if (priv->channels)
    {
      guint i;

      for (i = 0; i < priv->channels->len; i++)
        g_object_unref (g_ptr_array_index (priv->channels, i));

      g_ptr_array_free (priv->channels, TRUE);
      priv->channels = NULL;
    }

  if (priv->channels_by_path)
    {
      g_hash_table_destroy (priv->channels_by_path);
      priv->channels_by_path = NULL;
    }

  g_list_foreach (priv->preview_sinks, (GFunc) g_object_unref, NULL);
  g_list_free (priv->preview_sinks);
  priv->preview_sinks = NULL;

  if (priv->pipeline)
    {
      /*
       * Lets not check the return values
       * if it fails or is async, it will crash on the following
       * unref anyways
       */
      gst_element_set_state (priv->videosrc, GST_STATE_NULL);
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_object_unref (priv->pipeline);
      priv->pipeline = NULL;
      priv->videosrc = NULL;
    }

  if (priv->notifier)
    {
      g_object_unref (priv->notifier);
      priv->notifier = NULL;
    }

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose (object);
}

static void
tp_stream_engine_finalize (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);

  g_mutex_free (self->priv->mutex);

  G_OBJECT_CLASS (tp_stream_engine_parent_class)->finalize (object);
}

/**
 * tp_stream_engine_error
 *
 * Used to inform the stream engine than an exceptional situation has ocurred.
 *
 * @error:   The error ID, as per the
 *           org.freedesktop.Telepathy.Media.StreamHandler::Error signal.
 * @message: The human-readable error message.
 */
void
tp_stream_engine_error (TpStreamEngine *self, int error, const char *message)
{
  guint i;

  for (i = 0; i < self->priv->channels->len; i++)
    tp_stream_engine_channel_error (
      g_ptr_array_index (self->priv->channels, i), error, message);
}

static void
tp_stream_engine_start_source (TpStreamEngine *self)
{
  GstStateChangeReturn state_ret;


  if (self->priv->force_fakesrc)
    {
      g_debug ("Don't have a video source, not starting it");
      return;
    }

  g_debug ("Starting video source");

  self->priv->source_use_count++;

  state_ret = gst_element_set_state (self->priv->videosrc, GST_STATE_PLAYING);

  if (state_ret == GST_STATE_CHANGE_FAILURE)
    g_error ("Error starting the video source");
}


static void
tp_stream_engine_stop_source (TpStreamEngine *self)
{
  GstStateChangeReturn state_ret;

  self->priv->source_use_count--;

  if (self->priv->source_use_count > 0)
    return;

  g_debug ("Stopping source");

  state_ret = gst_element_set_state (self->priv->videosrc, GST_STATE_NULL);

  if (state_ret == GST_STATE_CHANGE_FAILURE)
    g_error ("Error stopping the video source");
}

static void
stream_free_resource (TpStreamEngineStream *stream,
    TpMediaStreamDirection dir,
    gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpMediaStreamType media_type;

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO &&
      dir & TP_MEDIA_STREAM_DIRECTION_SEND)
    tp_stream_engine_stop_source (self);
}

static gboolean
stream_request_resource (TpStreamEngineStream *stream,
    TpMediaStreamDirection dir,
    gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpMediaStreamType media_type;

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO &&
      dir & TP_MEDIA_STREAM_DIRECTION_SEND)
    tp_stream_engine_start_source (self);

  return TRUE;
}


static void
session_invalidated (TpStreamEngineSession *session G_GNUC_UNUSED,
    gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GstElement *conf;

  g_object_get (session, "farsight-conference", &conf, NULL);

  gst_element_set_locked_state (conf, TRUE);
  gst_element_set_state (conf, GST_STATE_NULL);
  gst_element_get_state (conf, NULL, NULL, GST_CLOCK_TIME_NONE);

  gst_bin_remove (GST_BIN (self->priv->pipeline), conf);
}

static void
channel_session_created (TpStreamEngineChannel *chan G_GNUC_UNUSED,
    TpStreamEngineSession *session, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GstElement *conf;

  g_object_get (session, "farsight-conference", &conf, NULL);

  if (g_object_has_property ((GObject *)conf, "latency"))
    g_object_set (conf, "latency", 100, NULL);

  if (!gst_bin_add (GST_BIN (self->priv->pipeline), conf))
    g_error ("Could not add conference to pipeline");

  gst_element_set_state (conf, GST_STATE_PLAYING);

  g_signal_connect (session, "invalidated", G_CALLBACK (session_invalidated),
      user_data);
}



static void
stream_closed (TpStreamEngineStream *stream G_GNUC_UNUSED, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GObject *sestream = NULL;

  sestream = g_object_get_data (G_OBJECT (stream), "se-stream");
  if (sestream)
    {
      if (TP_STREAM_ENGINE_IS_VIDEO_STREAM (sestream))
        {
          TpStreamEngineVideoStream *videostream =
              (TpStreamEngineVideoStream *) sestream;
          GstPad *pad;

          g_object_get (videostream, "pad", &pad, NULL);

          /* Take the stream lock to make sure nothing is flowing through the
           * pad
           * We can only do that because we have no blocking elements before
           * a queue in our pipeline after the pads.
           */
          GST_PAD_STREAM_LOCK(pad);
          gst_element_release_request_pad (self->priv->tee, pad);
          GST_PAD_STREAM_UNLOCK(pad);

          gst_object_unref (pad);
       }

      g_object_unref (sestream);
    }
}

static void
stream_receiving (TpStreamEngineVideoStream *videostream, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpStreamEngineStream *stream = NULL;
  TpStreamEngineChannel *chan = NULL;
  guint stream_id;
  gchar *channel_path;

  g_object_get (videostream, "stream", &stream, NULL);

  g_object_get (stream, "channel", &chan, "stream-id", &stream_id, NULL);

  g_object_get (chan, "object-path", &channel_path, NULL);

  stream_engine_svc_stream_engine_emit_receiving (self,
      channel_path, stream_id, TRUE);

  g_object_unref (chan);
  g_object_unref (stream);
  g_free (channel_path);
}

static void
channel_stream_created (TpStreamEngineChannel *chan G_GNUC_UNUSED,
    TpStreamEngineStream *stream, gpointer user_data)
{
  guint media_type;
  GError *error = NULL;
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);


  g_object_get (G_OBJECT (stream), "media-type", &media_type, NULL);

  if (media_type == TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      TpStreamEngineAudioStream *audiostream;

      audiostream = tp_stream_engine_audio_stream_new (stream,
          GST_BIN (self->priv->pipeline), &error);

      if (!audiostream)
        {
          g_warning ("Could not create audio stream: %s", error->message);
          return;
        }
      g_clear_error (&error);
      g_object_set_data ((GObject*) stream, "se-stream", audiostream);
    }
  else if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO)
    {
      TpStreamEngineVideoStream *videostream = NULL;
      GstPad *pad;

      pad = gst_element_get_request_pad (self->priv->tee, "src%d");

      videostream = tp_stream_engine_video_stream_new (stream,
          GST_BIN (self->priv->pipeline), pad, &error);

      if (!videostream)
        {
          g_warning ("Could not create video stream: %s", error->message);
          return;
        }
      g_clear_error (&error);
      g_object_set_data ((GObject*) stream, "se-stream", videostream);

      g_mutex_lock (self->priv->mutex);
      self->priv->preview_sinks = g_list_prepend (self->priv->output_sinks,
          videostream);
      g_mutex_unlock (self->priv->mutex);

      g_signal_connect (videostream, "receiving",
          G_CALLBACK (stream_receiving), self);
    }

  g_signal_connect (stream, "closed", G_CALLBACK (stream_closed), self);

  g_signal_connect (stream, "request-resource",
      G_CALLBACK (stream_request_resource), self);
  g_signal_connect (stream, "free-resource",
      G_CALLBACK (stream_free_resource), self);
}


static void
check_if_busy (TpStreamEngine *self)
{
  guint num_previews;

  g_mutex_lock (self->priv->mutex);
  num_previews = g_list_length (self->priv->preview_sinks);
  g_mutex_unlock (self->priv->mutex);

  if (self->priv->channels->len == 0 && num_previews == 0)
    {
      g_debug ("no channels or previews remaining; emitting no-more-channels");
      g_signal_emit (self, signals[NO_MORE_CHANNELS], 0);
    }
  else
    {
      g_debug ("channels remaining: %d", self->priv->channels->len);
      g_debug ("preview windows remaining: %d", num_previews);
    }
}


static void
channel_closed (TpStreamEngineChannel *chan, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  gchar *object_path;

  g_object_get (chan, "object-path", &object_path, NULL);

  g_debug ("channel %s (%p) closed", object_path, chan);

  g_ptr_array_remove_fast (self->priv->channels, chan);
  g_object_unref (chan);

  g_hash_table_remove (self->priv->channels_by_path, object_path);
  g_free (object_path);

  check_if_busy (self);
}

static void
close_one_stream (TpStreamEngineChannel *chan G_GNUC_UNUSED,
                        guint stream_id G_GNUC_UNUSED,
                        TpStreamEngineStream *stream,
                        gpointer user_data)
{
  const gchar *message = (const gchar *) user_data;

  tp_stream_engine_stream_error (stream, TP_MEDIA_STREAM_ERROR_UNKNOWN,
      message);
}


static void
close_all_streams (TpStreamEngine *self, const gchar *message)
{
  guint i;

  g_debug ("Closing all streams");

  for (i = 0; i < self->priv->channels->len; i++)
    {
      TpStreamEngineChannel *channel = g_ptr_array_index (self->priv->channels,
          i);
      tp_stream_engine_channel_foreach_stream (channel,
          close_one_stream, (gpointer) message);
    }
}

static gboolean
bus_async_handler (GstBus *bus G_GNUC_UNUSED,
                   GstMessage *message,
                   gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = engine->priv;
  GError *error = NULL;
  gchar *error_string, *tmp;
  guint i;
  GstElement *source = GST_ELEMENT (GST_MESSAGE_SRC (message));
  gchar *name = gst_element_get_name (source);


  for (i = 0; i < priv->channels->len; i++)
    if (tp_stream_engine_channel_bus_message (
            g_ptr_array_index (priv->channels, i), message))
      return TRUE;

  switch (GST_MESSAGE_TYPE (message))
    {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (message, &error, &error_string);

        g_debug ("%s: got error from %s: %s: %s (%d %d), destroying video "
            "pipeline", G_STRFUNC, name, error->message, error_string,
            error->domain, error->code);

        tmp = g_strdup_printf ("%s: %s", error->message, error_string);

        close_all_streams (engine, error->message);

        gst_element_set_state (engine->priv->pipeline, GST_STATE_NULL);
        gst_element_set_state (engine->priv->videosrc, GST_STATE_NULL);
        gst_element_set_state (engine->priv->pipeline, GST_STATE_PLAYING);

        g_free (error_string);
        g_error_free (error);
        break;
      case GST_MESSAGE_WARNING:
        {
          gchar *debug_msg = NULL;
          gst_message_parse_warning (message, &error, &debug_msg);
          g_warning ("%s: got warning: %s (%s)", G_STRFUNC, error->message,
              debug_msg);
          g_free (debug_msg);
          g_error_free (error);
          break;
        }

      default:
        break;
    }

  g_free (name);
  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus *bus G_GNUC_UNUSED, GstMessage *message, gpointer data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (data);
  GList *item;
  gboolean handled = FALSE;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_structure_has_name (message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;


  g_mutex_lock (self->priv->mutex);
  for (item = g_list_first (self->priv->preview_sinks);
       item && !handled;
       item = g_list_next (item))
    {
      TpStreamEngineVideoSink *preview = item->data;

      handled = tp_stream_engine_video_sink_bus_sync_message (preview, message);
      if (handled)
        break;
    }

  if (!handled)
    {
      for (item = g_list_first (self->priv->preview_sinks);
           item && !handled;
           item = g_list_next (item))
        {
          TpStreamEngineVideoSink *output = item->data;

          handled = tp_stream_engine_video_sink_bus_sync_message (output, message);
          if (handled)
            break;
        }
    }
  g_mutex_unlock (self->priv->mutex);

  if (handled)
    {
      gst_message_unref (message);
      return GST_BUS_DROP;
    }
  else
    return GST_BUS_PASS;
}

static void
_create_pipeline (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = self->priv;
  GstElement *videosrc = NULL;
  GstElement *tee;
  GstElement *capsfilter;
#ifndef MAEMO_OSSO_SUPPORT
  GstElement *tmp;
#endif
  GstBus *bus;
  GstCaps *filter = NULL;
  const gchar *elem;
  const gchar *caps_str;
  GstStateChangeReturn state_ret;
  gboolean ret;
  GstElement *fakesink;

  priv->pipeline = gst_pipeline_new (NULL);

  fs_element_added_notifier_add (priv->notifier, GST_BIN (priv->pipeline));

 try_again:

  if ((elem = getenv ("FS_VIDEO_SRC")) || (elem = getenv ("FS_VIDEOSRC")))
    {
      if (priv->force_fakesrc)
        g_error ("Invalid video source passed in FS_VIDEOSRC");
      g_debug ("making video src with pipeline \"%s\"", elem);
      videosrc = gst_parse_bin_from_description (elem, TRUE, NULL);
      g_assert (videosrc);
      gst_element_set_name (videosrc, "videosrc");
    }
  else
    {
#ifdef MAEMO_OSSO_SUPPORT
      videosrc = gst_element_factory_make ("gconfv4l2src", NULL);
#else
      if (priv->force_fakesrc)
        {
          videosrc = gst_element_factory_make ("fakesrc", NULL);
          g_object_set (videosrc, "is-live", TRUE, NULL);
        }

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("gconfvideosrc", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4l2src", NULL);

      if (videosrc == NULL)
        videosrc = gst_element_factory_make ("v4lsrc", NULL);
#endif

      if (videosrc != NULL)
        {
          g_debug ("using %s as video source", GST_ELEMENT_NAME (videosrc));
        }
      else
        {
          videosrc = gst_element_factory_make ("videotestsrc", NULL);
          g_object_set (videosrc, "is-live", TRUE, NULL);
          if (videosrc == NULL)
            g_error ("failed to create any video source");
        }
    }

  if ((caps_str = getenv ("FS_VIDEO_SRC_CAPS")) || (caps_str = getenv ("FS_VIDEOSRC_CAPS")))
    {
      filter = gst_caps_from_string (caps_str);
    }

  if (!filter)
    {
      filter = gst_caps_new_simple ("video/x-raw-yuv",
#ifdef MAEMO_OSSO_SUPPORT
          "width", G_TYPE_INT, 176,
          "height", G_TYPE_INT, 144,
#else
          "width", G_TYPE_INT, 352,
          "height", G_TYPE_INT, 288,
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
#endif
          NULL);
    }
  else
    {
      g_debug ("applying custom caps '%s' on the video source\n", caps_str);
    }

  fakesink = gst_element_factory_make ("fakesink", NULL);

  gst_bin_add_many (GST_BIN (priv->pipeline), videosrc, fakesink, NULL);

  if (!gst_element_link (videosrc, fakesink))
    g_error ("Could not link fakesink to videosrc");

  state_ret = gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    {
      if (priv->force_fakesrc)
        {
          g_error ("Could not even start fakesrc");
        }
      else
        {
          g_debug ("Video source failed, falling back to fakesrc");
          state_ret = gst_element_set_state (priv->pipeline, GST_STATE_NULL);
          g_assert (state_ret == GST_STATE_CHANGE_SUCCESS);
          if (!gst_bin_remove (GST_BIN (priv->pipeline), fakesink))
            g_error ("Could not remove fakesink");

          priv->force_fakesrc = TRUE;
          gst_bin_remove (GST_BIN (priv->pipeline), videosrc);
          goto try_again;
        }
    }
  else
    {
      state_ret = gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_assert (state_ret == GST_STATE_CHANGE_SUCCESS);
      gst_bin_remove (GST_BIN (priv->pipeline), fakesink);
    }

  priv->videosrc = videosrc;

  gst_element_set_locked_state (videosrc, TRUE);

  tee = gst_element_factory_make ("tee", "tee");
  g_assert (tee);
  if (!gst_bin_add (GST_BIN (priv->pipeline), tee))
    g_error ("Could not add tee to pipeline");

  self->priv->tee = tee;


#ifndef MAEMO_OSSO_SUPPORT
#if 0
  /* <wtay> Tester_, yes, videorate does not play nice with live pipelines */
  tmp = gst_element_factory_make ("videorate", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videorate");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }
#endif

  tmp = gst_element_factory_make ("ffmpegcolorspace", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking ffmpegcolorspace");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }

  tmp = gst_element_factory_make ("videoscale", NULL);
  if (tmp != NULL)
    {
      g_debug ("linking videoscale");
      gst_bin_add (GST_BIN (priv->pipeline), tmp);
      gst_element_link (videosrc, tmp);
      videosrc = tmp;
    }
#endif

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  g_assert (capsfilter);
  ret = gst_bin_add (GST_BIN (priv->pipeline), capsfilter);
  g_assert (ret);

  g_object_set (capsfilter, "caps", filter, NULL);
  gst_caps_unref (filter);

  ret = gst_element_link (videosrc, capsfilter);
  g_assert (ret);
  ret = gst_element_link (capsfilter, tee);
  g_assert (ret);

  state_ret = gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  g_assert (state_ret != GST_STATE_CHANGE_FAILURE);

  /* connect a callback to the stream bus so that we can set X window IDs
   * at the right time, and detect when sinks have gone away */
  bus = gst_element_get_bus (priv->pipeline);
  gst_bus_set_sync_handler (bus, bus_sync_handler, self);
  priv->bus_async_source_id =
    gst_bus_add_watch (bus, bus_async_handler, self);
  gst_object_unref (bus);
}


static void
_preview_window_plug_deleted (TpStreamEngineVideoPreview *preview,
    gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  GstPad *pad = NULL;

  while (g_signal_handlers_disconnect_by_func(preview,
          _preview_window_plug_deleted,
          user_data)) {}

  g_mutex_lock (self->priv->mutex);
  self->priv->preview_sinks = g_list_remove (self->priv->preview_sinks,
      preview);
  g_mutex_unlock (self->priv->mutex);

  tp_stream_engine_stop_source (self);

  g_object_get (preview, "pad", &pad, NULL);

  if (pad)
    {
      /* Take the stream lock to make sure nothing is flowing through the
       * pad
       * We can only do that because we have no blocking elements before
       * a queue in our pipeline after the pads.
       */
      GST_PAD_STREAM_LOCK(pad);
      gst_element_release_request_pad (self->priv->tee, pad);
      GST_PAD_STREAM_UNLOCK(pad);

      gst_object_unref (pad);
    }

  gst_object_unref (preview);
}

/**
 * tp_stream_engine_create_preview_window
 *
 * Implements DBus method CreatePreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_create_preview_window (StreamEngineSvcStreamEngine *iface,
                                        DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  GError *error = NULL;
  GstPad *pad;
  TpStreamEngineVideoPreview *preview;
  guint window_id;

  if (self->priv->force_fakesrc)
    {
      GError *error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Could not get a video source");
      g_debug ("%s", error->message);

      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  pad = gst_element_get_request_pad (self->priv->tee, "src%d");

  preview = tp_stream_engine_video_preview_new (GST_BIN (self->priv->pipeline),
      pad, &error);

  if (!preview)
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }
  g_signal_connect (preview, "plug-deleted",
      G_CALLBACK (_preview_window_plug_deleted), self);

  g_mutex_lock (self->priv->mutex);
  self->priv->preview_sinks = g_list_prepend (self->priv->preview_sinks,
      preview);
  g_mutex_unlock (self->priv->mutex);


  g_object_get (preview, "window-id", &window_id, NULL);

  stream_engine_svc_stream_engine_return_from_create_preview_window (context,
      window_id);

  tp_stream_engine_start_source (self);

  g_signal_emit (self, signals[HANDLING_CHANNEL], 0);
}


static void
handler_result (TpStreamEngineChannel *chan G_GNUC_UNUSED,
                GError *error,
                DBusGMethodInvocation *context)
{
  if (error == NULL)
    stream_engine_svc_channel_handler_return_from_handle_channel (context);
  else
    dbus_g_method_return_error (context, error);
}

/**
 * tp_stream_engine_handle_channel
 *
 * Implements DBus method HandleChannel
 * on interface org.freedesktop.Telepathy.ChannelHandler
 */
static void
tp_stream_engine_handle_channel (StreamEngineSvcChannelHandler *iface,
                                 const gchar *bus_name,
                                 const gchar *connection,
                                 const gchar *channel_type,
                                 const gchar *channel,
                                 guint handle_type,
                                 guint handle,
                                 DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineChannel *chan = NULL;
  GError *error = NULL;

  g_debug("HandleChannel called");

  if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA) != 0)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Stream Engine was passed a channel that was not a "
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA };

      g_message ("%s", e.message);
      dbus_g_method_return_error (context, &e);
      return;
     }

  chan = tp_stream_engine_channel_new (self->priv->dbus_daemon, bus_name,
      connection, channel, handle_type, handle, &error);

  if (chan == NULL)
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_ptr_array_add (self->priv->channels, chan);
  g_hash_table_insert (self->priv->channels_by_path, g_strdup (channel), chan);

  g_signal_connect (chan, "handler-result", G_CALLBACK (handler_result),
      context);
  g_signal_connect (chan, "closed", G_CALLBACK (channel_closed), self);
  g_signal_connect (chan, "session-created",
      G_CALLBACK (channel_session_created), self);
  g_signal_connect (chan, "stream-created",
      G_CALLBACK (channel_stream_created), self);

  g_signal_emit (self, signals[HANDLING_CHANNEL], 0);
}

void
tp_stream_engine_register (TpStreamEngine *self)
{
  DBusGConnection *bus;
  GError *error = NULL;
  guint request_name_result;

  g_assert (TP_IS_STREAM_ENGINE (self));

  bus = tp_get_bus ();
  self->priv->dbus_daemon = tp_dbus_daemon_new (bus);

  g_debug("registering StreamEngine at " OBJECT_PATH);
  dbus_g_connection_register_g_object (bus, OBJECT_PATH, G_OBJECT (self));

  register_dbus_signal_marshallers();

  g_debug("Requesting " BUS_NAME);

  if (!tp_cli_dbus_daemon_run_request_name (self->priv->dbus_daemon, -1,
        BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &request_name_result, &error,
        NULL))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, stream engine already running?");
}

static TpStreamEngineStream *
_lookup_stream (TpStreamEngine *self,
                const gchar *path,
                guint stream_id,
                GError **error)
{
  TpStreamEngineChannel *channel;
  TpStreamEngineStream *stream;

  channel = g_hash_table_lookup (self->priv->channels_by_path, path);
  if (channel == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
        "stream-engine is not handling the channel %s", path);

      return NULL;
    }

  stream = tp_stream_engine_channel_lookup_stream (channel, stream_id);
  if (stream == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "the channel %s has no stream with id %d", path, stream_id);

      return NULL;
    }

  return stream;
}


/**
 * tp_stream_engine_mute_input
 *
 * Implements DBus method MuteInput
 * on interface org.freedesktop.Telepathy.StreamEngine
 */

static void
tp_stream_engine_mute_input (StreamEngineSvcStreamEngine *iface,
                             const gchar *channel_path,
                             guint stream_id,
                             gboolean mute_state,
                             DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;
  TpMediaStreamType media_type;
  TpStreamEngineAudioStream *audiostream;

  stream = _lookup_stream (self, channel_path, stream_id, &error);


  if (stream == NULL)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Stream does not exist");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type != TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteInput can only be called on audio streams");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  audiostream = g_object_get_data ((GObject*) stream, "se-stream");

  g_object_set (audiostream, "input-mute", mute_state, NULL);

  stream_engine_svc_stream_engine_return_from_mute_input (context);
}

/**
 * tp_stream_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamEngine
 */

static void
tp_stream_engine_mute_output (StreamEngineSvcStreamEngine *iface,
                             const gchar *channel_path,
                             guint stream_id,
                             gboolean mute_state,
                             DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;
  TpMediaStreamType media_type;
  TpStreamEngineAudioStream *audiostream;

  stream = _lookup_stream (self, channel_path, stream_id, &error);


  if (stream == NULL)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Stream does not exist");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type != TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteOutput can only be called on audio streams");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  audiostream = g_object_get_data ((GObject*) stream, "se-stream");

  g_object_set (audiostream, "output-mute", mute_state, NULL);

  stream_engine_svc_stream_engine_return_from_mute_output (context);
}



/**
 * tp_stream_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_set_output_volume (StreamEngineSvcStreamEngine *iface,
                                    const gchar *channel_path,
                                    guint stream_id,
                                    guint volume,
                                    DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;
  TpMediaStreamType media_type;
  TpStreamEngineAudioStream *audiostream;
  gdouble doublevolume = volume / 100.0;

  stream = _lookup_stream (self, channel_path, stream_id, &error);


  if (stream == NULL)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Stream does not exist");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type != TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteOutput can only be called on audio streams");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  audiostream = g_object_get_data ((GObject*) stream, "se-stream");

  g_object_set (audiostream, "output-volume", doublevolume, NULL);

  stream_engine_svc_stream_engine_return_from_mute_output (context);
}



/**
 * tp_stream_engine_set_input_volume
 *
 * Implements DBus method SetInputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_set_input_volume (StreamEngineSvcStreamEngine *iface,
                                    const gchar *channel_path,
                                    guint stream_id,
                                    guint volume,
                                    DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;
  TpMediaStreamType media_type;
  TpStreamEngineAudioStream *audiostream;
  gdouble doublevolume = volume / 100.0;

  stream = _lookup_stream (self, channel_path, stream_id, &error);


  if (stream == NULL)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Stream does not exist");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_object_get (stream, "media-type", &media_type, NULL);

  if (media_type != TP_MEDIA_STREAM_TYPE_AUDIO)
    {
      error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "MuteInput can only be called on audio streams");
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  audiostream = g_object_get_data ((GObject*) stream, "se-stream");

  g_object_set (audiostream, "input-volume", doublevolume, NULL);

  stream_engine_svc_stream_engine_return_from_mute_input (context);
}

/**
 * tp_stream_engine_get_output_window
 *
 * Implements DBus method SetOutputWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_get_output_window (StreamEngineSvcStreamEngine *iface,
                                    const gchar *channel_path,
                                    guint stream_id,
                                    DBusGMethodInvocation *context)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (iface);
  TpStreamEngineStream *stream;
  GError *error = NULL;

  g_debug ("%s: channel_path=%s, stream_id=%u", G_STRFUNC, channel_path,
      stream_id);

  stream = _lookup_stream (self, channel_path, stream_id, &error);

  if (stream)
    {
      guint window_id;
      TpStreamEngineVideoSink *videosink;

      videosink = TP_STREAM_ENGINE_VIDEO_SINK (
          g_object_get_data ((GObject*) stream, "se-stream"));

      g_object_get (videosink, "window-id", &window_id, NULL);

      g_debug ("Returning window id %u", window_id);

      stream_engine_svc_stream_engine_return_from_get_output_window (context,
          window_id);
    }
  else
    {
      GError error = {TP_ERRORS, 0, "No stream"};
      dbus_g_method_return_error (context, &error);
    }
}

/*
 * tp_stream_engine_get
 *
 * Return the stream engine singleton. Caller does not own a reference to the
 * stream engine.
 */

TpStreamEngine *
tp_stream_engine_get ()
{
  static TpStreamEngine *engine = NULL;

  if (NULL == engine)
    {
      engine = g_object_new (TP_TYPE_STREAM_ENGINE, NULL);
      g_object_add_weak_pointer (G_OBJECT (engine), (gpointer) &engine);
    }

  return engine;
}

/**
 * tp_stream_engine_shutdown
 *
 * Implements DBus method Shutdown
 * on interface org.freedesktop.Telepathy.StreamEngine
 */
static void
tp_stream_engine_shutdown (StreamEngineSvcStreamEngine *iface,
                           DBusGMethodInvocation *context)
{
  g_debug ("%s: Emitting shutdown signal", G_STRFUNC);
  g_signal_emit (iface, signals[SHUTDOWN_REQUESTED], 0);
  stream_engine_svc_stream_engine_return_from_shutdown (context);
}

static void
se_iface_init (gpointer iface, gpointer data G_GNUC_UNUSED)
{
  StreamEngineSvcStreamEngineClass *klass = iface;

#define IMPLEMENT(x) stream_engine_svc_stream_engine_implement_##x (\
    klass, tp_stream_engine_##x)
  IMPLEMENT (set_output_volume);
  IMPLEMENT (set_input_volume);
  IMPLEMENT (mute_input);
  IMPLEMENT (mute_output);
  IMPLEMENT (get_output_window);
  IMPLEMENT (create_preview_window);
  IMPLEMENT (shutdown);
#undef IMPLEMENT
}

static void
ch_iface_init (gpointer iface, gpointer data G_GNUC_UNUSED)
{
  StreamEngineSvcChannelHandlerClass *klass = iface;

#define IMPLEMENT(x) stream_engine_svc_channel_handler_implement_##x (\
    klass, tp_stream_engine_##x)
  IMPLEMENT (handle_channel);
#undef IMPLEMENT
}
