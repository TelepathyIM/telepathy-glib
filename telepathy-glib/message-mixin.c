/*
 * message-mixin.c - Source for TpMessageMixin
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

/**
 * SECTION:message-mixin
 * @title: TpMessageMixin
 * @short_description: a mixin implementation of the text channel type and the
 *  Messages interface
 * @see_also: #TpSvcChannelTypeText, #TpSvcChannelInterfaceMessages,
 *  #TpDBusPropertiesMixin
 *
 * This mixin can be added to a channel GObject class to implement the
 * text channel type (with the Messages interface) in a general way.
 * The channel class should also have a #TpDBusPropertiesMixinClass.
 *
 * To use the messages mixin, include a #TpMessageMixin somewhere in your
 * instance structure, and call tp_message_mixin_init() from your
 * constructor function, and tp_message_mixin_finalize() from your dispose
 * or finalize function. In the class_init function, call
 * tp_message_mixin_init_dbus_properties() to hook this mixin into the D-Bus
 * properties mixin class. Finally, include the following in the fourth
 * argument of G_DEFINE_TYPE_WITH_CODE():
 *
 * <informalexample><programlisting>
 *  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
 *    tp_message_mixin_text_iface_init);
 *  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
 *    tp_message_mixin_messages_iface_init);
 * </programlisting></informalexample>
 *
 * To support sending messages, you must call
 * tp_message_mixin_implement_sending() in the constructor function. If you do
 * not, any attempt to send a message will fail with NotImplemented.
 *
 * @since 0.7.21
 */

#include <telepathy-glib/message-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#define DEBUG_FLAG TP_DEBUG_IM

#include "debug-internal.h"

/**
 * TpMessageMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_message_mixin_init().
 *
 * There are no public fields.
 *
 * @since 0.7.21
 */

struct _TpMessageMixinPrivate
{
  TpBaseConnection *connection;

  /* Sending */
  TpMessageMixinSendImpl send_message;
  GArray *msg_types;
  TpMessagePartSupportFlags message_part_support_flags;
  TpDeliveryReportingSupportFlags delivery_reporting_support_flags;
  gchar **supported_content_types;

  /* Receiving */
  guint recv_id;
  GQueue *pending;
};


static const char * const forbidden_keys[] = {
    "pending-message-id",
    NULL
};


static const char * const body_only[] = {
    "alternative",
    "content-type",
    "type",                     /* deprecated in 0.17.14 */
    "content",
    "identifier",
    "needs-retrieval",
    "truncated",
    "size",
    NULL
};


static const char * const body_only_incoming[] = {
    "needs-retrieval",
    "truncated",
    "size",
    NULL
};


static const char * const headers_only[] = {
    "message-type",
    "message-sender",
    "message-sent",
    "message-received",
    "message-token",
    NULL
};


static const char * const headers_only_incoming[] = {
    "message-sender",
    "message-sent",
    "message-received",
    NULL
};


#define TP_MESSAGE_MIXIN_OFFSET_QUARK (tp_message_mixin_get_offset_quark ())
#define TP_MESSAGE_MIXIN_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_TYPE (o), \
                                       TP_MESSAGE_MIXIN_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN(o) \
  ((TpMessageMixin *) tp_mixin_offset_cast (o, TP_MESSAGE_MIXIN_OFFSET (o)))


/**
 * TpMessage:
 *
 * Opaque structure representing a message in the Telepathy messages interface
 * (an array of at least one mapping from string to variant, where the first
 * mapping contains message headers and subsequent mappings contain the
 * message body).
 */


struct _TpMessage {
    TpBaseConnection *connection;

    /* array of hash tables, allocated string => sliced GValue */
    GPtrArray *parts;

    /* handles referenced by this message */
    TpHandleSet *reffed_handles[NUM_TP_HANDLE_TYPES];

    /* from here down is implementation-specific for TpMessageMixin */

    /* for receiving */
    guint32 incoming_id;
    /* A non-NULL reference until we have been queued; borrowed afterwards */
    GObject *incoming_target;

    /* for sending */
    DBusGMethodInvocation *outgoing_context;
    TpMessageSendingFlags outgoing_flags;
    gboolean outgoing_text_api:1;
};


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

  self = g_slice_new0 (TpMessage);
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
 * Destroy @self.
 *
 * @since 0.7.21
 */
