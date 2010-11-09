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
 * SECTION:cm-message
 * @title: TpCMMessage
 * @short_description: a message in the Telepathy message interface, CM side
 *
 *  #TpCMMessage is used within connection managers to represent a
 *  message sent or received using the Messages interface.
 *
 * @since 0.13.UNRELEASED
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
  TpBaseConnection *connection;

  /* handles referenced by this message */
  TpHandleSet *reffed_handles[NUM_TP_HANDLE_TYPES];
};

static void
tp_cm_message_dispose (GObject *object)
{
  TpCMMessage *self = TP_CM_MESSAGE (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_cm_message_parent_class)->dispose;
  guint i;

  tp_clear_object (&self->priv->connection);

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      tp_clear_pointer (&self->priv->reffed_handles[i], tp_handle_set_destroy);
    }

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

/**
 * tp_cm_message_new:
 * @connection: a connection on which to reference handles
 * @initial_parts: number of parts to create (at least 1)
 * @size_hint: preallocate space for this many parts (at least @initial_parts)
 *
 * <!-- nothing more to say -->
 *
 * Returns: a newly allocated message suitable to be passed to
 * tp_cm_message_mixin_take_received
 *
 * @since 0.13.UNRELEASED
 */
TpMessage *
tp_cm_message_new (TpBaseConnection *connection,
    guint initial_parts,
    guint size_hint)
{
  TpCMMessage *self;
  TpMessage *msg;

  g_return_val_if_fail (connection != NULL, NULL);

  self = g_object_new (TP_TYPE_CM_MESSAGE,
      "initial-parts", initial_parts,
      "size-hint", size_hint,
      NULL);

  msg = (TpMessage *) self;

  self->priv->connection = g_object_ref (connection);
  msg->incoming_id = G_MAXUINT32;
  msg->outgoing_context = NULL;

  return msg;
}

static void
_ensure_handle_set (TpCMMessage *self,
                    TpHandleType handle_type)
{
  if (self->priv->reffed_handles[handle_type] == NULL)
    {
      TpHandleRepoIface *handles = tp_base_connection_get_handles (
          self->priv->connection, handle_type);

      g_return_if_fail (handles != NULL);

      self->priv->reffed_handles[handle_type] = tp_handle_set_new (handles);
    }
}

/**
 * tp_cm_message_ref_handles:
 * @self: a message
 * @handle_type: a handle type, greater than %TP_HANDLE_TYPE_NONE and less
 *  than %NUM_TP_HANDLE_TYPES
 * @handles: a set of handles of the given type
 *
 * References all of the given handles until this message is destroyed.
 *
 * @since 0.13.UNRELEASED
 */
static void
tp_cm_message_ref_handles (TpMessage *msg,
                        TpHandleType handle_type,
                        TpIntset *handles)
{
  TpIntset *updated;
  TpCMMessage *self = (TpCMMessage *) msg;

  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (!tp_intset_is_member (handles, 0));

  _ensure_handle_set (self, handle_type);

  updated = tp_handle_set_update (self->priv->reffed_handles[handle_type], handles);
  tp_intset_destroy (updated);
}


/**
 * tp_cm_message_take_message:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @message: another (distinct) message created for the same #TpBaseConnection
 *
 * Set @key in part @part of @self to have @message as an aa{sv} value (that
 * is, an array of Message_Part), and take ownership of @message.  The caller
 * should not use @message after passing it to this function.  All handle
 * references owned by @message will subsequently belong to and be released
 * with @self.
 *
 * @since 0.13.UNRELEASED
 */
void
tp_cm_message_take_message (TpMessage *self,
    guint part,
    const gchar *key,
    TpMessage *message)
{
  guint i;
  TpCMMessage *cm_message;

  g_return_if_fail (self != NULL);
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (self != message);
  g_return_if_fail (TP_IS_CM_MESSAGE (self));
  g_return_if_fail (TP_IS_CM_MESSAGE (message));

  cm_message = (TpCMMessage *) message;

  g_return_if_fail (TP_CM_MESSAGE (self)->priv->connection ==
      TP_CM_MESSAGE (message)->priv->connection);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key),
      tp_g_value_slice_new_take_boxed (TP_ARRAY_TYPE_MESSAGE_PART_LIST,
          message->parts));

  /* Now that @self has stolen @message's parts, replace them with a stub to
   * keep tp_message_destroy happy.
   */
  message->parts = g_ptr_array_sized_new (1);
  g_ptr_array_add (message->parts, g_hash_table_new_full (g_str_hash,
        g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free));

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      if (cm_message->priv->reffed_handles[i] != NULL)
        tp_cm_message_ref_handles (self, i,
            tp_handle_set_peek (cm_message->priv->reffed_handles[i]));
    }

  tp_message_destroy (message);
}

/**
 * tp_cm_message_ref_handle:
 * @self: a message
 * @handle_type: a handle type, greater than %TP_HANDLE_TYPE_NONE and less than
 *  %NUM_TP_HANDLE_TYPES
 * @handle: a handle of the given type
 *
 * Reference the given handle until this message is destroyed.
 *
 * @since 0.13.UNRELEASED
 */
void
tp_cm_message_ref_handle (TpMessage *msg,
    TpHandleType handle_type,
    TpHandle handle)
{
  TpCMMessage *self;

  g_return_if_fail (TP_IS_CM_MESSAGE (msg));
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (handle != 0);

  self = (TpCMMessage *) msg;

  _ensure_handle_set (self, handle_type);

  tp_handle_set_add (self->priv->reffed_handles[handle_type], handle);
}

/**
 * tp_cm_message_set_sender:
 * @self: a #TpCMMessage
 * @handle: the #TpHandle of the sender of the message
 *
 * Set the sender of @self.
 *
 * @since 0.13.UNRELEASED
 */
void
tp_cm_message_set_sender (TpMessage *self,
    TpHandle handle)
{
  TpCMMessage *cm_msg;
  TpHandleRepoIface *contact_repo;
  const gchar *id;

  g_return_if_fail (TP_IS_CM_MESSAGE (self));
  g_return_if_fail (handle != 0);

  tp_cm_message_ref_handle (self, TP_HANDLE_TYPE_CONTACT, handle);

  tp_message_set_uint32 (self, 0, "message-sender", handle);

  cm_msg = (TpCMMessage *) self;

  contact_repo = tp_base_connection_get_handles (cm_msg->priv->connection,
      TP_HANDLE_TYPE_CONTACT);

  id = tp_handle_inspect (contact_repo, handle);
  if (id != NULL)
    tp_message_set_string (self, 0, "message-sender-id", id);
}
