/*
 * tp-media-engine.c - Source for TpMediaEngine
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

#include "tp-media-engine.h"
#include "tp-media-engine-signals-marshal.h"

#include "tp-media-engine-glue.h"

G_DEFINE_TYPE(TpMediaEngine, tp_media_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpMediaEnginePrivate TpMediaEnginePrivate;

struct _TpMediaEnginePrivate
{
  gboolean dispose_has_run;
};

#define TP_MEDIA_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_MEDIA_ENGINE, TpMediaEnginePrivate))

static void
tp_media_engine_init (TpMediaEngine *obj)
{
  TpMediaEnginePrivate *priv = TP_MEDIA_ENGINE_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void tp_media_engine_dispose (GObject *object);
static void tp_media_engine_finalize (GObject *object);

static void
tp_media_engine_class_init (TpMediaEngineClass *tp_media_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_media_engine_class);

  g_type_class_add_private (tp_media_engine_class, sizeof (TpMediaEnginePrivate));

  object_class->dispose = tp_media_engine_dispose;
  object_class->finalize = tp_media_engine_finalize;

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_media_engine_class), &dbus_glib_tp_media_engine_object_info);
}

void
tp_media_engine_dispose (GObject *object)
{
  TpMediaEngine *self = TP_MEDIA_ENGINE (object);
  TpMediaEnginePrivate *priv = TP_MEDIA_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_media_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_media_engine_parent_class)->dispose (object);
}

void
tp_media_engine_finalize (GObject *object)
{
  TpMediaEngine *self = TP_MEDIA_ENGINE (object);
  TpMediaEnginePrivate *priv = TP_MEDIA_ENGINE_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (tp_media_engine_parent_class)->finalize (object);
}



/**
 * tp_media_engine_handle_channel
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
gboolean tp_media_engine_handle_channel (TpMediaEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  return TRUE;
}


/**
 * tp_media_engine_mute_input
 *
 * Implements DBus method MuteInput
 * on interface org.freedesktop.Telepathy.StreamingEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_media_engine_mute_input (TpMediaEngine *obj, gboolean mute_state, GError **error)
{
  return TRUE;
}


/**
 * tp_media_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamingEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_media_engine_mute_output (TpMediaEngine *obj, gboolean mute_state, GError **error)
{
  return TRUE;
}


/**
 * tp_media_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamingEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_media_engine_set_output_volume (TpMediaEngine *obj, guint volume, GError **error)
{
  return TRUE;
}

