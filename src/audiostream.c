/*
 * stream.c - Source for TpStreamEngineAudioStream
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

#include <gst/farsight/fs-element-added-notifier.h>
#include <gst/farsight/fs-conference-iface.h>

#include "audiostream.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineAudioStream, tp_stream_engine_audio_stream,
    G_TYPE_OBJECT);

#define DEBUG(stream, format, ...)          \
  g_debug ("stream %d (audio) %s: " format,             \
      ((TpStreamEngineStream *)stream)->stream_id,      \
      G_STRFUNC,                                        \
      ##__VA_ARGS__)

#define WARNING(stream, format, ...)          \
  g_warning ("stream %d (audio) %s: " format,             \
      ((TpStreamEngineStream *)stream)->stream_id,      \
      G_STRFUNC,                                        \
      ##__VA_ARGS__)


struct _TpStreamEngineAudioStreamPrivate
{
  TpStreamEngineStream *stream;

  FsElementAddedNotifier *element_added_notifier;

  GstElement *srcbin;

  gdouble output_volume;
  gboolean output_mute;

  GstPad *pad;

  GstElement *bin;

  gulong src_pad_added_handler_id;

  GError *construction_error;

  GMutex *mutex;
  /* Everything below this line is protected by the mutex */
  guint error_idle_id;
  GList *sinkbins;
};


/* properties */
enum
{
  PROP_0,
  PROP_STREAM,
  PROP_BIN,
  PROP_PAD,
  PROP_OUTPUT_VOLUME,
  PROP_OUTPUT_MUTE,
  PROP_INPUT_VOLUME,
  PROP_INPUT_MUTE
};

/* signals */
enum
{
  REQUEST_PAD,
  RELEASE_PAD,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static GstElement *
tp_stream_engine_audio_stream_make_src_bin (TpStreamEngineAudioStream *self);


static void set_audio_props (FsElementAddedNotifier *notifier G_GNUC_UNUSED,
    GstBin *parent G_GNUC_UNUSED,
    GstElement *element,
    gpointer user_data);

static void tp_stream_engine_audio_stream_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static void tp_stream_engine_audio_stream_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);

static void src_pad_added_cb (TpStreamEngineStream *stream, GstPad *pad,
    FsCodec *codec, gpointer user_data);

static void
tp_stream_engine_audio_stream_init (TpStreamEngineAudioStream *self)
{
  TpStreamEngineAudioStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_AUDIO_STREAM, TpStreamEngineAudioStreamPrivate);

  self->priv = priv;

  self->priv->mutex = g_mutex_new ();

  self->priv->element_added_notifier = fs_element_added_notifier_new ();

  self->priv->output_volume = 1;
  self->priv->output_mute = FALSE;

  g_signal_connect (self->priv->element_added_notifier,
      "element-added", G_CALLBACK (set_audio_props), self);
}


static GObject *
tp_stream_engine_audio_stream_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineAudioStream *self = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  GstElement *srcbin = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->constructor (type, n_props, props);

  self = (TpStreamEngineAudioStream *) obj;

  srcbin = tp_stream_engine_audio_stream_make_src_bin (self);
  if (!srcbin)
    {
      WARNING (self, "Could not make source");
      goto out;
    }

  if (!gst_bin_add (GST_BIN (self->priv->bin), srcbin))
    {
      gst_object_unref (srcbin);
      WARNING (self, "Could not add src to bin");
      goto out;
    }
  self->priv->srcbin = srcbin;


  g_object_get (self->priv->stream, "sink-pad", &sink_pad, NULL);

  if (!sink_pad)
    {
      WARNING (self, "Could not get stream sink pad");
      goto out;
    }

  src_pad = gst_element_get_static_pad (self->priv->srcbin, "src");

  if (!src_pad)
    {
      WARNING (self, "Could not get src pad from src");
      gst_object_unref (sink_pad);
      goto out;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (src_pad, sink_pad)))
    {
      WARNING (self, "Could not link src to stream");
      gst_object_unref (src_pad);
      gst_object_unref (sink_pad);
      goto out;
    }

  gst_object_unref (src_pad);
  gst_object_unref (sink_pad);



  sink_pad = gst_element_get_static_pad (self->priv->srcbin, "sink");

  if (!sink_pad)
    {
      WARNING (self, "Could not get sink pad from srcbin");
      goto out;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (self->priv->pad, sink_pad)))
    {
      WARNING (self, "Could not link src to srcbin");
      gst_object_unref (sink_pad);
      goto out;
    }

  gst_object_unref (sink_pad);

  gst_element_set_state (self->priv->srcbin, GST_STATE_PLAYING);

  self->priv->src_pad_added_handler_id = g_signal_connect (self->priv->stream,
      "src-pad-added", G_CALLBACK (src_pad_added_cb), self);

 out:
  return obj;
}

