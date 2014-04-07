#include "config.h"

#include <stdio.h>

#include <glib.h>

#include "telepathy-glib/capabilities-internal.h"

#include <telepathy-glib/asv.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/value-array.h>

#include "tests/lib/util.h"

typedef struct {
    gpointer unused;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  tp_debug_set_flags ("all");
}

static GVariant *
text_chat_class (TpEntityType entity_type)
{
  return g_variant_new_parsed ("({%s: <%s>, %s: <%u>}, @as [])",
      TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) entity_type);
}

static GVariant *
ft_class (const gchar * const *allowed)
{
  const gchar * const default_allowed[] = {
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_FILENAME,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_SIZE,
      NULL };

  if (allowed == NULL)
    allowed = default_allowed;

  return g_variant_new_parsed ("({%s: <%s>, %s: <%u>}, %^as)",
      TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT,
      allowed);
}

static void
test_basics (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  GVariant *classes;
  GVariant *fixed;
  const gchar **allowed;
  const gchar *chan_type;
  TpEntityType entity_type;
  gboolean valid;

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        text_chat_class (TP_ENTITY_TYPE_CONTACT),
        ft_class (NULL)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_is_specific_to_contact (caps));
  classes = tp_capabilities_dup_channel_classes (caps);

  g_assert_cmpuint (g_variant_n_children (classes), ==, 2);

  /* Check text chats class */
  g_variant_get_child (classes, 0, "(@a{sv}^a&s)", &fixed, &allowed);

  g_assert_cmpuint (g_variant_n_children (fixed), ==, 2);

  chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_TEXT);

  entity_type = tp_vardict_get_uint32 (fixed,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);
  g_assert (valid);
  g_assert_cmpuint (entity_type, ==, TP_ENTITY_TYPE_CONTACT);

  g_assert_cmpuint (g_strv_length ((GStrv) allowed), ==, 0);

  g_variant_unref (fixed);
  g_free (allowed);

  /* Check ft class */
  g_variant_get_child (classes, 1, "(@a{sv}^a&s)", &fixed, &allowed);

  g_assert_cmpuint (g_variant_n_children (fixed), ==, 2);

  chan_type = tp_vardict_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1);

  entity_type = tp_vardict_get_uint32 (fixed,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, &valid);
  g_assert (valid);
  g_assert_cmpuint (entity_type, ==, TP_ENTITY_TYPE_CONTACT);

  g_assert_cmpuint (g_strv_length ((GStrv) allowed), ==, 2);
  g_assert (tp_strv_contains ((const gchar * const * ) allowed,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_FILENAME));
  g_assert (tp_strv_contains ((const gchar * const * ) allowed,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_SIZE));

  g_variant_unref (fixed);
  g_free (allowed);

  g_variant_unref (classes);
  g_object_unref (caps);
}