void
tp_message_destroy (TpMessage *self)
{
  guint i;

  g_return_if_fail (TP_IS_BASE_CONNECTION (self->connection));
  g_return_if_fail (self->parts != NULL);
  g_return_if_fail (self->parts->len >= 1);

  for (i = 0; i < self->parts->len; i++)
    {
      g_hash_table_destroy (g_ptr_array_index (self->parts, i));
    }

  g_ptr_array_free (self->parts, TRUE);

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      if (self->reffed_handles[i] != NULL)
        tp_handle_set_destroy (self->reffed_handles[i]);
    }

  g_object_unref (self->connection);

  g_slice_free (TpMessage, self);
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
                        TpIntSet *handles)
{
  TpIntSet *updated;

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


/**
 * tp_message_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
 *
 * @since 0.7.21
 */
static GQuark
tp_message_mixin_get_offset_quark (void)
{
  static GQuark offset_quark = 0;

  if (G_UNLIKELY (offset_quark == 0))
    offset_quark = g_quark_from_static_string (
        "tp_message_mixin_get_offset_quark@0.7.7");

  return offset_quark;
}


static gint
pending_item_id_equals_data (gconstpointer item,
                             gconstpointer data)
{
  const TpMessage *self = item;
  guint id = GPOINTER_TO_UINT (data);

  /* The sense of this comparison is correct: the callback passed to
   * g_queue_find_custom() should return 0 when the desired item is found.
   */
  return (self->incoming_id != id);
}


static inline void
nullify_hash (GHashTable **hash)
{
  if (*hash == NULL)
    return;

  g_hash_table_destroy (*hash);
  *hash = NULL;
}


static void
subtract_from_hash (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  DEBUG ("... removing %s", (gchar *) key);
  g_hash_table_remove (user_data, key);
}


static gchar *
parts_to_text (const GPtrArray *parts,
               TpChannelTextMessageFlags *out_flags,
               TpChannelTextMessageType *out_type,
               TpHandle *out_sender,
               guint *out_timestamp)
{
  guint i;
  GHashTable *header = g_ptr_array_index (parts, 0);
  /* Lazily created hash tables, used as a sets: keys are borrowed
   * "alternative" string values from @parts, value == key. */
  /* Alternative IDs for which we have already extracted an alternative */
  GHashTable *alternatives_used = NULL;
  /* Alternative IDs for which we expect to extract text, but have not yet;
   * cleared if the flag Channel_Text_Message_Flag_Non_Text_Content is set.
   * At the end, if this contains any item not in alternatives_used,
   * Channel_Text_Message_Flag_Non_Text_Content must be set. */
  GHashTable *alternatives_needed = NULL;
  GString *buffer = g_string_new ("");
  TpChannelTextMessageFlags flags = 0;

  if (tp_asv_get_boolean (header, "scrollback", NULL))
    flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_SCROLLBACK;

  if (tp_asv_get_boolean (header, "rescued", NULL))
    flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_RESCUED;

  /* If the message is on an extended interface or only contains headers,
   * definitely set the "your client is too old" flag. */
  if (parts->len <= 1 || g_hash_table_lookup (header, "interface") != NULL)
    {
      flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT;
    }

  for (i = 1; i < parts->len; i++)
    {
      GHashTable *part = g_ptr_array_index (parts, i);
      const gchar *type = tp_asv_get_string (part, "content-type");
      const gchar *alternative = tp_asv_get_string (part, "alternative");

      /* Renamed to "content-type" in spec 0.17.14 */
      if (type == NULL)
        type = tp_asv_get_string (part, "type");

      DEBUG ("Parsing part %u, type %s, alternative %s", i, type, alternative);

      if (!tp_strdiff (type, "text/plain"))
        {
          GValue *value;

          DEBUG ("... is text/plain");

          if (alternative != NULL && alternative[0] != '\0')
            {
              if (alternatives_used == NULL)
                {
                  /* We can't have seen an alternative for this part yet.
                   * However, we need to create the hash table now */
                  alternatives_used = g_hash_table_new (g_str_hash,
                      g_str_equal);
                }
              else if (g_hash_table_lookup (alternatives_used,
                    alternative) != NULL)
                {
                  /* we've seen a "better" alternative for this part already.
                   * Skip it */
                  DEBUG ("... already saw a better alternative, skipping it");
                  continue;
                }

              g_hash_table_insert (alternatives_used, (gpointer) alternative,
                  (gpointer) alternative);
            }

          value = g_hash_table_lookup (part, "content");

          if (value != NULL && G_VALUE_HOLDS_STRING (value))
            {
              DEBUG ("... using its text");
              g_string_append (buffer, g_value_get_string (value));

              value = g_hash_table_lookup (part, "truncated");

              if (value != NULL && (!G_VALUE_HOLDS_BOOLEAN (value) ||
                  g_value_get_boolean (value)))
                {
                  DEBUG ("... appears to have been truncated");
                  flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_TRUNCATED;
                }
            }
          else
            {
              /* There was a text/plain part we couldn't parse:
               * that counts as "non-text content" I think */
              DEBUG ("... didn't understand it, setting NON_TEXT_CONTENT");
              flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT;
              nullify_hash (&alternatives_needed);
            }
        }
      else if ((flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT) == 0)
        {
          DEBUG ("... wondering whether this is NON_TEXT_CONTENT?");

          if (alternative == NULL || alternative[0] == '\0')
            {
              /* This part can't possibly have a text alternative, since it
               * isn't part of a multipart/alternative group
               * (attached image or something, perhaps) */
              DEBUG ("... ... yes, no possibility of a text alternative");
              flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT;
              nullify_hash (&alternatives_needed);
            }
          else if (alternatives_used != NULL &&
              g_hash_table_lookup (alternatives_used, (gpointer) alternative)
              != NULL)
            {
              DEBUG ("... ... no, we already saw a text alternative");
            }
          else
            {
              /* This part might have a text alternative later, if we're
               * lucky */
              if (alternatives_needed == NULL)
                alternatives_needed = g_hash_table_new (g_str_hash,
                    g_str_equal);

              DEBUG ("... ... perhaps, but might have text alternative later");
              g_hash_table_insert (alternatives_needed, (gpointer) alternative,
                  (gpointer) alternative);
            }
        }
    }

  if ((flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT) == 0 &&
      alternatives_needed != NULL)
    {
      if (alternatives_used != NULL)
        g_hash_table_foreach (alternatives_used, subtract_from_hash,
            alternatives_needed);

      if (g_hash_table_size (alternatives_needed) > 0)
        flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT;
    }

  if (alternatives_needed != NULL)
    g_hash_table_destroy (alternatives_needed);

  if (alternatives_used != NULL)
    g_hash_table_destroy (alternatives_used);

  if (out_flags != NULL)
    {
      *out_flags = flags;
    }

  if (out_type != NULL)
    {
      /* The fallback behaviour of tp_asv_get_uint32 is OK here, because
       * message type NORMAL is zero */
      *out_type = tp_asv_get_uint32 (header, "message-type", NULL);
    }

  if (out_sender != NULL)
    {
      /* The fallback behaviour of tp_asv_get_uint32 is OK here - if there's
       * no good sender, then 0 is the least bad */
      *out_sender = tp_asv_get_uint32 (header, "message-sender", NULL);
    }

  if (out_timestamp != NULL)
    {
      /* The fallback behaviour of tp_asv_get_uint32 is OK here - we assume
       * that we won't legitimately receive messages from 1970-01-01 :-) */
      *out_timestamp = tp_asv_get_uint32 (header, "message-sent", NULL);

      if (*out_timestamp == 0)
        *out_timestamp = tp_asv_get_uint32 (header, "message-received", NULL);

      if (*out_timestamp == 0)
        *out_timestamp = time (NULL);
    }

  return g_string_free (buffer, FALSE);
}


/**
 * TpMessageMixinSendImpl:
 * @object: An instance of the implementation that uses this mixin
 * @message: An outgoing message
 * @flags: flags with which to send the message
 *
 * Signature of a virtual method which may be implemented to allow messages
 * to be sent. It must arrange for tp_message_mixin_sent() to be called when
 * the message has submitted or when message submission has failed.
 */


/**
 * tp_message_mixin_implement_sending:
 * @object: An instance of the implementation that uses this mixin
 * @send: An implementation of SendMessage()
 * @n_types: Number of supported message types
 * @types: @n_types supported message types
 * @message_part_support_flags: Flags indicating what message part structures
 *  are supported
 * @delivery_reporting_support_flags: Flags indicating what kind of delivery
 *  reports are supported
 * @supported_content_types: The supported content types
 *
 * Set the callback used to implement SendMessage, and the types of message
 * that can be sent. This must be called from the init, constructor or
 * constructed callback, after tp_message_mixin_init(), and may only be called
 * once per object.
 *
 * @since 0.7.21
 */
void
tp_message_mixin_implement_sending (GObject *object,
                                    TpMessageMixinSendImpl send,
                                    guint n_types,
                                    const TpChannelTextMessageType *types,
                                    TpMessagePartSupportFlags
                                        message_part_support_flags,
                                    TpDeliveryReportingSupportFlags
                                        delivery_reporting_support_flags,
                                    const gchar * const *
                                        supported_content_types)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (mixin->priv->send_message == NULL);
  mixin->priv->send_message = send;

  if (mixin->priv->msg_types->len > 0)
    g_array_remove_range (mixin->priv->msg_types, 0,
        mixin->priv->msg_types->len);

  g_assert (mixin->priv->msg_types->len == 0);
  g_array_append_vals (mixin->priv->msg_types, types, n_types);

  mixin->priv->message_part_support_flags = message_part_support_flags;
  mixin->priv->delivery_reporting_support_flags = delivery_reporting_support_flags;

  g_strfreev (mixin->priv->supported_content_types);
  mixin->priv->supported_content_types = g_strdupv (
      (gchar **) supported_content_types);
}