static void
free_sinkbin (gpointer data, gpointer user_data)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (user_data);
  GstElement *bin = data;
  GstPad *adderpad = NULL, *adderpeer = NULL;
  GstPad *binsink = NULL, *sinkpeer = NULL;

  binsink = gst_element_get_static_pad (bin, "sink");

  sinkpeer = gst_pad_get_peer (binsink);
  if (sinkpeer)
    {
      gst_pad_unlink (sinkpeer, binsink);
      gst_object_unref (sinkpeer);
    }

  GST_PAD_STREAM_LOCK(binsink);
  GST_PAD_STREAM_UNLOCK(binsink);
  gst_object_unref (binsink);


  adderpeer = gst_element_get_static_pad (bin, "src");
  adderpad = gst_pad_get_peer (adderpeer);
  gst_object_unref (adderpeer);

  gst_element_set_locked_state (bin, TRUE);
  gst_element_set_state (bin, GST_STATE_NULL);

  gst_bin_remove (GST_BIN (self->priv->bin), bin);

  if (adderpad)
    {
      g_signal_emit (self, signals[RELEASE_PAD], 0, adderpad);
      gst_object_unref (adderpad);
    }
}

static void
tp_stream_engine_audio_stream_dispose (GObject *object)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (object);

  g_mutex_lock (self->priv->mutex);
  if (self->priv->error_idle_id)
    {
      g_source_remove (self->priv->error_idle_id);
      self->priv->error_idle_id = 0;
    }
  g_mutex_unlock (self->priv->mutex);

  if (self->priv->src_pad_added_handler_id)
    {
      g_signal_handler_disconnect (self->priv->stream,
          self->priv->src_pad_added_handler_id);
      self->priv->src_pad_added_handler_id = 0;
    }

  if (self->priv->element_added_notifier)
    {
      g_object_unref (self->priv->element_added_notifier);
      self->priv->element_added_notifier = NULL;
    }


  g_mutex_lock (self->priv->mutex);
  g_list_foreach (self->priv->sinkbins, free_sinkbin, self);
  g_list_free (self->priv->sinkbins);
  self->priv->sinkbins = NULL;
  g_mutex_unlock (self->priv->mutex);

  if (self->priv->srcbin)
    {
      gst_element_set_locked_state (self->priv->srcbin, TRUE);
      gst_element_set_state (self->priv->srcbin, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self->priv->bin), self->priv->srcbin);
      self->priv->srcbin = NULL;
    }

  if (self->priv->bin)
    {
      gst_object_unref (self->priv->bin);
      self->priv->bin = NULL;
    }

  if (self->priv->pad)
    {
      gst_object_unref (self->priv->pad);
      self->priv->pad = NULL;
    }

  if (self->priv->stream)
    {
      g_object_unref (self->priv->stream);
      self->priv->stream = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->dispose (object);
}