static void
test_supports (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  GVariant *classes;

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        text_chat_class (TP_ENTITY_TYPE_CONTACT),
        ft_class (NULL)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (tp_capabilities_supports_text_chats (caps));
  g_assert (!tp_capabilities_supports_text_chatrooms (caps));
  g_assert (!tp_capabilities_supports_sms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing the text chatrooms caps */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        text_chat_class (TP_ENTITY_TYPE_ROOM)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (!tp_capabilities_supports_text_chats (caps));
  g_assert (tp_capabilities_supports_text_chatrooms (caps));
  g_assert (!tp_capabilities_supports_sms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing both caps */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        text_chat_class (TP_ENTITY_TYPE_CONTACT),
        text_chat_class (TP_ENTITY_TYPE_ROOM)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (tp_capabilities_supports_text_chats (caps));
  g_assert (tp_capabilities_supports_text_chatrooms (caps));
  g_assert (!tp_capabilities_supports_sms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing no caps */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("@a(a{sv}as) []"),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (!tp_capabilities_supports_text_chats (caps));
  g_assert (!tp_capabilities_supports_text_chatrooms (caps));
  g_assert (!tp_capabilities_supports_sms (caps));

  classes = tp_capabilities_dup_channel_classes (caps);
  g_assert_cmpuint (g_variant_n_children (classes), ==, 0);
  g_variant_unref (classes);

  g_object_unref (caps);
}

static GVariant *
stream_tube_class (TpEntityType entity_type,
    const gchar *service)
{
  GVariantBuilder fixed;

  g_variant_builder_init (&fixed, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_CHANNEL_TYPE,
      g_variant_new_string (TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1));
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
      g_variant_new_uint32 (entity_type));

  if (service != NULL)
    {
      g_variant_builder_add (&fixed, "{sv}",
          TP_PROP_CHANNEL_TYPE_STREAM_TUBE1_SERVICE,
          g_variant_new_string (service));
    }

  return g_variant_new_parsed ("(%@a{sv}, @as [])",
      g_variant_builder_end (&fixed));
}

static GVariant *
dbus_tube_class (TpEntityType entity_type,
    const gchar *service_name,
    gboolean add_extra_fixed)
{
  GVariantBuilder fixed;

  g_variant_builder_init (&fixed, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_CHANNEL_TYPE,
      g_variant_new_string (TP_IFACE_CHANNEL_TYPE_DBUS_TUBE1));
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
      g_variant_new_uint32 (entity_type));

  if (service_name != NULL)
    {
      g_variant_builder_add (&fixed, "{sv}",
          TP_PROP_CHANNEL_TYPE_DBUS_TUBE1_SERVICE_NAME,
          g_variant_new_string (service_name));
    }

  if (add_extra_fixed)
    {
      g_variant_builder_add (&fixed, "{sv}", "ExtraBadgersRequired",
          g_variant_new_boolean (TRUE));
    }

  return g_variant_new_parsed ("(%@a{sv}, @as [])",
      g_variant_builder_end (&fixed));
}

static void
test_supports_tube (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;

  /* TpCapabilities containing no caps */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("@a(a{sv}as) []"),
      "contact-specific", TRUE,
      NULL);

  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private stream tube caps without service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        stream_tube_class (TP_ENTITY_TYPE_CONTACT, NULL)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private and muc stream tube caps without
   * service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        stream_tube_class (TP_ENTITY_TYPE_CONTACT, NULL),
        stream_tube_class (TP_ENTITY_TYPE_ROOM, NULL)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private and muc stream tube caps and
   * one with a service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*, %*, %*]",
        stream_tube_class (TP_ENTITY_TYPE_CONTACT, NULL),
        stream_tube_class (TP_ENTITY_TYPE_ROOM, NULL),
        stream_tube_class (TP_ENTITY_TYPE_CONTACT, "test-service"),
        stream_tube_class (TP_ENTITY_TYPE_ROOM, "test-service")),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "test-service"));
  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "badger"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "badger"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* Connection capabilities */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        stream_tube_class (TP_ENTITY_TYPE_CONTACT, NULL),
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, NULL, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  /* the service is meaningless for connection capabilities */
  g_assert (tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  /* the service name is meaningless for connection capabilities */
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private dbus tube caps without service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, NULL, FALSE)),
      "contact-specific", TRUE,
      NULL);

  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private and muc dbus tube caps without
   * service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, NULL, FALSE),
        dbus_tube_class (TP_ENTITY_TYPE_ROOM, NULL, FALSE)),
      "contact-specific", TRUE,
      NULL);

  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));

  g_object_unref (caps);

  /* TpCapabilities containing the private and muc dbus tube caps and
   * one with a service */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*, %*, %*]",
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, NULL, FALSE),
        dbus_tube_class (TP_ENTITY_TYPE_ROOM, NULL, FALSE),
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, "com.Test", FALSE),
        dbus_tube_class (TP_ENTITY_TYPE_ROOM, "com.Test", FALSE)),
      "contact-specific", TRUE,
      NULL);

  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (!tp_capabilities_supports_stream_tubes (caps,
        TP_ENTITY_TYPE_CONTACT, "test-service"));
  g_assert (!tp_capabilities_supports_stream_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "test-service"));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        NULL));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        NULL));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Test"));
  g_assert (tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Test"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_CONTACT,
        "com.Badger"));
  g_assert (!tp_capabilities_supports_dbus_tubes (caps, TP_ENTITY_TYPE_ROOM,
        "com.Badger"));

  g_object_unref (caps);

  /* Any extra fixed prop make it unsupported */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        dbus_tube_class (TP_ENTITY_TYPE_CONTACT, NULL, TRUE)),
      "contact-specific", TRUE,
      NULL);

  g_assert (!tp_capabilities_supports_dbus_tubes (caps,
      TP_ENTITY_TYPE_CONTACT, NULL));

  g_object_unref (caps);
}

