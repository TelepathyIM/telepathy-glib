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

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "examples/cm/echo-message-parts/connection-manager.h"
#include "examples/cm/echo-message-parts/chan.h"
#include "examples/cm/echo-message-parts/conn.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static guint received_count = 0;
static guint last_received_id = 0;
static guint last_received_sender = 0;
static guint last_received_type = 0;
static guint last_received_flags = 0;
static gchar *last_received_text = NULL;

static guint sent_count = 0;
static guint last_sent_type = 0;
static gchar *last_sent_text = NULL;

static void
on_sent (TpChannel *chan,
         guint timestamp,
         guint type,
         const gchar *text,
         gpointer data,
         GObject *object)
{
  g_print ("%p: Sent: time %u, type %u, text '%s'\n",
      chan, timestamp, type, text);

  sent_count++;
  last_sent_type = type;
  g_free (last_sent_text);
  last_sent_text = g_strdup (text);
}

static void
on_received (TpChannel *chan,
             guint id,
             guint timestamp,
             guint sender,
             guint type,
             guint flags,
             const gchar *text,
             gpointer data,
             GObject *object)
{
  g_print ("%p: Received #%u: time %u, sender %u, type %u, flags %u, "
      "text '%s'\n", chan, id, timestamp, sender, type, flags, text);

  received_count++;
  last_received_id = id;
  last_received_sender = sender;
  last_received_type = type;
  last_received_flags = flags;
  g_free (last_received_text);
  last_received_text = g_strdup (text);
}

