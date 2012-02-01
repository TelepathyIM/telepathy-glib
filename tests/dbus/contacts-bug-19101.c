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
  g_assert_cmpuint (result.error->domain, ==, TP_DBUS_ERRORS);
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
  TpTestsContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *client_conn;

  /* Setup */

  tp_tests_abort_after (10);
  g_type_init ();
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
