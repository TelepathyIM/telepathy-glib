/*
 * debug-sender.c - Telepathy debug interface implementation
 * Copyright (C) 2009 Collabora Ltd.
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

#include "debug-sender.h"
#include "config.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/interfaces.h>

/**
 * SECTION:debug-sender
 * @title: TpDebugSender
 * @short_description: proxy object for Telepathy debug interface
 *
 * A #TpDebugSender object is an object exposing the Telepathy debug interface.
 *
 * Since: 0.7.UNRELEASED
 */

/**
 * TpDebugSenderClass:
 *
 * The class of a #TpDebugSender.
 *
 * Since: 0.7.UNRELEASED
 */

/**
 * TpDebugSender:
 * @parent: The parent class instance
 * @enabled: %TRUE if debug messages should be sent using the NewDebugMessage
 *  signal.
 * @messages: A queue of debug messages.
 *
 * A proxy object for the Telepathy debug interface.
 *
 * Since: 0.7.UNRELEASED
 */

/**
 * TpDebugMessage:
 * @timestamp: Time of the debug message
 * @domain: Message domain
 * @level: A debug level, from #TpDebugLevel
 * @string: Message text
 *
 * A structure representing a single debug message.
 *
 * Since: 0.7.UNRELEASED
 */

static TpDebugSender *debug_sender = NULL;

/* On the basis that messages are around 60 bytes on average, and that 50kb is
 * a reasonable maximum size for a frame buffer.
 */

#define DEBUG_MESSAGE_LIMIT 800

static void debug_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpDebugSender, tp_debug_sender, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DEBUG, debug_iface_init));

/* properties */
enum
{
  PROP_ENABLED = 1,
  NUM_PROPERTIES
};