static guint message_received_count = 0;
static guint last_message_received_sender = 0;
static guint last_message_received_type = 0;
static guint last_message_received_n_parts = 0;

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
print_part_content (gpointer k,
                    gpointer v,
                    gpointer d)
{
  guint part_number = GPOINTER_TO_UINT (k);
  gchar *contents = g_strdup_value_contents (v);

  g_print ("        %u: %s\n", part_number, contents);
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

int
main (int argc,
      char **argv)
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

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");
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

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  g_assert_no_error (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  g_assert_no_error (error);

    {
      GHashTable *properties = NULL;
      GPtrArray *arr;

      /* check that it has the requestable channel class */
      tp_cli_dbus_properties_run_get_all (conn, -1,
          TP_IFACE_CONNECTION_INTERFACE_REQUESTS, &properties, &error, NULL);
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
          TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
              G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
          TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, "them@example.com",
          NULL);

      tp_cli_connection_interface_requests_run_create_channel (conn, -1,
          request, &chan_path, &parameters, &error, NULL);
      g_assert_no_error (error);

      g_hash_table_unref (request);
    }

  chan = tp_channel_new_from_properties (conn, chan_path, parameters, &error);
  g_assert_no_error (error);
  g_hash_table_unref (parameters);

  tp_channel_run_until_ready (chan, &error, NULL);
  g_assert_no_error (error);

  handle = tp_channel_get_handle (chan, NULL);

  MYASSERT (tp_cli_channel_type_text_connect_to_received (chan, on_received,
      NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_channel_type_text_connect_to_sent (chan, on_sent,
      NULL, NULL, NULL, NULL) != NULL, "");

  MYASSERT (
      tp_cli_channel_interface_messages_connect_to_message_received (chan,
          on_message_received, NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_channel_interface_messages_connect_to_message_sent (
        chan, on_message_sent, NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (
      tp_cli_channel_interface_messages_connect_to_pending_messages_removed (
        chan, on_messages_removed, NULL, NULL, NULL, NULL) != NULL, "");

  /* Get the initial properties */

    {
      const GValue *value;
      gchar *contents;
      GArray *types;
      GPtrArray *messages;
      GHashTable *properties = NULL;

      tp_cli_dbus_properties_run_get_all (chan, -1,
          TP_IFACE_CHANNEL_INTERFACE_MESSAGES, &properties, &error, NULL);
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

  /* Send three messages using the old Text API:
   *
   * (normal) Hello, world!
   * (action) /me drinks coffee
   * (notice) Printer on fire
   *
   * Verify that for each of them, we get a Sent signal in the old Text
   * API, and a Received signal for the echo; also a MessageSent signal
   * in the new Messages API, and a MessageReceived signal for the echo.
   */

  g_print ("\n\n==== Starting test: NORMAL ====\n");

  sent_count = 0;
  received_count = 0;
  message_sent_count = 0;
  message_received_count = 0;
  tp_cli_channel_type_text_run_send (chan, -1,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, "Hello, world!",
      &error, NULL);
  /* wait for pending events to be delivered */
  while (received_count < 1 || message_received_count < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (error);
  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, "Hello, world!"),
      ": '%s' != '%s'", last_sent_text, "Hello, world!");
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, "Hello, world!"),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_n_parts == 2,
      ": %u != 2", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 2,
      ": %u != 2", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: ACTION ====\n");

  sent_count = 0;
  received_count = 0;
  message_sent_count = 0;
  message_received_count = 0;
  tp_cli_channel_type_text_run_send (chan, -1,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION, "drinks coffee",
      &error, NULL);
  /* wait for pending events to be delivered */
  while (received_count < 1 || message_received_count < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (error);
  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      ": %u != ACTION", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, "drinks coffee"),
      ": '%s' != '%s'", last_sent_text, "drinks coffee");
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      ": %u != ACTION", last_received_type);
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text,
        "drinks coffee"),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      ": %u != ACTION", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_n_parts == 2,
      ": %u != 2", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      ": %u != ACTION", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 2,
      ": %u != 2", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: NOTICE ====\n");

  sent_count = 0;
  received_count = 0;
  message_sent_count = 0;
  message_received_count = 0;
  tp_cli_channel_type_text_run_send (chan, -1,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE, "Printer on fire",
      &error, NULL);
  /* wait for pending events to be delivered */
  while (received_count < 1 || message_received_count < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_no_error (error);
  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
      ": %u != NOTICE", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, "Printer on fire"),
      ": '%s' != '%s'", last_sent_text, "Printer on fire");
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
      ": %u != NOTICE", last_received_type);
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text,
        "Printer on fire"),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
      ": %u != NOTICE", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
  g_assert_cmpstr (last_message_sent_sender_id, ==, "me@example.com");
  MYASSERT (last_message_sent_n_parts == 2,
      ": %u != 2", last_message_sent_n_parts);
  MYASSERT (last_message_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE,
      ": %u != NOTICE", last_message_received_type);
  MYASSERT (last_message_received_sender == handle,
      ": %u != %u", last_message_received_sender, handle);
  MYASSERT (last_message_received_n_parts == 2,
      ": %u != 2", last_message_received_n_parts);

  g_print ("\n\n==== Starting test: lolcat ====\n");

  /* Send a multi-part message using the Messages API.
   *
   * Again, verify that we get a Sent signal in the old Text
   * API, and a Received signal for the echo; also a MessageSent signal
   * in the new Messages API, and a MessageReceived signal for the echo.
   *
   * Because this message contains an image, we must set the
   * Channel_Text_Message_Flag_Non_Text_Content.
   */

  sent_count = 0;
  received_count = 0;
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
      gchar *token = NULL;

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

      tp_cli_channel_interface_messages_run_send_message (chan, -1,
          send_parts, 0 /* flags */, &token, &error, NULL);
      g_assert_no_error (error);

      /* wait for pending events to be delivered */
      while (received_count < 1 || message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message, got token '%s'\n", token);
      g_free (token);

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, EXPECTED_TEXT),
      ": '%s' != '%s'", last_sent_text, EXPECTED_TEXT);
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_flags ==
      TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT,
      ": %u != NON_TEXT_CONTENT", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, EXPECTED_TEXT),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
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

  sent_count = 0;
  received_count = 0;
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
      gchar *token = NULL;

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

      tp_cli_channel_interface_messages_run_send_message (chan, -1,
          send_parts, 0 /* flags */, &token, &error, NULL);
      g_assert_no_error (error);

      /* wait for pending events to be delivered */
      while (received_count < 1 || message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message, got token '%s'\n", token);
      g_free (token);

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, EXPECTED_TEXT),
      ": '%s' != '%s'", last_sent_text, EXPECTED_TEXT);
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_flags ==
      TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT,
      ": %u != NON_TEXT_CONTENT", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, EXPECTED_TEXT),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
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

  sent_count = 0;
  received_count = 0;
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
      gchar *token = NULL;

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

      tp_cli_channel_interface_messages_run_send_message (chan, -1,
          send_parts, 0 /* flags */, &token, &error, NULL);
      g_assert_no_error (error);

      /* wait for pending events to be delivered */
      while (received_count < 1 || message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message, got token '%s'\n", token);
      g_free (token);

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, EXPECTED_TEXT),
      ": '%s' != '%s'", last_sent_text, EXPECTED_TEXT);
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, EXPECTED_TEXT),
      ": '%s'", last_received_text);
  MYASSERT (message_sent_count == 1, ": %u != 1", message_sent_count);
  MYASSERT (message_received_count == 1, ": %u != 1", message_received_count);
  MYASSERT (last_message_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_message_sent_type);
  g_assert_cmpuint (last_message_sent_sender, ==,
      tp_connection_get_self_handle (conn));
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

  sent_count = 0;
  received_count = 0;
  message_sent_count = 0;
  message_received_count = 0;