/**
 * tp_message_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpMessageMixin within the object structure
 * @connection: A #TpBaseConnection
 *
 * Initialize the mixin. Should be called from the implementation's
 * instance init function or constructor like so:
 *
 * <informalexample><programlisting>
 * tp_message_mixin_init ((GObject *) self,
 *     G_STRUCT_OFFSET (SomeObject, message_mixin),
 *     self->connection);
 * </programlisting></informalexample>
 *
 * @since 0.7.21
 */
void
tp_message_mixin_init (GObject *obj,
                       gsize offset,
                       TpBaseConnection *connection)
{
  TpMessageMixin *mixin;

  g_assert (G_IS_OBJECT (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_MESSAGE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  mixin = TP_MESSAGE_MIXIN (obj);

  mixin->priv = g_slice_new0 (TpMessageMixinPrivate);

  mixin->priv->pending = g_queue_new ();
  mixin->priv->recv_id = 0;
  mixin->priv->msg_types = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      NUM_TP_CHANNEL_TEXT_MESSAGE_TYPES);
  mixin->priv->connection = g_object_ref (connection);

  mixin->priv->supported_content_types = g_new0 (gchar *, 1);
}


/**
 * tp_message_mixin_clear:
 * @obj: An object with this mixin
 *
 * Clear the pending message queue, deleting all messages without emitting
 * PendingMessagesRemoved.
 */
void
tp_message_mixin_clear (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  TpMessage *item;

  while ((item = g_queue_pop_head (mixin->priv->pending)) != NULL)
    {
      tp_message_destroy (item);
    }
}


/**
 * tp_message_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the text mixin.
 *
 * @since 0.7.21
 */
void
tp_message_mixin_finalize (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);

  DEBUG ("%p", obj);

  tp_message_mixin_clear (obj);
  g_assert (g_queue_is_empty (mixin->priv->pending));
  g_queue_free (mixin->priv->pending);
  g_array_free (mixin->priv->msg_types, TRUE);
  g_strfreev (mixin->priv->supported_content_types);

  g_object_unref (mixin->priv->connection);

  g_slice_free (TpMessageMixinPrivate, mixin->priv);
}