static GVariant *
room_list_class (gboolean server,
    gboolean add_extra_fixed)
{
  GVariantBuilder fixed;
  const gchar * const allowed[] = {
      TP_PROP_CHANNEL_TYPE_ROOM_LIST1_SERVER,
      NULL };
  const gchar * const no_allowed[] = { NULL };

  g_variant_builder_init (&fixed, G_VARIANT_TYPE_VARDICT);

  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_CHANNEL_TYPE,
      g_variant_new_string (TP_IFACE_CHANNEL_TYPE_ROOM_LIST1));
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
      g_variant_new_uint32 (TP_ENTITY_TYPE_NONE));

  if (add_extra_fixed)
    {
      g_variant_builder_add (&fixed, "{sv}", "ExtraBadgersRequired",
          g_variant_new_boolean (TRUE));
    }

  return g_variant_new_parsed ("(%@a{sv}, %^as)",
      g_variant_builder_end (&fixed),
      server ? allowed : no_allowed);
}

static void
test_supports_room_list (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  gboolean with_server = TRUE;

  /* Does not support room list */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (NULL)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_room_list (caps, &with_server));
  g_assert (!with_server);

  g_object_unref (caps);

  /* Support room list but no server */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        ft_class (NULL),
        room_list_class (FALSE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_room_list (caps, &with_server));
  g_assert (!with_server);

  g_object_unref (caps);

  /* Support room list with server */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        ft_class (NULL),
        room_list_class (TRUE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_room_list (caps, &with_server));
  g_assert (with_server);

  g_object_unref (caps);

  /* Any extra fixed prop make it unsupported */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        room_list_class (FALSE, TRUE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_room_list (caps, NULL));

  g_object_unref (caps);
}

static GVariant *
sms_class (gboolean add_extra_fixed,
    gboolean use_allowed)
{
  GVariantBuilder fixed;
  const gchar *allowed[] = { NULL, NULL };

  g_variant_builder_init (&fixed, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_CHANNEL_TYPE,
      g_variant_new_string (TP_IFACE_CHANNEL_TYPE_TEXT));
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
      g_variant_new_uint32 (TP_ENTITY_TYPE_CONTACT));

  if (use_allowed)
    {
      allowed[0] = TP_PROP_CHANNEL_INTERFACE_SMS1_SMS_CHANNEL;
    }
  else
    {
      g_variant_builder_add (&fixed, "{sv}",
          TP_PROP_CHANNEL_INTERFACE_SMS1_SMS_CHANNEL,
          g_variant_new_boolean (TRUE));
    }

  if (add_extra_fixed)
    {
      g_variant_builder_add (&fixed, "{sv}", "ExtraBadgersRequired",
          g_variant_new_boolean (TRUE));
    }

  return g_variant_new_parsed ("(%@a{sv}, %^as)",
      g_variant_builder_end (&fixed),
      allowed);
}

static void
test_supports_sms (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        sms_class (FALSE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_sms (caps));

  g_object_unref (caps);

  /* Reject if more fixed properties are required */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        sms_class (TRUE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_sms (caps));

  g_object_unref (caps);

  /* Test with SMS as an allowed property */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        sms_class (FALSE, TRUE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_sms (caps));

  g_object_unref (caps);
}

static GVariant *
call_class (TpEntityType entity_type,
    gboolean initial_audio,
    gboolean initial_video,
    gboolean use_allowed,
    gboolean add_extra_fixed)
{
  GVariantBuilder fixed;
  GPtrArray *allowed;
  GVariant *ret;

  g_variant_builder_init (&fixed, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_CHANNEL_TYPE,
      g_variant_new_string (TP_IFACE_CHANNEL_TYPE_CALL1));
  g_variant_builder_add (&fixed, "{sv}", TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
      g_variant_new_uint32 (entity_type));

  allowed = g_ptr_array_new ();

  if (initial_audio)
    {
      if (use_allowed)
        {
          g_ptr_array_add (allowed, TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_AUDIO);
        }
      else
        {
          g_variant_builder_add (&fixed, "{sv}",
              TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_AUDIO,
              g_variant_new_boolean (TRUE));
        }
    }

  if (initial_video)
    {
      if (use_allowed)
        {
          g_ptr_array_add (allowed, TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_VIDEO);
        }
      else
        {
          g_variant_builder_add (&fixed, "{sv}",
              TP_PROP_CHANNEL_TYPE_CALL1_INITIAL_VIDEO,
              g_variant_new_boolean (TRUE));
        }
    }

  if (add_extra_fixed)
    {
      g_variant_builder_add (&fixed, "{sv}", "ExtraBadgersRequired",
          g_variant_new_boolean (TRUE));
    }

  g_ptr_array_add (allowed, NULL);
  ret = g_variant_new_parsed ("(%@a{sv}, %^as)",
      g_variant_builder_end (&fixed), allowed->pdata);
  g_ptr_array_unref (allowed);
  return ret;
}

