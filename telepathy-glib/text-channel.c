/*
 * text-channel.h - high level API for Text channels
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:text-channel
 * @title: TpTextChannel
 * @short_description: proxy object for a text channel
 *
 * #TpTextChannel is a sub-class of #TpChannel providing convenient API
 * to send and receive #TpMessage.
 */

/**
 * TpTextChannel:
 *
 * Data structure representing a #TpTextChannel.
 *
 * Since: 0.13.10
 */

/**
 * TpTextChannelClass:
 *
 * The class of a #TpTextChannel.
 *
 * Since: 0.13.10
 */

#include <config.h>

#include "telepathy-glib/text-channel.h"

#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/message-internal.h>
#include <telepathy-glib/proxy-internal.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/signalled-message-internal.h>
#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#include "_gen/signals-marshal.h"

#include <stdio.h>
#include <glib/gstdio.h>

G_DEFINE_TYPE (TpTextChannel, tp_text_channel, TP_TYPE_CHANNEL)

struct _TpTextChannelPrivate
{
  GStrv supported_content_types;
  TpMessagePartSupportFlags message_part_support_flags;
  TpDeliveryReportingSupportFlags delivery_reporting_support;
  GArray *message_types;

  /* queue of owned TpSignalledMessage */
  GQueue *pending_messages;
  gboolean got_initial_messages;
};

enum
{
  PROP_SUPPORTED_CONTENT_TYPES = 1,
  PROP_MESSAGE_PART_SUPPORT_FLAGS,
  PROP_DELIVERY_REPORTING_SUPPORT,
  PROP_MESSAGE_TYPES,
};

enum /* signals */
{
  SIG_MESSAGE_RECEIVED,
  SIG_PENDING_MESSAGE_REMOVED,
  SIG_MESSAGE_SENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void
tp_text_channel_dispose (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;

  tp_clear_pointer (&self->priv->supported_content_types, g_strfreev);
  tp_clear_pointer (&self->priv->message_types, g_array_unref);

  g_queue_foreach (self->priv->pending_messages, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&self->priv->pending_messages, g_queue_free);

  G_OBJECT_CLASS (tp_text_channel_parent_class)->dispose (obj);
}

static void
tp_text_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTextChannel *self = (TpTextChannel *) object;

