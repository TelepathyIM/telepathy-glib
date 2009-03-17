/* Regression test for fd.o bug #19101. */

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
    GPtrArray *contacts;
    gchar **good_ids;
    GHashTable *bad_ids;
} Result;

static void
finish (gpointer r)
{
  Result *result = r;

  g_main_loop_quit (result->loop);
}

static void
by_id_cb (TpConnection *connection,
          guint n_contacts,
          TpContact * const *contacts,
          const gchar * const *good_ids,
          GHashTable *bad_ids,
          const GError *error,
          gpointer user_data,
          GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->contacts == NULL);
  g_assert (result->error == NULL);
  g_assert (result->good_ids == NULL);
  g_assert (result->bad_ids == NULL);

  if (error == NULL)
    {
      GHashTableIter iter;
      gpointer key, value;
      guint i;

      DEBUG ("got %u contacts and %u bad IDs", n_contacts,
          g_hash_table_size (bad_ids));

      result->bad_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
          g_free, (GDestroyNotify) g_error_free);
      tp_g_hash_table_update (result->bad_ids, bad_ids,
          (GBoxedCopyFunc) g_strdup, (GBoxedCopyFunc) g_error_copy);

      g_hash_table_iter_init (&iter, result->bad_ids);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          gchar *id = key;
          GError *e = value;

          DEBUG ("bad ID %s: %s %u: %s", id, g_quark_to_string (e->domain),
              e->code, e->message);
        }

      result->good_ids = g_strdupv ((GStrv) good_ids);

      result->contacts = g_ptr_array_sized_new (n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          DEBUG ("contact #%u: %p", i, contact);
          DEBUG ("contact #%u we asked for ID %s", i, good_ids[i]);
          DEBUG ("contact #%u we got ID %s", i,
              tp_contact_get_identifier (contact));
          DEBUG ("contact #%u alias: %s", i, tp_contact_get_alias (contact));
          DEBUG ("contact #%u avatar token: %s", i,
              tp_contact_get_avatar_token (contact));
          DEBUG ("contact #%u presence type: %u", i,
              tp_contact_get_presence_type (contact));
          DEBUG ("contact #%u presence status: %s", i,
              tp_contact_get_presence_status (contact));
          DEBUG ("contact #%u presence message: %s", i,
              tp_contact_get_presence_message (contact));
          g_ptr_array_add (result->contacts, g_object_ref (contact));
        }
    }
  else
    {
      DEBUG ("got an error: %s %u: %s", g_quark_to_string (error->domain),
          error->code, error->message);
      result->error = g_error_copy (error);
    }
}

static void
test_by_id (TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE) };
  static const gchar * const ids[] = { "Alice", "Bob", "Not valid", "Chris",
      "not valid either", NULL };

  tp_connection_get_contacts_by_id (client_conn,
      2, ids,
      0, NULL,
      by_id_cb,
      &result, finish, NULL);

  g_main_loop_run (result.loop);

  MYASSERT (result.error != NULL, ": should fail as the CM is broken");
  MYASSERT_SAME_UINT (result.error->domain, TP_DBUS_ERRORS);
  MYASSERT (result.error->code == TP_DBUS_ERROR_INCONSISTENT,
      ": %i != %i", result.error->code, TP_DBUS_ERROR_INCONSISTENT);

  MYASSERT (result.contacts == NULL, "");
  MYASSERT (result.good_ids == NULL, "");
  MYASSERT (result.bad_ids == NULL, "");

  /* clean up */
  g_main_loop_unref (result.loop);
  g_error_free (result.error);
}

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *dbus;
  ContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error = NULL;
  TpConnection *client_conn;

  /* Setup */

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_dbus_daemon_new (tp_get_bus ());

  DEBUG ("a");
  service_conn = CONTACTS_CONNECTION (g_object_new (
        BUG_19101_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  DEBUG ("b");
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  /* Tests */

  test_by_id (client_conn);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  test_assert_no_error (error);
  g_object_unref (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);

  return 0;
}
