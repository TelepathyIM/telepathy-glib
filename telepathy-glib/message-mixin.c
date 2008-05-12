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
 *  Messages mixin
 * @see_also: #TpSvcChannelTypeText, #TpSvcChannelInterfaceMessages
 *
 * This mixin can be added to a channel GObject class to implement the
 * text channel type (with the Messages interface) in a general way.
 *
 * To use the messages mixin, include a #TpMessageMixin somewhere in your
 * instance structure, and call tp_message_mixin_init() from your init
 * function or constructor, and tp_message_mixin_finalize() from your dispose
 * or finalize function.
 *
 * Also pass tp_message_mixin_text_iface_init() and
 * tp_message_mixin_messages_iface_init() to G_IMPLEMENT_INTERFACE().
 *
 * To support sending messages, you must call
 * tp_message_mixin_implement_sending(). If you do not, any attempt to send a
 * message will fail with NotImplemented.
 *
 * @since 0.7.9
 */

#include <telepathy-glib/message-mixin.h>

#include <dbus/dbus-glib.h>
#include <string.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>

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
 * @since 0.7.9
 */

struct _TpMessageMixinPrivate
{
  /* Sending */
  TpMessageMixinSendImpl send_message;
  GArray *msg_types;

  /* Receiving */
  TpMessageMixinCleanUpReceivedImpl clean_up_received;
  guint recv_id;
  GQueue *pending;
};


static const char * const forbidden_keys[] = {
    "pending-message-id",
    NULL
};