  switch (property_id)
    {
      case PROP_SUPPORTED_CONTENT_TYPES:
        g_value_set_boxed (value,
            tp_text_channel_get_supported_content_types (self));
        break;

      case PROP_MESSAGE_PART_SUPPORT_FLAGS:
        g_value_set_uint (value,
            tp_text_channel_get_message_part_support_flags (self));
        break;

      case PROP_DELIVERY_REPORTING_SUPPORT:
        g_value_set_uint (value,
            tp_text_channel_get_delivery_reporting_support (self));
        break;

      case PROP_MESSAGE_TYPES:
        g_value_set_boxed (value,
            tp_text_channel_get_message_types (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static TpHandle
get_sender (TpTextChannel *self,
    const GPtrArray *message,
    TpContact **contact,
    const gchar **out_sender_id)
{
  const GHashTable *header;
  TpHandle handle;
  const gchar *sender_id = NULL;
  TpConnection *conn;

  g_assert (contact != NULL);

  header = g_ptr_array_index (message, 0);
  handle = tp_asv_get_uint32 (header, "message-sender", NULL);
  if (handle == 0)
    {
      DEBUG ("Message received on Channel %s doesn't have message-sender",
          tp_proxy_get_object_path (self));

      *contact = NULL;
      goto out;
    }

  sender_id = tp_asv_get_string (header, "message-sender-id");

  conn = tp_channel_borrow_connection ((TpChannel *) self);
  *contact = tp_connection_dup_contact_if_possible (conn, handle, sender_id);

  if (*contact == NULL)
    {
      if (!tp_connection_has_immortal_handles (conn))
        DEBUG ("Connection %s don't have immortal handles, please fix CM",
            tp_proxy_get_object_path (conn));
      else if (tp_str_empty (sender_id))
        DEBUG ("Message received on %s doesn't include message-sender-id, "
            "please fix CM", tp_proxy_get_object_path (self));
    }

out:
  if (out_sender_id != NULL)
    *out_sender_id = sender_id;

  return handle;
}

static void
message_sent_cb (TpChannel *channel,
    const GPtrArray *content,
    guint flags,
    const gchar *token,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) channel;
  TpMessage *msg;
  TpContact *contact;

  get_sender (self, content, &contact, NULL);

  if (contact == NULL)
    {
      TpConnection *conn;

      conn = tp_channel_borrow_connection (channel);

      DEBUG ("Failed to get our self contact, please fix CM (%s)",
          tp_proxy_get_object_path (conn));

      /* Use the connection self contact as a fallback */
      contact = tp_connection_get_self_contact (conn);
      if (contact != NULL)
        g_object_ref (contact);
    }

  msg = _tp_signalled_message_new (content, contact);

  g_signal_emit (channel, signals[SIG_MESSAGE_SENT], 0, msg, flags,
      tp_str_empty (token) ? NULL : token);

  g_object_unref (msg);
  tp_clear_object (&contact);
}

static void
tp_text_channel_constructed (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_text_channel_parent_class)->constructed;
  TpChannel *chan = (TpChannel *) obj;
  GHashTable *props;
  gboolean valid;
  GError *err = NULL;

  if (chain_up != NULL)
    chain_up (obj);

  if (tp_channel_get_channel_type_id (chan) !=
      TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel is not of type Text" };

      DEBUG ("Channel %s is not of type Text: %s",
          tp_proxy_get_object_path (self), tp_channel_get_channel_type (chan));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

  if (!tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_MESSAGES))
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel does not implement the Messages interface" };

      DEBUG ("Channel %s does not implement the Messages interface",
          tp_proxy_get_object_path (self));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;

    }

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));

  self->priv->supported_content_types = (GStrv) tp_asv_get_strv (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_SUPPORTED_CONTENT_TYPES);
  if (self->priv->supported_content_types == NULL)
    {
      const gchar * const plain[] = { "text/plain", NULL };

      DEBUG ("Channel %s doesn't have Messages.SupportedContentTypes in its "
          "immutable properties", tp_proxy_get_object_path (self));

      /* spec mandates that plain text is always allowed. */
      self->priv->supported_content_types = g_strdupv ((GStrv) plain);
    }
  else
    {
      self->priv->supported_content_types = g_strdupv (
          self->priv->supported_content_types);
    }

  self->priv->message_part_support_flags = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_MESSAGE_PART_SUPPORT_FLAGS, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have Messages.MessagePartSupportFlags in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->delivery_reporting_support = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_DELIVERY_REPORTING_SUPPORT, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have Messages.DeliveryReportingSupport in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->message_types = tp_asv_get_boxed (props,
      TP_PROP_CHANNEL_INTERFACE_MESSAGES_MESSAGE_TYPES, DBUS_TYPE_G_UINT_ARRAY);
  if (self->priv->message_types != NULL)
    {
      self->priv->message_types = g_boxed_copy (DBUS_TYPE_G_UINT_ARRAY,
          self->priv->message_types);
    }
  else
    {
      self->priv->message_types = g_array_new (FALSE, FALSE,
          sizeof (TpChannelTextMessageType));

      DEBUG ("Channel %s doesn't have Messages.MessageTypes in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  tp_cli_channel_interface_messages_connect_to_message_sent (chan,
      message_sent_cb, NULL, NULL, NULL, &err);
  if (err != NULL)
    {
      WARNING ("Failed to connect to MessageSent on %s: %s",
          tp_proxy_get_object_path (self), err->message);
      g_error_free (err);
    }
}

static void
add_message_received (TpTextChannel *self,
    const GPtrArray *parts,
    TpContact *sender,
    gboolean fire_received)
{
  TpMessage *msg;

  msg = _tp_signalled_message_new (parts, sender);

  g_queue_push_tail (self->priv->pending_messages, msg);

  if (fire_received)
    g_signal_emit (self, signals[SIG_MESSAGE_RECEIVED], 0, msg);
}

