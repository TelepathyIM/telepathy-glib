/* Regression test for fd.o bug #19101. */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
    guint serial;
} Result;

static GDBusMessage *
filter_cb (GDBusConnection *connection,
    GDBusMessage *message,
    gboolean incoming,
    gpointer user_data)
{
  Result *r = user_data;

  /* We are only interested in outgoing messages */
  if (incoming)
    return message;

  if (!tp_strdiff (g_dbus_message_get_member (message), "GetContactByID"))
    {
      /* Remember the serial of the message so we can catch the reply */
      g_assert (r->serial == 0);
      r->serial = g_dbus_message_get_serial (message);
    }
  else if (r->serial != 0 &&
      g_dbus_message_get_message_type (message) ==
          G_DBUS_MESSAGE_TYPE_METHOD_RETURN &&
      g_dbus_message_get_reply_serial (message) == r->serial)
    {
      GDBusMessage *tmp;
      GVariant *body;
      TpHandle handle;
      GError *error = NULL;

      /* Replace message by a copy to be able to modify it */
      tmp = g_dbus_message_copy (message, &error);
      g_assert_no_error (error);
      g_object_unref (message);
      message = tmp;

      /* Replace body's asv to an empty one */
      body = g_dbus_message_get_body (message);
      g_variant_get (body, "(ua{sv})", &handle, NULL);
      g_dbus_message_set_body (message,
          g_variant_new ("(u@a{sv})", handle,
              g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0)));

      r->serial = 0;
    }

  return message;
}

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
  GDBusConnection *dbus_connection = tp_proxy_get_dbus_connection (client_conn);
  guint filter_id;

  filter_id = g_dbus_connection_add_filter (dbus_connection,
      filter_cb, &result, NULL);

  tp_connection_dup_contact_by_id_async (client_conn,
      "Alice", NULL, by_id_cb, &result);

  g_main_loop_run (result.loop);

  /* Should fail as the CM is broken */
  g_assert_error (result.error, TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT);

  /* clean up */
  g_main_loop_unref (result.loop);
  g_error_free (result.error);
  g_dbus_connection_remove_filter (dbus_connection, filter_id);
}

int
main (int argc,
      char **argv)
{
  TpTestsContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *client_conn;
  GTestDBus *test_dbus;

  /* Setup */

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_dbus_unset ();
  test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (test_dbus);

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &client_conn);
  service_conn = TP_TESTS_CONTACTS_CONNECTION (service_conn_as_base);

  /* Tests */

  test_by_id (client_conn);

  /* Teardown */

  tp_tests_connection_assert_disconnect_succeeds (client_conn);
  g_object_unref (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);

  g_test_dbus_down (test_dbus);
  tp_tests_assert_last_unref (&test_dbus);

  return 0;
}