static TpDebugLevel
log_level_flags_to_debug_level (GLogLevelFlags level)
{
  switch (level)
    {
    case G_LOG_LEVEL_ERROR:
      return TP_DEBUG_LEVEL_ERROR;
      break;
    case G_LOG_LEVEL_CRITICAL:
      return TP_DEBUG_LEVEL_CRITICAL;
      break;
    case G_LOG_LEVEL_WARNING:
      return TP_DEBUG_LEVEL_WARNING;
      break;
    case G_LOG_LEVEL_MESSAGE:
      return TP_DEBUG_LEVEL_MESSAGE;
      break;
    case G_LOG_LEVEL_INFO:
      return TP_DEBUG_LEVEL_INFO;
      break;
    case G_LOG_LEVEL_DEBUG:
      return TP_DEBUG_LEVEL_DEBUG;
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static TpDebugMessage *
debug_message_new (GTimeVal *timestamp,
    const gchar *domain,
    GLogLevelFlags level,
    const gchar *string)
{
  TpDebugMessage *msg;

  msg = g_slice_new0 (TpDebugMessage);
  msg->timestamp = timestamp->tv_sec + timestamp->tv_usec / 1e6;
  msg->domain = g_strdup (domain);
  msg->level = log_level_flags_to_debug_level (level);
  msg->string = g_strdup (string);
  return msg;
}

static void
debug_message_free (TpDebugMessage *msg)
{
  g_free (msg->domain);
  g_free (msg->string);
  g_slice_free (TpDebugMessage, msg);
}

static void
tp_debug_sender_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpDebugSender *self = TP_DEBUG_SENDER (object);

  switch (property_id)
    {
      case PROP_ENABLED:
        g_value_set_boolean (value, self->enabled);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
tp_debug_sender_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpDebugSender *self = TP_DEBUG_SENDER (object);

  switch (property_id)
    {
      case PROP_ENABLED:
        self->enabled = g_value_get_boolean (value);
        break;

     default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
tp_debug_sender_finalize (GObject *object)
{
  TpDebugSender *self = TP_DEBUG_SENDER (object);

  g_queue_foreach (self->messages, (GFunc) debug_message_free, NULL);
  g_queue_free (self->messages);
  self->messages = NULL;

  G_OBJECT_CLASS (tp_debug_sender_parent_class)->finalize (object);
}

static void
tp_debug_sender_class_init (TpDebugSenderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  static TpDBusPropertiesMixinPropImpl debug_props[] = {
      { "Enabled", "enabled", "enabled" },
      { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { TP_IFACE_DEBUG,
        tp_dbus_properties_mixin_getter_gobject_properties,
        tp_dbus_properties_mixin_setter_gobject_properties,
        debug_props,
      },
      { NULL }
  };

  object_class->get_property = tp_debug_sender_get_property;
  object_class->set_property = tp_debug_sender_set_property;
  object_class->finalize = tp_debug_sender_finalize;

  /**
   * TpDebugSender:enabled
   *
   * %TRUE if the NewDebugMessage signal should be emitted when a new debug
   * message is generated.
   */
  g_object_class_install_property (object_class, PROP_ENABLED,
      g_param_spec_boolean ("enabled", "Enabled?",
          "True if the new-debug-message signal is enabled.",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpDebugSenderClass, dbus_props_class));
}

static void
get_messages (TpSvcDebug *self,
    DBusGMethodInvocation *context)
{
  TpDebugSender *dbg = TP_DEBUG_SENDER (self);
  GPtrArray *messages;
  static GType struct_type = 0;
  GList *i;
  guint j;

  if (G_UNLIKELY (struct_type == 0))
    {
      struct_type = dbus_g_type_get_struct (
          "GValueArray", G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_UINT,
          G_TYPE_STRING, G_TYPE_INVALID);
    }

  messages = g_ptr_array_sized_new (g_queue_get_length (dbg->messages));

  for (i = dbg->messages->head; i; i = i->next)
    {
      GValue gvalue = { 0 };
      TpDebugMessage *message = (TpDebugMessage *) i->data;

      g_value_init (&gvalue, struct_type);
      g_value_take_boxed (&gvalue,
          dbus_g_type_specialized_construct (struct_type));
      dbus_g_type_struct_set (&gvalue,
          0, message->timestamp,
          1, message->domain,
          2, message->level,
          3, message->string,
          G_MAXUINT);
      g_ptr_array_add (messages, g_value_get_boxed (&gvalue));
    }

  tp_svc_debug_return_from_get_messages (context, messages);

  for (j = 0; j < messages->len; j++)
    g_boxed_free (struct_type, messages->pdata[j]);

  g_ptr_array_free (messages, TRUE);
}

static void
debug_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TpSvcDebugClass *klass = (TpSvcDebugClass *) g_iface;

  tp_svc_debug_implement_get_messages (klass, get_messages);
}

static void
tp_debug_sender_init (TpDebugSender *self)
{
  self->messages = g_queue_new ();
}

/**
 * tp_debug_sender_get:
 *
 * <!-- -->
 *
 * Returns: the #TpDebugSender instance for the current starter bus
 *
 * Since: 0.7.UNRELEASED
 */
TpDebugSender *
tp_debug_sender_get (void)
{
  if (G_UNLIKELY (debug_sender == NULL))
    {
      TpDBusDaemon *dbus_daemon;
      GError *error = NULL;

      debug_sender = g_object_new (TP_TYPE_DEBUG_SENDER, NULL);
      dbus_daemon = tp_dbus_daemon_dup (&error);

      if (error != NULL)
        {
          g_error_free (error);
          return NULL;
        }

      dbus_g_connection_register_g_object (
          tp_proxy_get_dbus_connection (dbus_daemon),
          "/org/freedesktop/Telepathy/debug", (GObject *) debug_sender);

      g_object_unref (dbus_daemon);
    }

  return debug_sender;
}


/**
 * tp_debug_sender_add_message:
 * self: A #TpDebugSender instance
 * timestamp: Time of the message
 * domain: Message domain
 * level: The message level
 * string: The message string itself
 *
 * Adds a new message to the debug sender message queue. If the
 * TpDebugSender:enabled property is set to %TRUE, then a NewDebugMessage
 * signal will be fired too.
 *
 * Since: 0.7.UNRELEASED
 */
void
tp_debug_sender_add_message (TpDebugSender *self,
    GTimeVal *timestamp,
    const gchar *domain,
    GLogLevelFlags level,
    const gchar *string)
{
  TpDebugMessage *new_msg;

  if (g_queue_get_length (self->messages) >= DEBUG_MESSAGE_LIMIT)
    {
      TpDebugMessage *old_head =
        (TpDebugMessage *) g_queue_pop_head (self->messages);

      debug_message_free (old_head);
    }

  new_msg = debug_message_new (timestamp, domain, level, string);
  g_queue_push_tail (self->messages, new_msg);

  if (self->enabled)
    {
      tp_svc_debug_emit_new_debug_message (self, new_msg->timestamp,
          domain, new_msg->level, string);
    }
}