static void
got_sender_contact_by_handle_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) weak_object;
  GPtrArray *parts = user_data;
  TpContact *sender = NULL;

  if (error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", error->message);
    }
  else if (n_failed > 0)
    {
      DEBUG ("Failed to prepare TpContact (InvalidHandle)");
    }
  else if (n_contacts > 0)
    {
      sender = contacts[0];
    }
  else
    {
      DEBUG ("TpContact of the sender hasn't been prepared");
    }

  add_message_received (self, parts, sender, TRUE);
  g_boxed_free (TP_ARRAY_TYPE_MESSAGE_PART_LIST, parts);
}

static void
got_sender_contact_by_id_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    const gchar * const *requested_ids,
    GHashTable *failed_id_errors,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) weak_object;
  GPtrArray *parts = user_data;
  TpContact *sender = NULL;

  if (error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", error->message);
    }
  else if (n_contacts > 0)
    {
      sender = contacts[0];
    }
  else
    {
      DEBUG ("TpContact of the sender hasn't be prepared");

      if (DEBUGGING)
        {
          GHashTableIter iter;
          gpointer key, value;

          g_hash_table_iter_init (&iter, failed_id_errors);
          while (g_hash_table_iter_next (&iter, &key, &value))
            {
              DEBUG ("Failed to get a TpContact for %s: %s",
                  (const gchar *) key, ((GError *) value)->message);
            }
        }
    }

  add_message_received (self, parts, sender, TRUE);
  g_boxed_free (TP_ARRAY_TYPE_MESSAGE_PART_LIST, parts);
}

static GPtrArray *
copy_parts (const GPtrArray *parts)
{
  return g_boxed_copy (TP_ARRAY_TYPE_MESSAGE_PART_LIST, parts);
}

static void
message_received_cb (TpChannel *proxy,
    const GPtrArray *message,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = user_data;
  TpHandle sender;
  TpConnection *conn;
  TpContact *contact;
  const gchar *sender_id;

  /* If we are still retrieving pending messages, no need to add the message,
   * it will be in the initial set of messages retrieved. */
  if (!self->priv->got_initial_messages)
    return;

  DEBUG ("New message received");

  sender = get_sender (self, message, &contact, &sender_id);

  if (sender == 0)
    {
      add_message_received (self, message, NULL, TRUE);
      return;
    }

  if (contact != NULL)
    {
      /* We have the sender, all good */
      add_message_received (self, message, contact, TRUE);

      g_object_unref (contact);
      return;
    }

  conn = tp_channel_borrow_connection (proxy);

  /* We have to request the sender which may result in message re-ordering. We
   * use the ID if possible as the handle may have expired so it's safer. */
  if (sender_id != NULL)
    {
      tp_connection_get_contacts_by_id (conn, 1, &sender_id,
          0, NULL, got_sender_contact_by_id_cb, copy_parts (message),
          NULL, G_OBJECT (self));
    }
  else
    {
      tp_connection_get_contacts_by_handle (conn, 1, &sender,
          0, NULL, got_sender_contact_by_handle_cb, copy_parts (message),
          NULL, G_OBJECT (self));
    }
}

static gint
find_msg_by_id (gconstpointer a,
    gconstpointer b)
{
  TpMessage *msg = TP_MESSAGE (a);
  guint id = GPOINTER_TO_UINT (b);
  gboolean valid;
  guint msg_id;

  msg_id = _tp_signalled_message_get_pending_message_id (msg, &valid);
  if (!valid)
    return 1;

  return msg_id != id;
}

static void
pending_messages_removed_cb (TpChannel *proxy,
    const GArray *ids,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) proxy;
  guint i;

  if (!self->priv->got_initial_messages)
    return;

  for (i = 0; i < ids->len; i++)
    {
      guint id = g_array_index (ids, guint, i);
      GList *link_;
      TpMessage *msg;

      link_ = g_queue_find_custom (self->priv->pending_messages,
          GUINT_TO_POINTER (id), find_msg_by_id);

      if (link_ == NULL)
        {
          DEBUG ("Unable to find pending message having id %d", id);
          continue;
        }

      msg = link_->data;

      g_queue_delete_link (self->priv->pending_messages, link_);

      g_signal_emit (self, signals[SIG_PENDING_MESSAGE_REMOVED], 0, msg);

      g_object_unref (msg);
    }
}

