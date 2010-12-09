/* Feature test for the user's self-handle/self-contact changing.
 *
 * Copyright (C) 2009-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

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
} Fixture;

static void
setup (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  gboolean ok;

  f->dbus = tp_tests_dbus_daemon_dup_or_die ();

  f->service_conn = TP_TESTS_SIMPLE_CONNECTION (
      tp_tests_object_new_static_class (TP_TESTS_TYPE_SIMPLE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  f->service_conn_as_base = TP_BASE_CONNECTION (f->service_conn);
  g_object_ref (f->service_conn_as_base);
  g_assert (f->service_conn != NULL);
  g_assert (f->service_conn_as_base != NULL);

  ok = tp_base_connection_register (f->service_conn_as_base, "simple",
        &f->name, &f->conn_path, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->client_conn = tp_connection_new (f->dbus, f->name, f->conn_path,
      &f->error);
  g_assert_no_error (f->error);
  g_assert (f->client_conn != NULL);
  ok = tp_connection_run_until_ready (f->client_conn, TRUE, &f->error, NULL);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->contact_repo = tp_base_connection_get_handles (f->service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
}

static void
on_self_handle_changed (TpConnection *client_conn,
                        GParamSpec *param_spec G_GNUC_UNUSED,
                        gpointer user_data)
{
  guint *times = user_data;

  ++*times;
}

static void
test_self_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpHandle handle;
  guint times = 0;

  g_signal_connect (f->client_conn, "notify::self-handle",
      G_CALLBACK (on_self_handle_changed), &times);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      NULL);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_assert_cmpuint (times, ==, 0);

  /* similar to /nick in IRC */
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);

  while (times < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (times, ==, 1);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  g_assert_cmpuint (tp_connection_get_self_handle (f->client_conn), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-handle", &handle,
      NULL);
  g_assert_cmpuint (handle, ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  gboolean ok;

  g_clear_error (&f->error);

  if (f->client_conn != NULL)
    {
      ok = tp_cli_connection_run_disconnect (f->client_conn, -1, &f->error,
          NULL);
      g_assert_no_error (f->error);
      g_assert (ok);
    }

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
  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");
  g_set_prgname ("self-handle");
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/self-handle", Fixture, NULL, setup, test_self_handle,
      teardown);

  return g_test_run ();
}
