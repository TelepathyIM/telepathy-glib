#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/cm-message.h>

#include "telepathy-glib/cm-message-internal.h"
#include <telepathy-glib/util-internal.h>

#include "tests/lib/simple-conn.h"
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
  g_type_init ();
  tp_debug_set_flags ("all");

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

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
  const GHashTable *part;
  gboolean valid;

  parts = _tp_g_ptr_array_new_full (2, (GDestroyNotify) g_hash_table_unref);

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

  g_ptr_array_free (parts, TRUE);

  g_assert (TP_IS_CM_MESSAGE (msg));
  g_assert_cmpuint (tp_message_count_parts (msg), ==, 2);

  part = tp_message_peek (msg, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (part, "message-sender", NULL), ==,
      sender);
  g_assert_cmpstr (tp_asv_get_string (part, "message-sender-id"), ==,
      "bob");
  g_assert_cmpstr (tp_asv_get_string (part, "message-token"), ==,
      "token");

  part = tp_message_peek (msg, 1);
  g_assert_cmpstr (tp_asv_get_string (part, "content-type"), ==,
      "text/plain");
  g_assert_cmpstr (tp_asv_get_string (part, "content"), ==,
      "Badger");

  g_assert_cmpuint (tp_message_get_message_type (msg), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE);
  g_assert_cmpuint (tp_cm_message_get_sender (msg), ==, sender);
  g_assert_cmpstr (tp_message_get_token (msg), ==, "token");
  g_assert_cmpint ((gint) tp_message_get_sent_timestamp (msg), ==, 42);
  g_assert_cmpint ((gint) tp_message_get_received_timestamp (msg), ==, 666);
  g_assert_cmpint (tp_message_is_scrollback (msg), ==, TRUE);
  g_assert_cmpint (tp_message_is_rescued (msg), ==, FALSE);
  g_assert_cmpstr (tp_message_get_supersedes (msg), ==, NULL);
  g_assert_cmpstr (tp_message_get_specific_to_interface (msg), ==, NULL);
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
  const GHashTable *part;

  sender = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  msg = tp_cm_message_new_text (test->base_connection, sender,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "builds some stuff");
  g_assert (TP_IS_CM_MESSAGE (msg));
  g_assert_cmpuint (tp_message_count_parts (msg), ==, 2);

  part = tp_message_peek (msg, 0);
  g_assert_cmpuint (tp_asv_get_uint32 (part, "message-sender", NULL), ==,
      sender);
  g_assert_cmpuint (tp_asv_get_uint32 (part, "message-type", NULL), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_assert_cmpstr (tp_asv_get_string (part, "message-sender-id"), ==,
      "bob");
  g_assert_cmpstr (tp_asv_get_string (part, "message-token"), ==, NULL);

  part = tp_message_peek (msg, 1);
  g_assert_cmpstr (tp_asv_get_string (part, "content-type"), ==,
      "text/plain");
  g_assert_cmpstr (tp_asv_get_string (part, "content"), ==,
      "builds some stuff");

  g_assert_cmpuint (tp_message_get_message_type (msg), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  g_assert_cmpuint (tp_cm_message_get_sender (msg), ==, sender);
  g_assert_cmpstr (tp_message_get_token (msg), ==, NULL);
  g_assert_cmpint ((gint) tp_message_get_sent_timestamp (msg), ==, 0);
  g_assert_cmpint ((gint) tp_message_get_received_timestamp (msg), ==, 0);
  g_assert_cmpint (tp_message_is_scrollback (msg), ==, FALSE);
  g_assert_cmpint (tp_message_is_rescued (msg), ==, FALSE);
  g_assert_cmpstr (tp_message_get_supersedes (msg), ==, NULL);
  g_assert_cmpstr (tp_message_get_specific_to_interface (msg), ==, NULL);
  g_assert_cmpint (tp_message_is_delivery_report (msg), ==, FALSE);

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

  return g_test_run ();
}
