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

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

static void destroyable_iface_init (gpointer iface, gpointer data);
static void sms_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ExampleEcho2Channel,
    example_echo_2_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
      tp_message_mixin_text_iface_init)
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
      tp_message_mixin_messages_iface_init)
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_CHAT_STATE,
      tp_message_mixin_chat_state_iface_init)
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_DESTROYABLE,
      destroyable_iface_init)
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_SMS, sms_iface_init)
    )

/* type definition stuff */

static GPtrArray *
example_echo_2_channel_get_interfaces (TpBaseChannel *self)
{
  GPtrArray *interfaces;

  interfaces = TP_BASE_CHANNEL_CLASS (example_echo_2_channel_parent_class)->
    get_interfaces (self);

  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_MESSAGES);
  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_DESTROYABLE);
  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_SMS);
  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_CHAT_STATE);

  return interfaces;
};

enum
{
  PROP_SMS = 1,
  PROP_SMS_FLASH,
  N_PROPS
};

struct _ExampleEcho2ChannelPrivate
{
  gboolean sms;
};

static void
example_echo_2_channel_init (ExampleEcho2Channel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_ECHO_2_CHANNEL, ExampleEcho2ChannelPrivate);
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

static gboolean
send_chat_state (GObject *object,
    TpChannelChatState state,
    GError **error)
{
  return TRUE;
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

  tp_message_mixin_implement_send_chat_state (object, send_chat_state);

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

  tp_message_mixin_maybe_send_gone (object);

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
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "MessageTypes",
      TP_IFACE_CHANNEL_INTERFACE_SMS, "Flash",
      NULL);
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  ExampleEcho2Channel *self = (ExampleEcho2Channel *) object;

  switch (property_id)
    {
      case PROP_SMS:
        self->priv->sms = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  ExampleEcho2Channel *self = (ExampleEcho2Channel *) object;

  switch (property_id)
    {
      case PROP_SMS:
        g_value_set_boolean (value, self->priv->sms);
        break;
      case PROP_SMS_FLASH:
        g_value_set_boolean (value, TRUE);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
example_echo_2_channel_class_init (ExampleEcho2ChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseChannelClass *base_class = (TpBaseChannelClass *) klass;
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl sms_props[] = {
      { "SMSChannel", "sms", NULL },
      { "Flash", "sms-flash", NULL },
      { NULL }
  };

  g_type_class_add_private (klass, sizeof (ExampleEcho2ChannelPrivate));

  object_class->constructor = constructor;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->finalize = finalize;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
  base_class->target_handle_type = TP_HANDLE_TYPE_CONTACT;
  base_class->get_interfaces = example_echo_2_channel_get_interfaces;
  base_class->close = example_echo_2_channel_close;
  base_class->fill_immutable_properties =
    example_echo_2_channel_fill_immutable_properties;

  param_spec = g_param_spec_boolean ("sms", "SMS",
      "SMS.SMSChannel",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SMS, param_spec);

  param_spec = g_param_spec_boolean ("sms-flash", "SMS Flash",
      "SMS.Flash",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SMS_FLASH, param_spec);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_INTERFACE_SMS,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      sms_props);

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


void
example_echo_2_channel_set_sms (ExampleEcho2Channel *self,
    gboolean sms)
{
  if (self->priv->sms == sms)
    return;

  self->priv->sms = sms;

  tp_svc_channel_interface_sms_emit_sms_channel_changed (self, sms);
}

static void
sms_get_sms_length (TpSvcChannelInterfaceSMS *self,
    const GPtrArray *parts,
    DBusGMethodInvocation *context)
{
  TpMessage *message;
  guint i;
  gchar *txt;
  size_t len;

  message = tp_cm_message_new (
      tp_base_channel_get_connection (TP_BASE_CHANNEL (self)), parts->len);

  for (i = 0; i < parts->len; i++)
    {
      GHashTableIter iter;
      gpointer key, value;

      tp_message_append_part (message);
      g_hash_table_iter_init (&iter, g_ptr_array_index (parts, i));
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          tp_message_set (message, i, key, value);
        }
    }

  txt = tp_message_to_text (message, NULL);
  len = strlen (txt);

  tp_svc_channel_interface_sms_return_from_get_sms_length (context, len,
      EXAMPLE_ECHO_2_CHANNEL_MAX_SMS_LENGTH - len, -1);

  g_object_unref (message);
  g_free (txt);
}

static void
sms_iface_init (gpointer iface,
    gpointer data)
{
  TpSvcChannelInterfaceSMSClass *klass = iface;

#define IMPLEMENT(x) \
  tp_svc_channel_interface_sms_implement_##x (klass, sms_##x)
  IMPLEMENT (get_sms_length);
#undef IMPLEMENT
}
