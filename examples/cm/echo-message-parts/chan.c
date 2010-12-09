/*
 * chan.c - an example text channel talking to a particular
 * contact. Similar code is used for 1-1 IM channels in many protocols
 * (IRC private messages ("/query"), XMPP IM etc.)
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "chan.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/svc-channel.h>

static void destroyable_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ExampleEcho2Channel,
    example_echo_2_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
      tp_message_mixin_text_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
      tp_message_mixin_messages_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_DESTROYABLE,
      destroyable_iface_init)
    )

/* type definition stuff */

static const char * example_echo_2_channel_interfaces[] = {
    TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
    TP_IFACE_CHANNEL_INTERFACE_DESTROYABLE,
    NULL };

static void
example_echo_2_channel_init (ExampleEcho2Channel *self)
{
}


static void
send_message (GObject *object,
              TpMessage *message,
              TpMessageSendingFlags flags)
{
  ExampleEcho2Channel *self = EXAMPLE_ECHO_2_CHANNEL (object);
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  time_t timestamp = time (NULL);
  guint len = tp_message_count_parts (message);
  TpMessage *received = NULL;
  guint i;

  if (tp_asv_get_string (tp_message_peek (message, 0), "interface") != NULL)
    {
      /* this message is interface-specific - let's not echo it */
      goto finally;
    }

  received = tp_cm_message_new (tp_base_channel_get_connection (base), 1);

  /* Copy/modify the headers for the "received" message */
    {
      TpChannelTextMessageType message_type;
      gboolean valid;

      tp_cm_message_set_sender (received,
          tp_base_channel_get_target_handle (base));

      message_type = tp_asv_get_uint32 (tp_message_peek (message, 0),
          "message-type", &valid);

      /* The check for 'valid' means that if message-type is missing or of the
       * wrong type, fall back to NORMAL (this is in fact a no-op, since
       * NORMAL == 0 and tp_asv_get_uint32 returns 0 on missing or wrongly
       * typed values) */
      if (valid && message_type != TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL)
        tp_message_set_uint32 (received, 0, "message-type", message_type);

      tp_message_set_uint32 (received, 0, "message-sent", timestamp);
      tp_message_set_uint32 (received, 0, "message-received", timestamp);
    }

  /* Copy the content for the "received" message */
  for (i = 1; i < len; i++)
    {
      const GHashTable *input = tp_message_peek (message, i);
      const gchar *s;
      const GValue *value;
      guint j;

      /* in this example we ignore interface-specific parts */

      s = tp_asv_get_string (input, "content-type");

      if (s == NULL)
        continue;

      s = tp_asv_get_string (input, "interface");

      if (s != NULL)
        continue;

      /* OK, we want to copy this part */

      j = tp_message_append_part (received);

      s = tp_asv_get_string (input, "content-type");
      g_assert (s != NULL);   /* already checked */
      tp_message_set_string (received, j, "content-type", s);

      s = tp_asv_get_string (input, "identifier");

      if (s != NULL)
        tp_message_set_string (received, j, "identifier", s);

      s = tp_asv_get_string (input, "alternative");

      if (s != NULL)
        tp_message_set_string (received, j, "alternative", s);

      s = tp_asv_get_string (input, "lang");

      if (s != NULL)
        tp_message_set_string (received, j, "lang", s);

      value = tp_asv_lookup (input, "content");

      if (value != NULL)
        tp_message_set (received, j, "content", value);
    }

finally:
  /* "OK, we've sent the message" (after calling this, message must not be
   * dereferenced) */
  tp_message_mixin_sent (object, message, flags, "", NULL);

  if (received != NULL)
    {
      /* Pretend the other user sent us back the same message. After this call,
       * the received message is owned by the mixin */
      tp_message_mixin_take_received (object, received);
    }
}


static GObject *
constructor (GType type,
             guint n_props,
             GObjectConstructParam *props)
{
  GObject *object =
      G_OBJECT_CLASS (example_echo_2_channel_parent_class)->constructor (type,
          n_props, props);
  ExampleEcho2Channel *self = EXAMPLE_ECHO_2_CHANNEL (object);
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  static TpChannelTextMessageType const types[] = {
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE
  };
  static const char * const content_types[] = { "*/*", NULL };

  tp_base_channel_register (base);

  tp_message_mixin_init (object, G_STRUCT_OFFSET (ExampleEcho2Channel, text),
      tp_base_channel_get_connection (base));

  tp_message_mixin_implement_sending (object, send_message,
      G_N_ELEMENTS (types), types,
      TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT |
      TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS,
      TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES,
      content_types);

  return object;
}

static void
finalize (GObject *object)
{
  tp_message_mixin_finalize (object);

  ((GObjectClass *) example_echo_2_channel_parent_class)->finalize (object);
}

static void
example_echo_2_channel_close (TpBaseChannel *self)
{
  GObject *object = (GObject *) self;

  if (!tp_base_channel_is_destroyed (self))
    {
      TpHandle first_sender;

      /* The manager wants to be able to respawn the channel if it has pending
       * messages. When respawned, the channel must have the initiator set
       * to the contact who sent us those messages (if it isn't already),
       * and the messages must be marked as having been rescued so they
       * don't get logged twice. */
      if (tp_message_mixin_has_pending_messages (object, &first_sender))
        {
          tp_message_mixin_set_rescued (object);
          tp_base_channel_reopened (self, first_sender);
        }
      else
        {
          tp_base_channel_destroyed (self);
        }
    }
}

static void
example_echo_2_channel_fill_immutable_properties (TpBaseChannel *chan,
    GHashTable *properties)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (
      example_echo_2_channel_parent_class);

  klass->fill_immutable_properties (chan, properties);

  tp_dbus_properties_mixin_fill_properties_hash (
      G_OBJECT (chan), properties,
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "MessagePartSupportFlags",
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "DeliveryReportingSupport",
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "SupportedContentTypes",
      NULL);
}

static void
example_echo_2_channel_class_init (ExampleEcho2ChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseChannelClass *base_class = (TpBaseChannelClass *) klass;

  object_class->constructor = constructor;
  object_class->finalize = finalize;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
  base_class->target_handle_type = TP_HANDLE_TYPE_CONTACT;
  base_class->interfaces = example_echo_2_channel_interfaces;
  base_class->close = example_echo_2_channel_close;
  base_class->fill_immutable_properties =
    example_echo_2_channel_fill_immutable_properties;

  tp_message_mixin_init_dbus_properties (object_class);
}

static void
destroyable_destroy (TpSvcChannelInterfaceDestroyable *iface,
                     DBusGMethodInvocation *context)
{
  TpBaseChannel *self = TP_BASE_CHANNEL (iface);

  tp_message_mixin_clear ((GObject *) self);
  example_echo_2_channel_close (self);
  g_assert (tp_base_channel_is_destroyed (self));
  tp_svc_channel_interface_destroyable_return_from_destroy (context);
}

static void
destroyable_iface_init (gpointer iface,
                        gpointer data)
{
  TpSvcChannelInterfaceDestroyableClass *klass = iface;

#define IMPLEMENT(x) \
  tp_svc_channel_interface_destroyable_implement_##x (klass, destroyable_##x)
  IMPLEMENT (destroy);
#undef IMPLEMENT
}
