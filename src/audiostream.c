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
  gpointer filling;
};

static GstElement *
tp_stream_engine_audio_stream_make_src (TpStreamEngineStream *stream);

static void
tp_stream_engine_audio_stream_init (TpStreamEngineAudioStream *self)
{
  TpStreamEngineAudioStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_AUDIO_STREAM, TpStreamEngineAudioStreamPrivate);

  self->priv = priv;
}

static void
tp_stream_engine_audio_stream_dispose (GObject *object)
{
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

  stream_class->make_src = tp_stream_engine_audio_stream_make_src;
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
set_audio_src_props (GstBin *bin,
                     GstElement *src,
                     void *user_data)
{
  if (g_object_has_property ((GObject *) src, "blocksize"))
    g_object_set ((GObject *) src, "blocksize", 320, NULL);

  if (g_object_has_property ((GObject *) src, "latency-time"))
    g_object_set ((GObject *) src, "latency-time", G_GINT64_CONSTANT (20000),
        NULL);

  if (g_object_has_property ((GObject *) src, "is-live"))
    g_object_set ((GObject *) src, "is-live", TRUE, NULL);
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
    {
      src = gst_element_factory_make ("gconfaudiosrc", NULL);

      if (src == NULL)
        src = gst_element_factory_make ("alsasrc", NULL);
    }

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
