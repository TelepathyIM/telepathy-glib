/*
 * voip-engine.c - Source for VoipEngine
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

#include "voip-engine.h"
#include "voip-engine-signals-marshal.h"

#include "voip-engine-glue.h"
#include "common/telepathy-constants.h"
#include "common/telepathy-errors.h"


static gboolean handling_channel = FALSE;

G_DEFINE_TYPE(VoipEngine, voip_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _VoipEnginePrivate VoipEnginePrivate;

struct _VoipEnginePrivate
{
  gboolean dispose_has_run;
};

#define VOIP_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), VOIP_TYPE_ENGINE, VoipEnginePrivate))

static void
voip_engine_init (VoipEngine *obj)
{
  VoipEnginePrivate *priv = VOIP_ENGINE_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void voip_engine_dispose (GObject *object);
static void voip_engine_finalize (GObject *object);

static void
voip_engine_class_init (VoipEngineClass *voip_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (voip_engine_class);

  g_type_class_add_private (voip_engine_class, sizeof (VoipEnginePrivate));

  object_class->dispose = voip_engine_dispose;
  object_class->finalize = voip_engine_finalize;

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (voip_engine_class), &dbus_glib_voip_engine_object_info);
}

void
voip_engine_dispose (GObject *object)
{
  VoipEngine *self = VOIP_ENGINE (object);
  VoipEnginePrivate *priv = VOIP_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (voip_engine_parent_class)->dispose)
    G_OBJECT_CLASS (voip_engine_parent_class)->dispose (object);
}

void
voip_engine_finalize (GObject *object)
{
  VoipEngine *self = VOIP_ENGINE (object);
  VoipEnginePrivate *priv = VOIP_ENGINE_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (voip_engine_parent_class)->finalize (object);
}



/**
 * voip_engine_handle_channel
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
gboolean voip_engine_handle_channel (VoipEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  if (handling_channel)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "VoIP Engine is already handling a channel");

      return FALSE;
    }
  return TRUE;
}

