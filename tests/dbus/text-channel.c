/* Tests of TpTextChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/message-mixin.h>

#include "examples/cm/echo-message-parts/chan.h"

#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    ExampleEcho2Channel *chan_service;
    ExampleEcho2Channel *sms_chan_service;
    TpHandleRepoIface *contact_repo;
    TpHandle bob;

    /* Client side objects */
    TpConnection *connection;
    TpTextChannel *channel;
    TpTextChannel *sms_channel;

    TpMessage *received_msg;
    TpMessage *removed_msg;
    TpMessage *sent_msg;
    gchar *token;
    gchar *sent_token;
    TpMessageSendingFlags sending_flags;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
create_contact_chan (Test *test)
{
  gchar *chan_path;
  GHashTable *props;

  tp_clear_object (&test->chan_service);
  tp_clear_object (&test->sms_chan_service);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  test->bob = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);
  g_assert (test->bob != 0);

  test->chan_service = g_object_new (
      EXAMPLE_TYPE_ECHO_2_CHANNEL,
      "connection", test->base_connection,
      "handle", test->bob,
      "object-path", chan_path,
      NULL);

  g_object_get (test->chan_service,
      "channel-properties", &props,
      NULL);

  test->channel = tp_text_channel_new (test->connection, chan_path,
      props, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (props);

  /* Register channel implementing SMS */
  chan_path = g_strdup_printf ("%s/ChannelSMS",
      tp_proxy_get_object_path (test->connection));

  test->sms_chan_service = g_object_new (
      EXAMPLE_TYPE_ECHO_2_CHANNEL,
      "connection", test->base_connection,
      "handle", test->bob,
      "object-path", chan_path,
      "sms", TRUE,
      NULL);

  g_object_get (test->chan_service,
      "channel-properties", &props,
      NULL);

  test->sms_channel = tp_text_channel_new (test->connection, chan_path,
      props, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (props);
}

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  create_contact_chan (test);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->chan_service);
  tp_clear_object (&test->sms_chan_service);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->received_msg);
  tp_clear_object (&test->removed_msg);
  tp_clear_object (&test->sent_msg);
  tp_clear_pointer (&test->token, g_free);
  tp_clear_pointer (&test->sent_token, g_free);

  tp_clear_object (&test->channel);
  tp_clear_object (&test->sms_channel);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const GError *error = NULL;

  g_assert (TP_IS_TEXT_CHANNEL (test->channel));

  error = tp_proxy_get_invalidated (test->channel);
  g_assert_no_error (error);
}

