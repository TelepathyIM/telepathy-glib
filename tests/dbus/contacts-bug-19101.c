/* Regression test for fd.o bug #19101. */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/bug-19101-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
} Result;

static void
by_id_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) source;
  Result *r = user_data;
  TpContact *contact;

  contact = tp_connection_dup_contact_by_id_finish (connection, result,
      &r->error);
  g_assert (contact == NULL);

  g_main_loop_quit (r->loop);
}

static void
test_by_id (TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE) };

  tp_connection_dup_contact_by_id_async (client_conn,
      "Alice", NULL, by_id_cb, &result);

  g_main_loop_run (result.loop);

  /* Should fail as the CM is broken */
  g_assert_error (result.error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT);

  /* clean up */
  g_main_loop_unref (result.loop);
  g_error_free (result.error);
}

int
main (int argc,
      char **argv)
{
  TpTestsContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *client_conn;

  /* Setup */

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  tp_tests_create_conn (TP_TESTS_TYPE_BUG19101_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &client_conn);
  service_conn = TP_TESTS_CONTACTS_CONNECTION (service_conn_as_base);

  /* Tests */

  test_by_id (client_conn);

  /* Teardown */

  tp_tests_connection_assert_disconnect_succeeds (client_conn);
  g_object_unref (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);

  return 0;
}
