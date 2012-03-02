/* Tests of TpFileTransferChannel
 *
 * Copyright (C) 2010-2011 Morten Mjelva <morten.mjelva@gmail.com>
 * Copyright (C) 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include <telepathy-glib/file-transfer-channel.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/dbus.h>

#include "tests/lib/util.h"
#include "tests/lib/debug.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/file-transfer-chan.h"
#include "tests/lib/stream-tube-chan.h"

typedef struct {
    TpSocketAddressType address_type;
    TpSocketAccessControl access_control;
} TestContext;

TestContext contexts[] = {
  { TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_LOCALHOST },
//  { TP_SOCKET_ADDRESS_TYPE_UNIX, TP_SOCKET_ACCESS_CONTROL_CREDENTIALS },
//  { TP_SOCKET_ADDRESS_TYPE_IPV4, TP_SOCKET_ACCESS_CONTROL_LOCALHOST },
//  { TP_SOCKET_ADDRESS_TYPE_IPV4, TP_SOCKET_ACCESS_CONTROL_PORT },
//  { TP_SOCKET_ADDRESS_TYPE_IPV6, TP_SOCKET_ACCESS_CONTROL_LOCALHOST },
//  { TP_SOCKET_ADDRESS_TYPE_IPV6, TP_SOCKET_ACCESS_CONTROL_PORT },

  { TP_NUM_SOCKET_ADDRESS_TYPES, TP_NUM_SOCKET_ACCESS_CONTROLS }
};

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsFileTransferChannel *chan_service;
    TpHandleRepoIface *contact_repo;
    TpHandleRepoIface *room_repo;

    /* Client side objects */
    TpConnection *connection;
    TpFileTransferChannel *channel;
    GIOStream *cm_stream;

    GError *error /* initialized where needed */;
    gint wait;
} Test;


/* Callbacks */