static void
check_messages_types (GArray *message_types)
{
  TpChannelTextMessageType type;

  g_assert (message_types != NULL);
  g_assert_cmpuint (message_types->len, ==, 3);

  type = g_array_index (message_types, TpChannelTextMessageType, 0);
  g_assert_cmpuint (type, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);
  type = g_array_index (message_types, TpChannelTextMessageType, 1);
  g_assert_cmpuint (type, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION);
  type = g_array_index (message_types, TpChannelTextMessageType, 2);
  g_assert_cmpuint (type, ==, TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GStrv content_types;
  const gchar * const * content_types2;
  TpMessagePartSupportFlags message_part;
  TpDeliveryReportingSupportFlags delivery;
  GArray *message_types;

  g_object_get (test->channel,
      "supported-content-types", &content_types,
      "message-part-support-flags", &message_part,
      "delivery-reporting-support", &delivery,
      "message-types", &message_types,
      NULL);

  /* SupportedContentTypes */
  g_assert_cmpuint (g_strv_length (content_types), ==, 1);
  g_assert_cmpstr (content_types[0], ==, "*/*");
  g_strfreev (content_types);

  content_types2 = tp_text_channel_get_supported_content_types (test->channel);
  g_assert_cmpstr (content_types2[0], ==, "*/*");

  /* MessagePartSupportFlags */
  g_assert_cmpuint (message_part, ==,
      TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT |
      TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS |
      TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);
  g_assert_cmpuint (message_part, ==,
      tp_text_channel_get_message_part_support_flags (test->channel));

  /* DeliveryReportingSupport */
  g_assert_cmpuint (delivery, ==,
      TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);
  g_assert_cmpuint (delivery, ==,
      tp_text_channel_get_delivery_reporting_support (test->channel));

  /* MessageTypes */
  check_messages_types (message_types);
  g_array_unref (message_types);

  message_types = tp_text_channel_get_message_types (test->channel);
  check_messages_types (message_types);

  g_assert (tp_text_channel_supports_message_type (test->channel,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL));
  g_assert (tp_text_channel_supports_message_type (test->channel,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION));
  g_assert (tp_text_channel_supports_message_type (test->channel,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE));
  g_assert (!tp_text_channel_supports_message_type (test->channel,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY));
  g_assert (!tp_text_channel_supports_message_type (test->channel,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT));
}

static void
proxy_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (source, result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
send_message_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_clear_pointer (&test->token, g_free);

  tp_text_channel_send_message_finish (TP_TEXT_CHANNEL (source), result,
      &test->token, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
on_received (TpChannel *chan,
    guint id,
    guint timestamp,
    guint sender,
    guint type,
    guint flags,
    const gchar *text,
    gpointer user_data,
    GObject *object)
{
  Test *test = user_data;

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_pending_messages (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  GList *messages;
  TpMessage *msg;
  gchar *text;
  TpContact *sender;

  /* connect on the Received sig to check if the message has been received */
  tp_cli_channel_type_text_connect_to_received (TP_CHANNEL (test->channel),
      on_received, test, NULL, NULL, NULL);

  /* Send a first message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Badger");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Send a second message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Snake");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* We didn't prepare the feature yet so there is no pending msg */
  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 0);
  g_list_free (messages);

  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->channel,
        TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES));

  /* We have the pending messages now */
  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 2);

  /* Check first message */
  msg = messages->data;
  g_assert (TP_IS_SIGNALLED_MESSAGE (msg));

  text = tp_message_to_text (msg, NULL);
  g_assert_cmpstr (text, ==, "Badger");
  g_free (text);
  sender = tp_signalled_message_get_sender (msg);
  g_assert (sender != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (sender), ==, "bob");

  /* Check second message */
  msg = messages->next->data;
  g_assert (TP_IS_SIGNALLED_MESSAGE (msg));

  text = tp_message_to_text (msg, NULL);
  g_assert_cmpstr (text, ==, "Snake");
  g_free (text);
  sender = tp_signalled_message_get_sender (msg);
  g_assert (sender != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (sender), ==, "bob");

  g_list_free (messages);
}

