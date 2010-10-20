/*
 * cm-message.c - Source for TpCMMessage
 * Copyright (C) 2006-2010 Collabora Ltd.
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

/**
 * SECTION:message
 * @title: TpCMMessage
 * @short_description: a message in the Telepathy message interface
 *
 * #TpCMMessage represent a message send or received using the Message
 * interface.
 *
 * @since 0.7.21
 */

#include "cm-message-internal.h"
#include "cm-message.h"
#include "message-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

G_DEFINE_TYPE (TpCMMessage, tp_cm_message, TP_TYPE_MESSAGE)

/**
 * TpCMMessage:
 *
 * Opaque structure representing a message in the Telepathy messages interface
 * (an array of at least one mapping from string to variant, where the first
 * mapping contains message headers and subsequent mappings contain the
 * message body).
 */

struct _TpCMMessagePrivate
{
  gpointer unused;
};

static void
tp_cm_message_dispose (GObject *object)
{
  //TpCMMessage *self = TP_CM_MESSAGE (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_cm_message_parent_class)->dispose;

  if (dispose != NULL)
    dispose (object);
}

static void
tp_cm_message_class_init (TpCMMessageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = tp_cm_message_dispose;

  g_type_class_add_private (gobject_class, sizeof (TpCMMessagePrivate));
}

static void
tp_cm_message_init (TpCMMessage *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_MESSAGE,
      TpCMMessagePrivate);
}
