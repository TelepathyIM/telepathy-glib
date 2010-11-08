/* Tests of TpChannel
 * TODO: tests/dbus/channel-introspect.c should probably move here at some
 * point
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"
#include "tests/lib/textchan-group.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsTextChannelNull *chan_contact_service;
    TpTestsTextChannelGroup *chan_room_service;
    TpHandleRepoIface *contact_repo;
    TpHandleRepoIface *room_repo;

    /* Client side objects */
    TpConnection *connection;
    TpChannel *channel_contact;
    TpChannel *channel_room;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
create_contact_chan (Test *test)
{
  gchar *chan_path;
  TpHandle handle;
  GHashTable *props;

  tp_clear_object (&test->chan_contact_service);
  tp_clear_object (&test->chan_room_service);

  /* Create service-side channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);

  g_assert_no_error (test->error);

  test->chan_contact_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_TEXT_CHANNEL_NULL,
      "connection", test->base_connection,
      "handle", handle,
      "object-path", chan_path,
      NULL);

  props = tp_tests_text_channel_get_props (test->chan_contact_service);

  test->channel_contact = tp_channel_new_from_properties (test->connection,
      chan_path, props, &test->error);
  g_assert_no_error (test->error);

  g_free (chan_path);

  tp_handle_unref (test->contact_repo, handle);
  g_hash_table_unref (props);
}

static void
create_room_chan (Test *test)
{
  gchar *chan_path;
  GHashTable *props;

  tp_clear_object (&test->chan_room_service);

  /* Create service-side channel object */
  chan_path = g_strdup_printf ("%s/Channel2",
      tp_proxy_get_object_path (test->connection));

  test->room_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_ROOM);
  g_assert (test->room_repo != NULL);

  test->chan_room_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
      "connection", test->base_connection,
      "object-path", chan_path,
      NULL);

  /* FIXME: This is crack a chan_room_service is actually not a
   * TpTestsTextChannelNull */
  props = tp_tests_text_channel_get_props (
      (TpTestsTextChannelNull *) test->chan_room_service);

  test->channel_room = tp_channel_new_from_properties (test->connection,
      chan_path, props, &test->error);
  g_assert_no_error (test->error);

  /* We are in the muc */
  tp_tests_text_channel_group_join (test->chan_room_service);

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
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  create_contact_chan (test);
  create_room_chan (test);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->chan_contact_service);
  tp_clear_object (&test->chan_room_service);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->channel_contact);
  tp_clear_object (&test->channel_room);
}

static void
channel_leave_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_leave_finish (TP_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_leave_contact_unprepared_no_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (tp_proxy_get_invalidated (test->channel_contact) == NULL);

  tp_channel_leave_async (test->channel_contact,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      NULL, channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_contact) != NULL);
}

static void
test_leave_contact_unprepared_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (tp_proxy_get_invalidated (test->channel_contact) == NULL);

  tp_channel_leave_async (test->channel_contact,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY,
      "Bye Bye", channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_contact) != NULL);
}

static void
channel_prepared_cb (GObject *source,
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
test_leave_contact_prepared_no_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_FEATURE_CORE, 0 };

  g_assert (tp_proxy_get_invalidated (test->channel_contact) == NULL);

  tp_proxy_prepare_async (test->channel_contact, features,
      channel_prepared_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_channel_leave_async (test->channel_contact,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      NULL, channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_contact) != NULL);
}

static void
test_leave_contact_prepared_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_FEATURE_CORE, 0 };

  g_assert (tp_proxy_get_invalidated (test->channel_contact) == NULL);

  tp_proxy_prepare_async (test->channel_contact, features,
      channel_prepared_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_channel_leave_async (test->channel_contact,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY,
      "Bye Bye", channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_contact) != NULL);
}

/* Room tests */
static void
check_not_removed (TpTestsTextChannelGroup *chan)
{
  g_assert_cmpuint (chan->removed_handle, ==, 0);
  g_assert (chan->removed_message == NULL);
  g_assert_cmpuint (chan->removed_reason, ==, 0);
}

static void
check_removed (TpTestsTextChannelGroup *chan)
{
  g_assert_cmpuint (chan->removed_handle, !=, 0);
  g_assert_cmpstr (chan->removed_message, ==, "Bye Bye");
  g_assert_cmpuint (chan->removed_reason, ==,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY);
}

static void
test_leave_room_unprepared_no_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (tp_proxy_get_invalidated (test->channel_room) == NULL);

  tp_channel_leave_async (test->channel_room,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      NULL, channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_room) != NULL);
  check_not_removed (test->chan_room_service);
}

static void
test_leave_room_unprepared_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (tp_proxy_get_invalidated (test->channel_room) == NULL);

  tp_channel_leave_async (test->channel_room,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY,
      "Bye Bye", channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_room) != NULL);
  check_removed (test->chan_room_service);
}

static void
test_leave_room_prepared_no_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_FEATURE_CORE, 0 };

  g_assert (tp_proxy_get_invalidated (test->channel_room) == NULL);

  tp_proxy_prepare_async (test->channel_room, features,
      channel_prepared_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_channel_leave_async (test->channel_room,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      NULL, channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_room) != NULL);
  check_not_removed (test->chan_room_service);
}

static void
test_leave_room_prepared_reason (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CHANNEL_FEATURE_CORE, 0 };

  g_assert (tp_proxy_get_invalidated (test->channel_room) == NULL);

  tp_proxy_prepare_async (test->channel_room, features,
      channel_prepared_cb, test);
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  tp_channel_leave_async (test->channel_room,
      TP_CHANNEL_GROUP_CHANGE_REASON_BUSY,
      "Bye Bye", channel_leave_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_room) != NULL);
  check_removed (test->chan_room_service);
}

static void
channel_close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_channel_close_finish (TP_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_close (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (tp_proxy_get_invalidated (test->channel_contact) == NULL);

  tp_channel_close_async (test->channel_contact, channel_close_cb, test);

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_get_invalidated (test->channel_contact) != NULL);
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

  g_test_add ("/channel/leave/contact/unprepared/no-reason", Test, NULL, setup,
      test_leave_contact_unprepared_no_reason, teardown);
  g_test_add ("/channel/leave/contact/unprepared/reason", Test, NULL, setup,
      test_leave_contact_unprepared_reason, teardown);
  g_test_add ("/channel/leave/contact/prepared/no-reason", Test, NULL, setup,
      test_leave_contact_prepared_no_reason, teardown);
  g_test_add ("/channel/leave/contact/prepared/reason", Test, NULL, setup,
      test_leave_contact_prepared_reason, teardown);

  g_test_add ("/channel/leave/room/unprepared/no-reason", Test, NULL, setup,
      test_leave_room_unprepared_no_reason, teardown);
  g_test_add ("/channel/leave/room/unprepared/reason", Test, NULL, setup,
      test_leave_room_unprepared_reason, teardown);
  g_test_add ("/channel/leave/room/prepared/no-reason", Test, NULL, setup,
      test_leave_room_prepared_no_reason, teardown);
  g_test_add ("/channel/leave/room/prepared/reason", Test, NULL, setup,
      test_leave_room_prepared_reason, teardown);

  g_test_add ("/channel/close", Test, NULL, setup,
      test_close, teardown);

  return g_test_run ();
}
