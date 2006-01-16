/*
 * tp-channel-handler.c - Source for TpChannelHandler
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

#include "tp-channel-handler.h"
#include "tp-channel-handler-signals-marshal.h"

#include "tp-channel-handler-glue.h"

G_DEFINE_TYPE(TpChannelHandler, tp_channel_handler, G_TYPE_OBJECT)

/* signal enum */
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpChannelHandlerPrivate TpChannelHandlerPrivate;

struct _TpChannelHandlerPrivate
{
  gboolean dispose_has_run;
};

#define TP_CHANNEL_HANDLER_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_CHANNEL_HANDLER, TpChannelHandlerPrivate))

static void
tp_channel_handler_init (TpChannelHandler *obj)
{
  TpChannelHandlerPrivate *priv = TP_CHANNEL_HANDLER_GET_PRIVATE (obj);

  /* allocate any data required by the object here */
}

static void tp_channel_handler_dispose (GObject *object);
static void tp_channel_handler_finalize (GObject *object);

static void
tp_channel_handler_class_init (TpChannelHandlerClass *tp_channel_handler_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_channel_handler_class);

  g_type_class_add_private (tp_channel_handler_class, sizeof (TpChannelHandlerPrivate));

  object_class->dispose = tp_channel_handler_dispose;
  object_class->finalize = tp_channel_handler_finalize;

  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_channel_handler_class), &dbus_glib_tp_channel_handler_object_info);
}

void
tp_channel_handler_dispose (GObject *object)
{
  TpChannelHandler *self = TP_CHANNEL_HANDLER (object);
  TpChannelHandlerPrivate *priv = TP_CHANNEL_HANDLER_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_channel_handler_parent_class)->dispose)
    G_OBJECT_CLASS (tp_channel_handler_parent_class)->dispose (object);
}

void
tp_channel_handler_finalize (GObject *object)
{
  TpChannelHandler *self = TP_CHANNEL_HANDLER (object);
  TpChannelHandlerPrivate *priv = TP_CHANNEL_HANDLER_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (tp_channel_handler_parent_class)->finalize (object);
}



/**
 * tp_channel_handler_handle_channel
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
gboolean tp_channel_handler_handle_channel (TpChannelHandler *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  return TRUE;
}

