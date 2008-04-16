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
 * @short_description: a mixin implementation of the text channel type
 * @see_also: #TpSvcChannelTypeText
 *
 * This mixin can be added to a channel GObject class to implement the
 * text channel type in a general way. It implements the pending message
 * queue and GetMessageTypes, so the implementation should only need to
 * implement Send.
 *
 * To use the text mixin, include a #TpMessageMixinClass somewhere in your
 * class structure and a #TpMessageMixin somewhere in your instance structure,
 * and call tp_message_mixin_class_init() from your class_init function,
 * tp_message_mixin_init() from your init function or constructor, and
 * tp_message_mixin_finalize() from your dispose or finalize function.
 *
 * To use the text mixin as the implementation of
 * #TpSvcTextInterface, in the function you pass to G_IMPLEMENT_INTERFACE,
 * you should first call tp_message_mixin_iface_init(), then call
 * tp_svc_channel_type_text_implement_send() to register your implementation
 * of the Send method.
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
 * TpMessageMixinClass:
 *
 * Structure to be included in the class structure of objects that
 * use this mixin. Initialize it with tp_message_mixin_class_init().
 *
 * There are no public fields.
 */

/**
 * TpMessageMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_message_mixin_init().
 *
 * There are no public fields.
 */

struct _TpMessageMixinPrivate
{
  TpMessageMixinSendImpl send_message;

  TpHandleRepoIface *contact_repo;
  guint recv_id;

  GQueue *pending;

  GArray *msg_types;
};

#define TP_MESSAGE_MIXIN_CLASS_OFFSET_QUARK \
  (tp_message_mixin_class_get_offset_quark ())
#define TP_MESSAGE_MIXIN_CLASS_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_CLASS_TYPE (o), \
                                       TP_MESSAGE_MIXIN_CLASS_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN_CLASS(o) \
  ((TpMessageMixinClass *) tp_mixin_offset_cast (o, \
    TP_MESSAGE_MIXIN_CLASS_OFFSET (o)))

#define TP_MESSAGE_MIXIN_OFFSET_QUARK (tp_message_mixin_get_offset_quark ())
#define TP_MESSAGE_MIXIN_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_TYPE (o), \
                                       TP_MESSAGE_MIXIN_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN(o) \
  ((TpMessageMixin *) tp_mixin_offset_cast (o, TP_MESSAGE_MIXIN_OFFSET (o)))

/**
 * tp_message_mixin_class_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObjectClass
 */
static GQuark
tp_message_mixin_class_get_offset_quark (void)
{
  static GQuark offset_quark = 0;

  if (G_UNLIKELY (offset_quark == 0))
    offset_quark = g_quark_from_static_string (
        "tp_message_mixin_class_get_offset_quark@0.7.7");

  return offset_quark;
}

/**
 * tp_message_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
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


/**
 * tp_message_mixin_class_init:
 * @obj_cls: The class of the implementation that uses this mixin
 * @offset: The byte offset of the TpMessageMixinClass within the class
 *  structure
 *
 * Initialize the mixin. Should be called from the implementation's
 * class_init function like so:
 *
 * <informalexample><programlisting>
 * tp_message_mixin_class_init ((GObjectClass *) klass,
 *     G_STRUCT_OFFSET (SomeObjectClass, message_mixin));
 * </programlisting></informalexample>
 */

void
tp_message_mixin_class_init (GObjectClass *obj_cls,
                             gsize offset)
{
  TpMessageMixinClass *mixin_cls;

  g_assert (G_IS_OBJECT_CLASS (obj_cls));

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (obj_cls),
      TP_MESSAGE_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  mixin_cls = TP_MESSAGE_MIXIN_CLASS (obj_cls);
}