static void
tp_stream_engine_audio_stream_finalize (GObject *object)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (object);

  g_mutex_free (self->priv->mutex);

  if (G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->finalize)
    G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->finalize (object);
}

static gboolean
request_pad_accumulator (GSignalInvocationHint *ihint,
    GValue *return_accu,
    const GValue *handler_return,
    gpointer data)
{
  if (G_TYPE_CHECK_VALUE_TYPE (handler_return, GST_TYPE_PAD))
    {
      g_value_copy (handler_return, return_accu);
      return FALSE;
    }

  return TRUE;
}

static void
tp_stream_engine_audio_stream_class_init (TpStreamEngineAudioStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineAudioStreamPrivate));
  object_class->dispose = tp_stream_engine_audio_stream_dispose;
  object_class->finalize = tp_stream_engine_audio_stream_finalize;
  object_class->constructor = tp_stream_engine_audio_stream_constructor;
  object_class->set_property = tp_stream_engine_audio_stream_set_property;
  object_class->get_property = tp_stream_engine_audio_stream_get_property;

  param_spec = g_param_spec_object ("stream",
      "Tp StreamEngine Stream",
      "The Telepathy Stream Engine Stream",
      TP_STREAM_ENGINE_TYPE_STREAM,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_STREAM, param_spec);

  param_spec = g_param_spec_object ("bin",
      "The Bin to add stuff to",
      "The Bin to add the elements to",
      GST_TYPE_BIN,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_BIN, param_spec);

  param_spec = g_param_spec_object ("pad",
      "The pad that the src data comes from",
      "The GstPad the src data comes from",
      GST_TYPE_PAD,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PAD, param_spec);

  param_spec = g_param_spec_double ("output-volume",
      "Output volume",
      "The output volume for this stream.",
      0, 10, 1,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OUTPUT_VOLUME,
      param_spec);

  param_spec = g_param_spec_boolean ("output-mute",
      "Output volume",
      "Mute stream",
      FALSE,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OUTPUT_MUTE,
      param_spec);

  param_spec = g_param_spec_double ("input-volume",
      "Input volume",
      "The input volume for this stream.",
      0, 10, 1,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_INPUT_VOLUME,
      param_spec);

  param_spec = g_param_spec_boolean ("input-mute",
      "Input volume",
      "Mute stream",
      FALSE,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_INPUT_MUTE,
      param_spec);

  /*
   * BEWARE:
   * This signal is emitted from the streaming thread
   */

  signals[REQUEST_PAD] = g_signal_new ("request-pad",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  request_pad_accumulator, NULL,
                  tp_stream_engine_marshal_OBJECT__VOID,
                  GST_TYPE_PAD, 0);

  signals[RELEASE_PAD] = g_signal_new ("release-pad",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GST_TYPE_PAD);

}