static const char * const body_only[] = {
    "alternative",
    "type",
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
 * tp_message_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
 *
 * @since 0.7.9
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


typedef struct {
    guint32 id;
    GPtrArray *parts;
    gpointer cleanup_data;
    /* We replace the header (the first part) with a copy, so we can safely
     * add to it. This means we need to hang on to the original header, so we
     * can pass it back to the caller. */
    GHashTable *orig_header;

    /* For the Text API */
    guint old_timestamp;
    TpHandle old_sender;    /* borrowed from header, so not reffed */
    TpChannelTextMessageType old_message_type;
    TpChannelTextMessageFlags old_flags;
    gchar *old_text;

    /* GValues that need freeing later */
    GValueArray *added_values;

    /* A non-NULL reference until we have been queued; borrowed afterwards */
    GObject *target;
} PendingItem;


static gint
pending_item_id_equals_data (gconstpointer item,
                             gconstpointer data)
{
  const PendingItem *self = item;
  guint id = GPOINTER_TO_UINT (data);

  return (self->id != id);
}


static void
pending_item_free (PendingItem *pending,
                   GObject *obj,
                   TpMessageMixinCleanUpReceivedImpl clean_up_received)
{
  g_assert (clean_up_received != NULL);
  g_assert (pending->parts->len >= 1);

  g_hash_table_destroy (g_ptr_array_index (pending->parts, 0));
  g_ptr_array_index (pending->parts, 0) = pending->orig_header;
  pending->orig_header = NULL;

  clean_up_received (obj, pending->parts, pending->cleanup_data);

  g_free (pending->old_text);
  pending->old_text = NULL;

  g_value_array_free (pending->added_values);
  pending->added_values = NULL;

  g_slice_free (PendingItem, pending);
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


static TpChannelTextMessageFlags
parts_to_text (const GPtrArray *parts,
               GString *buffer,
               TpChannelTextMessageType *out_type,
               TpHandle *out_sender,
               guint *out_timestamp)
{
  guint i;
  TpChannelTextMessageFlags flags = 0;
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

  /* If the message is on an extended interface or only contains headers,
   * definitely set the "your client is too old" flag. */
  if (parts->len <= 1 || g_hash_table_lookup (header, "interface") != NULL)
    {
      flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT;
    }

  for (i = 1; i < parts->len; i++)
    {
      GHashTable *part = g_ptr_array_index (parts, i);
      const gchar *type = tp_asv_get_string (part, "type");
      const gchar *alternative = tp_asv_get_string (part, "alternative");

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

  return flags;
}


/**
 * tp_message_mixin_implement_sending:
 * @obj: An instance of the implementation that uses this mixin
 * @send: An implementation of SendMessage()
 * @n_types: Number of supported message types
 * @types: @n_types supported message types
 *
 * Set the callback used to implement SendMessage, and the types of message
 * that can be sent
 *
 * @since 0.7.9
 */
void
tp_message_mixin_implement_sending (GObject *object,
                                    TpMessageMixinSendImpl send,
                                    guint n_types,
                                    const TpChannelTextMessageType *types)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (mixin->priv->send_message == NULL);
  mixin->priv->send_message = send;

  if (mixin->priv->msg_types->len > 0)
    g_array_remove_range (mixin->priv->msg_types, 0,
        mixin->priv->msg_types->len);

  g_assert (mixin->priv->msg_types->len == 0);
  g_array_append_vals (mixin->priv->msg_types, types, n_types);
}


/**
 * TpMessageMixinCleanUpReceivedImpl:
 * @object: An instance of the implementation that uses this mixin
 * @parts: An array of GHashTable containing a message
 *
 * Assume that @parts was passed to tp_message_mixin_take_received(),
 * clean up any allocations or handle references that would have occurred
 * when the message was received, and free the parts and the array.
 *
 * This typically means unreferencing any handles in the array of hash tables
 * (notably "message-sender"), destroying the hash tables, and freeing the
 * pointer array.
 *
 * (The #TpMessageMixin code can't do this automatically, because the
 * extensibility of the API means that the mixin doesn't
 * know which integers are handles and which are just numbers.)
 */


/**
 * tp_message_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpMessageMixin within the object structure
 * @contact_repo: The connection's %TP_HANDLE_TYPE_CONTACT repository
 * @clean_up_received:
 *
 * Initialize the mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_message_mixin_init ((GObject *) self,
 *     G_STRUCT_OFFSET (SomeObject, message_mixin),
 *     self->contact_repo,
 *     some_object_clean_up_received);
 * </programlisting></informalexample>
 *
 * @since 0.7.9
 */
void
tp_message_mixin_init (GObject *obj,
                       gsize offset,
                       TpMessageMixinCleanUpReceivedImpl clean_up_received)
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
  mixin->priv->clean_up_received = clean_up_received;
}

static void
tp_message_mixin_clear (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  PendingItem *item;

  while ((item = g_queue_pop_head (mixin->priv->pending)) != NULL)
    {
      pending_item_free (item, obj, mixin->priv->clean_up_received);
    }
}

/**
 * tp_message_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the text mixin.
 *
 * @since 0.7.9
 */
void
tp_message_mixin_finalize (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);

  DEBUG ("%p", obj);

  tp_message_mixin_clear (obj);
  g_queue_free (mixin->priv->pending);
  g_array_free (mixin->priv->msg_types, TRUE);

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
  PendingItem *item;
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

      DEBUG ("acknowledging message id %u", item->id);

      g_queue_remove (mixin->priv->pending, item);

      pending_item_free (item, (GObject *) iface,
          mixin->priv->clean_up_received);
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
      PendingItem *msg = cur->data;
      GValue val = { 0, };
      guint flags;
      GString *text;
      TpChannelTextMessageType type;
      TpHandle sender;
      guint timestamp;

      text = g_string_new ("");
      flags = parts_to_text (msg->parts, text, &type, &sender, &timestamp);

      g_value_init (&val, pending_type);
      g_value_take_boxed (&val,
          dbus_g_type_specialized_construct (pending_type));
      dbus_g_type_struct_set (&val,
          0, msg->id,
          1, timestamp,
          2, sender,
          3, type,
          4, flags,
          5, text->str,
          G_MAXUINT);

      g_string_free (text, TRUE);

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
          PendingItem *msg = cur->data;
          GList *next = cur->next;

          i = msg->id;
          g_array_append_val (ids, i);
          g_queue_delete_link (mixin->priv->pending, cur);
          pending_item_free (msg, (GObject *) iface,
              mixin->priv->clean_up_received);

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
  PendingItem *item;
  GHashTable *ret;
  guint i;
  GValue empty = { 0 };

  g_value_init (&empty, G_TYPE_STRING);
  g_value_set_static_string (&empty, "");

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

      if (part >= item->parts->len)
        {
          GError *error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "part number %u out of range", part);

          DEBUG ("%s", error->message);
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
        }
    }

  ret = g_hash_table_new (NULL, NULL);

  for (i = 0; i < part_numbers->len; i++)
    {
      guint part = g_array_index (part_numbers, guint, i);
      GHashTable *part_data;
      GValue *content;

      g_assert (part < item->parts->len);
      part_data = g_ptr_array_index (item->parts, part);

      content = g_hash_table_lookup (part_data, "content");

      if (content == NULL)
        content = &empty;

      g_hash_table_insert (ret, GUINT_TO_POINTER (part), content);
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
  PendingItem *pending = data;
  GObject *object = pending->target;
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_queue_push_tail (mixin->priv->pending, pending);

  tp_svc_channel_type_text_emit_received (object, pending->id,
      pending->old_timestamp, pending->old_sender, pending->old_message_type,
      pending->old_flags, pending->old_text);

  tp_svc_channel_interface_messages_emit_message_received (object,
      pending->parts);

  g_object_unref (object);

  return FALSE;
}


