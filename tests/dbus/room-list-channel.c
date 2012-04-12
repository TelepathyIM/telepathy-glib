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
#include <telepathy-glib/room-list-channel-internal.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/room-list-chan.h"
#include "tests/lib/util.h"

#define SERVER "TestServer"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsRoomListChan *chan_service;

    /* Client side objects */
    TpConnection *connection;
    TpRoomListChannel *channel;

    GPtrArray *rooms; /* reffed TpRoomInfo */
    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
create_room_list_chan (Test *test)
{
  gchar *chan_path;
  GHashTable *props;

  tp_clear_object (&test->chan_service);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->chan_service = g_object_new (
      TP_TESTS_TYPE_ROOM_LIST_CHAN,
      "connection", test->base_connection,
      "object-path", chan_path,
      "server", SERVER,
      NULL);

  g_object_get (test->chan_service,
      "channel-properties", &props,
      NULL);

  test->channel = _tp_room_list_channel_new (NULL,
      test->connection, chan_path,
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

  test->rooms = g_ptr_array_new_with_free_func (g_object_unref);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  create_room_list_chan (test);
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

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->channel);
  g_ptr_array_unref (test->rooms);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const GError *error = NULL;

  g_assert (TP_IS_ROOM_LIST_CHANNEL (test->channel));

  error = tp_proxy_get_invalidated (test->channel);
  g_assert_no_error (error);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gchar *server;
  gboolean listing;

  g_object_get (test->channel,
      "server", &server,
      "listing", &listing,
      NULL);

  g_assert_cmpstr (server, ==, SERVER);
  g_assert_cmpstr (tp_room_list_channel_get_server (test->channel), ==,
      SERVER);

  g_assert (!listing);
  g_assert (!tp_room_list_channel_get_listing (test->channel));
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
start_listing_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_room_list_channel_start_listing_finish (TP_ROOM_LIST_CHANNEL (source),
      result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
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
got_rooms_cb (TpRoomListChannel *channel,
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
  GQuark features[] = { TP_ROOM_LIST_CHANNEL_FEATURE_LISTING, 0 };
  TpRoomInfo *room;
  gboolean known;

  g_assert (!tp_room_list_channel_get_listing (test->channel));

  tp_proxy_prepare_async (test->channel, features, proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (!tp_room_list_channel_get_listing (test->channel));

  g_signal_connect (test->channel, "notify::listing",
      G_CALLBACK (notify_cb), test);

  g_signal_connect (test->channel, "got-rooms",
      G_CALLBACK (got_rooms_cb), test);

  tp_room_list_channel_start_listing_async (test->channel, start_listing_cb,
      test);

  test->wait = 5;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_room_list_channel_get_listing (test->channel));

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
  g_assert_cmpuint (tp_room_info_get_members (room, &known), ==, 10);
  g_assert (known);
  g_assert (tp_room_info_get_requires_password (room, &known));
  g_assert (known);
  g_assert (tp_room_info_get_invite_only (room, &known));
  g_assert (known);
  g_assert_cmpstr (tp_room_info_get_room_id (room), ==, "the room id");
  g_assert_cmpstr (tp_room_info_get_server (room), ==, "the server");
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

  return g_test_run ();
}
