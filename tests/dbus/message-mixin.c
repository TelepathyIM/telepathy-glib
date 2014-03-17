/* Regression test for the message mixin and the echo-2 example CM.
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "telepathy-glib/reentrants.h"

#include "examples/cm/echo-message-parts/connection-manager.h"
#include "examples/cm/echo-message-parts/chan.h"
#include "examples/cm/echo-message-parts/conn.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
}

static guint message_received_count = 0;
static guint last_message_received_sender = 0;
static guint last_message_received_type = 0;
static guint last_message_received_n_parts = 0;
static guint last_message_received_id = 0;

static guint message_sent_count = 0;
static guint last_message_sent_type = 0;
static gchar *last_message_sent_token = NULL;
static guint last_message_sent_n_parts = 0;
static guint last_message_sent_sender = 0;
static gchar *last_message_sent_sender_id = NULL;

static void
print_part (gpointer k,
            gpointer v,
            gpointer user_data)
{
  const gchar *key = k;
  gchar *contents = g_strdup_value_contents (v);

  g_print ("        %s: %s\n", key, contents);
  g_free (contents);
}

static void
on_message_received (TpChannel *chan,
                     const GPtrArray *parts,
                     gpointer data,
                     GObject *object)
{
  guint i;
  GHashTable *headers = g_ptr_array_index (parts, 0);
  guint id;
  guint received;
  guint sender;
  guint type;

  g_assert (parts->len >= 1);

  id = tp_asv_get_uint32 (headers, "pending-message-id", NULL);
  type = tp_asv_get_uint32 (headers, "message-type", NULL);
  sender = tp_asv_get_uint32 (headers, "message-sender", NULL);
  received = tp_asv_get_uint32 (headers, "message-received", NULL);

  g_print ("%p: MessageReceived #%u: received at %u, sender %u, type %u, "
      "%u parts\n", chan, id, received, sender, type, parts->len);

  for (i = 0; i < parts->len; i++)
    {
      g_print ("    Part %u:\n", i);
      g_hash_table_foreach (g_ptr_array_index (parts, i), print_part, NULL);
    }

  message_received_count++;
  last_message_received_type = type;
  last_message_received_sender = sender;
  last_message_received_n_parts = parts->len;
  last_message_received_id = id;
}

static void
on_message_sent (TpChannel *chan,
                 const GPtrArray *parts,
                 guint flags,
                 const gchar *token,
                 gpointer data,
                 GObject *object)
{
  guint i;
  GHashTable *headers = g_ptr_array_index (parts, 0);
  guint type;
  guint sender;
  const gchar *sender_id;

  g_assert (parts->len >= 1);

  type = tp_asv_get_uint32 (headers, "message-type", NULL);
  sender = tp_asv_get_uint32 (headers, "message-sender", NULL);
  sender_id = tp_asv_get_string (headers, "message-sender-id");

  g_print ("%p: MessageSent with token '%s': type %u, %u parts\n",
      chan, token, type, parts->len);

  for (i = 0; i < parts->len; i++)
    {
      g_print ("    Part %u:\n", i);
      g_hash_table_foreach (g_ptr_array_index (parts, i), print_part, NULL);
    }

  message_sent_count++;
  last_message_sent_type = type;
  last_message_sent_n_parts = parts->len;
  g_free (last_message_sent_token);
  last_message_sent_token = g_strdup (token);
  g_free (last_message_sent_sender_id);
  last_message_sent_sender_id = g_strdup (sender_id);
  last_message_sent_sender = sender;
}

static void
on_messages_removed (TpChannel *chan,
                     const GArray *ids,
                     gpointer data,
                     GObject *object)
{
  guint i;

  g_print ("%p: PendingMessagesRemoved: %u messages\n", chan, ids->len);

  for (i = 0; i < ids->len; i++)
    {
      g_print ("    %u\n", g_array_index (ids, guint, i));
    }
}

static void
test (Fixture *f,
    gconstpointer data)
{
  ExampleEcho2ConnectionManager *service_cm;
  TpBaseConnectionManager *service_cm_as_base;
  TpDBusDaemon *dbus;
  TpConnectionManager *cm;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;
  TpHandle handle;
  gboolean ok;
  GHashTable *parameters;
  GQuark connected_feature[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  GTestDBus *test_dbus;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_dbus_unset ();
  test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (test_dbus);

  dbus = tp_tests_dbus_daemon_dup_or_die ();

  service_cm = EXAMPLE_ECHO_2_CONNECTION_MANAGER (
      tp_tests_object_new_static_class (
        EXAMPLE_TYPE_ECHO_2_CONNECTION_MANAGER,
        NULL));
  g_assert (service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  cm = tp_connection_manager_new (dbus, "example_echo_2", NULL, &error);
  g_assert (cm != NULL);
  tp_tests_proxy_run_until_prepared (cm, NULL);

  parameters = tp_asv_new (
      "account", G_TYPE_STRING, "me@example.com",
      NULL);

  tp_cli_connection_manager_run_request_connection (cm, -1,
      "example", parameters, &name, &conn_path, &error, NULL);
  g_assert_no_error (error);

  g_hash_table_unref (parameters);

  conn = tp_tests_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  g_assert_no_error (error);

  tp_cli_connection_call_connect (conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (conn, connected_feature);

    {
      GHashTable *properties = NULL;
      GPtrArray *arr;

      /* check that it has the requestable channel class */
      tp_cli_dbus_properties_run_get_all (conn, -1,
          TP_IFACE_CONNECTION, &properties, &error, NULL);
      g_assert_no_error (error);

      arr = tp_asv_get_boxed (properties, "RequestableChannelClasses",
          TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST);
      g_assert_cmpuint (arr->len, ==, 1);

      g_hash_table_unref (properties);
    }

    {
      GHashTable *request = tp_asv_new (
          TP_PROP_CHANNEL_CHANNEL_TYPE,
              G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
          TP_PROP_CHANNEL_TARGET_ENTITY_TYPE,
              G_TYPE_UINT, TP_ENTITY_TYPE_CONTACT,
          TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, "them@example.com",
          NULL);

      tp_cli_connection_interface_requests_run_create_channel (conn, -1,
          request, &chan_path, &parameters, &error, NULL);
      g_assert_no_error (error);

      g_hash_table_unref (request);
    }

  chan = tp_tests_channel_new_from_properties (conn, chan_path, parameters, &error);
  g_assert_no_error (error);
  g_hash_table_unref (parameters);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  handle = tp_channel_get_handle (chan, NULL);

  MYASSERT (
      tp_cli_channel_type_text_connect_to_message_received (chan,
          on_message_received, NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_channel_type_text_connect_to_message_sent (
        chan, on_message_sent, NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (
      tp_cli_channel_type_text_connect_to_pending_messages_removed (
        chan, on_messages_removed, NULL, NULL, NULL, NULL) != NULL, "");

  /* Get the initial properties */

    {
      const GValue *value;
      gchar *contents;
      GArray *types;
      GPtrArray *messages;
      GHashTable *properties = NULL;

      tp_cli_dbus_properties_run_get_all (chan, -1,
          TP_IFACE_CHANNEL_TYPE_TEXT, &properties, &error, NULL);
      g_assert_no_error (error);

      g_print ("\n\n==== Examining properties ====\n\n");

      g_assert_cmpuint (g_hash_table_size (properties), ==, 5);

      MYASSERT (tp_asv_get_uint32 (properties, "MessagePartSupportFlags", NULL)
          == ( TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT
             | TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS
             ), "");

      MYASSERT ((value = tp_asv_lookup (properties, "SupportedContentTypes"))
          != NULL, "");
      MYASSERT (G_VALUE_HOLDS (value, G_TYPE_STRV), "");
      contents = g_strdup_value_contents (value);
      g_message ("%s", contents);
      g_free (contents);

      g_assert ((value = tp_asv_lookup (properties, "MessageTypes"))
          != NULL);
      g_assert (G_VALUE_HOLDS (value, DBUS_TYPE_G_UINT_ARRAY));
      types = g_value_get_boxed (value);
      g_assert_cmpuint (types->len, ==, 3);
      g_assert_cmpuint (g_array_index (types, guint, 0), ==,
          TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);
      g_assert_cmpuint (g_array_index (types, guint, 1), ==,
          TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
      g_assert_cmpuint (g_array_index (types, guint, 2), ==,
          TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE);

      MYASSERT ((value = tp_asv_lookup (properties, "PendingMessages"))
          != NULL, "");
      MYASSERT (G_VALUE_HOLDS_BOXED (value), "");
      messages = g_value_get_boxed (value);
      MYASSERT (messages->len == 0, "%u", messages->len);

      g_assert_cmpuint (tp_asv_get_uint32 (properties,
            "DeliveryReportingSupport", NULL), ==,
          TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);

      g_hash_table_unref (properties);
    }

  g_print ("\n\n==== Starting test: lolcat ====\n");

  /* Send a multi-part message
   *
   * Verify that we get a MessageSent signal for said message, and a
   * MessageReceived signal for the echo.
   *
   * Because this message contains an image, we must set the
   * Channel_Text_Message_Flag_Non_Text_Content.
   */

  message_sent_count = 0;
  message_received_count = 0;

#undef EXPECTED_TEXT
#define EXPECTED_TEXT ("Here is a photo of a cat:\n"\
    "[IMG: lol!]\n"\
    "It's in ur regression tests verifying ur designs!")

    {
      GPtrArray *send_parts = g_ptr_array_sized_new (3);
      GHashTable *part;
      guint i;

      /* Empty headers part */
      part = tp_asv_new (NULL, NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "alternative", G_TYPE_STRING, "main",
          "content-type", G_TYPE_STRING, "text/html",
          "content", G_TYPE_STRING,
            "Here is a photo of a cat:<br />"
            "<img src=\"cid:lolcat\" alt=\"lol!\" /><br />"
            "It's in ur regression tests verifying ur designs!",
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "alternative", G_TYPE_STRING, "main",
          "content-type", G_TYPE_STRING, "text/plain",
          "content", G_TYPE_STRING, EXPECTED_TEXT,
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "identifier", G_TYPE_STRING, "lolcat",
          "content-type", G_TYPE_STRING, "image/jpeg",
          NULL);
      g_hash_table_insert (part, "content", tp_g_value_slice_new_bytes (
            14, "\xff\xd8\xff\xe0\x00\x10JFIF\x00..."));
      g_ptr_array_add (send_parts, part);

      tp_cli_channel_type_text_call_send_message (chan, -1,
          send_parts, 0 /* flags */, NULL, NULL, NULL, NULL);

      /* wait for pending events to be delivered */
      while (message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message\n");

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_contact_get_handle (tp_connection_get_self_contact (conn)));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  MYASSERT (last_message_sent_n_parts == 4,
      ": %u != 4", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 4,
      ": %u != 4", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: lolcat with PNG alternative ====\n");

  /* This time, the non-text content has an alternative. */

  message_sent_count = 0;
  message_received_count = 0;

#undef EXPECTED_TEXT
#define EXPECTED_TEXT ("Here is a photo of a cat:\n"\
    "[IMG: lol!]\n"\
    "It's in ur regression tests verifying ur designs!")

    {
      GPtrArray *send_parts = g_ptr_array_sized_new (3);
      GHashTable *part;
      guint i;

      /* Empty headers part */
      part = tp_asv_new (NULL, NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "alternative", G_TYPE_STRING, "main",
          "identifier", G_TYPE_STRING, "html",
          "content-type", G_TYPE_STRING, "text/html",
          "content", G_TYPE_STRING,
            "Here is a photo of a cat:<br />"
            "<img src=\"cid:lolcat\" alt=\"lol!\" /><br />"
            "It's in ur regression tests verifying ur designs!",
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "alternative", G_TYPE_STRING, "main",
          "identifier", G_TYPE_STRING, "text",
          "content-type", G_TYPE_STRING, "text/plain",
          "content", G_TYPE_STRING, EXPECTED_TEXT,
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "identifier", G_TYPE_STRING, "jpeg",
          "alternative", G_TYPE_STRING, "lolcat",
          "content-type", G_TYPE_STRING, "image/jpeg",
          NULL);
      g_hash_table_insert (part, "content", tp_g_value_slice_new_bytes (
            14, "\xff\xd8\xff\xe0\x00\x10JFIF\x00..."));
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "identifier", G_TYPE_STRING, "png",
          "alternative", G_TYPE_STRING, "lolcat",
          "content-type", G_TYPE_STRING, "image/png",
          NULL);
      g_hash_table_insert (part, "content", tp_g_value_slice_new_bytes (
            12, "\x89PNG\x0d\x0a\x1a\x0a\x00..."));
      g_ptr_array_add (send_parts, part);

      tp_cli_channel_type_text_call_send_message (chan, -1,
          send_parts, 0 /* flags */, NULL, NULL, NULL, NULL);

      /* wait for pending events to be delivered */
      while (message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message\n");

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_contact_get_handle (tp_connection_get_self_contact (conn)));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_n_parts == 5,
      ": %u != 5", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 5,
      ": %u != 5", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: defragment ====\n");

  /* Send a multi-part message using the Messages API.
   * This one has multiple text/plain parts, which the Text API needs to
   * concatenate.
   */

  message_sent_count = 0;
  message_received_count = 0;

#undef EXPECTED_TEXT
#define EXPECTED_TEXT ("I'm on a roll\n"\
    "I'm on a roll this time\n"\
    "I feel my luck could change\n")

    {
      GPtrArray *send_parts = g_ptr_array_sized_new (3);
      GHashTable *part;
      guint i;

      /* Empty headers part */
      part = tp_asv_new (NULL, NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "content", G_TYPE_STRING, "I'm on a roll\n",
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "content", G_TYPE_STRING, "I'm on a roll this time\n",
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "content", G_TYPE_STRING, "I feel my luck could change\n",
          NULL);
      g_ptr_array_add (send_parts, part);

      tp_cli_channel_type_text_call_send_message (chan, -1,
          send_parts, 0 /* flags */, NULL, NULL, NULL, NULL);

      /* wait for pending events to be delivered */
      while (message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message\n");

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_contact_get_handle (tp_connection_get_self_contact (conn)));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_n_parts == 4,
      ": %u != 4", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 4,
      ": %u != 4", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: multilingual ====\n");

  /* Send a multi-part message using the Messages API.
   * This one has multiple text/plain parts, but they're alternatives,
   * so the old Text API picks the "best" (first) one.
   */

  message_sent_count = 0;
  message_received_count = 0;

#undef EXPECTED_TEXT
#define EXPECTED_TEXT "nous badgerez le coleur du stream de la video"

    {
      GPtrArray *send_parts = g_ptr_array_sized_new (3);
      GHashTable *part;
      guint i;

      /* Empty headers part */
      part = tp_asv_new (NULL, NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "alternative", G_TYPE_STRING, "alt",
          "lang", G_TYPE_STRING, "fr_CA@collabora",
          "content", G_TYPE_STRING, EXPECTED_TEXT,
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "alternative", G_TYPE_STRING, "alt",
          "lang", G_TYPE_STRING, "en_GB",
          "content", G_TYPE_STRING,
            "we're fixing the colour of the video stream",
          NULL);
      g_ptr_array_add (send_parts, part);

      part = tp_asv_new (
          "content-type", G_TYPE_STRING, "text/plain",
          "alternative", G_TYPE_STRING, "alt",
          "lang", G_TYPE_STRING, "en_US",
          "content", G_TYPE_STRING,
            "we're fixing the color of the video stream",
          NULL);
      g_ptr_array_add (send_parts, part);

      tp_cli_channel_type_text_call_send_message (chan, -1,
          send_parts, 0 /* flags */, NULL, NULL, NULL, NULL);

      /* wait for pending events to be delivered */
      while (message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message\n");

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  MYASSERT (last_message_sent_n_parts == 4,
      ": %u != 4", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 4,
      ": %u != 4", last_message_received_n_parts);

  g_print ("\n\n==== Listing messages ====\n");

    {
      GValue *value = NULL;

      tp_cli_dbus_properties_run_get (chan, -1, TP_IFACE_CHANNEL_TYPE_TEXT,
          "PendingMessages", &value, &error, NULL);
      g_assert_no_error (error);

      g_print ("Freeing\n");
      g_value_unset (value);
      g_free (value);
    }

  g_print ("\n\n==== Acknowledging messages using a wrong ID ====\n");

    {
      GArray *ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 2);
      /* we assume this ID won't be valid (implementation detail: message
       * IDs are increasing integers) */
      guint bad_id = 31337;

      g_array_append_val (ids, last_message_received_id);
      g_array_append_val (ids, bad_id);

      MYASSERT (
          !tp_cli_channel_type_text_run_acknowledge_pending_messages (chan, -1,
          ids, &error, NULL),
          "");
      MYASSERT (error != NULL, "");
      MYASSERT (error->domain == TP_ERROR, "%s",
          g_quark_to_string (error->domain));
      MYASSERT (error->code == TP_ERROR_INVALID_ARGUMENT, "%u", error->code);
      g_error_free (error);
      error = NULL;

      g_array_unref (ids);

      /* The test "Acknowledging one message", will fail if the
       * last_message_received_id was acknowledged despite the
       * error */
    }

  g_print ("\n\n==== Getting properties again ====\n");

    {
      const GValue *value;
      gchar *contents;
      GPtrArray *messages;
      GHashTable *properties = NULL;
      guint i;

      tp_cli_dbus_properties_run_get_all (chan, -1,
          TP_IFACE_CHANNEL_TYPE_TEXT, &properties, &error, NULL);
      g_assert_no_error (error);

      g_print ("\n\n==== Examining properties ====\n\n");

      g_assert_cmpuint (g_hash_table_size (properties), ==, 5);

      MYASSERT (tp_asv_get_uint32 (properties, "MessagePartSupportFlags", NULL)
          == ( TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT
             | TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS
             ), "");

      MYASSERT ((value = tp_asv_lookup (properties, "SupportedContentTypes"))
          != NULL, "");
      MYASSERT (G_VALUE_HOLDS (value, G_TYPE_STRV), "");
      contents = g_strdup_value_contents (value);
      g_message ("%s", contents);
      g_free (contents);

      g_assert_cmpuint (tp_asv_get_uint32 (properties,
            "DeliveryReportingSupport", NULL), ==,
          TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);

      MYASSERT ((value = tp_asv_lookup (properties, "PendingMessages"))
          != NULL, "");
      MYASSERT (G_VALUE_HOLDS_BOXED (value), "");
      messages = g_value_get_boxed (value);
      MYASSERT (messages->len == 4, ": %u", messages->len);

      for (i = 0; i < messages->len; i++)
        {
          GPtrArray *message = g_ptr_array_index (messages, i);
          guint j;

          g_print ("Message %u:\n", i);

          for (j = 0; j < message->len; j++)
            {
              g_print ("    Part %u:\n", j);
              g_hash_table_foreach (g_ptr_array_index (message, j),
                  print_part, NULL);
            }
        }

      g_hash_table_unref (properties);
    }

  g_print ("\n\n==== Acknowledging one message ====\n");

    {
      /* As a regression test for
       * <https://bugs.freedesktop.org/show_bug.cgi?id=40523>, we include the
       * ID of the message we want to ack twice. This used to cause a
       * double-free.
       */
      GArray *msgid = g_array_sized_new (FALSE, FALSE, sizeof (guint), 2);

      g_array_append_val (msgid, last_message_received_id);
      g_array_append_val (msgid, last_message_received_id);

      tp_cli_channel_type_text_run_acknowledge_pending_messages (chan, -1,
          msgid, &error, NULL);
      g_assert_no_error (error);

      g_array_unref (msgid);
    }

  g_print ("\n\n==== Closing channel ====\n");

    {
      GValue *value = NULL;
      GPtrArray *channels;

      MYASSERT (tp_cli_channel_interface_destroyable1_run_destroy (chan,
              -1, &error, NULL), "");
      g_assert_no_error (error);
      MYASSERT (tp_proxy_get_invalidated (chan) != NULL, "");

      /* assert that the channel has really gone */
      MYASSERT (tp_cli_dbus_properties_run_get (conn, -1,
              TP_IFACE_CONNECTION_INTERFACE_REQUESTS, "Channels",
              &value, &error, NULL), "");
      g_assert_no_error (error);

      channels = g_value_get_boxed (value);
      MYASSERT (channels->len == 0, "%u != 0", channels->len);

      g_value_unset (value);
      g_free (value);
    }

  g_print ("\n\n==== End of tests ====\n");

  tp_tests_connection_assert_disconnect_succeeds (conn);

  g_object_unref (chan);
  g_object_unref (conn);

  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  g_free (last_message_sent_token);
  g_free (last_message_sent_sender_id);

  g_test_dbus_down (test_dbus);
  tp_tests_assert_last_unref (&test_dbus);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/message-mixin", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
