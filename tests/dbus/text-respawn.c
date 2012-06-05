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
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/echo-chan.h"
#include "tests/lib/echo-conn.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

static guint received_count = 0;
static guint last_received_id = 0;
static guint last_received_time = 0;
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
  g_message ("%p: Sent: time %u, type %u, text '%s'",
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
  TpHandleRepoIface *contact_repo = data;

  g_message ("%p: Received #%u: time %u, sender %u '%s', type %u, flags %u, "
      "text '%s'", chan, id, timestamp, sender,
      tp_handle_inspect (contact_repo, sender), type, flags, text);

  received_count++;
  last_received_id = id;
  last_received_time = timestamp;
  last_received_sender = sender;
  last_received_type = type;
  last_received_flags = flags;
  g_free (last_received_text);
  last_received_text = g_strdup (text);
}

int
main (int argc,
      char **argv)
{
  TpTestsEchoConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TpTestsEchoChannel *service_chan;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;
  TpHandle handle;

  tp_tests_abort_after (10);
  g_type_init ();
  /* tp_debug_set_flags ("all"); */
  dbus = tp_tests_dbus_daemon_dup_or_die ();

  service_conn = TP_TESTS_ECHO_CONNECTION (tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CONNECTION,
        "account", "me@example.com",
        "protocol", "example",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "example",
        &name, &conn_path, &error), "");
  g_assert_no_error (error);

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  g_assert_no_error (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  g_assert_no_error (error);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  handle = tp_handle_ensure (contact_repo, "them@example.org", NULL, &error);
  g_assert_no_error (error);

  /* FIXME: exercise RequestChannel rather than just pasting on a channel */

  chan_path = g_strdup_printf ("%s/Channel", conn_path);

  service_chan = TP_TESTS_ECHO_CHANNEL (tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", service_conn,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  tp_channel_run_until_ready (chan, &error, NULL);
  g_assert_no_error (error);

  MYASSERT (tp_cli_channel_type_text_connect_to_received (chan, on_received,
      g_object_ref (contact_repo), g_object_unref, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_channel_type_text_connect_to_sent (chan, on_sent,
      NULL, NULL, NULL, NULL) != NULL, "");

  sent_count = 0;
  received_count = 0;
  tp_cli_channel_type_text_run_send (chan, -1,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL, "Hello, world!",
      &error, NULL);
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
  MYASSERT (last_received_flags == 0, ": %u != 0", last_received_flags);
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

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  tp_channel_run_until_ready (chan, &error, NULL);
  g_assert_no_error (error);

  g_print ("\n\n==== Listing messages ====\n");

    {
      GPtrArray *messages;
      GValueArray *structure;

      tp_cli_channel_type_text_run_list_pending_messages (chan, -1,
          FALSE, &messages, &error, NULL);
      g_assert_no_error (error);

      g_assert_cmpuint (messages->len, ==, 1);
      structure = g_ptr_array_index (messages, 0);
      g_assert_cmpuint (g_value_get_uint (structure->values + 0), ==,
          last_received_id);
      g_assert_cmpuint (g_value_get_uint (structure->values + 1), ==,
          last_received_time);
      g_assert_cmpuint (g_value_get_uint (structure->values + 2), ==,
          handle);
      g_assert_cmpuint (g_value_get_uint (structure->values + 3), ==,
          TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);
      g_assert_cmpuint (g_value_get_uint (structure->values + 4), ==,
          TP_CHANNEL_TEXT_MESSAGE_FLAG_RESCUED);
      g_assert_cmpstr (g_value_get_string (structure->values + 5), ==,
          "You said: Hello, world!");

      g_print ("Freeing\n");
      g_boxed_free (TP_ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST, messages);
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

      MYASSERT (tp_cli_channel_interface_destroyable_run_destroy (chan, -1,
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

  tp_handle_unref (contact_repo, handle);
  g_object_unref (chan);
  g_object_unref (conn);
  g_object_unref (service_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  g_free (last_sent_text);
  g_free (last_received_text);

  return 0;
}
