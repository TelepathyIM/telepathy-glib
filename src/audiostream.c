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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "audiostream.h"
#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "util.h"

G_DEFINE_TYPE (TpStreamEngineAudioStream, tp_stream_engine_audio_stream,
    TP_STREAM_ENGINE_TYPE_STREAM);

#define DEBUG(stream, format, ...)          \
  g_debug ("stream %d (audio) %s: " format,             \
      ((TpStreamEngineStream *)stream)->stream_id,      \
      G_STRFUNC,                                        \
      ##__VA_ARGS__)

struct _TpStreamEngineAudioStreamPrivate
{
  guint output_volume;
  gboolean output_mute;
  gboolean input_mute;
};

static GstElement *
tp_stream_engine_audio_stream_make_src (TpStreamEngineStream *stream);
static GstElement *
tp_stream_engine_audio_stream_make_sink (TpStreamEngineStream *stream);


static void
cb_set_remote_codecs (TpMediaStreamHandler *proxy,
    const GPtrArray *codecs,
    gpointer user_data,
    GObject *object);
static void
cb_fs_codec_changed (FarsightStream *stream,
    gint codec_id,
    gpointer user_data);

static void
tp_stream_engine_audio_stream_init (TpStreamEngineAudioStream *self)
{
  TpStreamEngineAudioStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_AUDIO_STREAM, TpStreamEngineAudioStreamPrivate);

  self->priv = priv;
  self->priv->output_volume = 100;
}


static GObject *
tp_stream_engine_audio_stream_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *obj;
  TpMediaStreamHandler *stream_handler_proxy = NULL;
  TpStreamEngineStream *stream = NULL;

  obj = G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->constructor (type, n_props, props);

  stream = (TpStreamEngineStream *) obj;

  g_object_get (obj, "proxy", &stream_handler_proxy, NULL);

  tp_cli_media_stream_handler_connect_to_set_remote_codecs
      (stream_handler_proxy, cb_set_remote_codecs, NULL, NULL, obj, NULL);

  g_object_unref (stream_handler_proxy);

  g_signal_connect (G_OBJECT (stream->fs_stream),
      "codec-changed", G_CALLBACK (cb_fs_codec_changed), obj);

  return obj;
}

static void
tp_stream_engine_audio_stream_dispose (GObject *object)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (object);

  if (stream->fs_stream)
    g_signal_handlers_disconnect_by_func (stream->fs_stream,
        cb_fs_codec_changed, stream);

  if (G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_audio_stream_parent_class)->dispose (object);
}

static void
tp_stream_engine_audio_stream_class_init (TpStreamEngineAudioStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpStreamEngineStreamClass *stream_class =
      TP_STREAM_ENGINE_STREAM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineAudioStreamPrivate));
  object_class->dispose = tp_stream_engine_audio_stream_dispose;

  object_class->constructor = tp_stream_engine_audio_stream_constructor;

  stream_class->make_src = tp_stream_engine_audio_stream_make_src;
  stream_class->make_sink = tp_stream_engine_audio_stream_make_sink;
}

static GstElement *
get_volume_element (GstElement *element)
{
  GstElement *volume_element = NULL;

  if (g_object_has_property (G_OBJECT (element), "volume") &&
      g_object_has_property (G_OBJECT (element), "mute"))
    return gst_object_ref (element);

  if (GST_IS_BIN (element))
    {
      GstIterator *it = NULL;
      gboolean done = FALSE;
      gpointer item;

      it = gst_bin_iterate_recurse (GST_BIN (element));

      while (!volume_element && !done) {
        switch (gst_iterator_next (it, &item)) {
        case GST_ITERATOR_OK:
          if (g_object_has_property (G_OBJECT (item), "volume") &&
              g_object_has_property (G_OBJECT (item), "mute"))
            volume_element = GST_ELEMENT (item);
          else
            gst_object_unref (item);
          break;
        case GST_ITERATOR_RESYNC:
          if (volume_element)
            gst_object_unref (volume_element);
          volume_element = NULL;
          gst_iterator_resync (it);
          break;
        case GST_ITERATOR_ERROR:
          g_error ("Can not iterate sink");
          done = TRUE;
          break;
        case GST_ITERATOR_DONE:
          done = TRUE;
          break;
        }
      }
      gst_iterator_free (it);
    }

  return volume_element;
}