static void
test_supports_call (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;

  /* A class with no audio/video can't do anything */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        call_class (TP_ENTITY_TYPE_CONTACT, FALSE, FALSE, FALSE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_audio_call (caps,
      TP_ENTITY_TYPE_CONTACT));
  g_assert (!tp_capabilities_supports_audio_video_call (caps,
      TP_ENTITY_TYPE_CONTACT));

  g_object_unref (caps);

  /* A class with only audio can't do audio_video */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        call_class (TP_ENTITY_TYPE_CONTACT, TRUE, FALSE, FALSE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_audio_call (caps,
      TP_ENTITY_TYPE_CONTACT));
  g_assert (!tp_capabilities_supports_audio_video_call (caps,
      TP_ENTITY_TYPE_CONTACT));

  g_object_unref (caps);

  /* A class with audio and video in fixed can't do audio only */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        call_class (TP_ENTITY_TYPE_CONTACT, TRUE, TRUE, FALSE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_audio_call (caps,
      TP_ENTITY_TYPE_CONTACT));
  g_assert (tp_capabilities_supports_audio_video_call (caps,
      TP_ENTITY_TYPE_CONTACT));

  g_object_unref (caps);

  /* A class with audio and video in allowed can do audio only */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        call_class (TP_ENTITY_TYPE_CONTACT, TRUE, TRUE, TRUE, FALSE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (tp_capabilities_supports_audio_call (caps,
      TP_ENTITY_TYPE_CONTACT));
  g_assert (tp_capabilities_supports_audio_video_call (caps,
      TP_ENTITY_TYPE_CONTACT));

  g_object_unref (caps);

  /* A class with unknown extra fixed can't do anything */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        call_class (TP_ENTITY_TYPE_CONTACT, TRUE, TRUE, TRUE, TRUE)),
      "contact-specific", FALSE,
      NULL);

  g_assert (!tp_capabilities_supports_audio_call (caps,
      TP_ENTITY_TYPE_CONTACT));
  g_assert (!tp_capabilities_supports_audio_video_call (caps,
      TP_ENTITY_TYPE_CONTACT));

  g_object_unref (caps);
}