static void
tp_message_mixin_acknowledge_pending_messages_async (
    TpSvcChannelTypeText *iface,
    const GArray *ids,
    DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  GList **nodes;
  TpMessage *item;
  guint i;

  nodes = g_new (GList *, ids->len);

  for (i = 0; i < ids->len; i++)
    {
      guint id = g_array_index (ids, guint, i);

      nodes[i] = g_queue_find_custom (mixin->priv->pending,
          GUINT_TO_POINTER (id), pending_item_id_equals_data);

      if (nodes[i] == NULL)
        {
          GError *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "invalid message id %u", id);

          DEBUG ("%s", error->message);
          dbus_g_method_return_error (context, error);
          g_error_free (error);

          g_free (nodes);
          return;
        }
    }

  tp_svc_channel_interface_messages_emit_pending_messages_removed (iface,
      ids);

  for (i = 0; i < ids->len; i++)
    {
      item = nodes[i]->data;

      DEBUG ("acknowledging message id %u", item->incoming_id);

      g_queue_remove (mixin->priv->pending, item);
      tp_message_destroy (item);
    }

  g_free (nodes);
  tp_svc_channel_type_text_return_from_acknowledge_pending_messages (context);
}

static void
tp_message_mixin_list_pending_messages_async (TpSvcChannelTypeText *iface,
                                              gboolean clear,
                                              DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  GType pending_type = TP_STRUCT_TYPE_PENDING_TEXT_MESSAGE;
  guint count;
  GPtrArray *messages;
  GList *cur;
  guint i;

  count = g_queue_get_length (mixin->priv->pending);
  messages = g_ptr_array_sized_new (count);

  for (cur = g_queue_peek_head_link (mixin->priv->pending);
       cur != NULL;
       cur = cur->next)
    {
      TpMessage *msg = cur->data;
      GValue val = { 0, };
      gchar *text;
      TpChannelTextMessageFlags flags;
      TpChannelTextMessageType type;
      TpHandle sender;
      guint timestamp;

      text = parts_to_text (msg->parts, &flags, &type, &sender, &timestamp);

      g_value_init (&val, pending_type);
      g_value_take_boxed (&val,
          dbus_g_type_specialized_construct (pending_type));
      dbus_g_type_struct_set (&val,
          0, msg->incoming_id,
          1, timestamp,
          2, sender,
          3, type,
          4, flags,
          5, text,
          G_MAXUINT);

      g_free (text);

      g_ptr_array_add (messages, g_value_get_boxed (&val));
    }

  if (clear)
    {
      GArray *ids;

      DEBUG ("WARNING: ListPendingMessages(clear=TRUE) is deprecated");
      cur = g_queue_peek_head_link (mixin->priv->pending);

      ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), count);

      while (cur != NULL)
        {
          TpMessage *msg = cur->data;
          GList *next = cur->next;

          i = msg->incoming_id;
          g_array_append_val (ids, i);
          g_queue_delete_link (mixin->priv->pending, cur);
          tp_message_destroy (msg);

          cur = next;
        }

      tp_svc_channel_interface_messages_emit_pending_messages_removed (iface,
          ids);
      g_array_free (ids, TRUE);
    }

  tp_svc_channel_type_text_return_from_list_pending_messages (context,
      messages);

  for (i = 0; i < messages->len; i++)
    g_value_array_free (g_ptr_array_index (messages, i));

  g_ptr_array_free (messages, TRUE);
}

