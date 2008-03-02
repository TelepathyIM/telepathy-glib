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

  g_type_class_add_private (klass, sizeof (TpStreamEngineAudioStreamPrivate));
  object_class->dispose = tp_stream_engine_audio_stream_dispose;
}
