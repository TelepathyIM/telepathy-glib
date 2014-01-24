/* Basic test for the text mixin and the echo example CM.
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
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <telepathy-glib/reentrants.h>

#include "tests/lib/echo-chan.h"
#include "tests/lib/echo-conn.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static guint received_count = 0;
static guint last_received_id = 0;
static gint64 last_received_time = 0;
static guint last_received_sender = 0;
static guint last_received_type = 0;
static gboolean last_received_rescued = FALSE;
static gchar *last_received_text = NULL;

static guint sent_count = 0;
static guint last_sent_type = 0;
static gchar *last_sent_text = NULL;

static void
on_sent (TpChannel *chan,
    const GPtrArray *message,
    guint flags,
    const gchar *message_token,
    gpointer user_data,
    GObject *weak_object)
{
  GHashTable *header, *body;
  gint64 timestamp;
  guint type;
  const gchar *text;

  header = g_ptr_array_index (message, 0);
  timestamp = tp_asv_get_int64 (header, "message-sent", NULL);
  type = tp_asv_get_uint32 (header, "message-type", NULL);

  body = g_ptr_array_index (message, 1);
  text = tp_asv_get_string (body, "content");

  g_message ("%p: Sent: time %" G_GINT64_FORMAT ", type %u, text '%s'",
      chan, timestamp, type, text);

  sent_count++;
  last_sent_type = type;
  g_free (last_sent_text);
  last_sent_text = g_strdup (text);
}

static void
on_received (TpChannel *chan,
    const GPtrArray *message,
    gpointer user_data,
    GObject *weak_object)
{
  TpHandleRepoIface *contact_repo = user_data;

  GHashTable *header, *body;
  guint id;
  gint64 timestamp;
  TpHandle sender;
  guint type;
  gboolean rescued;
  const gchar *text;

  header = g_ptr_array_index (message, 0);
  id = tp_asv_get_uint32 (header, "message-pending-id", NULL);
  timestamp = tp_asv_get_int64 (header, "message-sent", NULL);
  sender = tp_asv_get_uint32 (header, "message-sender", NULL);
  type = tp_asv_get_uint32 (header, "message-type", NULL);
  rescued = tp_asv_get_boolean (header, "rescued", NULL);

  body = g_ptr_array_index (message, 1);
  text = tp_asv_get_string (body, "content");

  g_message ("%p: Received #%u: time %" G_GINT64_FORMAT ", sender %u '%s', type %u, rescued %s, "
      "text '%s'", chan, id, timestamp, sender,
      tp_handle_inspect (contact_repo, sender), type,
      rescued ? "yes" : "no", text);

  received_count++;
  last_received_id = id;
  last_received_time = timestamp;
  last_received_sender = sender;
  last_received_type = type;
  last_received_rescued = rescued;
  g_free (last_received_text);
  last_received_text = g_strdup (text);
}

static GPtrArray *
build_message (TpChannelTextMessageType type,
    const gchar *content)
{
  GPtrArray *out;
  GHashTable *header, *body;

  header = tp_asv_new (
      "message-type", G_TYPE_UINT, type,
      NULL);

  body = tp_asv_new (
      "content", G_TYPE_STRING, content,
      NULL);

  out = g_ptr_array_new_full (2, (GDestroyNotify) g_hash_table_unref);
  g_ptr_array_add (out, header);
  g_ptr_array_add (out, body);

  return out;
}

int
main (int argc,
      char **argv)
{
  TpTestsEchoConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TpTestsEchoChannel *service_chan;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *chan_path;
  TpHandle handle;
  GPtrArray *message;
  TpCapabilities *caps;
  GPtrArray *classes;
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CAPABILITIES, 0 };

  tp_tests_abort_after (10);
  /* tp_debug_set_flags ("all"); */

  tp_tests_create_conn (TP_TESTS_TYPE_ECHO_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &conn);
  service_conn = TP_TESTS_ECHO_CONNECTION (service_conn_as_base);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  handle = tp_handle_ensure (contact_repo, "them@example.org", NULL, &error);
  g_assert_no_error (error);

  /* FIXME: exercise RequestChannel rather than just pasting on a channel */

  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (conn));

  service_chan = TP_TESTS_ECHO_CHANNEL (tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", service_conn,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  chan = tp_tests_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  /* check connection requestable channels */
  tp_tests_proxy_run_until_prepared (conn, conn_features);

  caps = tp_connection_get_capabilities (conn);
  g_assert (caps != NULL);
  classes = tp_capabilities_get_channel_classes (caps);
  g_assert (classes != NULL);
  g_assert_cmpint (classes->len, ==, 1);
  g_assert (tp_capabilities_supports_text_chats (caps));

  MYASSERT (tp_cli_channel_type_text_connect_to_message_received (chan, on_received,
      g_object_ref (contact_repo), g_object_unref, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_channel_type_text_connect_to_message_sent (chan, on_sent,
      NULL, NULL, NULL, NULL) != NULL, "");

  sent_count = 0;
  received_count = 0;
  message = build_message (TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "Hello, world!");
  tp_cli_channel_type_text_run_send_message (chan, -1,
      message, 0, NULL, &error, NULL);
  g_ptr_array_unref (message);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);
  MYASSERT (sent_count == 1, ": %u != 1", sent_count);
  MYASSERT (received_count == 1, ": %u != 1", received_count);
  MYASSERT (last_sent_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_sent_type);
  MYASSERT (!tp_strdiff (last_sent_text, "Hello, world!"),
      "'%s' != '%s'", last_sent_text, "Hello, world!");
  MYASSERT (last_received_type == TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      ": %u != NORMAL", last_received_type);
  MYASSERT (last_received_rescued == FALSE, ": %s != FALSE",
      last_received_rescued ? "TRUE" : "FALSE");
  MYASSERT (last_received_sender == handle,
      ": %u != %u", last_received_sender, handle);
  MYASSERT (!tp_strdiff (last_received_text, "You said: Hello, world!"),
      "'%s'", last_received_text);

  g_print ("\n\n==== Closing channel (it will respawn) ====\n");

    {
      gboolean dead;
      TpHandle new_initiator;

      MYASSERT (tp_cli_channel_run_close (chan, -1, &error, NULL), "");
      g_assert_no_error (error);
      MYASSERT (tp_proxy_get_invalidated (chan) != NULL, "");

      g_object_get (service_chan,
          "channel-destroyed", &dead,
          "initiator-handle", &new_initiator,
          NULL);

      MYASSERT (!dead, "");
      g_assert_cmpuint (new_initiator, ==, handle);
    }

  g_print ("\n\n==== Re-creating TpChannel ====\n");

  g_object_unref (chan);

  chan = tp_tests_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  g_print ("\n\n==== Listing messages ====\n");

    {
      GValue *value;
      GPtrArray *messages, *parts;
      GHashTable *header;
      GHashTable *body;

      tp_cli_dbus_properties_run_get (chan, -1,
          TP_IFACE_CHANNEL_TYPE_TEXT, "PendingMessages", &value,
          &error, NULL);
      g_assert_no_error (error);

      messages = g_value_get_boxed (value);
      g_assert_cmpuint (messages->len, ==, 1);

      parts = g_ptr_array_index (messages, 0);
      g_assert_cmpuint (parts->len, ==, 2);

      header = g_ptr_array_index (parts, 0);
      body = g_ptr_array_index (parts, 1);

      g_assert_cmpuint (tp_asv_get_uint32 (header, "pending-message-id", NULL), ==,
          last_received_id);
      g_assert_cmpuint (tp_asv_get_int64 (header, "message-sent", NULL), ==,
          last_received_time);
      g_assert_cmpuint (tp_asv_get_uint32 (header, "message-sender", NULL), ==,
          handle);
      g_assert_cmpuint (tp_asv_get_uint32 (header, "message-type", NULL), ==,
          TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);
      g_assert_cmpuint (tp_asv_get_boolean (header, "rescued", NULL), ==,
          TRUE);
      g_assert_cmpstr (tp_asv_get_string (body, "content"), ==,
          "You said: Hello, world!");

      g_value_unset (value);
      g_free (value);
    }

  g_print ("\n\n==== Disappearing channel ====\n");

    {
      TpBaseChannel *base = TP_BASE_CHANNEL (service_chan);
      TpHandle self_handle = tp_base_connection_get_self_handle (
          service_conn_as_base);

      /* first make the channel disappear and make sure it's off the
       * bus */
      tp_base_channel_disappear (base);

      g_assert (!tp_base_channel_is_registered (base));

      /* now reopen it and make sure it's got new requested/initiator
       * values, as well as being back on the bus. */
      tp_base_channel_reopened_with_requested (base, TRUE, self_handle);

      g_assert_cmpuint (tp_base_channel_get_initiator (base), ==, self_handle);
      g_assert (tp_base_channel_is_requested (base));

      g_assert (tp_base_channel_is_registered (base));
    }

  g_print ("\n\n==== Destroying channel ====\n");

    {
      gboolean dead;

      MYASSERT (tp_cli_channel_interface_destroyable1_run_destroy (chan, -1,
            &error, NULL), "");
      g_assert_no_error (error);
      MYASSERT (tp_proxy_get_invalidated (chan) != NULL, "");

      g_object_get (service_chan,
          "channel-destroyed", &dead,
          NULL);

      MYASSERT (dead, "");
    }

  g_print ("\n\n==== End of tests ====\n");

  tp_tests_connection_assert_disconnect_succeeds (conn);

  g_object_unref (chan);
  g_object_unref (conn);
  g_object_unref (service_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (chan_path);

  g_free (last_sent_text);
  g_free (last_received_text);

  return 0;
}