#undef EXPECTED_TEXT
#define EXPECTED_TEXT "nous badgerez le coleur du stream de la video"

    {
      GPtrArray *send_parts = g_ptr_array_sized_new (3);
      GHashTable *part;
      guint i;
      gchar *token = NULL;

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

      tp_cli_channel_interface_messages_run_send_message (chan, -1,
          send_parts, 0 /* flags */, &token, &error, NULL);
      g_assert_no_error (error);

      /* wait for pending events to be delivered */
      while (received_count < 1 || message_received_count < 1)
        g_main_context_iteration (NULL, TRUE);

      g_print ("Sent message, got token '%s'\n", token);
      g_free (token);

      for (i = 0; i < send_parts->len; i++)
        g_hash_table_unref (g_ptr_array_index (send_parts, i));

      g_ptr_array_unref (send_parts);
    }

  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, EXPECTED_TEXT),
      ": '%s' != '%s'", last_sent_text, EXPECTED_TEXT);
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, EXPECTED_TEXT),
      ": '%s'", last_received_text);
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

  g_print ("\n\n==== Getting partial content of last message ====\n");

    {
      GArray *part_numbers = g_array_sized_new (FALSE, FALSE, sizeof (guint),
          2);
      GHashTable *ret;
      guint i;

      i = 2;
      g_array_append_val (part_numbers, i);
      i = 1;
      g_array_append_val (part_numbers, i);

      tp_cli_channel_interface_messages_run_get_pending_message_content (chan,
          -1, last_received_id, part_numbers, &ret, &error, NULL);
      g_assert_no_error (error);

      MYASSERT (g_hash_table_size (ret) == 2, ": %u",
          g_hash_table_size (ret));

      g_hash_table_foreach (ret, print_part_content, NULL);
      g_hash_table_unref (ret);

      i = 47;
      g_array_append_val (part_numbers, i);

      tp_cli_channel_interface_messages_run_get_pending_message_content (chan,
          -1, last_received_id, part_numbers, &ret, &error, NULL);
      MYASSERT (error != NULL, "");
      g_print ("Testing out-of-range part number: correctly got error %s\n",
          error->message);
      g_error_free (error);
      error = NULL;

      g_array_unref (part_numbers);
    }

  g_print ("\n\n==== Listing messages ====\n");

    {
      GPtrArray *messages;

      tp_cli_channel_type_text_run_list_pending_messages (chan, -1,
          FALSE, &messages, &error, NULL);
      g_assert_no_error (error);

      g_print ("Freeing\n");
      g_boxed_free (TP_ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST, messages);
    }

  g_print ("\n\n==== Acknowledging messages using a wrong ID ====\n");

    {
      GArray *ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 2);
      /* we assume this ID won't be valid (implementation detail: message
       * IDs are increasing integers) */
      guint bad_id = 31337;

      g_array_append_val (ids, last_received_id);
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
       * last_received_id was acknowledged despite the error */
    }

  g_print ("\n\n==== Getting properties again ====\n");

    {
      const GValue *value;
      gchar *contents;
      GPtrArray *messages;
      GHashTable *properties = NULL;
      guint i;

      tp_cli_dbus_properties_run_get_all (chan, -1,
          TP_IFACE_CHANNEL_INTERFACE_MESSAGES, &properties, &error, NULL);
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
      MYASSERT (messages->len == 7, ": %u", messages->len);

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

      g_array_append_val (msgid, last_received_id);
      g_array_append_val (msgid, last_received_id);

      tp_cli_channel_type_text_run_acknowledge_pending_messages (chan, -1,
          msgid, &error, NULL);
      g_assert_no_error (error);

      g_array_unref (msgid);
    }

  g_print ("\n\n==== Acknowledging all remaining messages using deprecated "
      "API ====\n");

    {
      GPtrArray *messages;

      tp_cli_channel_type_text_run_list_pending_messages (chan, -1,
          TRUE, &messages, &error, NULL);
      g_assert_no_error (error);

      g_print ("Freeing\n");
      g_boxed_free (TP_ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST, messages);
    }

  g_print ("\n\n==== Closing channel ====\n");

    {
      GPtrArray *channels;

      MYASSERT (tp_cli_channel_run_close (chan, -1, &error, NULL), "");
      g_assert_no_error (error);
      MYASSERT (tp_proxy_get_invalidated (chan) != NULL, "");

      /* assert that the channel has really gone */
      MYASSERT (tp_cli_connection_run_list_channels (conn, -1,
            &channels, &error, NULL), "");
      g_assert_no_error (error);
      MYASSERT (channels->len == 0, "%u != 0", channels->len);
      g_boxed_free (TP_ARRAY_TYPE_CHANNEL_INFO_LIST, channels);
    }

  g_print ("\n\n==== End of tests ====\n");

  tp_tests_connection_assert_disconnect_succeeds (conn);

  g_object_unref (chan);
  g_object_unref (conn);

  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  g_free (last_sent_text);
  g_free (last_received_text);
  g_free (last_message_sent_token);
  g_free (last_message_sent_sender_id);

  return 0;
}
