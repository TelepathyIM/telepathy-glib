/* Tests of TpStreamTube
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/stream-tube.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/stream-tube-chan.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsStreamTubeChannel *tube_chan_service;

    /* Client side objects */
    TpConnection *connection;
    TpStreamTube *tube;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

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

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
create_tube_service (Test *test,
    gboolean requested)
{
  gchar *chan_path;
  TpHandle handle;
  TpHandleRepoIface *contact_repo;
  GHashTable *props;

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (contact_repo != NULL);

  handle = tp_handle_ensure (contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  test->tube_chan_service = g_object_new (TP_TESTS_TYPE_STREAM_TUBE_CHANNEL,
      "connection", test->base_connection,
      "handle", handle,
      "requested", requested,
      "object-path", chan_path,
      NULL);

  /* Create client-side tube channel object */
  g_object_get (test->tube_chan_service, "channel-properties", &props, NULL);

  test->tube = tp_stream_tube_new (test->connection,
      chan_path, props, &test->error);

  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (props);
  tp_handle_unref (contact_repo, handle);
}

/* Test Basis */

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_tube_service (test, TRUE);

  g_assert (TP_IS_STREAM_TUBE (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));

  create_tube_service (test, FALSE);

  g_assert (TP_IS_STREAM_TUBE (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/stream-tube/creation", Test, NULL, setup, test_creation,
      teardown);

  return g_test_run ();
}