static void
got_pending_senders_contact (TpTextChannel *self,
    GList *parts_list,
    guint n_contacts,
    TpContact * const *contacts)
{
  GList *l;

  for (l = parts_list; l != NULL; l = g_list_next (l))
    {
      GPtrArray *parts = l->data;
      const GHashTable *header;
      TpHandle sender;
      guint i;

      header = g_ptr_array_index (parts, 0);
      sender = tp_asv_get_uint32 (header, "message-sender", NULL);

      if (sender == 0)
        continue;

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          if (tp_contact_get_handle (contact) == sender)
            {
              add_message_received (self, parts, contact, FALSE);

              break;
            }
        }
    }
}

static void
free_parts_list (GList *parts_list)
{
  GList *l;

  for (l = parts_list; l != NULL; l = g_list_next (l))
    g_boxed_free (TP_ARRAY_TYPE_MESSAGE_PART_LIST, l->data);

  g_list_free (parts_list);
}

static void
got_pending_senders_contact_by_handle_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = (GSimpleAsyncResult *) weak_object;
  GList *parts_list = user_data;
  TpTextChannel *self = TP_TEXT_CHANNEL (g_async_result_get_source_object (
        G_ASYNC_RESULT (result)));

  if (error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", error->message);
      goto out;
    }

  if (n_failed > 0)
    {
      DEBUG ("Failed to prepare some TpContact (InvalidHandle)");
    }

  got_pending_senders_contact (self, parts_list, n_contacts, contacts);

out:
  _tp_proxy_set_feature_prepared (TP_PROXY (self),
      TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, TRUE);

  free_parts_list (parts_list);
}

static void
got_pending_senders_contact_by_id_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    const gchar * const *requested_ids,
    GHashTable *failed_id_errors,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = (GSimpleAsyncResult *) weak_object;
  GList *parts_list = user_data;
  TpTextChannel *self = TP_TEXT_CHANNEL (g_async_result_get_source_object (
        G_ASYNC_RESULT (result)));

  if (error != NULL)
    {
      DEBUG ("Failed to prepare TpContact: %s", error->message);
      goto out;
    }

  if (DEBUGGING)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, failed_id_errors);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          DEBUG ("Failed to get a TpContact for %s: %s",
              (const gchar *) key, ((GError *) value)->message);
        }
    }

  got_pending_senders_contact (self, parts_list, n_contacts, contacts);

out:
  _tp_proxy_set_feature_prepared (TP_PROXY (self),
      TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, TRUE);

  free_parts_list (parts_list);
}

/* There is no TP_ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST (fdo #32433) */
#define ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST dbus_g_type_get_collection (\
    "GPtrArray", TP_ARRAY_TYPE_MESSAGE_PART_LIST)

static void
get_pending_messages_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) weak_object;
  GSimpleAsyncResult *result = user_data;
  guint i;
  GPtrArray *messages;
  TpIntSet *senders;
  GList *parts_list = NULL;
  GPtrArray *sender_ids;

  self->priv->got_initial_messages = TRUE;

  if (error != NULL)
    {
      DEBUG ("Failed to get PendingMessages property: %s", error->message);

      g_simple_async_result_set_error (result, error->domain, error->code,
          "Failed to get PendingMessages property: %s", error->message);

      g_simple_async_result_complete (result);
    }

  if (!G_VALUE_HOLDS (value, ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST))
    {
      DEBUG ("PendingMessages property is of the wrong type");

      g_simple_async_result_set_error (result, TP_ERRORS, TP_ERROR_CONFUSED,
          "PendingMessages property is of the wrong type");

      g_simple_async_result_complete (result);
    }

  senders = tp_intset_new ();
  sender_ids = g_ptr_array_new ();

  messages = g_value_get_boxed (value);
  for (i = 0; i < messages->len; i++)
    {
      GPtrArray *parts = g_ptr_array_index (messages, i);
      TpHandle sender;
      TpContact *contact;
      const gchar *sender_id;

      sender = get_sender (self, parts, &contact, &sender_id);

      if (sender == 0)
        {
          DEBUG ("Message doesn't have a sender");
          add_message_received (self, parts, NULL, FALSE);
          continue;
        }

      if (contact != NULL)
        {
          /* We have the sender */
          add_message_received (self, parts, contact, FALSE);
          g_object_unref (contact);
          continue;
        }

      tp_intset_add (senders, sender);

      if (sender_id != NULL)
        g_ptr_array_add (sender_ids, (gpointer) sender_id);

      /* We'll revert the list below when requesting the TpContact objects */
      parts_list = g_list_prepend (parts_list, copy_parts (parts));
    }

  if (tp_intset_size (senders) == 0)
    {
      g_simple_async_result_complete (result);
    }
  else
    {
      TpConnection *conn;

      parts_list = g_list_reverse (parts_list);

      conn = tp_channel_borrow_connection (TP_CHANNEL (proxy));

      DEBUG ("Pending messages may be re-ordered, please fix CM (%s)",
          tp_proxy_get_object_path (conn));

      /* Pass ownership of parts_list to the callback */
      if (sender_ids->len == g_list_length (parts_list))
        {
          /* Use the sender ID rather than the handles */
          tp_connection_get_contacts_by_id (conn, sender_ids->len,
              (const gchar * const *) sender_ids->pdata,
              0, NULL, got_pending_senders_contact_by_id_cb, parts_list,
              NULL, G_OBJECT (result));
        }
      else
        {
          GArray *tmp = tp_intset_to_array (senders);

          tp_connection_get_contacts_by_handle (conn, tmp->len,
              (TpHandle *) tmp->data,
              0, NULL, got_pending_senders_contact_by_handle_cb, parts_list,
              NULL, G_OBJECT (result));

          g_array_unref (tmp);
        }
    }

  tp_intset_destroy (senders);
  g_ptr_array_free (sender_ids, TRUE);
}

