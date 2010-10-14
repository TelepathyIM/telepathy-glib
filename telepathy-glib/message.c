/*
 * message.c - Source for TpMessage
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
 * @title: TpMessage
 * @short_description: a message in the Telepathy message interface
 *
 * #TpMessage represent a message send or received using the Message
 * interface.
 *
 * @since 0.7.21
 */

#include "message-internal.h"
#include "message.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

G_DEFINE_TYPE (TpMessage, tp_message, G_TYPE_OBJECT)

/**
 * TpMessage:
 *
 * Opaque structure representing a message in the Telepathy messages interface
 * (an array of at least one mapping from string to variant, where the first
 * mapping contains message headers and subsequent mappings contain the
 * message body).
 */

struct _TpMessagePrivate
{
  gpointer unused;
};

static void
tp_message_dispose (GObject *object)
{
  TpMessage *self = TP_MESSAGE (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_message_parent_class)->dispose;
  guint i;

  if (self->parts != NULL)
    {
      for (i = 0; i < self->parts->len; i++)
        {
          g_hash_table_destroy (g_ptr_array_index (self->parts, i));
        }

      g_ptr_array_free (self->parts, TRUE);
      self->parts = NULL;
    }

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      if (self->reffed_handles[i] != NULL)
        {
          tp_handle_set_destroy (self->reffed_handles[i]);
          self->reffed_handles[i] = NULL;
        }
    }

  tp_clear_object (&self->connection);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_message_class_init (TpMessageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = tp_message_dispose;

  g_type_class_add_private (gobject_class, sizeof (TpMessagePrivate));
}

static void
tp_message_init (TpMessage *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_MESSAGE,
      TpMessagePrivate);
}


/**
 * tp_message_new:
 * @connection: a connection on which to reference handles
 * @initial_parts: number of parts to create (at least 1)
 * @size_hint: preallocate space for this many parts (at least @initial_parts)
 *
 * <!-- nothing more to say -->
 *
 * Returns: a newly allocated message suitable to be passed to
 * tp_message_mixin_take_received
 *
 * @since 0.7.21
 */
TpMessage *
tp_message_new (TpBaseConnection *connection,
                guint initial_parts,
                guint size_hint)
{
  TpMessage *self;
  guint i;

  g_return_val_if_fail (connection != NULL, NULL);
  g_return_val_if_fail (initial_parts >= 1, NULL);
  g_return_val_if_fail (size_hint >= initial_parts, NULL);

  self = g_object_new (TP_TYPE_MESSAGE, NULL);
  self->connection = g_object_ref (connection);
  self->parts = g_ptr_array_sized_new (size_hint);
  self->incoming_id = G_MAXUINT32;
  self->outgoing_context = NULL;

  for (i = 0; i < initial_parts; i++)
    {
      g_ptr_array_add (self->parts, g_hash_table_new_full (g_str_hash,
            g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free));
    }

  return self;
}


/**
 * tp_message_destroy:
 * @self: a message
 *
 * Since 0.13.UNRELEASED this function is a simple wrapper around
 * g_object_unref()
 *
 * @since 0.7.21
 */
void
tp_message_destroy (TpMessage *self)
{
  g_object_unref (self);
}

/**
 * tp_message_count_parts:
 * @self: a message
 *
 * <!-- nothing more to say -->
 *
 * Returns: the number of parts in the message, including the headers in
 * part 0
 *
 * @since 0.7.21
 */
guint
tp_message_count_parts (TpMessage *self)
{
  return self->parts->len;
}

/**
 * tp_message_peek:
 * @self: a message
 * @part: a part number
 *
 * <!-- nothing more to say -->
 *
 * Returns: the #GHashTable used to implement the given part, or %NULL if the
 *  part number is out of range. The hash table is only valid as long as the
 *  message is valid and the part is not deleted.
 *
 * @since 0.7.21
 */
const GHashTable *
tp_message_peek (TpMessage *self,
                 guint part)
{
  if (part >= self->parts->len)
    return NULL;

  return g_ptr_array_index (self->parts, part);
}


/**
 * tp_message_append_part:
 * @self: a message
 *
 * Append a body part to the message.
 *
 * Returns: the part number
 *
 * @since 0.7.21
 */