static void
tp_message_mixin_get_pending_message_content_async (
    TpSvcChannelInterfaceMessages *iface,
    guint message_id,
    const GArray *part_numbers,
    DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  GList *node;
  TpMessage *item;
  GHashTable *ret;
  guint i;

  node = g_queue_find_custom (mixin->priv->pending,
      GUINT_TO_POINTER (message_id), pending_item_id_equals_data);

  if (node == NULL)
    {
      GError *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "invalid message id %u", message_id);

      DEBUG ("%s", error->message);
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  item = node->data;

  for (i = 0; i < part_numbers->len; i++)
    {
      guint part = g_array_index (part_numbers, guint, i);

      if (part == 0 || part >= item->parts->len)
        {
          GError *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "part number %u out of range", part);

          DEBUG ("%s", error->message);
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
        }
    }

  /* no free callbacks set - we borrow the content from the message */
  ret = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* FIXME: later, need a way to support streaming content */

  for (i = 0; i < part_numbers->len; i++)
    {
      guint part = g_array_index (part_numbers, guint, i);
      GHashTable *part_data;
      GValue *value;

      g_assert (part != 0 && part < item->parts->len);
      part_data = g_ptr_array_index (item->parts, part);

      /* skip parts with no type (reserved) */
      if (tp_asv_get_string (part_data, "content-type") == NULL &&
          /* Renamed to "content-type" in spec 0.17.14 */
          tp_asv_get_string (part_data, "type") == NULL)
        continue;

      value = g_hash_table_lookup (part_data, "content");

      /* skip parts with no content */
      if (value == NULL)
        continue;

      g_hash_table_insert (ret, GUINT_TO_POINTER (part), value);
    }

  tp_svc_channel_interface_messages_return_from_get_pending_message_content (
      context, ret);

  g_hash_table_destroy (ret);
}

static void
tp_message_mixin_get_message_types_async (TpSvcChannelTypeText *iface,
                                          DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);

  tp_svc_channel_type_text_return_from_get_message_types (context,
      mixin->priv->msg_types);
}


static gboolean
queue_pending (gpointer data)
{
  TpMessage *pending = data;
  GObject *object = pending->incoming_target;
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  TpChannelTextMessageFlags flags;
  TpChannelTextMessageType type;
  TpHandle sender;
  guint timestamp;
  gchar *text;
  const GHashTable *header;
  TpDeliveryStatus delivery_status;

  g_queue_push_tail (mixin->priv->pending, pending);

  text = parts_to_text (pending->parts, &flags, &type, &sender, &timestamp);
  tp_svc_channel_type_text_emit_received (object, pending->incoming_id,
      timestamp, sender, type, flags, text);
  g_free (text);

  tp_svc_channel_interface_messages_emit_message_received (object,
      pending->parts);


  /* Check if it's a failed delivery report; if so, emit SendError too. */
  header = tp_message_peek (pending, 0);
  delivery_status = tp_asv_get_uint32 (header, "delivery-status", NULL);

  if (delivery_status == TP_DELIVERY_STATUS_TEMPORARILY_FAILED ||
      delivery_status == TP_DELIVERY_STATUS_PERMANENTLY_FAILED)
    {
      /* Fallback behaviour here is okay: 0 is Send_Error_Unknown */
      TpChannelTextSendError send_error = tp_asv_get_uint32 (header,
          "delivery-error", NULL);
      GPtrArray *echo = tp_asv_get_boxed (header, "delivery-echo",
          TP_ARRAY_TYPE_MESSAGE_PART_LIST);

      type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;

      if (echo != NULL)
        {
          const GHashTable *echo_header = g_ptr_array_index (echo, 1);

          /* The specification says that the timestamp in SendError should be the
           * time at which the original message was sent.  parts_to_text falls
           * back to setting timestamp to time (NULL) if it can't find out when
           * the message was sent, but we want to use 0 in that case.  Hence,
           * we look up timestamp here rather than delegating to parts_to_text.
           * The fallback behaviour of tp_asv_get_uint32 is correct: we want
           * timestamp to be 0 if we can't determine when the original message
           * was sent.
           */
          text = parts_to_text (echo, NULL, &type, NULL, NULL);
          timestamp = tp_asv_get_uint32 (echo_header, "message-sent", NULL);
        }
      else
        {
          text = NULL;
          timestamp = 0;
        }

      tp_svc_channel_type_text_emit_send_error (object, send_error, timestamp,
          type, text != NULL ? text : "");

      g_free (text);
    }

  g_object_unref (object);

  return FALSE;
}