static void
state_notify_cb (GObject *source,
    GParamSpec *pspec,
    Test *test)
{
  DEBUG ("state_notify_cb was triggered");

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
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
file_provide_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  DEBUG ("file_provide_cb reached");

  tp_file_transfer_channel_provide_file_finish (
      TP_FILE_TRANSFER_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
file_accept_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  DEBUG ("file_accept_cb reached");

  tp_file_transfer_channel_accept_file_finish (
      TP_FILE_TRANSFER_CHANNEL (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}


/* Internal functions */

static void
destroy_socket_control_list (gpointer data)
{
  GArray *tab = data;
  g_array_unref (tab);
}

static GHashTable *
create_available_socket_types_hash (TpSocketAddressType address_type,
    TpSocketAccessControl access_control)
{
  GHashTable *ret;
  GArray *tab;

  ret = g_hash_table_new_full (NULL, NULL, NULL, destroy_socket_control_list);

  tab = g_array_sized_new (FALSE, FALSE, sizeof (TpSocketAccessControl),
      1);
  g_array_append_val (tab, access_control);

  g_hash_table_insert (ret, GUINT_TO_POINTER (address_type), tab);

  return ret;
}

static void
create_file_transfer_channel (Test *test,
    gboolean requested,
    TpSocketAddressType address_type,
    TpSocketAccessControl access_control)
{
  gchar *chan_path;
  TpHandle handle, alf_handle;
  GHashTable *props;
  GHashTable *sockets;
  GHashTable *metadata;
  GQuark features[] = { TP_FILE_TRANSFER_CHANNEL_FEATURE_CORE, 0};
  const gchar * const metadata_values[] = { "cheese", NULL };

  /* Create service-side file transfer channel object */
  tp_proxy_get_object_path (test->connection);
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  alf_handle = tp_handle_ensure (test->contact_repo, "alf", NULL, &test->error);
  g_assert_no_error (test->error);

  sockets = create_available_socket_types_hash (address_type, access_control);

  metadata = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (metadata, "banana", (gpointer) metadata_values);

  test->chan_service = g_object_new (
      TP_TESTS_TYPE_FILE_TRANSFER_CHANNEL,
      /* TpProxy properties */
      "object-path", chan_path,

      /* TpChannel properties */
      "connection", test->base_connection,
      "handle", handle,
      "initiator-handle", alf_handle,
      "requested", requested,

      /* TpFileTransferChannel properties */
      "available-socket-types", sockets,
      "content-type", "text/plain",
      "date", (guint64) 271828,
      "description", "badger",
      "filename", "snake.txt",
      "initial-offset", (guint64) 0,
      "size", (guint64) 9001,
      "state", TP_FILE_TRANSFER_STATE_PENDING,
      "transferred-bytes", (guint64) 42,
      /* Metadata properties */
      "service-name", "fit.service.name",
      "metadata", metadata,
      NULL);

  /* Create client-side file transfer channel object */
  g_object_get (test->chan_service,
      "channel-properties", &props,
      NULL);

  test->channel = tp_file_transfer_channel_new (test->connection, chan_path,
      props, &test->error);
  g_assert_no_error (test->error);

  /* Prepare core feature */
  tp_proxy_prepare_async (test->channel, features, channel_prepared_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (metadata);
  g_hash_table_unref (props);
  g_hash_table_unref (sockets);
  tp_handle_unref (test->contact_repo, handle);
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
  tp_clear_object (&test->cm_stream);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_object (&test->channel);
}

typedef void (*TestFunc) (Test *, gconstpointer);

static gchar *
test_context_to_str (TestContext *ctx,
    const gchar *base)
{
  const gchar *socket, *access_control;

  switch (ctx->address_type)
    {
      case TP_SOCKET_ADDRESS_TYPE_UNIX:
        socket = "unix";
        break;
      case TP_SOCKET_ADDRESS_TYPE_IPV4:
        socket = "ipv4";
        break;
      case TP_SOCKET_ADDRESS_TYPE_IPV6:
        socket = "ipv6";
        break;
      default:
        g_assert_not_reached ();
    }

  switch (ctx->access_control)
    {
      case TP_SOCKET_ACCESS_CONTROL_LOCALHOST:
        access_control = "localhost";
        break;
      case TP_SOCKET_ACCESS_CONTROL_PORT:
        access_control = "port";
        break;
      case TP_SOCKET_ACCESS_CONTROL_CREDENTIALS:
        access_control = "credentials";
        break;
      default:
        g_assert_not_reached ();
    }

  return g_strdup_printf ("%s/%s/%s", base, socket, access_control);
}

static void
socket_connected (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_clear_object (&test->cm_stream);

  test->cm_stream = G_IO_STREAM (g_socket_client_connect_finish (
        G_SOCKET_CLIENT (source), result, &test->error));

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
run_file_transfer_test (const char *test_path,
    TestFunc ftest)
{
  guint i;

  for (i = 0; contexts[i].address_type != TP_NUM_SOCKET_ADDRESS_TYPES; i++)
    {
      gchar *path = test_context_to_str (&contexts[i], test_path);

      g_test_add (path, Test, GUINT_TO_POINTER (i), setup, ftest, teardown);

      g_free (path);
    }
}


/* Tests */

/* Test channel creation */
static void
test_create_requested (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const GError *error = NULL;

  create_file_transfer_channel (test, TRUE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  g_assert (TP_IS_FILE_TRANSFER_CHANNEL (test->channel));
  g_assert (G_IS_OBJECT (test->channel));

  error = tp_proxy_get_invalidated (test->channel);
  g_assert_no_error (error);
}

static void
test_create_unrequested (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const GError *error = NULL;

  create_file_transfer_channel (test, FALSE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  g_assert (TP_IS_FILE_TRANSFER_CHANNEL (test->channel));
  g_assert (G_IS_OBJECT (test->channel));

  error = tp_proxy_get_invalidated (test->channel);
  g_assert_no_error (error);
}

/* Test setting and getting properties */
static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GDateTime *date1, *date2;
  TpFileTransferStateChangeReason reason;
  const GError *error = NULL;
  const GHashTable *metadata;
  const gchar * const *metadata_values;

  create_file_transfer_channel (test, FALSE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  g_assert_cmpstr (tp_file_transfer_channel_get_mime_type (test->channel),
      ==, "text/plain");

  date1 = tp_file_transfer_channel_get_date (test->channel);
  date2 = g_date_time_new_from_unix_utc (271828);
  g_assert (g_date_time_equal (date1, date2));
  g_date_time_unref (date2);

  g_assert_cmpstr (tp_file_transfer_channel_get_description (test->channel),
      ==, "badger");

  g_assert_cmpstr (tp_file_transfer_channel_get_filename (test->channel),
      ==, "snake.txt");

  g_assert_cmpuint (tp_file_transfer_channel_get_size (test->channel),
      ==, 9001);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_PENDING);

  g_assert_cmpuint (tp_file_transfer_channel_get_transferred_bytes
      (test->channel), ==, 42);

  g_assert_cmpstr (tp_file_transfer_channel_get_service_name (test->channel),
      ==, "fit.service.name");

  metadata = tp_file_transfer_channel_get_metadata (test->channel);
  g_assert_cmpuint (g_hash_table_size ((GHashTable *) metadata), ==, 1);
  metadata_values = g_hash_table_lookup ((GHashTable *) metadata, "banana");
  g_assert_cmpuint (g_strv_length ((GStrv) metadata_values), ==, 1);
  g_assert_cmpstr (metadata_values[0], ==, "cheese");

  error = tp_proxy_get_invalidated (test->channel);
  g_assert_no_error (error);
}

/* Test sending files */
static void
test_provide_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GSocketAddress *address;
  GSocketClient *client;
  GFile *file;
  TpFileTransferStateChangeReason reason;
  guint i = GPOINTER_TO_UINT (data);

  create_file_transfer_channel (test, TRUE, contexts[i].address_type,
      contexts[i].access_control);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_PENDING);

  /* not very pretty */
  g_file_set_contents ("/tmp/file-transfer", "test", -1, NULL);

  file = g_file_new_for_uri ("file:///tmp/file-transfer");
  tp_file_transfer_channel_provide_file_async (test->channel,
      file, file_provide_cb, test);
  g_object_unref (file);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_PENDING);

  g_signal_connect (test->channel, "notify::state",
      G_CALLBACK (state_notify_cb), test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* File transfer should be in the open state by now */
  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_OPEN);

  /* A wild CLIENT appears */
  address = tp_tests_file_transfer_channel_get_server_address
    (test->chan_service);
  g_assert (address != NULL);
  client = g_socket_client_new ();
  g_socket_client_connect_async (client, G_SOCKET_CONNECTABLE (address),
      NULL, socket_connected, test);

  g_object_unref (client);
  g_object_unref (address);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
  g_assert (test->cm_stream != NULL);
}

static void
test_cancel_transfer (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpFileTransferStateChangeReason reason;

  create_file_transfer_channel (test, FALSE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_PENDING);
}

/* Test receiving files */
static void
test_accept_success (Test *test, gconstpointer data G_GNUC_UNUSED)
{
  GFile *file;
  TpFileTransferStateChangeReason reason;
  guint i = GPOINTER_TO_UINT (data);

  create_file_transfer_channel (test, FALSE, contexts[i].address_type,
      contexts[i].access_control);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_PENDING);

  file = g_file_new_for_uri ("file:///tmp/file-transfer");
  tp_file_transfer_channel_accept_file_async (test->channel,
      file, 0, file_accept_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_ACCEPTED);

  g_signal_connect (test->channel, "notify::state",
      G_CALLBACK (state_notify_cb), test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* File transfer should be in the open state by now */
  g_assert_cmpuint (tp_file_transfer_channel_get_state (test->channel, &reason),
      ==, TP_FILE_TRANSFER_STATE_OPEN);

  g_object_unref (file);
}

static void
test_accept_twice (Test *test, gconstpointer data G_GNUC_UNUSED)
{
  GFile *file;

  create_file_transfer_channel (test, FALSE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  file = g_file_new_for_uri ("file:///tmp/file-transfer");

  tp_file_transfer_channel_accept_file_async (test->channel,
      file, 0, file_accept_cb, test);
  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Try to re-accept the transfer */
  tp_file_transfer_channel_accept_file_async (test->channel,
      file, 0, file_accept_cb, test);
  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);

  g_object_unref (file);
}

static void
test_accept_outgoing (Test *test, gconstpointer data G_GNUC_UNUSED)
{
  GFile *file;

  create_file_transfer_channel (test, TRUE, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST);

  file = g_file_new_for_uri ("file:///tmp/file-transfer");

  tp_file_transfer_channel_accept_file_async (test->channel,
      file, 0, file_accept_cb, test);
  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
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

  /* Test basic object creation etc */
  g_test_add ("/file-transfer-channel/create/requested", Test, NULL, setup,
      test_create_requested, teardown);
  g_test_add ("/file-transfer-channel/create/unrequested", Test, NULL, setup,
      test_create_unrequested, teardown);
  g_test_add ("/file-transfer-channel/properties", Test, NULL, setup,
      test_properties, teardown);

  /* Run provide and accept in different contexts */
  run_file_transfer_test ("/file-transfer-channel/accept/success",
      test_accept_success);
  run_file_transfer_test ("/file-transfer-channel/provide/success",
      test_provide_success);

  /* Test edge cases */
  /* FIXME: accept_twice has to be after provide/accept_success */
  g_test_add ("/file-transfer-channel/accept/twice", Test, NULL, setup,
      test_accept_twice, teardown);
  g_test_add ("/file-transfer-channel/accept/outgoing", Test, NULL, setup,
      test_accept_outgoing, teardown);
  g_test_add ("/file-transfer-channel/provide/cancel", Test, NULL, setup,
      test_cancel_transfer, teardown);

  return g_test_run ();
}