static void
message_received_cb (TpTextChannel *chan,
    TpSignalledMessage *msg,
    Test *test)
{
  tp_clear_object (&test->received_msg);

  test->received_msg = g_object_ref (msg);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_message_received (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  TpMessage *msg;
  gchar *text;
  TpContact *sender;

  /* We have to prepare the pending messages feature to be notified about
   * incoming messages */
  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_signal_connect (test->channel, "message-received",
      G_CALLBACK (message_received_cb), test);

  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Snake");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  text = tp_message_to_text (test->received_msg, NULL);
  g_assert_cmpstr (text, ==, "Snake");
  g_free (text);

  sender = tp_signalled_message_get_sender (test->received_msg);
  g_assert (sender != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (sender), ==, "bob");

  g_object_unref (msg);
}

static void
messages_acked_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_text_channel_ack_messages_finish (TP_TEXT_CHANNEL (source), result,
      &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_ack_messages (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  GList *messages;
  TpMessage *msg;

  /* Send a first message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Badger");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  /* Send a second message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Snake");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 2);

  tp_text_channel_ack_messages_async (test->channel, messages,
      messages_acked_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_list_free (messages);

  /* Messages have been acked so there is no pending messages */
  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 0);
}

static void
message_acked_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_text_channel_ack_message_finish (TP_TEXT_CHANNEL (source), result,
      &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
pending_message_removed_cb (TpTextChannel *chan,
    TpSignalledMessage *msg,
    Test *test)
{
  tp_clear_object (&test->removed_msg);

  test->removed_msg = g_object_ref (msg);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_ack_message (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  GList *messages;
  TpMessage *msg;

  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_signal_connect (test->channel, "message-received",
      G_CALLBACK (message_received_cb), test);

  /* Send message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Badger");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (TP_IS_SIGNALLED_MESSAGE (test->received_msg));

  g_signal_connect (test->channel, "pending-message-removed",
      G_CALLBACK (pending_message_removed_cb), test);

  tp_text_channel_ack_message_async (test->channel, test->received_msg,
      message_acked_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->received_msg == test->removed_msg);

  /* Messages has been acked so there is no pending messages */
  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 0);
}

static void
message_sent_cb (TpTextChannel *channel,
    TpSignalledMessage *message,
    TpMessageSendingFlags flags,
    const gchar *token,
    Test *test)
{
  tp_clear_object (&test->sent_msg);
  tp_clear_pointer (&test->sent_token, g_free);

  test->sent_msg = g_object_ref (message);
  test->sending_flags = flags;
  if (token != NULL)
    test->sent_token = g_strdup (token);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_message_sent (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpMessage *msg;
  gchar *text;

  g_signal_connect (test->channel, "message-sent",
      G_CALLBACK (message_sent_cb), test);

  /* Send message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Badger");

  tp_text_channel_send_message_async (test->channel, msg,
      TP_MESSAGE_SENDING_FLAG_REPORT_DELIVERY, send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (TP_IS_SIGNALLED_MESSAGE (test->sent_msg));
  text = tp_message_to_text (test->sent_msg, NULL);
  g_assert_cmpstr (text, ==, "Badger");
  g_free (text);

  g_assert_cmpuint (test->sending_flags, ==,
      TP_MESSAGE_SENDING_FLAG_REPORT_DELIVERY);
  g_assert (test->sent_token == NULL);
}

static void
notify_cb (GObject *object,
    GParamSpec *spec,
    Test *test)
{
  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}


static void
test_sms_feature (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean is_sms;
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_SMS, 0 };

  g_assert (tp_text_channel_get_sms_flash (test->sms_channel));

  /* SMS feature is not prepared yet */
  g_assert (!tp_text_channel_is_sms_channel (test->sms_channel));

  g_object_get (test->sms_channel, "is-sms-channel", &is_sms, NULL);
  g_assert (!is_sms);

  test->wait++;
  tp_proxy_prepare_async (test->sms_channel, features,
      proxy_prepare_cb, test);

  test->wait++;
  g_signal_connect (test->sms_channel, "notify::is-sms-channel",
      G_CALLBACK (notify_cb), test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Feature has been prepared */
  g_assert (tp_text_channel_is_sms_channel (test->sms_channel));

  g_object_get (test->sms_channel, "is-sms-channel", &is_sms, NULL);
  g_assert (is_sms);

  /* Property is changed */
  example_echo_2_channel_set_sms (test->sms_chan_service, FALSE);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (!tp_text_channel_is_sms_channel (test->sms_channel));

  g_object_get (test->sms_channel, "is-sms-channel", &is_sms, NULL);
  g_assert (!is_sms);
}

#define MSG "Oh hi!"

static void
get_sms_length_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  guint chunks_required;
  gint remaining_characters;
  gint estimated_cost;

  tp_text_channel_get_sms_length_finish (TP_TEXT_CHANNEL (source), result,
      &chunks_required, &remaining_characters, &estimated_cost, &test->error);

  g_assert_cmpuint (chunks_required, ==, strlen (MSG));
  g_assert_cmpint (remaining_characters, ==,
      EXAMPLE_ECHO_2_CHANNEL_MAX_SMS_LENGTH - strlen (MSG));
  g_assert_cmpint (estimated_cost, ==, -1);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}


static void
test_get_sms_length (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpMessage *msg;

  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, MSG);

  tp_text_channel_get_sms_length_async (test->channel, msg,
      get_sms_length_cb, test);

  test->wait++;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_object_unref (msg);
}

static void
all_pending_messages_acked_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_text_channel_ack_all_pending_messages_finish (TP_TEXT_CHANNEL (source),
      result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_ack_all_pending_messages (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  GList *messages;
  TpMessage *msg;

  /* Send a first message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Badger");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  /* Send a second message */
  msg = tp_client_message_new_text (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Snake");

  tp_text_channel_send_message_async (test->channel, msg, 0,
      send_message_cb, test);

  g_object_unref (msg);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 2);

  tp_text_channel_ack_all_pending_messages_async (test->channel,
      all_pending_messages_acked_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_list_free (messages);

  /* Messages have been acked so there is no pending messages */
  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert_cmpuint (g_list_length (messages), ==, 0);
}

static void
test_pending_messages_with_no_sender_id (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  TpMessage *cm_message;
  TpMessage *signalled_message;
  GList *messages;
  TpContact *sender;
  gchar *text;

  g_test_bug ("39172");

  /* Deliberately passing sender=0 so we can set message-sender manually; if we set
   * it here, or using tp_cm_message_set_sender(), message-sender-id will be
   * filled in, which is exactly what we don't want.
   */
  cm_message = tp_cm_message_new_text (test->base_connection, 0,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, "hi mum");
  tp_message_set_uint32 (cm_message, 0, "message-sender", test->bob);
  g_assert_cmpstr (NULL, ==,
      tp_asv_get_string (tp_message_peek (cm_message, 0), "message-sender-id"));
  tp_message_mixin_take_received (G_OBJECT (test->chan_service), cm_message);

  test->wait = 1;
  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  messages = tp_text_channel_get_pending_messages (test->channel);
  g_assert (messages != NULL);
  g_assert_cmpuint (g_list_length (messages), ==, 1);

  signalled_message = messages->data;
  sender = tp_signalled_message_get_sender (signalled_message);
  g_assert (sender != NULL);
  g_assert_cmpstr (tp_contact_get_identifier (sender), ==, "bob");

  text = tp_message_to_text ((TpMessage *) signalled_message, NULL);
  g_assert_cmpstr (text, ==, "hi mum");
  g_free (text);

  g_list_free (messages);
}

static void
test_sender_prepared (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  TpSimpleClientFactory *factory;
  TpHandle admin;
  TpContact *sender;
  TpMessage *msg;

  tp_tests_proxy_run_until_prepared (test->channel, features);

  /* Simulate a message received from a new contact */
  admin = tp_handle_ensure (test->contact_repo, "admin", NULL, NULL);
  msg = tp_cm_message_new_text (test->base_connection, admin,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Service interuption in 1h");
  tp_message_mixin_take_received ((GObject *) test->chan_service, msg);

  g_signal_connect (test->channel, "message-received",
      G_CALLBACK (message_received_cb), test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* No feature was set on the factory */
  sender = tp_signalled_message_get_sender (test->received_msg);
  g_assert (!tp_contact_has_feature (sender, TP_CONTACT_FEATURE_ALIAS));

  /* Now ask to prepare ALIAS, on next msg it will be prepared */
  factory = tp_proxy_get_factory (test->connection);
  tp_simple_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_INVALID);

  msg = tp_cm_message_new_text (test->base_connection, admin,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Service interuption in 30min");
  tp_message_mixin_take_received ((GObject *) test->chan_service, msg);

  g_signal_connect (test->channel, "message-received",
      G_CALLBACK (message_received_cb), test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  sender = tp_signalled_message_get_sender (test->received_msg);
  g_assert (tp_contact_has_feature (sender, TP_CONTACT_FEATURE_ALIAS));
}

static void
test_sent_with_no_sender (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *parts;
  TpContact *sender;

  tp_tests_proxy_run_until_prepared (test->channel, NULL);

  /* Simulate a message sent with no sender, it must fallback to
   * connection's self-contact. Unfortunately we cannot use the message mixin
   * because it force setting a sender, and we can't use TpCMMessage to create
   * parts because it's kept private. So back to old school. */
  parts = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hash_table_unref);
  g_ptr_array_add (parts, tp_asv_new (
      "message-type", G_TYPE_UINT, TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      NULL));
  g_ptr_array_add (parts, tp_asv_new (
      "content-type", G_TYPE_STRING, "text/plain",
      "content", G_TYPE_STRING, "bla bla bla",
      NULL));

  g_signal_connect (test->channel, "message-sent",
      G_CALLBACK (message_sent_cb), test);

  tp_svc_channel_interface_messages_emit_message_sent (test->chan_service,
      parts, 0, "this-is-a-token");

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  sender = tp_signalled_message_get_sender (test->sent_msg);
  g_assert (sender == tp_connection_get_self_contact (test->connection));

  g_ptr_array_unref (parts);
}

static void
test_receive_muc_delivery (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0 };
  GPtrArray *parts;
  GHashTable *header;

  g_test_bug ("41929 ");

  /* We have to prepare the pending messages feature to be notified about
   * incoming messages */
  tp_proxy_prepare_async (test->channel, features,
      proxy_prepare_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_signal_connect (test->channel, "message-received",
      G_CALLBACK (message_received_cb), test);

  /* build delivery report */
  parts = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hash_table_unref);
  header = tp_asv_new (NULL, NULL);
  g_ptr_array_add (parts, header);

  tp_asv_set_uint32 (header, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  tp_asv_set_uint32 (header, "pending-message-id", 5);
  tp_asv_set_string (header, "message-token", "message_token");
  tp_asv_set_string (header, "delivery-token", "delivery_token");
  tp_asv_set_uint32 (header, "delivery-status", TP_DELIVERY_STATUS_DELIVERED);

  tp_svc_channel_interface_messages_emit_message_received (test->chan_service,
      parts);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (tp_message_get_message_type (test->received_msg), ==,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);

  g_ptr_array_unref (parts);
}

static void
set_chat_state_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_text_channel_set_chat_state_finish (TP_TEXT_CHANNEL (source), result,
      &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
contact_chat_state_changed_cb (TpTextChannel *channel,
    TpContact *contact,
    TpChannelChatState state,
    Test *test)
{
  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_chat_state (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = {
      TP_CHANNEL_FEATURE_CONTACTS,
      TP_TEXT_CHANNEL_FEATURE_CHAT_STATES,
      0 };
  TpContact *contact;
  TpChannelChatState state;

  /* Set an initial chat state, prepare the channel, and verify target contact
   * has that state */
  tp_message_mixin_change_chat_state (G_OBJECT (test->chan_service),
      test->bob, TP_CHANNEL_CHAT_STATE_COMPOSING);

  tp_tests_proxy_run_until_prepared (test->channel, features);

  contact = tp_channel_get_target_contact ((TpChannel *) test->channel);
  state = tp_text_channel_get_chat_state (test->channel, contact);
  g_assert_cmpuint (state, ==, TP_CHANNEL_CHAT_STATE_COMPOSING);

  /* Test setting invalid chat state */
  tp_text_channel_set_chat_state_async (test->channel, -1,
      set_chat_state_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  tp_text_channel_set_chat_state_async (test->channel,
      TP_CHANNEL_CHAT_STATE_GONE, set_chat_state_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  /* Now set a valid chat state and verify self contact has that state */
  tp_text_channel_set_chat_state_async (test->channel,
      TP_CHANNEL_CHAT_STATE_COMPOSING, set_chat_state_cb, test);
  g_signal_connect (test->channel, "contact-chat-state-changed",
      G_CALLBACK (contact_chat_state_changed_cb), test);
  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  contact = tp_connection_get_self_contact (test->connection);
  state = tp_text_channel_get_chat_state (test->channel, contact);
  g_assert_cmpuint (state, ==, TP_CHANNEL_CHAT_STATE_COMPOSING);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/text-channel/creation", Test, NULL, setup,
      test_creation, teardown);
  g_test_add ("/text-channel/properties", Test, NULL, setup,
      test_properties, teardown);
  g_test_add ("/text-channel/pending-messages", Test, NULL, setup,
      test_pending_messages, teardown);
  g_test_add ("/text-channel/message-received", Test, NULL, setup,
      test_message_received, teardown);
  g_test_add ("/text-channel/ack-messages", Test, NULL, setup,
      test_ack_messages, teardown);
  g_test_add ("/text-channel/ack-message", Test, NULL, setup,
      test_ack_message, teardown);
  g_test_add ("/text-channel/message-sent", Test, NULL, setup,
      test_message_sent, teardown);
  g_test_add ("/text-channel/sms-feature", Test, NULL, setup,
      test_sms_feature, teardown);
  g_test_add ("/text-channel/get-sms-length", Test, NULL, setup,
      test_get_sms_length, teardown);
  g_test_add ("/text-channel/ack-all-pending-messages", Test, NULL, setup,
      test_ack_all_pending_messages, teardown);
  g_test_add ("/text-channel/pending-messages-with-no-sender-id", Test, NULL,
      setup, test_pending_messages_with_no_sender_id, teardown);
  g_test_add ("/text-channel/sender-prepared", Test, NULL, setup,
      test_sender_prepared, teardown);
  g_test_add ("/text-channel/sent-with-no-sender", Test, NULL, setup,
      test_sent_with_no_sender, teardown);
  g_test_add ("/text-channel/receive-muc-delivery", Test, NULL, setup,
      test_receive_muc_delivery, teardown);
  g_test_add ("/text-channel/chat-state", Test, NULL, setup,
      test_chat_state, teardown);

  return g_test_run ();
}
