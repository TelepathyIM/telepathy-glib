/*
 * tp-voip-engine.c - Source for TpVoipEngine
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

#include "tp-voip-engine.h"
#include "tp-voip-engine-signals-marshal.h"

#include "tp-voip-engine-glue.h"

G_DEFINE_TYPE(TpVoipEngine, tp_voip_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpVoipEnginePrivate TpVoipEnginePrivate;

struct _TpVoipEnginePrivate
{
  gboolean dispose_has_run;
};

#define TP_VOIP_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_VOIP_ENGINE, TpVoipEnginePrivate))

static void
tp_voip_engine_init (TpVoipEngine *obj)
{
  TpVoipEnginePrivate *priv = TP_VOIP_ENGINE_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void tp_voip_engine_dispose (GObject *object);
static void tp_voip_engine_finalize (GObject *object);

static void
tp_voip_engine_class_init (TpVoipEngineClass *tp_voip_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_voip_engine_class);

  g_type_class_add_private (tp_voip_engine_class, sizeof (TpVoipEnginePrivate));

  object_class->dispose = tp_voip_engine_dispose;
  object_class->finalize = tp_voip_engine_finalize;

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_voip_engine_class), &dbus_glib_tp_voip_engine_object_info);
}

void
tp_voip_engine_dispose (GObject *object)
{
  TpVoipEngine *self = TP_VOIP_ENGINE (object);
  TpVoipEnginePrivate *priv = TP_VOIP_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_voip_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_voip_engine_parent_class)->dispose (object);
}

void
tp_voip_engine_finalize (GObject *object)
{
  TpVoipEngine *self = TP_VOIP_ENGINE (object);
  TpVoipEnginePrivate *priv = TP_VOIP_ENGINE_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (tp_voip_engine_parent_class)->finalize (object);
}



/**
 * tp_voip_engine_handle_channel
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
gboolean tp_voip_engine_handle_channel (TpVoipEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  return TRUE;
}


/**
 * tp_voip_engine_mute
 *
 * Implements DBus method Mute
 * on interface org.freedesktop.Telepathy.StreamingEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_voip_engine_mute (TpVoipEngine *obj, gboolean mute_state, GError **error)
{
  return TRUE;
}


/**
 * tp_voip_engine_set_volume
 *
 * Implements DBus method SetVolume
 * on interface org.freedesktop.Telepathy.StreamingEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_voip_engine_set_volume (TpVoipEngine *obj, guint volume, GError **error)
{
  return TRUE;
}

