/* Tests of TpTextChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>

#include "examples/cm/echo-message-parts/chan.h"
#include "examples/cm/echo-message-parts/conn.h"

#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    ExampleEcho2Channel *chan_service;
    TpHandleRepoIface *contact_repo;

    /* Client side objects */
    TpConnection *connection;
    TpTextChannel *channel;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
create_contact_chan (Test *test)
{
  gchar *chan_path;
  TpHandle handle, alf_handle;
  GHashTable *props;

  tp_clear_object (&test->chan_service);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);

  g_assert_no_error (test->error);

  alf_handle = tp_handle_ensure (test->contact_repo, "alf", NULL, &test->error);
  g_assert_no_error (test->error);

  test->chan_service = g_object_new (
      EXAMPLE_TYPE_ECHO_2_CHANNEL,
      "connection", test->base_connection,
      "handle", handle,
      "object-path", chan_path,
      NULL);

  g_object_get (test->chan_service,
      "channel-properties", &props,
      NULL);

  test->channel = tp_text_channel_new (test->connection, chan_path,
      props, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);

  tp_handle_unref (test->contact_repo, handle);
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
  tp_tests_create_and_connect_conn (EXAMPLE_TYPE_ECHO_2_CONNECTION,
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

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->channel);
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
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GStrv content_types;
  TpMessagePartSupportFlags message_part;
  TpDeliveryReportingSupportFlags delivery;

  g_object_get (test->channel,
      "supported-content-types", &content_types,
      "message-part-support-flags", &message_part,
      "delivery-reporting-support", &delivery,
      NULL);

  g_assert_cmpuint (g_strv_length (content_types), ==, 1);
  g_assert_cmpstr (content_types[0], ==, "*/*");
  g_strfreev (content_types);

  content_types = tp_text_channel_get_supported_content_types (test->channel);
  g_assert_cmpstr (content_types[0], ==, "*/*");

  g_assert_cmpuint (message_part, ==,
      TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT |
      TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS |
      TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);
  g_assert_cmpuint (message_part, ==,
      tp_text_channel_get_message_part_support_flags (test->channel));

  g_assert_cmpuint (delivery, ==,
      TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES);
  g_assert_cmpuint (delivery, ==,
      tp_text_channel_get_delivery_reporting_support (test->channel));
}

int
main (int argc,
      char **argv)
{
  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/text-channel/creation", Test, NULL, setup,
      test_creation, teardown);
  g_test_add ("/text-channel/properties", Test, NULL, setup,
      test_properties, teardown);

  return g_test_run ();
}
