#include <stdio.h>

#include <glib.h>

#include "telepathy-glib/capabilities-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include "tests/lib/util.h"

typedef struct {
    gpointer unused;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");
}

static void
add_text_chat_class (GPtrArray *classes,
    TpHandleType handle_type)
{
  GHashTable *fixed;
  const gchar * const allowed[] = { NULL };
  GValueArray *arr;

  fixed = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          handle_type,
      NULL);

  arr = tp_value_array_build (2,
      TP_HASH_TYPE_STRING_VARIANT_MAP, fixed,
      G_TYPE_STRV, allowed,
      G_TYPE_INVALID);

  g_hash_table_unref (fixed);

  g_ptr_array_add (classes, arr);
}

static void
add_ft_class (GPtrArray *classes)
{
  GHashTable *fixed;
  const gchar * const allowed[] = {
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_FILENAME,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_SIZE,
      NULL };
  GValueArray *arr;

  fixed = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
      NULL);

  arr = tp_value_array_build (2,
      TP_HASH_TYPE_STRING_VARIANT_MAP, fixed,
      G_TYPE_STRV, allowed,
      G_TYPE_INVALID);

  g_hash_table_unref (fixed);

  g_ptr_array_add (classes, arr);
}

static void
test_basics (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  GPtrArray *classes;
  GValueArray *arr;
  GHashTable *fixed;
  GStrv allowed;
  const gchar *chan_type;
  TpHandleType handle_type;
  gboolean valid;

  /* TpCapabilities containing the text chats and ft caps */
  classes = g_ptr_array_sized_new (2);
  add_text_chat_class (classes, TP_HANDLE_TYPE_CONTACT);
  add_ft_class (classes);

  caps = test_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", FALSE,
      NULL);

  g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
     classes);

  g_assert (!tp_capabilities_is_specific_to_contact (caps));
  classes = tp_capabilities_get_channel_classes (caps);

  g_assert_cmpuint (classes->len, ==, 2);

  /* Check text chats class */
  arr = g_ptr_array_index (classes, 0);
  g_assert_cmpuint (arr->n_values, ==, 2);

  fixed = g_value_get_boxed (g_value_array_get_nth (arr, 0));
  allowed = g_value_get_boxed (g_value_array_get_nth (arr, 1));

  g_assert_cmpuint (g_hash_table_size (fixed), ==, 2);

  chan_type = tp_asv_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_TEXT);

  handle_type = tp_asv_get_uint32 (fixed, TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      &valid);
  g_assert (valid);
  g_assert_cmpuint (handle_type, ==, TP_HANDLE_TYPE_CONTACT);

  g_assert_cmpuint (g_strv_length (allowed), ==, 0);

  /* Check ft class */
  arr = g_ptr_array_index (classes, 1);
  g_assert_cmpuint (arr->n_values, ==, 2);

  fixed = g_value_get_boxed (g_value_array_get_nth (arr, 0));
  allowed = g_value_get_boxed (g_value_array_get_nth (arr, 1));

  g_assert_cmpuint (g_hash_table_size (fixed), ==, 2);

  chan_type = tp_asv_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE);
  g_assert_cmpstr (chan_type, ==, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);

  handle_type = tp_asv_get_uint32 (fixed, TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
      &valid);
  g_assert (valid);
  g_assert_cmpuint (handle_type, ==, TP_HANDLE_TYPE_CONTACT);

  g_assert_cmpuint (g_strv_length (allowed), ==, 2);
  g_assert (tp_strv_contains ((const gchar * const * ) allowed,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_FILENAME));
  g_assert (tp_strv_contains ((const gchar * const * ) allowed,
      TP_PROP_CHANNEL_TYPE_FILE_TRANSFER_SIZE));

  g_object_unref (caps);
}

static void
test_supports (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpCapabilities *caps;
  GPtrArray *classes;

  /* TpCapabilities containing the text chats caps */
  classes = g_ptr_array_sized_new (1);
  add_text_chat_class (classes, TP_HANDLE_TYPE_CONTACT);

  caps = test_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", TRUE,
      NULL);

  g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
     classes);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (tp_capabilities_supports_text_chats (caps));
  g_assert (!tp_capabilities_supports_text_chatrooms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing the text chatrooms caps */
  classes = g_ptr_array_sized_new (1);
  add_text_chat_class (classes, TP_HANDLE_TYPE_ROOM);

  caps = test_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", TRUE,
      NULL);

  g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
     classes);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (!tp_capabilities_supports_text_chats (caps));
  g_assert (tp_capabilities_supports_text_chatrooms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing both caps */
  classes = g_ptr_array_sized_new (2);
  add_text_chat_class (classes, TP_HANDLE_TYPE_CONTACT);
  add_text_chat_class (classes, TP_HANDLE_TYPE_ROOM);

  caps = test_object_new_static_class (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", TRUE,
      NULL);

  g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
     classes);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (tp_capabilities_supports_text_chats (caps));
  g_assert (tp_capabilities_supports_text_chatrooms (caps));

  g_object_unref (caps);

  /* TpCapabilities containing no caps */
  caps = _tp_capabilities_new (NULL, TRUE);

  g_assert (tp_capabilities_is_specific_to_contact (caps));
  g_assert (!tp_capabilities_supports_text_chats (caps));
  g_assert (!tp_capabilities_supports_text_chatrooms (caps));

  classes = tp_capabilities_get_channel_classes (caps);
  g_assert_cmpuint (classes->len, ==, 0);

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

  return g_test_run ();
}