static void
tp_stream_engine_audio_stream_set_property  (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (object);

    switch (property_id)
    {
    case PROP_STREAM:
      self->priv->stream = g_value_dup_object (value);
      break;
    case PROP_BIN:
      self->priv->bin = g_value_dup_object (value);
      break;
    case PROP_PAD:
      self->priv->pad = g_value_dup_object (value);
      break;
    case PROP_OUTPUT_VOLUME:
      {
        GList *item;
        g_mutex_lock (self->priv->mutex);
        for (item = self->priv->sinkbins;
             item;
             item = g_list_next (item))
          gst_child_proxy_set_property (GST_OBJECT (item->data),
              "volume::volume", value);
        self->priv->output_volume = g_value_get_double (value);
        g_mutex_unlock (self->priv->mutex);
      }
      break;
    case PROP_OUTPUT_MUTE:
     {
        GList *item;
        g_mutex_lock (self->priv->mutex);
        for (item = self->priv->sinkbins;
             item;
             item = g_list_next (item))
          gst_child_proxy_set_property (GST_OBJECT (item->data),
              "volume::mute", value);
        self->priv->output_mute = g_value_get_boolean (value);
        g_mutex_unlock (self->priv->mutex);
      }
      break;
    case PROP_INPUT_VOLUME:
      g_debug ("set input volume");
      gst_child_proxy_set_property (GST_OBJECT (self->priv->srcbin),
          "volume::volume", value);
      break;
    case PROP_INPUT_MUTE:
      gst_child_proxy_set_property (GST_OBJECT (self->priv->srcbin),
          "volume::mute", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_audio_stream_get_property  (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (object);

    switch (property_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, self->priv->stream);
      break;
    case PROP_BIN:
      g_value_set_object (value, self->priv->bin);
      break;
    case PROP_PAD:
      g_value_set_object (value, self->priv->pad);
      break;
    case PROP_OUTPUT_VOLUME:
      g_value_set_double (value, self->priv->output_volume);
      break;
    case PROP_OUTPUT_MUTE:
      g_value_set_boolean (value, self->priv->output_mute);
      break;
    case PROP_INPUT_VOLUME:
      gst_child_proxy_get_property (GST_OBJECT (self->priv->srcbin),
          "volume::volume", value);
      break;
    case PROP_INPUT_MUTE:
      gst_child_proxy_get_property (GST_OBJECT (self->priv->srcbin),
          "volume::mute", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
set_audio_props (FsElementAddedNotifier *notifier,
    GstBin *parent G_GNUC_UNUSED,
    GstElement *element,
    gpointer user_data)
{
#if 0
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (user_data);

  if (g_object_has_property ((GObject *) element, "blocksize"))
    g_object_set (element, "blocksize", 320, NULL);

  if (g_object_has_property ((GObject *) element, "latency-time") &&
      gst_object_has_ancestor ((GstObject * )element,
          (GstObject *) self->priv->sink))
    g_object_set (element, "latency-time", G_GINT64_CONSTANT (20000),
        NULL);
  if (g_object_has_property ((GObject *) element, "is-live"))
    g_object_set (element, "is-live", TRUE, NULL);

  if (g_object_has_property ((GObject *) element, "buffer-time") &&
      gst_object_has_ancestor ((GstObject *) element,
          (GstObject *) self->priv->srcbin))
    g_object_set (element, "buffer-time", G_GINT64_CONSTANT (100000), NULL);

  if (g_object_has_property ((GObject *) element, "profile"))
    g_object_set (element, "profile", 2 /* chat */ , NULL);
#endif
}

static GstElement *
tp_stream_engine_audio_stream_make_src_bin (TpStreamEngineAudioStream *self)
{
  GstElement *bin = NULL;
  GstElement *queue = NULL;
  GstElement *audioconvert = NULL;
  GstElement *volume = NULL;
  GstPad *pad;

  bin = gst_bin_new (NULL);

  queue = gst_element_factory_make ("queue", NULL);
  g_object_set (queue, "leaky", 2, NULL);

  if (!gst_bin_add (GST_BIN (bin), queue))
    {
      gst_object_unref (queue);
      gst_object_unref (bin);
      WARNING (self, "Could not add queue to bin");
      return NULL;
    }

  audioconvert = gst_element_factory_make ("audioconvert", NULL);
  if (!gst_bin_add (GST_BIN (bin), audioconvert))
    {
      gst_object_unref (audioconvert);
      gst_object_unref (bin);
      WARNING (self, "Could not add audioconvert to bin");
      return NULL;
    }

  volume = gst_element_factory_make ("volume", "volume");
  if (!gst_bin_add (GST_BIN (bin), volume))
    {
      gst_object_unref (volume);
      gst_object_unref (bin);
      WARNING (self, "Could not add volume to bin");
      return NULL;
    }

  if (!gst_element_link_many (queue, audioconvert, volume, NULL))
    {
      gst_object_unref (bin);
      WARNING (self, "Could not link queue, audioconvert and volume elements");
      return NULL;
    }

  pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SRC);

  if (!pad)
    {
      gst_object_unref (bin);
      WARNING (self, "Could not find unconnected sink pad in src bin");
      return NULL;
    }

  if (!gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad)))
    {
      gst_object_unref (bin);
      WARNING (self, "Could not add pad to bin");
      return NULL;
    }

  gst_object_unref (pad);

  pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SINK);

  if (!pad)
    {
      gst_object_unref (bin);
      WARNING (self, "Could not find unconnected sink pad in sink bin");
      return NULL;
    }

  if (!gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad)))
    {
      gst_object_unref (bin);
      WARNING (self, "Could not add pad to bin");
      return NULL;
    }

  gst_object_unref (pad);

  return bin;
}


