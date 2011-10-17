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
#include <telepathy-glib/message-mixin.h>

#include "examples/cm/contactlist/conn.h"

#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpHandleRepoIface *contact_repo;

    /* Client side objects */
    TpConnection *connection;
    TpTextChannel *channel;
    TpTextChannel *sms_channel;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  gchar *conn_name, *conn_path;
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Create (service and client sides) connection objects */
  test->base_connection = tp_tests_object_new_static_class (
        EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
        "account", "me@test.com",
        "simulation-delay", 0,
        "protocol", "test",
        NULL);

  g_assert (tp_base_connection_register (test->base_connection, "example",
        &conn_name, &conn_path, &test->error));
  g_assert_no_error (test->error);

  test->connection = tp_connection_new (test->dbus, conn_name, conn_path,
      &test->error);
  g_assert_no_error (test->error);

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  /* Connect the connection */
  tp_cli_connection_call_connect (test->connection, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (test->connection, conn_features);

  g_free (conn_name);
  g_free (conn_path);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
block_contacts_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_connection_block_contacts_finish (TP_CONNECTION (source), result,
      &test->error);
  g_assert_no_error (test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
unblock_contacts_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_connection_unblock_contacts_finish (TP_CONNECTION (source), result,
      &test->error);
  g_assert_no_error (test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_block_unblock (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *alice, *bob;
  GPtrArray *arr;

  /* Create contacts */
  handle = tp_handle_ensure (test->contact_repo, "alice", NULL, &test->error);
  g_assert_no_error (test->error);

  alice = tp_connection_dup_contact_if_possible (test->connection, handle,
      "alice");
  g_assert (alice != NULL);

  handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
  g_assert_no_error (test->error);

  bob = tp_connection_dup_contact_if_possible (test->connection, handle, "bob");
  g_assert (bob != NULL);

  arr = g_ptr_array_sized_new (2);
  g_ptr_array_add (arr, alice);
  g_ptr_array_add (arr, bob);

  /* Block contacts */
  tp_connection_block_contacts_async (test->connection,
      arr->len, (TpContact * const *) arr->pdata, FALSE,
      block_contacts_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Unblock contacts */
  tp_connection_unblock_contacts_async (test->connection,
      arr->len, (TpContact * const *) arr->pdata,
      unblock_contacts_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_object_unref (alice);
  g_object_unref (bob);
  g_ptr_array_unref (arr);
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
test_can_report_abusive (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_CONTACT_BLOCKING, 0 };
  gboolean abuse;

  /* Feature is not prepared yet */
  g_object_get (test->connection, "can-report-abusive", &abuse, NULL);
  g_assert (!abuse);
  g_assert (!tp_connection_can_report_abusive (test->connection));

  tp_proxy_prepare_async (test->connection, features,
      proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->connection,
        TP_CONNECTION_FEATURE_CONTACT_BLOCKING));

  g_object_get (test->connection, "can-report-abusive", &abuse, NULL);
  g_assert (abuse);
  g_assert (tp_connection_can_report_abusive (test->connection));
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/contact-list-clt/blocking/block-unblock", Test, NULL, setup,
      test_block_unblock, teardown);
  g_test_add ("/contact-list-clt/blocking/can-report-abusive", Test, NULL,
      setup, test_can_report_abusive, teardown);

  return g_test_run ();
}