/**
 * tp_message_mixin_take_received:
 * @object: a channel with this mixin
 * @parts: the content of the message, which is stolen by the message mixin and
 *  must not be modified or freed until the
 *  #TpMessageMixinCleanUpReceivedImpl is called. This is a non-empty array of
 *  GHashTables, the first of which contains headers (which must not include
 *  "pending-message-id").
 * @user_data: implementation-specific data which will be passed to the
 *  #TpMessageMixinCleanUpReceivedImpl
 *
 * Receive a message into the pending messages queue, where it will stay
 * until acknowledged, and emit the ReceivedMessage signal. Also emit the
 * Received signal if appropriate.
 *
 * It is an error to call this method if a %NULL
 * #TpMessageMixinCleanUpReceivedImpl was passed to tp_message_mixin_init().
 *
 * Returns: the message ID
 *
 * @since 0.7.9
 */
guint
tp_message_mixin_take_received (GObject *object,
                                GPtrArray *parts,
                                gpointer user_data)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  PendingItem *pending;
  GString *text;
  GHashTable *header;
  GValue v = { 0 };

  g_return_val_if_fail (mixin->priv->clean_up_received != NULL, 0);
  g_return_val_if_fail (parts->len >= 1, 0);

  header = g_ptr_array_index (parts, 0);

  g_return_val_if_fail (g_hash_table_lookup (header, "pending-message-id")
      == NULL, 0);

  pending = g_slice_new0 (PendingItem);
  /* FIXME: we don't check for overflow, so in highly pathological cases we
   * might end up with multiple messages with the same ID */
  pending->id = mixin->priv->recv_id++;
  pending->parts = parts;
  pending->cleanup_data = user_data;

  /* We replace the header (the first part) with a copy, so we can safely
   * add to it. This means we need to hang on to the original header, so we
   * can pass it back to the caller for destruction */
  pending->orig_header = header;
  /* The keys are either static strings in this file, or borrowed from
   * the orig_header - in either case, there's no need to free them. The
   * values are either borrowed from the orig_header, or from the
   * PendingItem's added_values array. */
  header = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  tp_g_hash_table_update (header, pending->orig_header, NULL, NULL);
  g_ptr_array_index (parts, 0) = header;

  /* Now that header is fully under our control, we can fill in any missing
   * keys */
  pending->added_values = g_value_array_new (1);
  g_value_init (&v, G_TYPE_UINT);
  g_value_array_append (pending->added_values, &v);
  g_value_unset (&v);
  g_value_set_uint (pending->added_values->values + 0, pending->id);
  g_hash_table_insert (header, "pending-message-id",
      pending->added_values->values + 0);

  /* Provide a message-received header if necessary - it's easy */
  if (tp_asv_get_uint32 (header, "message-received", NULL) == 0)
    {
      guint i = pending->added_values->n_values;

      g_value_init (&v, G_TYPE_UINT);
      g_value_array_append (pending->added_values, &v);
      g_value_unset (&v);
      g_value_set_uint (pending->added_values->values + i, time (NULL));
      g_hash_table_insert (header, "message-received",
          pending->added_values->values + i);
    }

  text = g_string_new ("");
  pending->old_flags = parts_to_text (parts, text,
      &(pending->old_message_type), &(pending->old_sender),
      &(pending->old_timestamp));

  DEBUG ("%p: time %u, sender %u, type %u, %u parts",
      object, (guint) (pending->old_timestamp), pending->old_sender,
      pending->old_message_type, parts->len);

  pending->old_text = g_string_free (text, FALSE);

  /* We don't actually add the pending message to the queue immediately,
   * to guarantee that the caller of this function gets to see the message ID
   * before anyone else does (so that it can acknowledge the message to the
   * network). */
  pending->target = g_object_ref (object);
  g_idle_add (queue_pending, pending);

  return pending->id;
}


struct _TpMessageMixinOutgoingMessagePrivate {
    DBusGMethodInvocation *context;
    gboolean messages:1;
};


