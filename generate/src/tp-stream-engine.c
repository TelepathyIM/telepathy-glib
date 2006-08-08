/*
 * tp-stream-engine.c - Source for TpStreamEngine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"

#include "tp-stream-engine-glue.h"

G_DEFINE_TYPE(TpStreamEngine, tp_stream_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpStreamEnginePrivate TpStreamEnginePrivate;

struct _TpStreamEnginePrivate
{
  gboolean dispose_has_run;
};

#define TP_STREAM_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate))

static void
tp_stream_engine_init (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void tp_stream_engine_dispose (GObject *object);
static void tp_stream_engine_finalize (GObject *object);

static void
tp_stream_engine_class_init (TpStreamEngineClass *tp_stream_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_stream_engine_class);

  g_type_class_add_private (tp_stream_engine_class, sizeof (TpStreamEnginePrivate));

  object_class->dispose = tp_stream_engine_dispose;
  object_class->finalize = tp_stream_engine_finalize;

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_stream_engine_class), &dbus_glib_tp_stream_engine_object_info);
}

void
tp_stream_engine_dispose (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose (object);
}

void
tp_stream_engine_finalize (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (tp_stream_engine_parent_class)->finalize (object);
}



/**
 * tp_stream_engine_handle_channel
 *
 * Implements DBus method HandleChannel
 * on interface org.freedesktop.Telepathy.ChannelHandler
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_handle_channel (TpStreamEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  return TRUE;
}


/**
 * tp_stream_engine_hold_stream
 *
 * Implements DBus method HoldStream
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_hold_stream (TpStreamEngine *obj, const gchar * channel, guint stream_id, gboolean mute_state, GError **error)
{
  return TRUE;
}


/**
 * tp_stream_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_mute_output (TpStreamEngine *obj, const gchar * channel, guint stream_id, gboolean mute_state, GError **error)
{
  return TRUE;
}


/**
 * tp_stream_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_volume (TpStreamEngine *obj, const gchar * channel, guint stream_id, guint volume, GError **error)
{
  return TRUE;
}


/**
 * tp_stream_engine_set_output_window
 *
 * Implements DBus method SetOutputWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_window (TpStreamEngine *obj, const gchar * channel, guint stream_id, guint window, GError **error)
{
  return TRUE;
}


/**
 * tp_stream_engine_set_preview_window
 *
 * Implements DBus method SetPreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_preview_window (TpStreamEngine *obj, const gchar * channel, guint stream_id, guint window, GError **error)
{
  return TRUE;
}