static void
tp_text_channel_prepare_pending_messages_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) proxy;
  GError *error = NULL;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_text_channel_prepare_pending_messages_async);

  tp_cli_channel_interface_messages_connect_to_message_received (channel,
      message_received_cb, proxy, NULL, G_OBJECT (proxy), &error);
  if (error != NULL)
    {
      DEBUG ("Failed to connect to MessageReceived signal: %s", error->message);
      goto fail;
    }

  tp_cli_channel_interface_messages_connect_to_pending_messages_removed (
      channel, pending_messages_removed_cb, proxy, NULL, G_OBJECT (proxy),
      &error);
  if (error != NULL)
    {
      DEBUG ("Failed to connect to PendingMessagesRemoved signal: %s",
          error->message);
      goto fail;
    }

  tp_cli_dbus_properties_call_get (proxy, -1,
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "PendingMessages",
      get_pending_messages_cb, result, g_object_unref, G_OBJECT (proxy));

  return;

fail:
  g_simple_async_result_take_error (result, error);

  g_simple_async_result_complete_in_idle (result);
  g_object_unref (result);
}

enum {
    FEAT_PENDING_MESSAGES,
    N_FEAT
};

static const TpProxyFeature *
tp_text_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_PENDING_MESSAGES].name =
    TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES;
  features[FEAT_PENDING_MESSAGES].prepare_async =
    tp_text_channel_prepare_pending_messages_async;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_text_channel_class_init (TpTextChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GParamSpec *param_spec;

  gobject_class->constructed = tp_text_channel_constructed;
  gobject_class->get_property = tp_text_channel_get_property;
  gobject_class->dispose = tp_text_channel_dispose;

  proxy_class->list_features = tp_text_channel_list_features;

  /**
   * TpTextChannel:supported-content-types:
   *
   * A #GStrv containing the MIME types supported by this channel, with more
   * preferred MIME types appearing earlier in the array.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_boxed ("supported-content-types",
      "SupportedContentTypes",
      "The Messages.SupportedContentTypes property of the channel",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SUPPORTED_CONTENT_TYPES,
      param_spec);

  /**
   * TpTextChannel:message-part-support-flags:
   *
   * A #TpMessagePartSupportFlags indicating the level of support for
   * message parts on this channel.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_uint ("message-part-support-flags",
      "MessagePartSupportFlags",
      "The Messages.MessagePartSupportFlags property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MESSAGE_PART_SUPPORT_FLAGS, param_spec);

  /**
   * TpTextChannel:delivery-reporting-support:
   *
   * A #TpDeliveryReportingSupportFlags indicating features supported
   * by this channel.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_uint ("delivery-reporting-support",
      "DeliveryReportingSupport",
      "The Messages.DeliveryReportingSupport property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_DELIVERY_REPORTING_SUPPORT, param_spec);

  /**
   * TpTextChannel:message-types:
   *
   * A #GArray containing the #TpChannelTextMessageType which may be sent on
   * this channel.
   *
   * Since: 0.13.16
   */
  param_spec = g_param_spec_boxed ("message-types",
      "MessageTypes",
      "The Messages.MessageTypes property of the channel",
      DBUS_TYPE_G_UINT_ARRAY,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MESSAGE_TYPES, param_spec);

  /**
   * TpTextChannel::message-received
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   *
   * The ::message-received signal is emitted when a new message has been
   * received on @self.
   *
   * Note that this signal is only fired once the
   * #TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES has been prepared.
   *
   * Since: 0.13.10
   */
  signals[SIG_MESSAGE_RECEIVED] = g_signal_new ("message-received",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_SIGNALLED_MESSAGE);

  /**
   * TpTextChannel::pending-message-removed
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   *
   * The ::pending-message-removed signal is emitted when @message
   * has been acked and so removed from the pending messages list.
   *
   * Note that this signal is only fired once the
   * #TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES has been prepared.
   *
   * Since: 0.13.10
   */
  signals[SIG_PENDING_MESSAGE_REMOVED] = g_signal_new (
      "pending-message-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, TP_TYPE_SIGNALLED_MESSAGE);

  /**
   * TpTextChannel::message-sent
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   * @flags: the #TpMessageSendingFlags affecting how the message was sent
   * @token: an opaque token used to match any incoming delivery or failure
   * reports against this message, or %NULL if the message is not
   * readily identifiable.
   *
   * The ::message-sent signal is emitted when @message
   * has been submitted for sending.
   *
   * Since: 0.13.10
   */
  signals[SIG_MESSAGE_SENT] = g_signal_new (
      "message-sent",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tp_marshal_VOID__OBJECT_UINT_STRING,
      G_TYPE_NONE,
      3, TP_TYPE_SIGNALLED_MESSAGE, G_TYPE_UINT, G_TYPE_STRING);

  g_type_class_add_private (gobject_class, sizeof (TpTextChannelPrivate));
}

static void
tp_text_channel_init (TpTextChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_TEXT_CHANNEL,
      TpTextChannelPrivate);

  self->priv->pending_messages = g_queue_new ();
}


