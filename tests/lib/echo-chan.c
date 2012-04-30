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

#include "config.h"

#include "echo-chan.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/svc-channel.h>

/* This is for text-mixin unit tests, others should be using ExampleEcho2Channel
 * which uses newer TpMessageMixin */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void text_iface_init (gpointer iface, gpointer data);
static void destroyable_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (TpTestsEchoChannel,
    tp_tests_echo_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT, text_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_DESTROYABLE,
      destroyable_iface_init);
    )

/* type definition stuff */

static GPtrArray *
tp_tests_echo_channel_get_interfaces (TpBaseChannel *self)
{
  GPtrArray *interfaces;

  interfaces = TP_BASE_CHANNEL_CLASS (tp_tests_echo_channel_parent_class)->
    get_interfaces (self);

  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_DESTROYABLE);
  return interfaces;
};

static void
tp_tests_echo_channel_init (TpTestsEchoChannel *self)
{
}

static GObject *
constructor (GType type,
             guint n_props,
             GObjectConstructParam *props)
{
  GObject *object =
      G_OBJECT_CLASS (tp_tests_echo_channel_parent_class)->constructor (type,
          n_props, props);
  TpTestsEchoChannel *self = TP_TESTS_ECHO_CHANNEL (object);
  TpHandleRepoIface *contact_repo = NULL;
  TpBaseConnection *conn = tp_base_channel_get_connection (TP_BASE_CHANNEL (self));
  g_assert (conn != NULL);

  contact_repo = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);

  tp_base_channel_register (TP_BASE_CHANNEL (self));

  tp_text_mixin_init (object, G_STRUCT_OFFSET (TpTestsEchoChannel, text),
      contact_repo);

  tp_text_mixin_set_message_types (object,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
      G_MAXUINT);

  return object;
}

static void
finalize (GObject *object)
{
  tp_text_mixin_finalize (object);

  ((GObjectClass *) tp_tests_echo_channel_parent_class)->finalize (object);
}

static void
tp_tests_echo_channel_close (TpTestsEchoChannel *self)
{
  GObject *object = (GObject *) self;
  gboolean closed = tp_base_channel_is_destroyed (TP_BASE_CHANNEL (self));

  if (!closed)
    {
      TpHandle first_sender;

      /* The manager wants to be able to respawn the channel if it has pending
       * messages. When respawned, the channel must have the initiator set
       * to the contact who sent us those messages (if it isn't already),
       * and the messages must be marked as having been rescued so they
       * don't get logged twice. */
      if (tp_text_mixin_has_pending_messages (object, &first_sender))
        {
          tp_base_channel_reopened (TP_BASE_CHANNEL (self), first_sender);
          tp_text_mixin_set_rescued (object);
        }
      else
        {
          tp_base_channel_destroyed (TP_BASE_CHANNEL (self));
        }
    }
}

static void
channel_close (TpBaseChannel *channel)
{
  TpTestsEchoChannel *self = TP_TESTS_ECHO_CHANNEL (channel);

  tp_tests_echo_channel_close (self);
}

static void
tp_tests_echo_channel_class_init (TpTestsEchoChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseChannelClass *base_class = TP_BASE_CHANNEL_CLASS (klass);

  object_class->constructor = constructor;
  object_class->finalize = finalize;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
  base_class->target_handle_type = TP_HANDLE_TYPE_CONTACT;
  base_class->get_interfaces = tp_tests_echo_channel_get_interfaces;
  base_class->close = channel_close;

  tp_text_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpTestsEchoChannelClass, text_class));
}

static void
text_send (TpSvcChannelTypeText *iface,
           guint type,
           const gchar *text,
           DBusGMethodInvocation *context)
{
  TpTestsEchoChannel *self = TP_TESTS_ECHO_CHANNEL (iface);
  time_t timestamp = time (NULL);
  gchar *echo;
  guint echo_type = type;
  TpHandle target = tp_base_channel_get_target_handle (TP_BASE_CHANNEL (self));

  /* Send should return just before Sent is emitted. */
  tp_svc_channel_type_text_return_from_send (context);

  /* Tell the client that the message was submitted for sending */
  tp_svc_channel_type_text_emit_sent ((GObject *) self, timestamp, type, text);

  /* Pretend that the remote contact has replied. Normally, you'd
   * call tp_text_mixin_receive or tp_text_mixin_receive_with_flags
   * in response to network events */

  switch (type)
    {
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL:
      echo = g_strdup_printf ("You said: %s", text);
      break;
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION:
      echo = g_strdup_printf ("notices that the user %s", text);
      break;
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE:
      echo = g_strdup_printf ("You sent a notice: %s", text);
      break;
    default:
      echo = g_strdup_printf ("You sent some weird message type, %u: \"%s\"",
          type, text);
      echo_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
    }

  tp_text_mixin_receive ((GObject *) self, echo_type, target, timestamp, echo);

  g_free (echo);
}

static void
text_iface_init (gpointer iface,
                 gpointer data)
{
  TpSvcChannelTypeTextClass *klass = iface;

  tp_text_mixin_iface_init (iface, data);
#define IMPLEMENT(x) tp_svc_channel_type_text_implement_##x (klass, text_##x)
  IMPLEMENT (send);
#undef IMPLEMENT
}

static void
destroyable_destroy (TpSvcChannelInterfaceDestroyable *iface,
                     DBusGMethodInvocation *context)
{
  TpTestsEchoChannel *self = TP_TESTS_ECHO_CHANNEL (iface);

  tp_text_mixin_clear ((GObject *) self);
  tp_base_channel_destroyed (TP_BASE_CHANNEL (self));

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

G_GNUC_END_IGNORE_DEPRECATIONS
