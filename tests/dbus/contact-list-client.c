/* Tests of TpTextChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

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

    GPtrArray *blocked_added;
    GPtrArray *blocked_removed;
    TpContact *contact;

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

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  tp_clear_pointer (&test->blocked_added, g_ptr_array_unref);
  tp_clear_pointer (&test->blocked_removed, g_ptr_array_unref);
  g_clear_object (&test->contact);
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
contact_block_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_contact_block_finish (TP_CONTACT (source), result, &test->error);
  g_assert_no_error (test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
contact_unblock_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_contact_unblock_finish (TP_CONTACT (source), result, &test->error);
  g_assert_no_error (test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static TpContact *
create_contact (Test *test,
    const gchar *id)
{
  TpHandle handle;
  TpContact *contact;

  handle = tp_handle_ensure (test->contact_repo, id, NULL, &test->error);
  g_assert_no_error (test->error);

  contact = tp_connection_dup_contact_if_possible (test->connection, handle,
      id);
  g_assert (contact != NULL);

  return contact;
}

static void
test_block_unblock (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpContact *alice, *bob;
  GPtrArray *arr;

  alice = create_contact (test, "alice");
  bob = create_contact (test, "bob");

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

static void
blocked_contacts_changed_cb (TpConnection *conn,
    GPtrArray *added,
    GPtrArray *removed,
    Test *test)
{
  tp_clear_pointer (&test->blocked_added, g_ptr_array_unref);
  tp_clear_pointer (&test->blocked_removed, g_ptr_array_unref);

  test->blocked_added = g_ptr_array_ref (added);
  test->blocked_removed = g_ptr_array_ref (removed);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_blocked_contacts (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_CONTACT_BLOCKING, 0 };
  GPtrArray *blocked;
  TpContact *alice, *bill, *guillaume, *sjoerd, *steve;
  gboolean use_contact_api = GPOINTER_TO_UINT (data);

  sjoerd = create_contact (test, "sjoerd@example.com");
  steve = create_contact (test, "steve@example.com");

  /* Feature is not prepared yet */
  g_object_get (test->connection, "blocked-contacts", &blocked, NULL);
  g_assert_cmpuint (blocked->len, == , 0);
  g_ptr_array_unref (blocked);

  blocked = tp_connection_get_blocked_contacts (test->connection);
  g_assert_cmpuint (blocked->len, == , 0);

  /* Prepare the feature */
  tp_proxy_prepare_async (test->connection, features,
      proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* 2 contacts are already blocked in the CM */
  g_object_get (test->connection, "blocked-contacts", &blocked, NULL);
  g_assert_cmpuint (blocked->len, == , 2);
  g_ptr_array_unref (blocked);

  blocked = tp_connection_get_blocked_contacts (test->connection);
  g_assert_cmpuint (blocked->len, == , 2);

  /* Preparing TP_CONNECTION_FEATURE_CONTACT_BLOCKING gives us
   * TP_CONTACT_FEATURE_CONTACT_BLOCKING for free. Test that this works with
   * existing and newly created TpContact. */
  bill = create_contact (test, "bill@example.com");
  guillaume = create_contact (test, "guillaume@example.com");

  g_assert (tp_contact_has_feature (sjoerd,
        TP_CONTACT_FEATURE_CONTACT_BLOCKING));
  g_assert (!tp_contact_is_blocked (sjoerd));

  g_assert (tp_contact_has_feature (steve,
        TP_CONTACT_FEATURE_CONTACT_BLOCKING));
  g_assert (tp_contact_is_blocked (steve));

  g_assert (tp_contact_has_feature (bill, TP_CONTACT_FEATURE_CONTACT_BLOCKING));
  g_assert (tp_contact_is_blocked (bill));

  g_assert (tp_contact_has_feature (guillaume,
        TP_CONTACT_FEATURE_CONTACT_BLOCKING));
  g_assert (!tp_contact_is_blocked (guillaume));

  g_object_unref (steve);
  g_object_unref (sjoerd);
  g_object_unref (bill);
  g_object_unref (guillaume);

  /* Let's block another contact */
  alice = create_contact (test, "alice");

  g_signal_connect (test->connection, "blocked-contacts-changed",
      G_CALLBACK (blocked_contacts_changed_cb), test);

  if (use_contact_api)
    {
      tp_contact_block_async (alice, FALSE, contact_block_cb, test);
    }
  else
    {
      tp_connection_block_contacts_async (test->connection,
          1,  &alice, FALSE, block_contacts_cb, test);
    }

  g_object_unref (alice);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->blocked_added->len, ==, 1);
  g_assert_cmpuint (test->blocked_removed->len, ==, 0);

  alice = g_ptr_array_index (test->blocked_added, 0);
  g_assert (TP_IS_CONTACT (alice));
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");

  blocked = tp_connection_get_blocked_contacts (test->connection);
  g_assert_cmpuint (blocked->len, == , 3);

  /* Cool, now unblock the poor Alice */
  if (use_contact_api)
    {
      tp_contact_unblock_async (alice, contact_unblock_cb, test);
    }
  else
    {
      tp_connection_unblock_contacts_async (test->connection,
          1,  &alice, unblock_contacts_cb, test);
    }

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (test->blocked_added->len, ==, 0);
  g_assert_cmpuint (test->blocked_removed->len, ==, 1);

  alice = g_ptr_array_index (test->blocked_removed, 0);
  g_assert (TP_IS_CONTACT (alice));
  g_assert_cmpstr (tp_contact_get_identifier (alice), ==, "alice");

  blocked = tp_connection_get_blocked_contacts (test->connection);
  g_assert_cmpuint (blocked->len, == , 2);
}

