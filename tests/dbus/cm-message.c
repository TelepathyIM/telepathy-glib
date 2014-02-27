#include "config.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>
#include <telepathy-glib/cm-message.h>

#include "telepathy-glib/cm-message-internal.h"
#include <telepathy-glib/util-internal.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

typedef struct {
  TpBaseConnection *base_connection;
  TpHandleRepoIface *contact_repo;

  TpConnection *connection;

  GError *error /* initialized where needed */;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  tp_debug_set_flags ("all");

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_ENTITY_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
test_new_from_parts (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *parts;
  TpHandle sender;
  TpMessage *msg;
  GVariant *part;
  GVariant *part_vardict;
  gboolean valid;
  const gchar *s;
  gchar *token;
  guint32 u;

  parts = g_ptr_array_new_full (2, (GDestroyNotify) g_hash_table_unref);

  sender = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  g_ptr_array_add (parts, tp_asv_new (
        "message-type", G_TYPE_UINT, TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
        "message-sender", G_TYPE_UINT, sender,
        "message-token", G_TYPE_STRING, "token",
        "message-sent", G_TYPE_INT64, G_GINT64_CONSTANT (42),
        "message-received", G_TYPE_INT64, G_GINT64_CONSTANT (666),
        "scrollback", G_TYPE_BOOLEAN, TRUE,
        "pending-message-id", G_TYPE_UINT, 666,
        NULL));

  g_ptr_array_add (parts, tp_asv_new (
        "content-type", G_TYPE_STRING, "text/plain",
        "content", G_TYPE_STRING, "Badger",
        NULL));

  msg = _tp_cm_message_new_from_parts (test->base_connection, parts);

  g_ptr_array_unref (parts);

  g_assert (TP_IS_CM_MESSAGE (msg));
  g_assert_cmpuint (tp_message_count_parts (msg), ==, 2);

  part = tp_message_dup_part (msg, 0);
  g_variant_lookup (part, "message-sender", "u", &u);
  g_assert_cmpuint (u, ==, sender);
  g_variant_lookup (part, "message-sender-id", "&s", &s);
  g_assert_cmpstr (s, ==, "bob");
  g_variant_lookup (part, "message-token", "&s", &s);
  g_assert_cmpstr (s, ==, "token");
  g_variant_unref (part);

  part = tp_message_dup_part (msg, 1);
  g_variant_lookup (part, "content-type", "&s", &s);
  g_assert_cmpstr (s, ==, "text/plain");
  g_variant_lookup (part, "content", "&s", &s);
  g_assert_cmpstr (s, ==, "Badger");
  g_variant_unref (part);

  part_vardict = tp_message_dup_part (msg, 1);
  g_assert_cmpstr (g_variant_get_type_string (part_vardict), ==, "a{sv}");
  valid = g_variant_lookup (part_vardict, "content-type", "&s", &s);
  g_assert (valid);
  g_assert_cmpstr (s, ==, "text/plain");
  valid = g_variant_lookup (part_vardict, "content", "&s", &s);
  g_assert (valid);
  g_assert_cmpstr (s, ==, "Badger");
  g_variant_unref (part_vardict);

  g_assert_cmpuint (tp_message_get_message_type (msg), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE);
  g_assert_cmpuint (tp_cm_message_get_sender (msg), ==, sender);
  token = tp_message_dup_token (msg);
  g_assert_cmpstr (token, ==, "token");
  g_free (token);
  g_assert_cmpint ((gint) tp_message_get_sent_timestamp (msg), ==, 42);
  g_assert_cmpint ((gint) tp_message_get_received_timestamp (msg), ==, 666);
  g_assert_cmpint (tp_message_is_scrollback (msg), ==, TRUE);
  g_assert_cmpint (tp_message_is_rescued (msg), ==, FALSE);
  g_assert_cmpstr (tp_message_dup_supersedes (msg), ==, NULL);
  g_assert_cmpstr (tp_message_dup_specific_to_interface (msg), ==, NULL);
  g_assert_cmpint (tp_message_is_delivery_report (msg), ==, FALSE);
  g_assert_cmpuint (tp_message_get_pending_message_id (msg, &valid), ==, 666);
  g_assert (valid);

  g_object_unref (msg);
}

static void
test_new_text (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpHandle sender;
  TpMessage *msg;
  GVariant *part;
  guint u;
  const gchar *s;

  sender = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  msg = tp_cm_message_new_text (test->base_connection, sender,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "builds some stuff");
  g_assert (TP_IS_CM_MESSAGE (msg));
  g_assert_cmpuint (tp_message_count_parts (msg), ==, 2);

  part = tp_message_dup_part (msg, 0);
  g_variant_lookup (part, "message-sender", "u", &u);
  g_assert_cmpuint (u, ==, sender);
  g_variant_lookup (part, "message-type", "u", &u);
  g_assert_cmpuint (u, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_variant_lookup (part, "message-sender-id", "&s", &s);
  g_assert_cmpstr (s, ==, "bob");
  s = NULL;
  g_variant_lookup (part, "message-token", "&s", &s);
  g_assert_cmpstr (s, ==, NULL);
  g_variant_unref (part);

  part = tp_message_dup_part (msg, 1);
  g_variant_lookup (part, "content-type", "&s", &s);
  g_assert_cmpstr (s, ==, "text/plain");
  g_variant_lookup (part, "content", "&s", &s);
  g_assert_cmpstr (s, ==, "builds some stuff");
  g_variant_unref (part);

  g_assert_cmpuint (tp_message_get_message_type (msg), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_assert_cmpuint (tp_cm_message_get_sender (msg), ==, sender);
  g_assert_cmpstr (tp_message_dup_token (msg), ==, NULL);
  g_assert_cmpint ((gint) tp_message_get_sent_timestamp (msg), ==, 0);
  g_assert_cmpint ((gint) tp_message_get_received_timestamp (msg), ==, 0);
  g_assert_cmpint (tp_message_is_scrollback (msg), ==, FALSE);
  g_assert_cmpint (tp_message_is_rescued (msg), ==, FALSE);
  g_assert_cmpstr (tp_message_dup_supersedes (msg), ==, NULL);
  g_assert_cmpstr (tp_message_dup_specific_to_interface (msg), ==, NULL);
  g_assert_cmpint (tp_message_is_delivery_report (msg), ==, FALSE);

  g_object_unref (msg);
}

static void
test_set_message (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpHandle sender;
  TpMessage *msg, *echo;
  GVariant *part, *echo_parts, *echo_part;
  guint u;
  const gchar *s;

  sender = tp_handle_ensure (test->contact_repo, "escher@tuxedo.cat", NULL,
      &test->error);
  g_assert_no_error (test->error);

  msg = tp_cm_message_new (test->base_connection, 1);
  echo = tp_cm_message_new_text (test->base_connection, sender,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "meows");

  g_assert_cmpuint (tp_message_count_parts (echo), ==, 2);

  tp_message_set_uint32 (msg, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  tp_message_set_uint32 (msg, 0, "delivery-status",
      TP_DELIVERY_STATUS_DELIVERED);
  tp_cm_message_set_message (msg, 0, "delivery-echo", echo);

  /* destroy the echo */
  g_object_unref (echo);

  part = tp_message_dup_part (msg, 0);

  echo_parts = g_variant_lookup_value (part, "delivery-echo",
      G_VARIANT_TYPE ("aa{sv}"));
  g_assert (echo_parts != NULL);
  g_assert_cmpuint (g_variant_n_children (echo_parts), ==, 2);
  g_variant_unref (part);

  echo_part = g_variant_get_child_value (echo_parts, 0);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "message-type", "u", &u);
  g_assert_cmpuint (u, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_variant_unref (echo_part);

  echo_part = g_variant_get_child_value (echo_parts, 1);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "content", "&s", &s);
  g_assert_cmpstr (s, ==, "meows");
  g_variant_unref (echo_part);

  g_variant_unref (echo_parts);
  g_object_unref (msg);
}

static void
test_set_message_2 (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpHandle sender;
  TpMessage *msg, *echo;
  GVariant *part, *echo_parts, *echo_part;
  guint u;
  const gchar *s;

  sender = tp_handle_ensure (test->contact_repo, "escher@tuxedo.cat", NULL,
      &test->error);
  g_assert_no_error (test->error);

  msg = tp_cm_message_new (test->base_connection, 1);
  echo = tp_cm_message_new_text (test->base_connection, sender,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "meows");

  tp_message_set_uint32 (msg, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  tp_message_set_uint32 (msg, 0, "delivery-status",
      TP_DELIVERY_STATUS_DELIVERED);
  tp_cm_message_set_message (msg, 0, "delivery-echo", echo);

  /* change the echo */
  tp_message_set_string (echo, 1, "content", "yawns");

  part = tp_message_dup_part (msg, 0);

  echo_parts = g_variant_lookup_value (part, "delivery-echo",
      G_VARIANT_TYPE ("aa{sv}"));
  g_assert (echo_parts != NULL);
  g_assert_cmpuint (g_variant_n_children (echo_parts), ==, 2);
  g_variant_unref (part);

  echo_part = g_variant_get_child_value (echo_parts, 0);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "message-type", "u", &u);
  g_assert_cmpuint (u, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_variant_unref (echo_part);

  echo_part = g_variant_get_child_value (echo_parts, 1);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "content", "&s", &s);
  g_assert_cmpstr (s, ==, "meows");
  g_variant_unref (echo_part);

  g_variant_unref (echo_parts);
  g_object_unref (echo);
  g_object_unref (msg);
}

static void
_test_take_message_echo_destroyed (gpointer data,
    GObject *obj)
{
  gboolean *destroyed = data;

  *destroyed = TRUE;
}

static void
test_take_message (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpHandle sender;
  TpMessage *msg, *echo;
  GVariant *part, *echo_parts, *echo_part;
  guint u;
  const gchar *s;
  gboolean destroyed = FALSE;

  sender = tp_handle_ensure (test->contact_repo, "escher@tuxedo.cat", NULL,
      &test->error);
  g_assert_no_error (test->error);

  msg = tp_cm_message_new (test->base_connection, 1);
  echo = tp_cm_message_new_text (test->base_connection, sender,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "meows");

  /* add a weak ref so we know echo was destroyed */
  g_object_weak_ref (G_OBJECT (echo), _test_take_message_echo_destroyed,
      &destroyed);

  tp_message_set_uint32 (msg, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  tp_message_set_variant (msg, 0, "delivery-status",
      g_variant_new_uint32 (TP_DELIVERY_STATUS_DELIVERED));
  tp_cm_message_take_message (msg, 0, "delivery-echo", echo);

  /* ensure the message was destroyed */
  g_assert (destroyed == TRUE);

  part = tp_message_dup_part (msg, 0);

  echo_parts = g_variant_lookup_value (part, "delivery-echo",
      G_VARIANT_TYPE ("aa{sv}"));
  g_assert (echo_parts != NULL);
  g_assert_cmpuint (g_variant_n_children (echo_parts), ==, 2);
  g_variant_unref (part);

  echo_part = g_variant_get_child_value (echo_parts, 0);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "message-type", "u", &u);
  g_assert_cmpuint (u, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_variant_unref (echo_part);

  echo_part = g_variant_get_child_value (echo_parts, 1);
  g_assert (echo_part != 0);
  g_variant_lookup (echo_part, "content", "&s", &s);
  g_assert_cmpstr (s, ==, "meows");
  g_variant_unref (echo_part);

  g_variant_unref (echo_parts);
  g_object_unref (msg);
}

int
main (int argc,
    char **argv)
{
#define TEST_PREFIX "/cm-message/"

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add (TEST_PREFIX "new_from_parts", Test, NULL, setup,
      test_new_from_parts, teardown);
  g_test_add (TEST_PREFIX "new_text", Test, NULL, setup,
      test_new_text, teardown);
  g_test_add (TEST_PREFIX "set_message", Test, NULL, setup,
      test_set_message, teardown);
  g_test_add (TEST_PREFIX "set_message_2", Test, NULL, setup,
      test_set_message_2, teardown);
  g_test_add (TEST_PREFIX "take_message", Test, NULL, setup,
      test_take_message, teardown);

  return tp_tests_run_with_bus ();
}
