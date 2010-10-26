/*
 * client-message.c - Source for TpClientMessage
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
 * SECTION:client-message
 * @title: TpClientMessage
 * @short_description: a message in the Telepathy message interface, client side
 *
 * #TpClientMessage represent a message send using the Message interface.
 *
 * @since 0.13.UNRELEASED
 */

#include "client-message.h"
#include "client-message-internal.h"
#include "message-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

/**
 * TpClientMessage:
 *
 * Opaque structure representing a message in the Telepathy messages interface
 * (client side).
 */

G_DEFINE_TYPE (TpClientMessage, tp_client_message, TP_TYPE_MESSAGE)

struct _TpClientMessagePrivate
{
  gpointer unused;
};

static void
tp_client_message_dispose (GObject *object)
{
  //TpClientMessage *self = TP_CLIENT_MESSAGE (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_client_message_parent_class)->dispose;

  if (dispose != NULL)
    dispose (object);
}

static void
tp_client_message_class_init (TpClientMessageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = tp_client_message_dispose;

  g_type_class_add_private (gobject_class, sizeof (TpClientMessagePrivate));
}

static void
tp_client_message_init (TpClientMessage *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_MESSAGE,
      TpClientMessagePrivate);
}

/**
 * tp_client_message_new:
 * @self: a #TpTextChannel
 * @initial_parts: number of parts to create and allocate (at least 1)
 *
 * A convenient function to create a new #TpClientMessage
 *
 * Returns: (transfer full): a newly allocated #TpClientMessage
 *
 * Since: 0.13.UNRELEASED
 */
TpMessage *
tp_client_message_new (guint initial_parts)
{
  return g_object_new (TP_TYPE_CLIENT_MESSAGE,
      "initial-parts", initial_parts,
      "size-hint", initial_parts,
      NULL);
}

/**
 * tp_client_message_text_new:
 * @type: the type of message
 * @text: content of the messsage
 *
 * A convenient function to create a new #TpClientMessage having
 * 'text/plain' as 'content-typee', @type as 'message-type' and
 * @text as 'content'.
 *
 * Returns: (transfer full): a newly allocated #TpClientMessage
 *
 * Since: 0.13.UNRELEASED
 */
TpMessage *
tp_client_message_text_new (TpChannelTextMessageType type,
    const gchar *text)
{
  TpMessage *msg;

  msg = g_object_new (TP_TYPE_CLIENT_MESSAGE,
      "initial-parts", 2,
      "size-hint", 2,
      NULL);

  if (type != TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL)
    tp_message_set_uint32 (msg, 0, "message-type", type);

  tp_message_set_string (msg, 1, "content-type", "text/plain");
  tp_message_set_string (msg, 1, "content", text);

  return msg;
}