TpStreamEngineAudioStream *
tp_stream_engine_audio_stream_new (TpStreamEngineStream *stream,
    GstBin *bin,
    GstPad *pad,
    GError **error)
{
  TpStreamEngineAudioStream *self = NULL;

  g_return_val_if_fail (TP_STREAM_ENGINE_IS_STREAM (stream) &&
      GST_IS_BIN (bin) &&
      GST_IS_PAD (pad), NULL);

  self = g_object_new (TP_STREAM_ENGINE_TYPE_AUDIO_STREAM,
      "stream", stream,
      "bin", bin,
      "pad", pad,
      NULL);

  if (self->priv->construction_error)
    {
      g_propagate_error (error, self->priv->construction_error);
      g_object_unref (self);
      return NULL;
    }

  return self;
}

static gboolean
src_pad_added_idle_error (gpointer user_data)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (user_data);

  tp_stream_engine_stream_error (self->priv->stream, 0,
      "Error setting up audio reception");


  g_mutex_lock (self->priv->mutex);
  self->priv->error_idle_id = 0;
  g_mutex_unlock (self->priv->mutex);

  return FALSE;
}

/* This creates the following pipeline
 *
 * farsight-pad -> { audioconvert -> audioresample -> volume } -> liveadder
 */

static void
src_pad_added_cb (TpStreamEngineStream *stream, GstPad *pad, FsCodec *codec,
    gpointer user_data)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (user_data);
  GstElement *audioconvert = NULL;
  GstElement *audioresample = NULL;
  GstElement *volume = NULL;
  GstPad *adderpad = NULL;
  GstPad *mypad = NULL;
  GstPad *ghostpad = NULL;
  gchar *padname = gst_pad_get_name (pad);
  gchar *tmp = NULL;
  GstElement *bin = NULL;
  gint session_id, ssrc, pt;

  DEBUG (self, "New pad added: %s", padname);

  if (sscanf (padname, "src_%d_%d_%d", &session_id, &ssrc, &pt) != 3)
    {
      WARNING (self, "Pad %s, is not a valid farsight src pad", padname);
      g_free (padname);
      goto error_finish;
    }

  g_free (padname);

  tmp = g_strdup_printf ("sink_bin_%d_%d_%d", session_id, ssrc, pt);
  bin = gst_bin_new (tmp);
  g_free (tmp);

    audioconvert = gst_element_factory_make ("audioconvert", NULL);
  if (!audioconvert)
    {
      WARNING (self, "Could not create audioconvert");
      goto error_created;
    }

  if (!gst_bin_add (GST_BIN (bin), audioconvert))
    {
      WARNING (self, "Could add audioconvert to bin");
      gst_object_unref (audioconvert);
      goto error_created;
    }

  audioresample = gst_element_factory_make ("audioresample", NULL);
  if (!audioresample)
    {
      WARNING (self, "Could not create audioresample");
      goto error_created;
    }

  if (!gst_bin_add (GST_BIN (bin), audioresample))
    {
      WARNING (self, "Could add audioresample to bin");
      gst_object_unref (audioresample);
      goto error_created;
    }

  volume = gst_element_factory_make ("volume", "volume");
  if (!volume)
    {
      WARNING (self, "Could not create volume");
      goto error_created;
    }

  g_mutex_lock (self->priv->mutex);
  g_object_set (volume,
      "volume", self->priv->output_volume,
      "mute", self->priv->output_mute,
      NULL);
  g_mutex_unlock (self->priv->mutex);

  if (!gst_bin_add (GST_BIN (bin), volume))
    {
      WARNING (self, "Could add volume to bin");
      gst_object_unref (volume);
      goto error_created;
    }

  if (!gst_element_link_many (audioconvert, audioresample, volume, NULL))
    {
      WARNING (self,"Could not link audioconvert, audioresample and volume");
      goto error_created;
    }

  if (!gst_bin_add (GST_BIN (self->priv->bin), bin))
    {
      WARNING (self, "could not add sinkbin to the pipeline");
      goto error_created;
    }

  if (gst_element_set_state (bin, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
    {
      WARNING (self, "Could not start audio sink filter bin");
      goto error_added;
    }

  g_signal_emit (self, signals[REQUEST_PAD], 0, &adderpad);

  if (!adderpad)
    {
      WARNING (self, "Could not get sink pad on from pipeline");
      goto error_added;
    }

  mypad = gst_element_get_static_pad (volume, "src");
  if (!mypad)
    {
      WARNING (self, "Could not get src pad from audioresample");
      goto error_got_pad;
    }

  ghostpad = gst_ghost_pad_new ("src", mypad);
  gst_object_unref (mypad);

  if (!gst_pad_set_active (ghostpad, TRUE))
    {
      WARNING (self, "Could not activate src ghost pad");
      gst_object_unref (ghostpad);
      goto error_got_pad;
    }

  if (!gst_element_add_pad (bin, ghostpad))
    {
      WARNING (self, "Could not add src ghost pad to bin");
      gst_object_unref (ghostpad);
      goto error_got_pad;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (ghostpad, adderpad)))
    {
      WARNING (self, "Could not link src ghost pad to adder");
      goto error_got_pad;
    }


  mypad = gst_element_get_static_pad (audioconvert, "sink");
  if (!mypad)
    {
      WARNING (self, "Could not get audioconvert pad");
      goto error_got_pad;
    }

  if (!mypad)
    {
      WARNING (self, "Could not get sink pad from audioresample");
      goto error_got_pad;
    }

  ghostpad = gst_ghost_pad_new ("sink", mypad);
  gst_object_unref (mypad);

  if (!gst_pad_set_active (ghostpad, TRUE))
    {
      WARNING (self, "Could not activate sink ghost pad");
      gst_object_unref (ghostpad);
      goto error_got_pad;
    }

  if (!gst_element_add_pad (bin, ghostpad))
    {
      WARNING (self, "Could not add sink ghost pad to bin");
      gst_object_unref (ghostpad);
      goto error_got_pad;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (pad, ghostpad)))
    {
      WARNING (self, "Could not link farsight pad to sink");
      goto error_got_pad;
    }

  g_mutex_lock (self->priv->mutex);
  self->priv->sinkbins = g_list_append (self->priv->sinkbins, bin);
  g_mutex_unlock (self->priv->mutex);

  return;

 error_finish:
  g_mutex_lock (self->priv->mutex);
  if (!self->priv->error_idle_id)
    self->priv->error_idle_id =
        g_idle_add (src_pad_added_idle_error, self);
  g_mutex_unlock (self->priv->mutex);
  return;

 error_created:
  gst_object_unref (bin);
  goto error_finish;

 error_added:
  gst_bin_remove (GST_BIN (self->priv->bin), bin);
  goto error_finish;

 error_got_pad:
  gst_bin_remove (GST_BIN (self->priv->bin), bin);
  g_signal_emit (self, signals[RELEASE_PAD], 0, adderpad);
  goto error_finish;

}