/**
 * tp_message_mixin_take_received:
 * @object: a channel with this mixin
 * @message: the message. Its ownership is claimed by the message
 *  mixin, so it must no longer be modified or freed
 *
 * Receive a message into the pending messages queue, where it will stay
 * until acknowledged, and emit the Received and ReceivedMessage signals. Also
 * emit the SendError signal if the message is a failed delivery report.
 *
 * Returns: the message ID
 *
 * @since 0.7.21
 */
guint
tp_message_mixin_take_received (GObject *object,
                                TpMessage *message)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  GHashTable *header;

  g_return_val_if_fail (message->incoming_id == G_MAXUINT32, 0);
  g_return_val_if_fail (message->parts->len >= 1, 0);

  header = g_ptr_array_index (message->parts, 0);

  g_return_val_if_fail (g_hash_table_lookup (header, "pending-message-id")
      == NULL, 0);

  /* FIXME: we don't check for overflow, so in highly pathological cases we
   * might end up with multiple messages with the same ID */
  message->incoming_id = mixin->priv->recv_id++;

  tp_message_set_uint32 (message, 0, "pending-message-id",
      message->incoming_id);

  if (tp_asv_get_uint64 (header, "message-received", NULL) == 0)
    tp_message_set_uint64 (message, 0, "message-received",
        time (NULL));

  /* We don't actually add the pending message to the queue immediately,
   * to guarantee that the caller of this function gets to see the message ID
   * before anyone else does (so that it can acknowledge the message to the
   * network). */
  message->incoming_target = g_object_ref (object);
  g_idle_add (queue_pending, message);

  return message->incoming_id;
}


/**
 * tp_message_mixin_has_pending_messages:
 * @object: An object with this mixin
 * @first_sender: If not %NULL, used to store the sender of the oldest pending
 *  message
 *
 * Return whether the channel @obj has unacknowledged messages. If so, and
 * @first_sender is not %NULL, the handle of the sender of the first message
 * is placed in it, without incrementing the handle's reference count.
 *
 * Returns: %TRUE if there are pending messages
 */
gboolean
tp_message_mixin_has_pending_messages (GObject *object,
                                       TpHandle *first_sender)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  TpMessage *msg = g_queue_peek_head (mixin->priv->pending);

  if (msg != NULL && first_sender != NULL)
    {
      const GHashTable *header = tp_message_peek (msg, 0);
      gboolean valid = TRUE;
      TpHandle h = tp_asv_get_uint32 (header, "message-sender", &valid);

      if (valid)
        *first_sender = h;
      else
        g_warning ("%s: oldest message's message-sender is mistyped", G_STRFUNC);
    }

  return (msg != NULL);
}


/**
 * tp_message_mixin_set_rescued:
 * @obj: An object with this mixin
 *
 * Mark all pending messages as having been "rescued" from a channel that
 * previously closed.
 */
void
tp_message_mixin_set_rescued (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  GList *cur;

  for (cur = g_queue_peek_head_link (mixin->priv->pending);
       cur != NULL;
       cur = cur->next)
    {
      TpMessage *msg = cur->data;

      tp_message_set_boolean (msg, 0, "rescued", TRUE);
    }
}


/**
 * TpMessageMixinOutgoingMessage:
 * @flags: Flags indicating how this message should be sent
 * @parts: The parts that make up the message (an array of #GHashTable,
 *  with the first one containing message headers)
 * @priv: Pointer to opaque private data used by the messages mixin
 *
 * Structure representing a message which is to be sent.
 *
 * Connection managers may (and should) edit the @parts in-place to remove
 * keys that could not be sent, using g_hash_table_remove(). Connection
 * managers may also alter @parts to include newly allocated GHashTable
 * structures.
 *
 * However, they must not add keys to an existing GHashTable (this is because
 * the connection manager has no way to know how the keys and values will be
 * freed).
 *
 * @since 0.7.21
 */


struct _TpMessageMixinOutgoingMessagePrivate {
    DBusGMethodInvocation *context;
    gboolean messages:1;
};


/**
 * tp_message_mixin_sent:
 * @object: An object implementing the Text and Messages interfaces with this
 *  mixin
 * @message: The outgoing message
 * @flags: The flags used when sending the message, which may be a subset of
 *  those passed to the #TpMessageMixinSendImpl implementation if not all are
 *  supported, or %0 on error.
 * @token: A token representing the sent message (see the Telepathy D-Bus API
 *  specification), or an empty string if no suitable identifier is available,
 *  or %NULL on error
 * @error: %NULL on success, or the error with which message submission failed
 *
 * Indicate to the message mixin that message submission to the IM server has
 * succeeded or failed.
 *
 * After this function is called, @message will have been freed, and must not
 * be dereferenced.
 *
 * @since 0.7.21
 */