void
tp_message_mixin_sent (GObject *object,
                       TpMessageMixinOutgoingMessage *message,
                       const gchar *token,
                       const GError *error)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  time_t now = time (NULL);
  GString *string;
  guint i;
  TpChannelTextMessageType message_type;

  g_return_if_fail (mixin != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (message != NULL);
  g_return_if_fail (message->parts != NULL);
  g_return_if_fail (message->priv != NULL);
  g_return_if_fail (message->priv->context != NULL);
  g_return_if_fail (token == NULL || error == NULL);
  g_return_if_fail (token != NULL || error != NULL);

  if (error != NULL)
    {
      GError *e = g_error_copy (error);

      dbus_g_method_return_error (message->priv->context, e);
      g_error_free (e);
    }
  else
    {
      if (token == NULL)
        token = "";

      /* emit Sent and MessageSent */

      tp_svc_channel_interface_messages_emit_message_sent (object,
          message->parts, token);
      string = g_string_new ("");
      parts_to_text (message->parts, string, &message_type, NULL, NULL);
      tp_svc_channel_type_text_emit_sent (object, now, message_type,
          string->str);
      g_string_free (string, TRUE);

      /* return successfully */

      if (message->priv->messages)
        {
          tp_svc_channel_interface_messages_return_from_send_message (
              message->priv->context, token);
        }
      else
        {
          tp_svc_channel_type_text_return_from_send (
              message->priv->context);
        }
    }

  message->priv->context = NULL;
  memset (message->priv, '\xee', sizeof (message->priv));
  g_slice_free (TpMessageMixinOutgoingMessagePrivate, message->priv);

  for (i = 0; i < message->parts->len; i++)
    g_hash_table_destroy (g_ptr_array_index (message->parts, i));

  g_ptr_array_free (message->parts, TRUE);
  /* poison message to make sure nobody dereferences it */
  memset (message, '\xee', sizeof (message));
  g_slice_free (TpMessageMixinOutgoingMessage, message);
}


static void
tp_message_mixin_send_async (TpSvcChannelTypeText *iface,
                             guint message_type,
                             const gchar *text,
                             DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  GPtrArray *parts;
  GHashTable *table;
  GValue *value;
  TpMessageMixinOutgoingMessage *message;

  if (mixin->priv->send_message == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  parts = g_ptr_array_sized_new (2);

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, message_type);
  g_hash_table_insert (table, "message-type", value);

  g_ptr_array_add (parts, table);

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);

  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, text);
  g_hash_table_insert (table, "content", value);

  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_static_string (value, "text/plain");
  g_hash_table_insert (table, "type", value);

  g_ptr_array_add (parts, table);

  message = g_slice_new0 (TpMessageMixinOutgoingMessage);
  message->flags = 0;
  message->parts = parts;
  message->priv = g_slice_new0 (TpMessageMixinOutgoingMessagePrivate);
  message->priv->context = context;
  message->priv->messages = FALSE;

  mixin->priv->send_message ((GObject *) iface, message);
}


static void
tp_message_mixin_send_message_async (TpSvcChannelInterfaceMessages *iface,
                                     const GPtrArray *parts,
                                     guint flags,
                                     DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  TpMessageMixinOutgoingMessage *message;
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

  message = g_slice_new0 (TpMessageMixinOutgoingMessage);
  message->flags = flags;

  message->parts = g_ptr_array_sized_new (parts->len);

  for (i = 0; i < parts->len; i++)
    {
      g_ptr_array_add (message->parts,
          g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
            (GDestroyNotify) tp_g_value_slice_free));
      tp_g_hash_table_update (g_ptr_array_index (message->parts, i),
          g_ptr_array_index (parts, i),
          (GBoxedCopyFunc) g_strdup,
          (GBoxedCopyFunc) tp_g_value_slice_dup);
    }

  message->priv = g_slice_new0 (TpMessageMixinOutgoingMessagePrivate);
  message->priv->context = context;
  message->priv->messages = TRUE;

  mixin->priv->send_message ((GObject *) iface, message);
}


/**
 * tp_message_mixin_iface_init:
 * @g_iface: A pointer to the #TpSvcChannelTypeTextClass in an object class
 * @iface_data: Ignored
 *
 * Fill in this mixin's AcknowledgePendingMessages, GetMessageTypes and
 * ListPendingMessages implementations in the given interface vtable.
 * In addition to calling this function during interface initialization, the
 * implementor is expected to call tp_svc_channel_type_text_implement_send(),
 * providing a Send implementation.
 *
 * @since 0.7.9
 */
void
tp_message_mixin_text_iface_init (gpointer g_iface, gpointer iface_data)
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
