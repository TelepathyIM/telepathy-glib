/* Feature test for the user's self-handle/self-contact changing.
 *
 * Copyright (C) 2009-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
  TpDBusDaemon *dbus;
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error /* zero-initialized */;
  TpConnection *client_conn;
  TpHandleRepoIface *contact_repo;
  GAsyncResult *result;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer arg)
{
  gboolean ok;

  f->dbus = tp_tests_dbus_daemon_dup_or_die ();

  f->service_conn = TP_TESTS_SIMPLE_CONNECTION (
      tp_tests_object_new_static_class (TP_TESTS_TYPE_SIMPLE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        "break-0192-properties", (!tp_strdiff (arg, "archaic")),
        NULL));
  f->service_conn_as_base = TP_BASE_CONNECTION (f->service_conn);
  g_object_ref (f->service_conn_as_base);
  g_assert (f->service_conn != NULL);
  g_assert (f->service_conn_as_base != NULL);

  f->contact_repo = tp_base_connection_get_handles (f->service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);

  ok = tp_base_connection_register (f->service_conn_as_base, "simple",
        &f->name, &f->conn_path, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->client_conn = tp_connection_new (f->dbus, f->name, f->conn_path,
      &f->error);
  g_assert_no_error (f->error);
  g_assert (f->client_conn != NULL);
}

static void
setup_and_connect (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  gboolean ok;

  setup (f, unused);

  ok = tp_connection_run_until_ready (f->client_conn, TRUE, &f->error, NULL);
  g_assert_no_error (f->error);
  g_assert (ok);
}

/* we'll get more arguments, but just ignore them */
static void
swapped_counter_cb (gpointer user_data)
{
  guint *times = user_data;

  ++*times;
}

static void
test_self_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *before, *after;
  guint handle_times = 0, contact_times = 0;

  g_signal_connect_swapped (f->client_conn, "notify::self-handle",
      G_CALLBACK (swapped_counter_cb), &handle_times);
  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      "self-contact", &before,
      NULL);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpuint (tp_contact_get_handle (before), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (before), ==, "me@example.com");

  g_assert_cmpuint (handle_times, ==, 0);
  g_assert_cmpuint (contact_times, ==, 0);

  /* similar to /nick in IRC */
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);

  while (handle_times < 1 || contact_times < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (handle_times, ==, 1);
  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      "self-contact", &after,
      NULL);
  g_assert (before != after);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpuint (tp_contact_get_handle (after), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (before);
  g_object_unref (after);
}

static void
test_change_early (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  TpContact *after;
  guint handle_times = 0, contact_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_signal_connect_swapped (f->client_conn, "notify::self-handle",
      G_CALLBACK (swapped_counter_cb), &handle_times);
  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect; emit signals in quick
   * succession, so that by the time the TpConnection tries to investigate
   * the self-handle, it has already changed */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-handle and self-contact change once during connection */
  g_assert_cmpuint (handle_times, ==, 1);
  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpuint (tp_contact_get_handle (after), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
test_change_inconveniently (Fixture *f,
    gconstpointer arg)
{
  TpHandle handle;
  TpContact *after;
  guint handle_times = 0, contact_times = 0, got_self_handle_times = 0;
  guint got_all_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_signal_connect_swapped (f->client_conn, "notify::self-handle",
      G_CALLBACK (swapped_counter_cb), &handle_times);
  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  if (!tp_strdiff (arg, "archaic"))
    {
      g_signal_connect_swapped (f->service_conn, "got-self-handle",
          G_CALLBACK (swapped_counter_cb), &got_self_handle_times);
    }
  else
    {
      g_signal_connect_swapped (f->service_conn,
          "got-all::" TP_IFACE_CONNECTION,
          G_CALLBACK (swapped_counter_cb), &got_all_times);
    }

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  /* run the main loop until just after GetSelfHandle or GetAll(Connection)
   * is processed, to make sure the client first saw the old self handle */
  while (got_self_handle_times == 0 && got_all_times == 0)
    g_main_context_iteration (NULL, TRUE);

  DEBUG ("changing my own identifier to something else");
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-handle and self-contact change once during connection */
  g_assert_cmpuint (handle_times, ==, 1);
  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpuint (tp_contact_get_handle (after), ==, handle);
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
test_self_handle_fails (Fixture *f,
    gconstpointer arg)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  gboolean ok;

  /* This test assumes that spec 0.19.2 properties are unsupported. */
  g_assert_cmpstr (arg, ==, "archaic");

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  tp_tests_simple_connection_set_get_self_handle_error (f->service_conn,
      TP_ERROR, TP_ERROR_CONFUSED, "totally wasted");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_error (f->error, TP_ERROR, TP_ERROR_CONFUSED);
  g_assert (!ok);
  g_clear_error (&f->error);

  g_assert_error (tp_proxy_get_invalidated (f->client_conn), TP_ERROR,
      TP_ERROR_CONFUSED);

  /* don't want to Disconnect during teardown - it'll just fail */
  tp_tests_simple_connection_inject_disconnect (f->service_conn);
  tp_clear_object (&f->client_conn);
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  g_clear_error (&f->error);

  if (f->client_conn != NULL)
    tp_tests_connection_assert_disconnect_succeeds (f->client_conn);

  tp_clear_object (&f->result);
  tp_clear_object (&f->client_conn);
  tp_clear_object (&f->service_conn_as_base);
  tp_clear_object (&f->service_conn);
  tp_clear_pointer (&f->name, g_free);
  tp_clear_pointer (&f->conn_path, g_free);
  tp_clear_object (&f->dbus);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_set_prgname ("self-handle");
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/self-handle", Fixture, NULL, setup_and_connect,
      test_self_handle, teardown);
  g_test_add ("/self-handle/archaic", Fixture, "archaic", setup_and_connect,
      test_self_handle, teardown);
  g_test_add ("/self-handle/change-early", Fixture, NULL, setup,
      test_change_early, teardown);
  g_test_add ("/self-handle/change-early/archaic", Fixture, "archaic", setup,
      test_change_early, teardown);
  g_test_add ("/self-handle/change-inconveniently", Fixture, NULL,
      setup, test_change_inconveniently, teardown);
  g_test_add ("/self-handle/change-inconveniently/archaic", Fixture,
      "archaic", setup, test_change_inconveniently, teardown);
  g_test_add ("/self-handle/fails", Fixture, "archaic", setup,
      test_self_handle_fails, teardown);

  return g_test_run ();
}