/**
 * tp_text_channel_new:
 * @conn: a #TpConnection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @immutable_properties: (transfer none) (element-type utf8 GObject.Value):
 *  the immutable properties of the channel,
 *  as signalled by the NewChannel D-Bus signal or returned by the
 *  CreateChannel and EnsureChannel D-Bus methods: a mapping from
 *  strings (D-Bus interface name + "." + property name) to #GValue instances
 * @error: used to indicate the error if %NULL is returned
 *
 * Convenient function to create a new #TpTextChannel
 *
 * Returns: (transfer full): a newly created #TpTextChannel
 *
 * Since: 0.13.10
 */
TpTextChannel *
tp_text_channel_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TP_TYPE_TEXT_CHANNEL,
      "connection", conn,
       "dbus-daemon", conn_proxy->dbus_daemon,
       "bus-name", conn_proxy->bus_name,
       "object-path", object_path,
       "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
       "channel-properties", immutable_properties,
       NULL);
}

/**
 * tp_text_channel_get_supported_content_types: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:supported-content-types property
 *
 * Returns: (transfer none) :
 * the value of #TpTextChannel:supported-content-types
 *
 * Since: 0.13.10
 */
const gchar * const *
tp_text_channel_get_supported_content_types (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), NULL);

  return (const gchar * const *) self->priv->supported_content_types;
}

/**
 * tp_text_channel_get_message_part_support_flags: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:message-part-support-flags property
 *
 * Returns: the value of #TpTextChannel:message-part-support-flags
 *
 * Since: 0.13.10
 */
TpMessagePartSupportFlags
tp_text_channel_get_message_part_support_flags (
    TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), 0);

  return self->priv->message_part_support_flags;
}

/**
 * tp_text_channel_get_delivery_reporting_support: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:delivery-reporting-support property
 *
 * Returns: the value of #TpTextChannel:delivery-reporting-support property
 *
 * Since: 0.13.10
 */