static void
get_contacts_by_id_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    const gchar * const *requested_ids,
    GHashTable *failed_id_errors,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;

  g_clear_object (&test->contact);

  if (error != NULL)
    {
      test->error = g_error_copy (error);
    }
  else
    {
      test->contact = g_object_ref (contacts[0]);
    }

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
contact_list_state_change_cb (GObject *object,
    GParamSpec *pspec,
    gpointer user_data)
{
  TpConnection *conn = (TpConnection *) object;
  Test *test = user_data;

  if (tp_connection_get_contact_list_state (conn) !=
      TP_CONTACT_LIST_STATE_SUCCESS)
    return;

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
property_change_cb (GObject *object,
    GParamSpec *pspec,
    gpointer user_data)
{
  Test *test = user_data;

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_is_blocked (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  const gchar *id = "bill@example.com";
  TpContactFeature features[] = { TP_CONTACT_FEATURE_CONTACT_BLOCKING };
  GQuark conn_features[] = { TP_CONNECTION_FEATURE_CONTACT_LIST, 0 };

  tp_proxy_prepare_async (test->connection, conn_features,
      proxy_prepare_cb, test);

  test->wait = 1;

  /* We have to wait that the ContactList has been fetched by the CM */
  if (tp_connection_get_contact_list_state (test->connection) !=
      TP_CONTACT_LIST_STATE_SUCCESS)
    {
      g_signal_connect (test->connection, "notify::contact-list-state",
          G_CALLBACK (contact_list_state_change_cb), test);

      test->wait++;
    }

  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  /* Bill is already blocked in the CM */
  tp_connection_get_contacts_by_id (test->connection, 1, &id,
      G_N_ELEMENTS (features), features, get_contacts_by_id_cb, test,
      NULL, NULL);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (TP_IS_CONTACT (test->contact));

  g_assert (tp_contact_has_feature (test->contact,
        TP_CONTACT_FEATURE_CONTACT_BLOCKING));
  g_assert (tp_contact_is_blocked (test->contact));

  /* Unblock Bill */
  g_signal_connect (test->contact, "notify::is-blocked",
      G_CALLBACK (property_change_cb), test);

  tp_contact_unblock_async (test->contact, contact_unblock_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (!tp_contact_is_blocked (test->contact));
}

static void
test_contact_list_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean props_only = GPOINTER_TO_UINT (data);
  GQuark conn_features[] = { 0, 0 };
  GPtrArray *contacts;

  if (props_only)
    conn_features[0] = TP_CONNECTION_FEATURE_CONTACT_LIST_PROPERTIES;
  else
    conn_features[0] = TP_CONNECTION_FEATURE_CONTACT_LIST;

  /* Feature isn't prepared yet */
  g_assert (!tp_proxy_is_prepared (test->connection,
        TP_CONNECTION_FEATURE_CONTACT_LIST));
  g_assert (!tp_proxy_is_prepared (test->connection,
        TP_CONNECTION_FEATURE_CONTACT_LIST_PROPERTIES));

  g_assert_cmpuint (tp_connection_get_contact_list_state (test->connection), ==,
      TP_CONTACT_LIST_STATE_NONE);
  g_assert (!tp_connection_get_contact_list_persists (test->connection));
  g_assert (!tp_connection_get_can_change_contact_list (test->connection));
  g_assert (!tp_connection_get_request_uses_message (test->connection));

  tp_proxy_prepare_async (test->connection, conn_features,
      proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_proxy_is_prepared (test->connection,
        TP_CONNECTION_FEATURE_CONTACT_LIST) == !props_only);
  g_assert (tp_proxy_is_prepared (test->connection,
        TP_CONNECTION_FEATURE_CONTACT_LIST_PROPERTIES));

  g_assert (tp_connection_get_contact_list_persists (test->connection));
  g_assert (tp_connection_get_can_change_contact_list (test->connection));
  g_assert (tp_connection_get_request_uses_message (test->connection));

  contacts = tp_connection_dup_contact_list (test->connection);
  if (props_only)
    {
      /* Contacts haven't be fetched */
      g_assert_cmpuint (contacts->len, ==, 0);
    }
  else
    {
      g_assert_cmpuint (contacts->len, >, 0);
    }
  g_ptr_array_unref (contacts);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/contact-list-client/blocking/block-unblock", Test, NULL, setup,
      test_block_unblock, teardown);
  g_test_add ("/contact-list-client/blocking/can-report-abusive", Test, NULL,
      setup, test_can_report_abusive, teardown);
  g_test_add ("/contact-list-client/blocking/connection/blocked-contacts", Test,
      GUINT_TO_POINTER (FALSE), setup, test_blocked_contacts, teardown);
  g_test_add ("/contact-list-client/blocking/contact/blocked-contacts", Test,
      GUINT_TO_POINTER (TRUE), setup, test_blocked_contacts, teardown);
  g_test_add ("/contact-list-client/blocking/is-blocked", Test, NULL,
      setup, test_is_blocked, teardown);

  g_test_add ("/contact-list-client/contact-list/properties", Test,
      GUINT_TO_POINTER (FALSE), setup, test_contact_list_properties, teardown);
  g_test_add ("/contact-list-client/contact-list/properties", Test,
      GUINT_TO_POINTER (TRUE), setup, test_contact_list_properties, teardown);

  return g_test_run ();
}
