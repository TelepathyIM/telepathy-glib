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
  TpHandleRepoIface *contact_repo;
  guint recv_id;
  gboolean message_lost;

  GQueue *pending;

  GArray *msg_types;
};

/**
 * tp_message_mixin_class_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObjectClass
 */
GQuark
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
GQuark
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


typedef enum {
    PENDING_TEXT_MESSAGE,       /* A message with text/plain content */
    PENDING_NON_TEXT_MESSAGE,   /* A message with no text/plain content */
    PENDING_DELIVERY_REPORT     /* A delivery report */
} PendingType;


typedef struct {
    guint32 id;
    PendingType pending_type;
    TpHandle sender;                        /* 0 for delivery reports */
    time_t timestamp;                       /* 0 for delivery reports */
    TpChannelTextMessageType message_type;  /* 0 for delivery reports */
    GPtrArray *content;                     /* has exactly one item for d.r. */
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

  g_slice_free (PendingItem, pending);
}


#if 0
static PendingItem *
pending_item_new (void)
{
  PendingItem *pending = g_slice_new0 (PendingItem);

  pending->content = g_ptr_array_sized_new (1);
  return pending;
}
#endif


static TpChannelTextMessageFlags
pending_item_get_text (PendingItem *item,
                       GString *buffer)
{
  guint i;
  TpChannelTextMessageFlags flags = 0;

  g_return_val_if_fail (item->pending_type != PENDING_TEXT_MESSAGE, 0);
  g_return_val_if_fail (item->content != NULL, 0);

  for (i = 0; i < item->content->len; i++)
    {
      GHashTable *part = g_ptr_array_index (item->content, i);

      if (!tp_strdiff (g_hash_table_lookup (part, "type"), "text/plain"))
        {
          GValue *value;
          /* FIXME: strictly, we should skip all but the first in any set of
           * alternatives... in practice, this is only likely to affect XMPP,
           * where this will result in getting *all* the languages for a
           * multilingual message, rather than just one of them */

          value = g_hash_table_lookup (part, "content");

          if (value != NULL && G_VALUE_HOLDS_STRING (value))
            {
              g_string_append (buffer, g_value_get_string (value));

              value = g_hash_table_lookup (part, "truncated");

              if (value != NULL && G_VALUE_HOLDS_BOOLEAN (value) &&
                  g_value_get_boolean (value))
                flags |= TP_CHANNEL_TEXT_MESSAGE_FLAG_TRUNCATED;
            }
        }
    }

  return flags;
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

  mixin->priv->message_lost = FALSE;
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

  for (i = 0; i < ids->len; i++)
    {
      item = nodes[i]->data;

      DEBUG ("acknowledging message id %u", item->id);

      g_queue_remove (mixin->priv->pending, item);

      /* FIXME: need a hook here to send acknowledgements out on the network
       * if the protocol requires it */

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

      if (msg->pending_type != PENDING_TEXT_MESSAGE)
        {
          /* No text/plain part */
          continue;
        }

      text = g_string_new ("");
      flags = pending_item_get_text (msg, text);

      g_value_init (&val, pending_type);
      g_value_take_boxed (&val,
          dbus_g_type_specialized_construct (pending_type));
      dbus_g_type_struct_set (&val,
          0, msg->id,
          1, msg->timestamp,
          2, msg->sender,
          3, msg->message_type,
          4, flags,
          5, text,
          G_MAXUINT);

      g_string_free (text, TRUE);

      g_ptr_array_add (messages, g_value_get_boxed (&val));
    }

  if (clear)
    {
      DEBUG ("WARNING: ListPendingMessages(clear=TRUE) is deprecated");
      cur = g_queue_peek_head_link (mixin->priv->pending);

      while (cur != NULL)
        {
          PendingItem *msg = cur->data;
          GList *next = cur->next;

          if (msg->pending_type == PENDING_TEXT_MESSAGE)
            {
              /* FIXME: need a hook here to send acknowledgements out on the
               * network if the protocol requires it */

              g_queue_delete_link (mixin->priv->pending, cur);
              pending_item_free (msg, mixin->priv->contact_repo);
            }

          cur = next;
        }
    }

  tp_svc_channel_type_text_return_from_list_pending_messages (context,
      messages);

  for (i = 0; i < messages->len; i++)
    g_value_array_free (g_ptr_array_index (messages, i));

  g_ptr_array_free (messages, TRUE);
}

static void
tp_message_mixin_get_pending_message_content_async (
    TpSvcChannelInterfaceMessageParts *iface,
    guint message_id,
    const GArray *part_numbers,
    DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED, "Not implemented" };

  dbus_g_method_return_error (context, &e);
}

static void
tp_message_mixin_get_message_types_async (TpSvcChannelTypeText *iface,
                                          DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED, "Not implemented" };

  dbus_g_method_return_error (context, &e);
}

static void
tp_message_mixin_send_async (TpSvcChannelTypeText *iface,
                             guint message_type,
                             const gchar *text,
                             DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED, "Not implemented" };

  dbus_g_method_return_error (context, &e);
}

static void
tp_message_mixin_send_message_async (TpSvcChannelInterfaceMessageParts *iface,
                                     guint message_type,
                                     const GPtrArray *parts,
                                     guint flags,
                                     DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED, "Not implemented" };

  dbus_g_method_return_error (context, &e);
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
tp_message_mixin_message_parts_iface_init (gpointer g_iface,
                                           gpointer iface_data)
{
  TpSvcChannelInterfaceMessagePartsClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_message_parts_implement_##x (\
    klass, tp_message_mixin_##x##_async)
  IMPLEMENT (send_message);
  IMPLEMENT (get_pending_message_content);
#undef IMPLEMENT
}