TpDeliveryReportingSupportFlags
tp_text_channel_get_delivery_reporting_support (
    TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), 0);

  return self->priv->delivery_reporting_support;
}

/**
 * TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES:
 *
 * Expands to a call to a function that returns a quark representing the
 * incoming messages features of a #TpTextChannel.
 *
 * When this feature is prepared, tp_text_channel_get_pending_messages() will
 * return a non-empty list if any unacknowledged messages are waiting, and the
 * #TpTextChannel::message-received and #TpTextChannel::pending-message-removed
 * signals will be emitted.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.13.10
 */
GQuark
tp_text_channel_get_feature_quark_incoming_messages (void)
{
  return g_quark_from_static_string (
      "tp-text-channel-feature-incoming-messages");
}

/**
 * tp_text_channel_get_pending_messages:
 * @self: a #TpTextChannel
 *
 * Return a newly allocated list of unacknowledged #TpSignalledMessage
 * objects.
 *
 * Returns: (transfer container) (element-type TelepathyGLib.SignalledMessage):
 * a #GList of borrowed #TpSignalledMessage
 *
 * Since: 0.13.10
 */
GList *
tp_text_channel_get_pending_messages (TpTextChannel *self)
{
  return g_list_copy (g_queue_peek_head_link (self->priv->pending_messages));
}

static void
send_message_cb (TpChannel *proxy,
    const gchar *token,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to send message: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_set_op_res_gpointer (result,
      tp_str_empty (token) ? NULL : g_strdup (token), g_free);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_text_channel_send_message_async:
 * @self: a #TpTextChannel
 * @message: a #TpClientMessage
 * @flags: flags affecting how the message is sent
 * @callback: a callback to call when the message has been submitted to the
 * server
 * @user_data: data to pass to @callback
 *
 * Submit a message to the server for sending. Once the message has been
 * submitted to the sever, @callback will be called. You can then call
 * tp_text_channel_send_message_finish() to get the result of the operation.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_send_message_async (TpTextChannel *self,
    TpMessage *message,
    TpMessageSendingFlags flags,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_CLIENT_MESSAGE (message));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_send_message_async);

  tp_cli_channel_interface_messages_call_send_message (TP_CHANNEL (self),
    -1, message->parts, flags, send_message_cb, result, NULL, NULL);
}

/**
 * tp_text_channel_send_message_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @token: (out) (transfer full): if not %NULL, used to return the
 * token of the sent message
 * @error: a #GError to fill
 *
 * Finishes to send a message.
 *
 * @token can be used to match any incoming delivery or failure reports
 * against the sent message. If the returned token is %NULL the
 * message is not readily identifiable.
 *
 * Returns: %TRUE if the message has been submitted to the server, %FALSE
 * otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_send_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    gchar **token,
    GError **error)
{
  _tp_implement_finish_copy_pointer (self, tp_text_channel_send_message_async,
      g_strdup, token);
}

static void
acknowledge_pending_messages_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to ack messages: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_text_channel_ack_messages_async:
 * @self: a #TpTextChannel
 * @messages: (element-type TelepathyGLib.SignalledMessage): a #GList of
 * #TpSignalledMessage
 * @callback: a callback to call when the message have been acked
 * @user_data: data to pass to @callback
 *
 * Acknowledge all the messages in @messages.
 * Once the messages have been acked, @callback will be called.
 * You can then call tp_text_channel_ack_messages_finish() to get the
 * result of the operation.
 *
 * See tp_text_channel_ack_message_async() about acknowledging messages.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_ack_messages_async (TpTextChannel *self,
    const GList *messages,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *chan = (TpChannel *) self;
  GArray *ids;
  GList *l;
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_ack_messages_async);

  if (messages == NULL)
    {
      /* Nothing to ack, succeed immediately */
      g_simple_async_result_complete_in_idle (result);

      g_object_unref (result);
      return;
    }

  ids = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      g_list_length ((GList *) messages));

  for (l = (GList *) messages; l != NULL; l = g_list_next (l))
    {
      TpMessage *msg = l->data;
      guint id;
      gboolean valid;

      g_return_if_fail (TP_IS_SIGNALLED_MESSAGE (msg));

      id = _tp_signalled_message_get_pending_message_id (msg, &valid);
      if (!valid)
        {
          DEBUG ("Message doesn't have pending-message-id ?!");
          continue;
        }

      g_array_append_val (ids, id);
    }

  tp_cli_channel_type_text_call_acknowledge_pending_messages (chan, -1, ids,
      acknowledge_pending_messages_cb, result, NULL, G_OBJECT (self));

  g_array_free (ids, TRUE);
}