typedef struct {
    guint32 id;
    TpHandle sender;
    time_t timestamp;
    TpChannelTextMessageType message_type;
    GPtrArray *content;
    TpChannelTextMessageFlags old_flags;
    gchar *old_text;
    /* A non-NULL reference until we have been queued, NULL afterwards */
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
                   TpHandleRepoIface *contact_repo)
{
  guint i;

  if (pending->sender != 0)
    tp_handle_unref (contact_repo, pending->sender);

  if (pending->content != NULL)
    {
      for (i = 0; i < pending->content->len; i++)
        {
          g_hash_table_destroy (g_ptr_array_index (pending->content, i));
        }

      g_ptr_array_free (pending->content, TRUE);
    }

  g_free (pending->old_text);

  g_slice_free (PendingItem, pending);
}


static inline const gchar *
value_force_string (const GValue *value)
{
  if (value == NULL || !G_VALUE_HOLDS_STRING (value))
    return NULL;

  return g_value_get_string (value);
}


static inline void
nullify_hash (GHashTable **hash)
{
  if (*hash != NULL)
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
               GString *buffer)
{
  guint i;
  TpChannelTextMessageFlags flags = 0;
  /* Lazily created hash tables, used as a sets: keys are borrowed
   * "alternative" string values from @parts, value == key. */
  /* Alternative IDs for which we have already extracted an alternative */
  GHashTable *alternatives_used = NULL;
  /* Alternative IDs for which we expect to extract text, but have not yet;
   * cleared if the flag Channel_Text_Message_Flag_Non_Text_Content is set.
   * At the end, if this contains any item not in alternatives_used,
   * Channel_Text_Message_Flag_Non_Text_Content must be set. */
  GHashTable *alternatives_needed = NULL;

  for (i = 0; i < parts->len; i++)
    {
      GHashTable *part = g_ptr_array_index (parts, i);
      const gchar *type = value_force_string (g_hash_table_lookup (part,
            "type"));
      const gchar *alternative = value_force_string (g_hash_table_lookup (part,
            "alternative"));

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

  return flags;
}


/**
 * tp_message_mixin_implement_sending:
 * @obj: An instance of the implementation that uses this mixin
 * @send: An implementation of SendMessage()
 *
 */
void
tp_message_mixin_implement_sending (GObject *object,
                                    TpMessageMixinSendImpl send)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (mixin->priv->send_message == NULL);
  mixin->priv->send_message = send;
}


/**
 * tp_message_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpMessageMixin within the object structure
 * @contact_repo: The connection's %TP_HANDLE_TYPE_CONTACT repository
 *
 * Initialize the mixin. Should be called from the implementation's
 * instance init function like so:
 *
 * <informalexample><programlisting>
 * tp_message_mixin_init ((GObject *) self,
 *     G_STRUCT_OFFSET (SomeObject, message_mixin),
 *     self->contact_repo);
 * </programlisting></informalexample>
 */