static gboolean has_volume_element (GstElement *element)
{
  GstElement *volume_element = get_volume_element (element);

  if (volume_element)
    {
      gst_object_unref (volume_element);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static GstElement *
make_volume_bin (TpStreamEngineAudioStream *stream, GstElement *element,
    gchar *padname)
{
  GstElement *bin = gst_bin_new (NULL);
  GstElement *volume = gst_element_factory_make ("volume", NULL);
  GstPad *volume_pad;
  GstPad *ghostpad;
  g_assert (volume);

  DEBUG (stream, "Putting the %s into a bin with a volume element", padname);

  if (!gst_bin_add (GST_BIN (bin), element) ||
      !gst_bin_add (GST_BIN (bin), volume))
    {
      g_warning ("Could not add %s and volume to the bin", padname);
      gst_object_unref (element);
      gst_object_unref (bin);
      gst_object_unref (volume);
      return NULL;
    }

  if (!strcmp (padname, "src"))
    {
      if (!gst_element_link (element, volume))
        {
          g_warning ("Could not link volume and %s", padname);
          gst_object_unref (bin);
          return NULL;
        }
    }
  else
    {
      if (!gst_element_link (volume, element))
        {
          g_warning ("Could not link volume and %s", padname);
          gst_object_unref (bin);
          return NULL;
        }
    }

  volume_pad = gst_element_get_static_pad (volume, padname);
  g_assert (volume_pad);

  ghostpad = gst_ghost_pad_new (padname, volume_pad);
  g_assert (ghostpad);

  gst_object_unref (volume_pad);

  if (!gst_element_add_pad (bin, ghostpad))
    {
      g_warning ("Could not add %s ghostpad to src element", padname);
      gst_object_unref (element);
      gst_object_unref (ghostpad);
      return NULL;
    }

  return bin;
}

static void
set_audio_src_props (GstBin *bin G_GNUC_UNUSED,
                     GstElement *src,
                     void *user_data G_GNUC_UNUSED)
{
  if (g_object_has_property ((GObject *) src, "blocksize"))
    g_object_set ((GObject *) src, "blocksize", 320, NULL);

  if (g_object_has_property ((GObject *) src, "latency-time"))
    g_object_set ((GObject *) src, "latency-time", G_GINT64_CONSTANT (20000),
        NULL);

  if (g_object_has_property ((GObject *) src, "is-live"))
    g_object_set ((GObject *) src, "is-live", TRUE, NULL);

  if (GST_IS_BIN (src))
    {
      gboolean done = FALSE;
      GstIterator *it = NULL;
      gpointer elem;

      g_signal_connect ((GObject *) src, "element-added",
        G_CALLBACK (set_audio_src_props), NULL);

      it = gst_bin_iterate_recurse (GST_BIN (src));
      while (!done)
        {
          switch (gst_iterator_next (it, &elem))
            {
              case GST_ITERATOR_OK:
                set_audio_src_props (NULL, GST_ELEMENT(elem), NULL);
                g_object_unref (elem);
                break;
              case GST_ITERATOR_RESYNC:
                gst_iterator_resync (it);
                break;
              case GST_ITERATOR_ERROR:
                g_error ("Can not iterate audiosrc bin");
                done = TRUE;
                break;
             case GST_ITERATOR_DONE:
               done = TRUE;
               break;
            }
        }
    }
}

static GstElement *
tp_stream_engine_audio_stream_make_src (TpStreamEngineStream *stream)
{
  const gchar *elem;
  GstElement *src = NULL;
  TpStreamEngineAudioStream *audiostream =
      TP_STREAM_ENGINE_AUDIO_STREAM (stream);

  if ((elem = getenv ("FS_AUDIO_SRC")) || (elem = getenv ("FS_AUDIOSRC")))
    {
      DEBUG (stream, "making audio src with pipeline \"%s\"", elem);
      src = gst_parse_bin_from_description (elem, TRUE, NULL);
      g_assert (src);
    }
  else
#ifdef MAEMO_OSSO_SUPPORT
    {
      DEBUG (stream, "running on Maemo platform, not making audio src");
      return NULL;
    }
#else /* MAEMO_OSSO_SUPPORT */
    {
      src = gst_element_factory_make ("gconfaudiosrc", NULL);

      if (src == NULL)
        src = gst_element_factory_make ("alsasrc", NULL);
    }
#endif /* MAEMO_OSSO_SUPPORT */

  if (src == NULL)
    {
      DEBUG (stream, "failed to make audio src element!");
      return NULL;
    }

  DEBUG (stream, "made audio src element %s", GST_ELEMENT_NAME (src));

  if (GST_IS_BIN (src))
    {
      g_signal_connect ((GObject *) src, "element-added",
          G_CALLBACK (set_audio_src_props), NULL);
    }
  else
    {
      set_audio_src_props (NULL, src, NULL);
    }

  if (!has_volume_element (src))
    src = make_volume_bin (audiostream, src, "src");

  return src;
}

static void
set_audio_sink_props (GstBin *bin G_GNUC_UNUSED,
                      GstElement *sink,
                      void *user_data G_GNUC_UNUSED)
{
  if (g_object_has_property ((GObject *) sink, "sync"))
    g_object_set ((GObject *) sink, "sync", FALSE, NULL);

  if (GST_IS_BIN (sink))
    {
      gboolean done = FALSE;
      GstIterator *it = NULL;
      gpointer elem;

      g_signal_connect ((GObject *) sink, "element-added",
        G_CALLBACK (set_audio_sink_props), NULL);

      it = gst_bin_iterate_recurse (GST_BIN (sink));
      while (!done)
        {
          switch (gst_iterator_next (it, &elem))
            {
              case GST_ITERATOR_OK:
                set_audio_sink_props (NULL, GST_ELEMENT(elem), NULL);
                g_object_unref (elem);
                break;
              case GST_ITERATOR_RESYNC:
                gst_iterator_resync (it);
                break;
              case GST_ITERATOR_ERROR:
                g_error ("Can not iterate audiosink bin");
                done = TRUE;
                break;
             case GST_ITERATOR_DONE:
               done = TRUE;
               break;
            }
        }
    }
}

static GstElement *
tp_stream_engine_audio_stream_make_sink (TpStreamEngineStream *stream)
{
  const gchar *elem;
  GstElement *sink = NULL;
  TpStreamEngineAudioStream *audiostream =
      TP_STREAM_ENGINE_AUDIO_STREAM (stream);

  if ((elem = getenv ("FS_AUDIO_SINK")) || (elem = getenv("FS_AUDIOSINK")))
    {
      DEBUG (stream, "making audio sink with pipeline \"%s\"", elem);
      sink = gst_parse_bin_from_description (elem, TRUE, NULL);
      g_assert (sink);
    }
  else
#ifdef MAEMO_OSSO_SUPPORT
    {
      DEBUG (stream, "running on Maemo platform, not making audio sink");
      return NULL;
    }
#else /* MAEMO_OSSO_SUPPORT */
    {
      sink = gst_element_factory_make ("gconfaudiosink", NULL);

      if (sink != NULL)
        {
          /* set profile=2 for gconfaudiosink "chat" profile */
          g_object_set ((GObject *) sink, "profile", 2, NULL);
        }
      else
        {
          sink = gst_element_factory_make ("autoaudiosink", NULL);
        }

      if (sink == NULL)
        sink = gst_element_factory_make ("alsasink", NULL);
    }
#endif /* MAEMO_OSSO_SUPPORT */

  if (sink == NULL)
    {
      DEBUG (stream, "failed to make audio sink element!");
      return NULL;
    }

  DEBUG (stream, "made audio sink element %s", GST_ELEMENT_NAME (sink));

  if (GST_IS_BIN (sink))
    {
      g_signal_connect ((GObject *) sink, "element-added",
          G_CALLBACK (set_audio_sink_props), NULL);
    }
  else
    {
      set_audio_sink_props (NULL, sink, NULL);
    }

  if (!has_volume_element (sink))
    sink = make_volume_bin (audiostream, sink, "sink");

  return sink;
}

gboolean tp_stream_engine_audio_stream_mute_output (
  TpStreamEngineAudioStream *audiostream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (audiostream);
  GstElement *sink = NULL;
  GstElement *muter;

  audiostream->priv->output_mute = mute_state;
  g_object_get (G_OBJECT (stream), "sink", &sink, NULL);

  if (!sink)
    return TRUE;

  muter = get_volume_element (sink);

  g_object_unref (sink);

  if (!muter)
    return TRUE;

  g_message ("%s: output mute set to %s", G_STRFUNC,
    mute_state ? "on" : "off");

  if (g_object_has_property (G_OBJECT (muter), "mute"))
    g_object_set (G_OBJECT (muter), "mute", mute_state, NULL);

  gst_object_unref (muter);

  return TRUE;
}

gboolean tp_stream_engine_audio_stream_set_output_volume (
  TpStreamEngineAudioStream *audiostream,
  guint volume,
  GError **error)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (audiostream);
  GstElement *sink = NULL;
  GstElement *volumer;
  GParamSpec *volume_prop;

  if (volume > 100)
    volume = 100;

  audiostream->priv->output_volume = volume;

  g_object_get (G_OBJECT (stream), "sink", &sink, NULL);

  if (!sink)
    return TRUE;

  volumer = get_volume_element (sink);

  g_object_unref (sink);

  if (!volumer)
    return TRUE;

  volume_prop = g_object_class_find_property (G_OBJECT_GET_CLASS (volumer),
      "volume");

  if (volume_prop)
    {
      if (volume_prop->value_type == G_TYPE_DOUBLE)
        {
          gdouble dvolume = volume / 100.0;

          DEBUG (stream, "Setting output volume to (%d) %f",
              audiostream->priv->output_volume, dvolume);

          g_object_set (volumer, "volume", dvolume, NULL);
        }
      else if (volume_prop->value_type == G_TYPE_INT)
        {
          gint scaled_volume;
          GParamSpecInt *pint = G_PARAM_SPEC_INT (volume_prop);

          scaled_volume = (volume * pint->maximum)/100;

          DEBUG (stream, "Setting output volume to %d (%d)",
              audiostream->priv->output_volume, scaled_volume);

          g_object_set (volumer, "volume", scaled_volume, NULL);
        }
      else
        {
          g_warning ("Volume is of an unknown type");
        }
    }

  gst_object_unref (volumer);

  return TRUE;
}

gboolean tp_stream_engine_audio_stream_mute_input (
  TpStreamEngineAudioStream *audiostream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (audiostream);
  GstElement *source = NULL;
  GstElement *muter;

  audiostream->priv->input_mute = mute_state;
  g_object_get (G_OBJECT (stream), "source", &source, NULL);

  if (!source)
    return TRUE;

  muter = get_volume_element (source);

  g_object_unref (source);

  if (!muter)
    return TRUE;

  g_message ("%s: input mute set to %s", G_STRFUNC,
    mute_state ? " on" : "off");

  if (g_object_has_property (G_OBJECT (muter), "mute"))
    g_object_set (G_OBJECT (muter), "mute", mute_state, NULL);

  gst_object_unref (muter);

  return TRUE;
}

static void
cb_set_remote_codecs (TpMediaStreamHandler *proxy,
    const GPtrArray *codecs,
    gpointer user_data,
    GObject *object)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (object);

  tp_stream_engine_audio_stream_mute_input (self, self->priv->input_mute, NULL);
}

static void
cb_fs_codec_changed (FarsightStream *stream,
                     gint codec_id,
                     gpointer user_data)
{
  TpStreamEngineAudioStream *self = TP_STREAM_ENGINE_AUDIO_STREAM (user_data);

  tp_stream_engine_audio_stream_mute_output (self, self->priv->output_mute,
      NULL);
  tp_stream_engine_audio_stream_mute_input (self, self->priv->input_mute, NULL);
  tp_stream_engine_audio_stream_set_output_volume (self,
      self->priv->output_volume, NULL);
}