guint
tp_message_append_part (TpMessage *self)
{
  g_ptr_array_add (self->parts, g_hash_table_new_full (g_str_hash,
        g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free));
  return self->parts->len - 1;
}

/**
 * tp_message_delete_part:
 * @self: a message
 * @part: a part number, which must be strictly greater than 0, and strictly
 *  less than the number returned by tp_message_count_parts()
 *
 * Delete the given body part from the message.
 *
 * @since 0.7.21
 */
void
tp_message_delete_part (TpMessage *self,
                        guint part)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (part > 0);

  g_hash_table_unref (g_ptr_array_remove_index (self->parts, part));
}


static void
_ensure_handle_set (TpMessage *self,
                    TpHandleType handle_type)
{
  if (self->reffed_handles[handle_type] == NULL)
    {
      TpHandleRepoIface *handles = tp_base_connection_get_handles (
          self->connection, handle_type);

      g_return_if_fail (handles != NULL);

      self->reffed_handles[handle_type] = tp_handle_set_new (handles);
    }
}


/**
 * tp_message_ref_handle:
 * @self: a message
 * @handle_type: a handle type, greater than %TP_HANDLE_TYPE_NONE and less than
 *  %NUM_TP_HANDLE_TYPES
 * @handle: a handle of the given type
 *
 * Reference the given handle until this message is destroyed.
 *
 * @since 0.7.21
 */
void
tp_message_ref_handle (TpMessage *self,
                       TpHandleType handle_type,
                       TpHandle handle)
{
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (handle != 0);

  _ensure_handle_set (self, handle_type);

  tp_handle_set_add (self->reffed_handles[handle_type], handle);
}


/**
 * tp_message_ref_handles:
 * @self: a message
 * @handle_type: a handle type, greater than %TP_HANDLE_TYPE_NONE and less
 *  than %NUM_TP_HANDLE_TYPES
 * @handles: a set of handles of the given type
 *
 * References all of the given handles until this message is destroyed.
 *
 * @since 0.7.21
 */
static void
tp_message_ref_handles (TpMessage *self,
                        TpHandleType handle_type,
                        TpIntset *handles)
{
  TpIntset *updated;

  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (!tp_intset_is_member (handles, 0));

  _ensure_handle_set (self, handle_type);

  updated = tp_handle_set_update (self->reffed_handles[handle_type], handles);
  tp_intset_destroy (updated);
}


/**
 * tp_message_delete_key:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 *
 * Remove the given key and its value from the given part.
 *
 * Returns: %TRUE if the key previously existed
 *
 * @since 0.7.21
 */
gboolean
tp_message_delete_key (TpMessage *self,
                       guint part,
                       const gchar *key)
{
  g_return_val_if_fail (part < self->parts->len, FALSE);

  return g_hash_table_remove (g_ptr_array_index (self->parts, part), key);
}


/**
 * tp_message_set_handle:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @handle_type: a handle type
 * @handle_or_0: a handle of that type, or 0
 *
 * If @handle_or_0 is not zero, reference it with tp_message_ref_handle().
 *
 * Set @key in part @part of @self to have @handle_or_0 as an unsigned integer
 * value.
 *
 * @since 0.7.21
 */
void
tp_message_set_handle (TpMessage *self,
                       guint part,
                       const gchar *key,
                       TpHandleType handle_type,
                       TpHandle handle_or_0)
{
  if (handle_or_0 != 0)
    tp_message_ref_handle (self, handle_type, handle_or_0);

  tp_message_set_uint32 (self, part, key, handle_or_0);
}


/**
 * tp_message_set_boolean:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @b: a boolean value
 *
 * Set @key in part @part of @self to have @b as a boolean value.
 *
 * @since 0.7.21
 */
void
tp_message_set_boolean (TpMessage *self,
                        guint part,
                        const gchar *key,
                        gboolean b)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_boolean (b));
}

/**
 * tp_message_set_int16:
 * @s: a message
 * @p: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @k: a key in the mapping representing the part
 * @i: an integer value
 *
 * Set @key in part @part of @self to have @i as a signed integer value.
 *
 * @since 0.7.21
 */