void
tp_message_mixin_init (GObject *obj,
                       gsize offset,
                       TpHandleRepoIface *contact_repo)
{
  TpMessageMixin *mixin;

  g_assert (G_IS_OBJECT (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_MESSAGE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  mixin = TP_MESSAGE_MIXIN (obj);

  mixin->priv = g_slice_new0 (TpMessageMixinPrivate);

  mixin->priv->pending = g_queue_new ();
  mixin->priv->contact_repo = contact_repo;
  mixin->priv->recv_id = 0;
  mixin->priv->msg_types = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      NUM_TP_CHANNEL_TEXT_MESSAGE_TYPES);
}

static void
tp_message_mixin_clear (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  PendingItem *item;

  while ((item = g_queue_pop_head (mixin->priv->pending)) != NULL)
    {
      pending_item_free (item, mixin->priv->contact_repo);
    }
}

/**
 * tp_message_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the text mixin.
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

      pending_item_free (item, mixin->priv->contact_repo);
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

      text = g_string_new ("");
      flags = parts_to_text (msg->content, text);

      g_value_init (&val, pending_type);
      g_value_take_boxed (&val,
          dbus_g_type_specialized_construct (pending_type));
      dbus_g_type_struct_set (&val,
          0, msg->id,
          1, msg->timestamp,
          2, msg->sender,
          3, msg->message_type,
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
          pending_item_free (msg, mixin->priv->contact_repo);

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

      if (part >= item->content->len)
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

      g_assert (part < item->content->len);
      part_data = g_ptr_array_index (item->content, part);

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

  pending->target = NULL;

  g_queue_push_tail (mixin->priv->pending, pending);

  tp_svc_channel_type_text_emit_received (object, pending->id,
      pending->timestamp, pending->sender, pending->message_type,
      pending->old_flags, pending->old_text);

  tp_svc_channel_interface_messages_emit_message_received (object,
      pending->id, pending->timestamp, pending->sender, pending->message_type,
      pending->content);

  g_object_unref (object);

  return FALSE;
}


/**
 * tp_message_mixin_take_received:
 * @object: a channel with this mixin
 * @timestamp: the time the message was received (if 0, time (NULL) will be
 *  used)
 * @sender: an owned reference to the handle of the sender, which is stolen
 *  by the message mixin
 * @message_type: the type of message
 * @content: the content of the message, which is stolen by the message mixin
 *
 * Receive a message into the pending messages queue, where it will stay
 * until acknowledged, and emit the ReceivedMessage signal. Also emit the
 * Received signal if appropriate.
 *
 * Note that the sender and content arguments are *not* copied, and the caller
 * loses ownership of them (this is to avoid lengthy and unnecessary copying
 * of the content).
 *
 * Returns: the message ID
 */
guint
tp_message_mixin_take_received (GObject *object,
                                time_t timestamp,
                                TpHandle sender,
                                TpChannelTextMessageType message_type,
                                GPtrArray *content)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  PendingItem *pending = g_slice_new0 (PendingItem);
  GString *text;

  if (timestamp == 0)
    timestamp = time (NULL);

  DEBUG ("%p: time %u, sender %u, type %u, %u parts",
      object, (guint) timestamp, sender, message_type, content->len);

  /* FIXME: we don't check for overflow, so in highly pathological cases we
   * might end up with multiple messages with the same ID */
  pending->id = mixin->priv->recv_id++;
  pending->sender = sender;
  pending->timestamp = timestamp;
  pending->message_type = message_type;
  pending->content = content;
  text = g_string_new ("");
  pending->old_flags = parts_to_text (content, text);
  pending->old_text = g_string_free (text, FALSE);

  /* We don't actually add the pending message to the queue immediately,
   * to guarantee that the caller of this function gets to see the message ID
   * before anyone else does (so that it can acknowledge the message to the
   * network). */
  pending->target = g_object_ref (object);
  g_idle_add (queue_pending, pending);

  return pending->id;
}


#if 0
void
tp_message_mixin_take_delivery_report (GObject *object,
                                       GHashTable *report)
{
}
#endif


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
          message->message_type, message->parts, token);
      string = g_string_new ("");
      parts_to_text (message->parts, string);
      tp_svc_channel_type_text_emit_sent (object, now, message->message_type,
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

  parts = g_ptr_array_sized_new (1);
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
  message->message_type = message_type;
  message->parts = parts;
  message->priv = g_slice_new0 (TpMessageMixinOutgoingMessagePrivate);
  message->priv->context = context;
  message->priv->messages = FALSE;

  mixin->priv->send_message ((GObject *) iface, message);
}


static void
tp_message_mixin_send_message_async (TpSvcChannelInterfaceMessages *iface,
                                     guint message_type,
                                     const GPtrArray *parts,
                                     guint flags,
                                     DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  TpMessageMixinOutgoingMessage *message;

  if (mixin->priv->send_message == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  message = g_slice_new0 (TpMessageMixinOutgoingMessage);
  message->flags = flags;
  message->message_type = message_type;
  /* FIXME: fix codegen so we get a GType-generator for
   * TP_ARRAY_TYPE_MESSAGE_PART */
  message->parts = g_boxed_copy (dbus_g_type_get_collection ("GPtrArray",
        TP_HASH_TYPE_MESSAGE_PART), parts);
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