void
tp_message_mixin_sent (GObject *object,
                       TpMessage *message,
                       TpMessageSendingFlags flags,
                       const gchar *token,
                       const GError *error)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  time_t now = time (NULL);

  g_return_if_fail (mixin != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (message->parts != NULL);
  g_return_if_fail (message->outgoing_context != NULL);
  g_return_if_fail (token == NULL || error == NULL);
  g_return_if_fail (token != NULL || error != NULL);

  if (error != NULL)
    {
      GError *e = g_error_copy (error);

      dbus_g_method_return_error (message->outgoing_context, e);
      g_error_free (e);
    }
  else
    {
      TpChannelTextMessageType message_type;
      gchar *string;
      GHashTable *header = g_ptr_array_index (message->parts, 0);

      if (tp_asv_get_uint64 (header, "message-sent", NULL) == 0)
        tp_message_set_uint64 (message, 0, "message-sent", time (NULL));

      /* emit Sent and MessageSent */

      tp_svc_channel_interface_messages_emit_message_sent (object,
          message->parts, flags, token);
      string = parts_to_text (message->parts, NULL, &message_type, NULL, NULL);
      tp_svc_channel_type_text_emit_sent (object, now, message_type,
          string);
      g_free (string);

      /* return successfully */

      if (message->outgoing_text_api)
        {
          tp_svc_channel_type_text_return_from_send (
              message->outgoing_context);
        }
      else
        {
          tp_svc_channel_interface_messages_return_from_send_message (
              message->outgoing_context, token);
        }
    }

  message->outgoing_context = NULL;
  tp_message_destroy (message);
}