/**
 * tp_message_set_int32:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @i: an integer value
 *
 * Set @key in part @part of @self to have @i as a signed integer value.
 *
 * @since 0.7.21
 */
void
tp_message_set_int32 (TpMessage *self,
                      guint part,
                      const gchar *key,
                      gint32 i)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_int (i));
}


/**
 * tp_message_set_int64:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @i: an integer value
 *
 * Set @key in part @part of @self to have @i as a signed integer value.
 *
 * @since 0.7.21
 */
void
tp_message_set_int64 (TpMessage *self,
                      guint part,
                      const gchar *key,
                      gint64 i)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_int64 (i));
}


/**
 * tp_message_set_uint16:
 * @s: a message
 * @p: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @k: a key in the mapping representing the part
 * @u: an unsigned integer value
 *
 * Set @key in part @part of @self to have @u as an unsigned integer value.
 *
 * @since 0.7.21
 */

/**
 * tp_message_set_uint32:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @u: an unsigned integer value
 *
 * Set @key in part @part of @self to have @u as an unsigned integer value.
 *
 * @since 0.7.21
 */
void
tp_message_set_uint32 (TpMessage *self,
                       guint part,
                       const gchar *key,
                       guint32 u)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_uint (u));
}


/**
 * tp_message_set_uint64:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @u: an unsigned integer value
 *
 * Set @key in part @part of @self to have @u as an unsigned integer value.
 *
 * @since 0.7.21
 */
void
tp_message_set_uint64 (TpMessage *self,
                       guint part,
                       const gchar *key,
                       guint64 u)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_uint64 (u));
}


/**
 * tp_message_set_string:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @s: a string value
 *
 * Set @key in part @part of @self to have @s as a string value.
 *
 * @since 0.7.21
 */
void
tp_message_set_string (TpMessage *self,
                       guint part,
                       const gchar *key,
                       const gchar *s)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (s != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_string (s));
}


/**
 * tp_message_set_string_printf:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @fmt: a printf-style format string for the string value
 * @...: arguments for the format string
 *
 * Set @key in part @part of @self to have a string value constructed from a
 * printf-style format string.
 *
 * @since 0.7.21
 */
void
tp_message_set_string_printf (TpMessage *self,
                              guint part,
                              const gchar *key,
                              const gchar *fmt,
                              ...)
{
  va_list va;
  gchar *s;

  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (fmt != NULL);

  va_start (va, fmt);
  s = g_strdup_vprintf (fmt, va);
  va_end (va);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_new_take_string (s));
}


/**
 * tp_message_set_bytes:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @len: a number of bytes
 * @bytes: an array of @len bytes
 *
 * Set @key in part @part of @self to have @bytes as a byte-array value.
 *
 * @since 0.7.21
 */
void
tp_message_set_bytes (TpMessage *self,
                      guint part,
                      const gchar *key,
                      guint len,
                      gconstpointer bytes)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (bytes != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key),
      tp_g_value_slice_new_bytes (len, bytes));
}


/**
 * tp_message_set:
 * @self: a message
 * @part: a part number, which must be strictly less than the number
 *  returned by tp_message_count_parts()
 * @key: a key in the mapping representing the part
 * @source: a value
 *
 * Set @key in part @part of @self to have a copy of @source as its value.
 *
 * If @source represents a data structure containing handles, they should
 * all be referenced with tp_message_ref_handle() first.
 *
 * @since 0.7.21
 */
void
tp_message_set (TpMessage *self,
                guint part,
                const gchar *key,
                const GValue *source)
{
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (source != NULL);

  g_hash_table_insert (g_ptr_array_index (self->parts, part),
      g_strdup (key), tp_g_value_slice_dup (source));
}


/**
 * tp_message_take_message:
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
 * @since 0.7.21
 */
void
tp_message_take_message (TpMessage *self,
                         guint part,
                         const gchar *key,
                         TpMessage *message)
{
  guint i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (part < self->parts->len);
  g_return_if_fail (key != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (self != message);
  g_return_if_fail (self->connection == message->connection);

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
      if (message->reffed_handles[i] != NULL)
        tp_message_ref_handles (self, i,
            tp_handle_set_peek (message->reffed_handles[i]));
    }

  tp_message_destroy (message);
}