/**
 * tp_text_channel_ack_messages_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes to ack a list of messages.
 *
 * Returns: %TRUE if the messages have been acked, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_ack_messages_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_ack_messages_async)
}

/**
 * tp_text_channel_ack_message_async:
 * @self: a #TpTextChannel
 * @message: a #TpSignalledMessage
 * @callback: a callback to call when the message have been acked
 * @user_data: data to pass to @callback
 *
 * Acknowledge @message. Once the message has been acked, @callback will be
 * called. You can then call tp_text_channel_ack_message_finish() to get the
 * result of the operation.
 *
 * A message should be acknowledged once it has been shown to the user by the
 * Handler of the channel. So Observers and Approvers should NOT acknowledge
 * messages themselves.
 * Once a message has been acknowledged, it is removed from the
 * pending-message queue and so the #TpTextChannel::pending-message-removed
 * signal is fired.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_ack_message_async (TpTextChannel *self,
    TpMessage *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *chan = (TpChannel *) self;
  GSimpleAsyncResult *result;
  GArray *ids;
  guint id;
  gboolean valid;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_SIGNALLED_MESSAGE (message));

  id = _tp_signalled_message_get_pending_message_id (message, &valid);
  if (!valid)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
          TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Message doesn't have a pending-message-id");

      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_ack_message_async);

  ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (ids, id);

  tp_cli_channel_type_text_call_acknowledge_pending_messages (chan, -1, ids,
      acknowledge_pending_messages_cb, result, NULL, G_OBJECT (self));

  g_array_free (ids, TRUE);
}

/**
 * tp_text_channel_ack_message_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes to ack a message.
 *
 * Returns: %TRUE if the message has been acked, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_ack_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_ack_message_async)
}

static void
set_chat_state_cb (TpChannel *proxy,
      const GError *error,
      gpointer user_data,
      GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("SetChatState failed: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_text_channel_set_chat_state_async:
 * @self: a #TpTextChannel
 * @state: a #TpChannelChatState to set
 * @callback: a callback to call when the chat state has been set
 * @user_data: data to pass to @callback
 *
 * Set the local state on channel @self to @state.
 * Once the state has been set, @callback will be called.
 * You can then call tp_text_channel_set_chat_state_finish() to get the
 * result of the operation.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_set_chat_state_async (TpTextChannel *self,
    TpChannelChatState state,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_set_chat_state_async);

  tp_cli_channel_interface_chat_state_call_set_chat_state (TP_CHANNEL (self),
      -1, state, set_chat_state_cb, result, NULL, G_OBJECT (self));
}

/**
 * tp_text_channel_set_chat_state_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finishes to set chat state.
 *
 * Returns: %TRUE if the chat state has been changed, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_set_chat_state_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_set_chat_state_finish)
}

/**
 * tp_text_channel_get_message_types: (skip)
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:message-types property
 *
 * Returns: (transfer none) (element-type TelepathyGLib.ChannelTextMessageType):
 * the value of #TpTextChannel:message-types
 *
 * Since: 0.13.16
 */
GArray *
tp_text_channel_get_message_types (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), NULL);

  return self->priv->message_types;
}

/**
 * tp_text_channel_supports_message_type
 * @self: a #TpTextChannel
 * @message_type: a #TpChannelTextMessageType
 *
 * Check if message of type @message_type can be sent on this channel.
 *
 * Returns: %TRUE if message of type @message_type can be sent on @self, %FALSE
 * otherwise
 *
 * Since: 0.13.16
 */
gboolean
tp_text_channel_supports_message_type (TpTextChannel *self,
    TpChannelTextMessageType message_type)
{
  guint i;

  for (i = 0; i < self->priv->message_types->len; i++)
    {
      TpChannelTextMessageType tmp = g_array_index (self->priv->message_types,
          TpChannelTextMessageType, i);

      if (tmp == message_type)
        return TRUE;
    }

  return FALSE;
}