static void
tp_message_mixin_send_async (TpSvcChannelTypeText *iface,
                             guint message_type,
                             const gchar *text,
                             DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  TpMessage *message;

  if (mixin->priv->send_message == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  message = tp_message_new (mixin->priv->connection, 2, 2);

  if (message_type != 0)
    tp_message_set_uint32 (message, 0, "message-type", message_type);

  tp_message_set_string (message, 1, "content-type", "text/plain");
  tp_message_set_string (message, 1, "type", "text/plain"); /* Removed in 0.17.14 */
  tp_message_set_string (message, 1, "content", text);

  message->outgoing_context = context;
  message->outgoing_text_api = TRUE;

  mixin->priv->send_message ((GObject *) iface, message, 0);
}


static void
tp_message_mixin_send_message_async (TpSvcChannelInterfaceMessages *iface,
                                     const GPtrArray *parts,
                                     guint flags,
                                     DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  TpMessage *message;
  GHashTable *header;
  guint i;
  const char * const *iter;

  if (mixin->priv->send_message == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  /* it must have at least a header part */
  if (parts->len < 1)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Cannot send a message that does not have at least one part" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  header = g_ptr_array_index (parts, 0);

  for (i = 0; i < parts->len; i++)
    {
      for (iter = forbidden_keys; *iter != NULL; iter++)
        {
          if (g_hash_table_lookup (header, *iter) != NULL)
            {
              GError *error = g_error_new (TP_ERRORS,
                  TP_ERROR_INVALID_ARGUMENT,
                  "Key '%s' not allowed in a sent message", *iter);

              dbus_g_method_return_error (context, error);
              return;
            }
        }
    }

  for (iter = body_only; *iter != NULL; iter++)
    {
      if (g_hash_table_lookup (header, *iter) != NULL)
        {
          GError *error = g_error_new (TP_ERRORS,
              TP_ERROR_INVALID_ARGUMENT,
              "Key '%s' not allowed in a message header", *iter);

          dbus_g_method_return_error (context, error);
          return;
        }
    }

  for (iter = headers_only_incoming; *iter != NULL; iter++)
    {
      if (g_hash_table_lookup (header, *iter) != NULL)
        {
          GError *error = g_error_new (TP_ERRORS,
              TP_ERROR_INVALID_ARGUMENT,
              "Key '%s' not allowed in an outgoing message header", *iter);

          dbus_g_method_return_error (context, error);
          return;
        }
    }

  for (i = 1; i < parts->len; i++)
    {
      for (iter = headers_only; *iter != NULL; iter++)
        {
          if (g_hash_table_lookup (g_ptr_array_index (parts, i), *iter)
              != NULL)
            {
              GError *error = g_error_new (TP_ERRORS,
                  TP_ERROR_INVALID_ARGUMENT,
                  "Key '%s' not allowed in a message body", *iter);

              dbus_g_method_return_error (context, error);
              return;
            }
        }
    }

  message = tp_message_new (mixin->priv->connection, parts->len, parts->len);

  for (i = 0; i < parts->len; i++)
    {
      tp_g_hash_table_update (g_ptr_array_index (message->parts, i),
          g_ptr_array_index (parts, i),
          (GBoxedCopyFunc) g_strdup,
          (GBoxedCopyFunc) tp_g_value_slice_dup);
    }

  message->outgoing_context = context;
  message->outgoing_text_api = FALSE;

  mixin->priv->send_message ((GObject *) iface, message, flags);
}


/**
 * tp_message_mixin_init_dbus_properties:
 * @cls: The class of an object with this mixin
 *
 * Set up a #TpDBusPropertiesMixinClass to use this mixin's implementation
 * of the Messages interface's properties.
 *
 * This uses tp_message_mixin_get_dbus_property() as the property getter
 * and sets a list of the supported properties for it.
 */
void
tp_message_mixin_init_dbus_properties (GObjectClass *cls)
{
  static TpDBusPropertiesMixinPropImpl props[] = {
      { "PendingMessages", NULL, NULL },
      { "SupportedContentTypes", NULL, NULL },
      { "MessagePartSupportFlags", NULL, NULL },
      { NULL }
  };

  tp_dbus_properties_mixin_implement_interface (cls,
      TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES,
      tp_message_mixin_get_dbus_property, NULL, props);
}


/**
 * tp_message_mixin_get_dbus_property:
 * @object: An object with this mixin
 * @interface: Must be %TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES
 * @name: A quark representing the D-Bus property name, either
 *  "PendingMessages", "SupportedContentTypes" or "MessagePartSupportFlags"
 * @value: A GValue pre-initialized to the right type, into which to put
 *  the value
 * @unused: Ignored
 *
 * An implementation of #TpDBusPropertiesMixinGetter which assumes that
 * the @object has the messages mixin. It can only be used for the Messages
 * interface.
 */
void
tp_message_mixin_get_dbus_property (GObject *object,
                                    GQuark interface,
                                    GQuark name,
                                    GValue *value,
                                    gpointer unused G_GNUC_UNUSED)
{
  TpMessageMixin *mixin;
  static GQuark q_pending_messages = 0;
  static GQuark q_supported_content_types = 0;
  static GQuark q_message_part_support_flags = 0;
  static GQuark q_delivery_reporting_support_flags = 0;

  if (G_UNLIKELY (q_pending_messages == 0))
    {
      q_pending_messages = g_quark_from_static_string ("PendingMessages");
      q_supported_content_types =
          g_quark_from_static_string ("SupportedContentTypes");
      q_message_part_support_flags =
          g_quark_from_static_string ("MessagePartSupportFlags");
      q_delivery_reporting_support_flags =
          g_quark_from_static_string ("DeliveryReportingSupportFlags");
    }

  mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (interface == TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES);
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != 0);
  g_return_if_fail (value != NULL);
  g_return_if_fail (mixin != NULL);

  if (name == q_pending_messages)
    {
      GPtrArray *arrays = g_ptr_array_sized_new (g_queue_get_length (
            mixin->priv->pending));
      GList *link;
      GType type = dbus_g_type_get_collection ("GPtrArray",
          TP_HASH_TYPE_MESSAGE_PART);

      for (link = g_queue_peek_head_link (mixin->priv->pending);
           link != NULL;
           link = g_list_next (link))
        {
          TpMessage *msg = link->data;

          g_ptr_array_add (arrays, g_boxed_copy (type, msg->parts));
        }

      g_value_take_boxed (value, arrays);
    }
  else if (name == q_message_part_support_flags)
    {
      g_value_set_uint (value, mixin->priv->message_part_support_flags);
    }
  else if (name == q_delivery_reporting_support_flags)
    {
      g_value_set_uint (value, mixin->priv->delivery_reporting_support_flags);
    }
  else if (name == q_supported_content_types)
    {
      g_value_set_boxed (value, mixin->priv->supported_content_types);
    }
}


/**
 * tp_message_mixin_text_iface_init:
 * @g_iface: A pointer to the #TpSvcChannelTypeTextClass in an object class
 * @iface_data: Ignored
 *
 * Fill in this mixin's Text method implementations in the given interface
 * vtable.
 *
 * @since 0.7.21
 */
void
tp_message_mixin_text_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  TpSvcChannelTypeTextClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_type_text_implement_##x (klass,\
    tp_message_mixin_##x##_async)
  IMPLEMENT (acknowledge_pending_messages);
  IMPLEMENT (get_message_types);
  IMPLEMENT (list_pending_messages);
  IMPLEMENT (send);
#undef IMPLEMENT
}

/**
 * tp_message_mixin_messages_iface_init:
 * @g_iface: A pointer to the #TpSvcChannelInterfaceMessagesClass in an object
 *  class
 * @iface_data: Ignored
 *
 * Fill in this mixin's Messages method implementations in the given interface
 * vtable.
 *
 * @since 0.7.21
 */
void
tp_message_mixin_messages_iface_init (gpointer g_iface,
                                      gpointer iface_data)
{
  TpSvcChannelInterfaceMessagesClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_messages_implement_##x (\
    klass, tp_message_mixin_##x##_async)
  IMPLEMENT (send_message);
  IMPLEMENT (get_pending_message_content);
#undef IMPLEMENT
}
