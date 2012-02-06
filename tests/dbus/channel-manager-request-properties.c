/* Basic test for the request properties given to channel managers.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "tests/lib/echo-channel-manager-conn.h"
#include "tests/lib/simple-channel-manager.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  TpTestsEchoChannelManagerConnection *service_conn;
  TpTestsSimpleChannelManager *channel_manager;

  TpConnection *conn;
  GError *error /* initialized where needed */;

  guint waiting;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  TpBaseConnection *service_conn_as_base;
  gboolean ok;
  gchar *name, *conn_path;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->channel_manager = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_MANAGER, NULL);
  g_assert (test->channel_manager != NULL);

  test->service_conn = TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION,
        "account", "me@example",
        "protocol", "example",
        "channel-manager", test->channel_manager,
        NULL));
  g_assert (test->service_conn != NULL);
  service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (service_conn_as_base != NULL);

  test->channel_manager->conn = TP_BASE_CONNECTION (test->service_conn);

  ok = tp_base_connection_register (service_conn_as_base, "example",
      &name, &conn_path, &test->error);
  g_assert (ok);
  g_assert_no_error (test->error);

  test->conn = tp_connection_new (test->dbus, name, conn_path, &test->error);
  g_assert (test->conn != NULL);
  g_assert_no_error (test->error);

  g_assert (tp_connection_run_until_ready (test->conn, TRUE, &test->error,
          NULL));
  g_assert_no_error (test->error);

  g_free (name);
  g_free (conn_path);

  test->waiting = 0;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  tp_tests_connection_assert_disconnect_succeeds (test->conn);

  g_object_unref (test->service_conn);
  test->service_conn = NULL;

  g_object_unref (test->dbus);
  test->dbus = NULL;

  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_wait (Test *test)
{
  while (test->waiting > 0)
    g_main_loop_run (test->mainloop);
}

static void
connection_inspect_handles_cb (TpConnection *conn,
    const gchar **ids,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  const gchar *id;

  g_assert_no_error (error);

  g_assert_cmpuint (g_strv_length ((gchar **) ids), ==, 1);

  id = ids[0];

  g_assert_cmpstr (id, ==, "lolbags");

  test->waiting--;
  g_main_loop_quit (test->mainloop);
}

static void
channel_manager_request_cb (TpTestsSimpleChannelManager *channel_manager,
    GHashTable *request_properties,
    Test *test)
{
  const gchar *target_id = tp_asv_get_string (request_properties,
      TP_PROP_CHANNEL_TARGET_ID);
  TpHandle handle = tp_asv_get_uint32 (request_properties,
      TP_PROP_CHANNEL_TARGET_HANDLE, NULL);
  GArray *handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);

  tp_asv_dump (request_properties);

  g_assert (handle != 0);
  g_assert (target_id != NULL);

  g_assert_cmpstr (target_id, ==, "lolbags#dingdong");

  g_array_append_val (handles, handle);

  tp_cli_connection_call_inspect_handles (test->conn, -1,
      TP_HANDLE_TYPE_CONTACT, handles,
      connection_inspect_handles_cb, test, NULL, NULL);
  test->waiting++;

  g_array_unref (handles);

  test->waiting--;
  g_main_loop_quit (test->mainloop);
}

static void
create_channel_cb (TpConnection *proxy,
    const gchar *object_path,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;
  const gchar *target_id = tp_asv_get_string (properties,
      TP_PROP_CHANNEL_TARGET_ID);

  g_assert_no_error (error);

  tp_asv_dump (properties);

  g_assert (target_id != NULL);
  g_assert_cmpstr (target_id, ==, "lolbags");

  test->waiting--;
  g_main_loop_quit (test->mainloop);
}

static void
test_target_id (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *request;

  g_test_bug ("27855");

  request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, "lolbags#dingdong",
      NULL);

  g_signal_connect (test->channel_manager, "request",
      G_CALLBACK (channel_manager_request_cb), test);
  test->waiting++;

  tp_cli_connection_interface_requests_call_create_channel (test->conn, -1,
      request, create_channel_cb, test, NULL, NULL);
  test->waiting++;

  test_wait (test);

  g_hash_table_unref (request);
}

int
main (int argc,
    char **argv)
{
  tp_tests_abort_after (10);
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/channel-manager-request-properties/target-id", Test, NULL, setup,
      test_target_id, teardown);

  return g_test_run ();
}