static void
test_supports_ft_props (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  const gchar * const allow_uri[] = { TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_URI,
      NULL };
  const gchar * const allow_desc[] = {
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_DESCRIPTION, NULL };
  const gchar * const allow_date[] = {
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_DATE, NULL };
  const gchar * const allow_initial_offset[] = {
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_INITIAL_OFFSET, NULL };

  /* TpCapabilities containing no caps */
  caps = _tp_capabilities_new (NULL, TRUE);

  g_assert (!tp_capabilities_supports_file_transfer (caps));
  g_assert (!tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (!tp_capabilities_supports_file_transfer_description (caps));
  g_assert (!tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (!tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (NULL)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_file_transfer (caps));
  g_assert (!tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (!tp_capabilities_supports_file_transfer_description (caps));
  g_assert (!tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (!tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (allow_uri)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_file_transfer (caps));
  g_assert (tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (!tp_capabilities_supports_file_transfer_description (caps));
  g_assert (!tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (!tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (allow_desc)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_file_transfer (caps));
  g_assert (!tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (tp_capabilities_supports_file_transfer_description (caps));
  g_assert (!tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (!tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (allow_date)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_file_transfer (caps));
  g_assert (!tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (!tp_capabilities_supports_file_transfer_description (caps));
  g_assert (tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (!tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);

  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*]",
        ft_class (allow_initial_offset)),
      "contact-specific", TRUE,
      NULL);

  g_assert (tp_capabilities_supports_file_transfer (caps));
  g_assert (!tp_capabilities_supports_file_transfer_uri (caps));
  g_assert (!tp_capabilities_supports_file_transfer_description (caps));
  g_assert (!tp_capabilities_supports_file_transfer_timestamp (caps));
  g_assert (tp_capabilities_supports_file_transfer_initial_offset (caps));

  g_object_unref (caps);
}

static void
test_classes_variant (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  GVariant *v, *v2, *class, *fixed, *allowed;
  const gchar *chan_type;
  guint32 entity_type;
  const gchar **strv;

  /* TpCapabilities containing the text chats and ft caps */
  caps = tp_tests_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", g_variant_new_parsed ("[%*, %*]",
        text_chat_class (TP_ENTITY_TYPE_CONTACT),
        ft_class (NULL)),
      "contact-specific", FALSE,
      NULL);

  v = tp_capabilities_dup_channel_classes (caps);

  g_assert (v != NULL);
  g_assert_cmpstr (g_variant_get_type_string (v), ==, "a(a{sv}as)");

  g_assert_cmpuint (g_variant_n_children (v), ==, 2);

  /* Check text chats class */
  class = g_variant_get_child_value (v, 0);
  g_assert_cmpstr (g_variant_get_type_string (class), ==, "(a{sv}as)");
  g_assert_cmpuint (g_variant_n_children (class), ==, 2);

  fixed = g_variant_get_child_value (class, 0);
  allowed = g_variant_get_child_value (class, 1);

  g_assert_cmpuint (g_variant_n_children (fixed), ==, 2);

  g_assert (g_variant_lookup (fixed,
      TP_PROP_CHANNEL_CHANNEL_TYPE, "&s", &chan_type));
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_TEXT);

  g_assert (g_variant_lookup (fixed,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, "u", &entity_type));
  g_assert_cmpuint (entity_type, ==, TP_ENTITY_TYPE_CONTACT);

  g_assert_cmpuint (g_variant_n_children (allowed), ==, 0);

  g_variant_unref (class);
  g_variant_unref (fixed);
  g_variant_unref (allowed);

  /* Check ft class */
  class = g_variant_get_child_value (v, 1);
  g_assert_cmpstr (g_variant_get_type_string (class), ==, "(a{sv}as)");
  g_assert_cmpuint (g_variant_n_children (class), ==, 2);

  fixed = g_variant_get_child_value (class, 0);
  allowed = g_variant_get_child_value (class, 1);

  g_assert_cmpuint (g_variant_n_children (fixed), ==, 2);

  g_assert (g_variant_lookup (fixed,
      TP_PROP_CHANNEL_CHANNEL_TYPE, "&s", &chan_type));
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1);

  g_assert (g_variant_lookup (fixed,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, "u", &entity_type));
  g_assert_cmpuint (entity_type, ==, TP_ENTITY_TYPE_CONTACT);

  g_assert_cmpuint (g_variant_n_children (allowed), ==, 2);
  strv = g_variant_get_strv (allowed, NULL);
  g_assert (tp_strv_contains ((const gchar * const * ) strv,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_FILENAME));
  g_assert (tp_strv_contains ((const gchar * const * ) strv,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER1_SIZE));
  g_free (strv);

  g_variant_unref (class);
  g_variant_unref (fixed);
  g_variant_unref (allowed);

  /* Test GObject getter */
  g_object_get (caps, "channel-classes", &v2, NULL);
  g_assert (g_variant_equal (v, v2));

  g_variant_unref (v);
  g_variant_unref (v2);
  g_object_unref (caps);
}

int
main (int argc,
    char **argv)
{
#define TEST_PREFIX "/capabilities/"

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add (TEST_PREFIX "basics", Test, NULL, setup, test_basics,
      NULL);
  g_test_add (TEST_PREFIX "supports", Test, NULL, setup, test_supports,
      NULL);
  g_test_add (TEST_PREFIX "supports/ft-props", Test, NULL, setup,
      test_supports_ft_props, NULL);
  g_test_add (TEST_PREFIX "supports/tube", Test, NULL, setup,
      test_supports_tube, NULL);
  g_test_add (TEST_PREFIX "supports/room-list", Test, NULL, setup,
      test_supports_room_list, NULL);
  g_test_add (TEST_PREFIX "supports/sms", Test, NULL, setup,
      test_supports_sms, NULL);
  g_test_add (TEST_PREFIX "supports/call", Test, NULL, setup,
      test_supports_call, NULL);
  g_test_add (TEST_PREFIX "classes-variant", Test, NULL, setup,
      test_classes_variant, NULL);

  return g_test_run ();
}
