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

#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"
#include "tests/lib/simple-channel-dispatcher.h"

#define SERVER "TestServer"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsSimpleChannelDispatcher *cd_service;

    /* Client side objects */
    TpAccount *account;
    TpConnection *connection;
    TpRoomList *room_list;

    GPtrArray *rooms; /* reffed TpRoomInfo */
    GError *error /* initialized where needed */;
    gint wait;
} Test;

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

static void
new_async_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  test->room_list = tp_room_list_new_finish (result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
create_room_list (Test *test,
    const char *server)
{
  tp_clear_object (&test->room_list);

  tp_room_list_new_async (test->account, server,
      new_async_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
}

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  test->rooms = g_ptr_array_new_with_free_func (g_object_unref);

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  /* Claim CD bus-name */
  tp_dbus_daemon_request_name (test->dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  /* Create and register CD */
  test->cd_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCHER,
      "connection", test->base_connection,
      NULL);

  tp_dbus_daemon_register_object (test->dbus, TP_CHANNEL_DISPATCHER_OBJECT_PATH,
      test->cd_service);

  create_room_list (test, SERVER);
  g_assert_no_error (test->error);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_dbus_daemon_release_name (test->dbus, TP_CHANNEL_DISPATCHER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  tp_clear_object (&test->cd_service);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  tp_clear_object (&test->account);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->room_list);
  g_ptr_array_unref (test->rooms);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (TP_IS_ROOM_LIST (test->room_list));
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gchar *server;
  gboolean listing;

  g_object_get (test->room_list,
      "server", &server,
      "listing", &listing,
      NULL);

  g_assert_cmpstr (server, ==, SERVER);
  g_assert_cmpstr (tp_room_list_get_server (test->room_list), ==,
      SERVER);

  g_assert (!listing);
  g_assert (!tp_room_list_is_listing (test->room_list));

  /* Create new one without server */
  tp_clear_object (&test->room_list);

  create_room_list (test, NULL);
  g_assert_no_error (test->error);

  g_assert_cmpstr (tp_room_list_get_server (test->room_list), ==,
      NULL);
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
got_room_cb (TpRoomList *channel,
    TpRoomInfo *room,
    Test *test)
{
  g_ptr_array_add (test->rooms, g_object_ref (room));

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_listing (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpRoomInfo *room;
  gboolean known;

  g_assert (!tp_room_list_is_listing (test->room_list));

  g_signal_connect (test->room_list, "notify::listing",
      G_CALLBACK (notify_cb), test);

  g_signal_connect (test->room_list, "got-room",
      G_CALLBACK (got_room_cb), test);

  tp_room_list_start (test->room_list);

  test->wait = 4;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_room_list_is_listing (test->room_list));

  g_assert_cmpuint (test->rooms->len, ==, 3);

  room = g_ptr_array_index (test->rooms, 0);
  g_assert (TP_IS_ROOM_INFO (room));

  g_assert_cmpuint (tp_room_info_get_handle (room), ==, 0);
  g_assert_cmpstr (tp_room_info_get_channel_type (room), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpstr (tp_room_info_get_handle_name (room), ==, "the handle name");
  g_assert_cmpstr (tp_room_info_get_name (room), ==, "the name");
  g_assert_cmpstr (tp_room_info_get_description (room), ==, "the description");
  g_assert_cmpstr (tp_room_info_get_subject (room), ==, "the subject");
  g_assert_cmpuint (tp_room_info_get_members_count (room, &known), ==, 10);
  g_assert (known);
  g_assert (tp_room_info_get_requires_password (room, &known));
  g_assert (known);
  g_assert (tp_room_info_get_invite_only (room, &known));
  g_assert (known);
  g_assert_cmpstr (tp_room_info_get_room_id (room), ==, "the room id");
  g_assert_cmpstr (tp_room_info_get_server (room), ==, "the server");
}

static void
room_list_failed_cb (TpRoomList *room_list,
    GError *error,
    Test *test)
{
  g_clear_error (&test->error);
  test->error = g_error_copy (error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_list_room_fails (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gulong id;

  /* Use magic server to tell to the channel to fail ListRooms() */
  tp_clear_object (&test->room_list);

  create_room_list (test, "ListRoomsFail");

  id = g_signal_connect (test->room_list, "failed",
      G_CALLBACK (room_list_failed_cb), test);

  tp_room_list_start (test->room_list);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_SERVICE_CONFUSED);

  /* We don't want the 'failed' cb be called when disconnecting the
   * connection */
  g_signal_handler_disconnect (test->room_list, id);
}

static void
test_invalidated (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const gchar *path;
  TpChannel *chan;
  gulong id;

  id = g_signal_connect (test->room_list, "failed",
      G_CALLBACK (room_list_failed_cb), test);

  /* Create a proxy on the room list channel to close it */
  path = tp_tests_simple_connection_ensure_room_list_chan (
      TP_TESTS_SIMPLE_CONNECTION (test->base_connection), SERVER, NULL);

  chan = tp_channel_new (test->connection, path,
      TP_IFACE_CHANNEL_TYPE_ROOM_LIST, TP_HANDLE_TYPE_NONE, 0,
      &test->error);
  g_assert_no_error (test->error);

  tp_channel_close_async (chan, NULL, NULL);
  g_object_unref (chan);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED);

  g_signal_handler_disconnect (test->room_list, id);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/room-list-channel/creation", Test, NULL, setup,
      test_creation, teardown);
  g_test_add ("/room-list-channel/properties", Test, NULL, setup,
      test_properties, teardown);
  g_test_add ("/room-list-channel/listing", Test, NULL, setup,
      test_listing, teardown);
  g_test_add ("/room-list-channel/list-rooms-fail", Test, NULL, setup,
      test_list_room_fails, teardown);
  g_test_add ("/room-list-channel/invalidated", Test, NULL, setup,
      test_invalidated, teardown);

  return g_test_run ();
}
