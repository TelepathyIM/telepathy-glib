/* Feature test for contact lists
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>

#include "examples/cm/contactlist/conn.h"
#include "tests/lib/util.h"

typedef enum {
    CONTACTS_CHANGED,
} LogEntryType;

typedef struct {
    LogEntryType type;
    GHashTable *contacts_changed;
    GArray *contacts_removed;
} LogEntry;

static void
log_entry_free (LogEntry *le)
{
  if (le->contacts_changed != NULL)
    g_hash_table_unref (le->contacts_changed);

  if (le->contacts_removed != NULL)
    g_array_unref (le->contacts_removed);

  g_slice_free (LogEntry, le);
}

typedef struct {
    TpDBusDaemon *dbus;
    ExampleContactListConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    TpChannel *publish;
    TpChannel *subscribe;
    TpChannel *stored;
    TpChannel *deny;

    TpChannel *group;

    TpHandleRepoIface *contact_repo;
    TpHandle sjoerd;
    TpHandle helen;
    TpHandle wim;
    TpHandle bill;
    TpHandle ninja;

    GArray *arr;

    /* list of LogEntry */
    GPtrArray *log;

    GAsyncResult *prepare_result;
} Test;

static void
contacts_changed_cb (TpConnection *connection,
    GHashTable *changes,
    const GArray *removals,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  g_assert (g_hash_table_size (changes) > 0 || removals->len > 0);

  le->type = CONTACTS_CHANGED;
  le->contacts_changed = g_boxed_copy (TP_HASH_TYPE_CONTACT_SUBSCRIPTION_MAP,
      changes);
  le->contacts_removed = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      removals->len);
  g_array_append_vals (le->contacts_removed, removals->data, removals->len);

  g_ptr_array_add (test->log, le);
}

static void
maybe_queue_disconnect (TpProxySignalConnection *sc)
{
  if (sc != NULL)
    g_test_queue_destroy (
        (GDestroyNotify) tp_proxy_signal_connection_disconnect, sc);
}

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = test_dbus_daemon_dup_or_die ();

  test->service_conn = test_object_new_static_class (
        EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
        "account", "me@example.com",
        "simulation-delay", 0,
        "protocol", "example-contact-list",
        NULL);
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "example",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->contact_repo = tp_base_connection_get_handles (
      test->service_conn_as_base, TP_HANDLE_TYPE_CONTACT);

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  test_proxy_run_until_prepared (test->conn, features);

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  test->log = g_ptr_array_new ();

  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_list_connect_to_contacts_changed (
        test->conn, contacts_changed_cb, test, NULL, NULL, NULL));

  test->sjoerd = tp_handle_ensure (test->contact_repo, "sjoerd@example.com",
      NULL, NULL);
  g_assert (test->sjoerd != 0);
  test->helen = tp_handle_ensure (test->contact_repo, "helen@example.com",
      NULL, NULL);
  g_assert (test->helen != 0);
  test->wim = tp_handle_ensure (test->contact_repo, "wim@example.com",
      NULL, NULL);
  g_assert (test->wim != 0);
  test->bill = tp_handle_ensure (test->contact_repo, "bill@example.com",
      NULL, NULL);
  g_assert (test->bill != 0);
  test->ninja = tp_handle_ensure (test->contact_repo, "ninja@example.com",
      NULL, NULL);
  g_assert (test->ninja != 0);

  test->arr = g_array_new (FALSE, FALSE, sizeof (TpHandle));
}

static void
teardown (Test *test,
    gconstpointer data)
{
  TpConnection *conn;
  gboolean ok;
  GError *error = NULL;

  g_array_free (test->arr, TRUE);

  g_ptr_array_foreach (test->log, (GFunc) log_entry_free, NULL);
  g_ptr_array_free (test->log, TRUE);

  tp_handle_unref (test->contact_repo, test->sjoerd);
  tp_handle_unref (test->contact_repo, test->helen);
  tp_handle_unref (test->contact_repo, test->wim);
  tp_handle_unref (test->contact_repo, test->bill);
  tp_handle_unref (test->contact_repo, test->ninja);

  tp_clear_object (&test->conn);
  tp_clear_object (&test->publish);
  tp_clear_object (&test->subscribe);
  tp_clear_object (&test->stored);
  tp_clear_object (&test->group);

  /* make a new TpConnection just to disconnect the underlying Connection,
   * so we don't leak it */
  conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);
  ok = tp_cli_connection_run_disconnect (conn, -1, &error, NULL);
  g_assert (ok);
  g_assert_no_error (error);
  g_assert (!tp_connection_run_until_ready (conn, FALSE, &error, NULL));
  g_assert_error (error, TP_ERRORS, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;
}

static TpChannel *
test_ensure_channel (Test *test,
    guint channel_type,
    const gchar *id)
{
  GError *error = NULL;
  GHashTable *asv, *props;
  gchar *path;
  TpChannel *ret;

  asv = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE,
          G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
          G_TYPE_UINT, channel_type,
      TP_PROP_CHANNEL_TARGET_ID,
          G_TYPE_STRING, id,
      NULL);
  tp_cli_connection_interface_requests_run_ensure_channel (test->conn, -1,
      asv, NULL, &path, &props, &error, NULL);
  g_assert_no_error (error);
  ret = tp_channel_new_from_properties (test->conn, path, props,
      &error);
  g_assert (ret != NULL);
  g_assert_no_error (error);
  g_free (path);
  g_hash_table_unref (props);
  g_hash_table_unref (asv);

  test_proxy_run_until_prepared (ret, NULL);
  return ret;
}

static void
test_assert_one_contact_changed (Test *test,
    guint index,
    TpHandle handle,
    TpPresenceState expected_sub_state,
    TpPresenceState expected_pub_state,
    const gchar *expected_pub_request)
{
  LogEntry *le;
  GValueArray *va;
  guint sub_state;
  guint pub_state;
  const gchar *pub_request;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, CONTACTS_CHANGED);

  g_assert_cmpuint (g_hash_table_size (le->contacts_changed), ==, 1);
  va = g_hash_table_lookup (le->contacts_changed, GUINT_TO_POINTER (handle));
  g_assert (va != NULL);
  tp_value_array_unpack (va, 3,
      &sub_state,
      &pub_state,
      &pub_request);
  g_assert_cmpuint (sub_state, ==, expected_sub_state);
  g_assert_cmpuint (pub_state, ==, expected_pub_state);
  g_assert_cmpstr (pub_request, ==, expected_pub_request);

  g_assert_cmpuint (le->contacts_removed->len, ==, 0);
}

static void
test_assert_one_contact_removed (Test *test,
    guint index,
    TpHandle handle)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, CONTACTS_CHANGED);

  g_assert_cmpuint (g_hash_table_size (le->contacts_changed), ==, 0);
  g_assert_cmpuint (le->contacts_removed->len, ==, 1);
  g_assert_cmpuint (g_array_index (le->contacts_removed, guint, 0), ==,
      handle);
}

static void
test_nothing (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  /* this is actually a valuable test - it ensures that shutting down the
   * CM before the contact list has been retrieved works! */
}

static void
test_initial_channels (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST,
      "subscribe");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");
  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->publish)), ==, 4);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->publish)),
      ==, 2);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_remote_pending (test->publish)),
      ==, 0);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->publish),
        test->sjoerd));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->wim));

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->subscribe)), ==, 4);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->subscribe)),
      ==, 0);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_remote_pending (test->subscribe)),
      ==, 2);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->sjoerd));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->helen));

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->stored)), ==, 8);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->stored)),
      ==, 0);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_remote_pending (test->stored)),
      ==, 0);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->stored),
        test->sjoerd));

  g_assert (!tp_intset_is_member (tp_channel_group_get_members (test->publish),
        test->ninja));
  g_assert (!tp_intset_is_member (tp_channel_group_get_members (
          test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (tp_channel_group_get_members (test->stored),
        test->ninja));

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->deny)), ==, 2);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->deny)),
      ==, 0);
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_remote_pending (test->deny)),
      ==, 0);
  g_assert (tp_intset_is_member (tp_channel_group_get_members (test->deny),
        test->bill));

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_accept_publish_request (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->publish)),
      ==, 2);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->wim));

  g_array_append_val (test->arr, test->wim);
  tp_cli_channel_interface_group_run_add_members (test->publish,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->publish)),
      ==, 1);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->wim));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->wim));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_changed (test, 0, test->wim, TP_PRESENCE_STATE_NO,
      TP_PRESENCE_STATE_YES, "");
}

static void
test_reject_publish_request (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->wim));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->wim));

  g_array_append_val (test->arr, test->wim);
  tp_cli_channel_interface_group_run_remove_members (test->publish,
      -1, test->arr, "", &error, NULL);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_local_pending (test->publish)),
      ==, 1);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->wim));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->wim));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_changed (test, 0, test->wim, TP_PRESENCE_STATE_NO,
      TP_PRESENCE_STATE_NO, "");
}

static void
test_add_to_publish_pre_approve (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  /* Unilaterally adding a member to the publish channel doesn't work, but
   * in the new contact list manager the method "succeeds" anyway, and
   * any subsequent subscription request succeeds instantly. */

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");

  g_array_append_val (test->arr, test->ninja);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja));

  tp_cli_channel_interface_group_run_add_members (test->publish,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja));

  /* the example CM's fake contacts accept requests that contain "please" */
  tp_cli_channel_interface_group_run_add_members (test->subscribe,
      -1, test->arr, "Please may I see your presence?", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->stored),
        test->ninja));

  /* after a short delay, the contact accepts our request */
  while (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja))
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));

  /* the contact also requests our presence after a short delay - we
   * pre-approved, so they go straight to full membership */
  while (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja) || test->log->len < 3)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja));

  g_assert_cmpuint (test->log->len, ==, 3);
  test_assert_one_contact_changed (test, 0, test->ninja, TP_PRESENCE_STATE_ASK,
      TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja, TP_PRESENCE_STATE_YES,
      TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 2, test->ninja, TP_PRESENCE_STATE_YES,
      TP_PRESENCE_STATE_YES, "");
}

static void
test_add_to_publish_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  /* Adding a member to the publish channel when they're already there is
   * valid. */

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_add_members (test->publish,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_remove_from_publish (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->publish)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_remove_members (test->publish,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  /* the contact re-requests our presence after a short delay */
  while (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->sjoerd) ||
      test->log->len < 2)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 2);
  test_assert_one_contact_changed (test, 0, test->sjoerd,
      TP_PRESENCE_STATE_YES, TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->sjoerd,
      TP_PRESENCE_STATE_YES, TP_PRESENCE_STATE_ASK,
      "May I see your presence, please?");
}

static void
test_remove_from_publish_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->publish)),
      ==, 4);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_remove_members (test->publish,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_add_to_stored (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST,
      "subscribe");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->stored)),
      ==, 8);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_add_members (test->stored,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->stored)),
      ==, 9);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_changed (test, 0, test->ninja,
      TP_PRESENCE_STATE_NO, TP_PRESENCE_STATE_NO, "");
}

static void
test_add_to_stored_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->stored)),
      ==, 8);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_add_members (test->stored,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_remove_from_stored (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST,
      "subscribe");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_remove_members (test->stored,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->sjoerd));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->sjoerd));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_removed (test, 0, test->sjoerd);
}

static void
test_remove_from_stored_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->stored)),
      ==, 8);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_remove_members (test->stored,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_accept_subscribe_request (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->subscribe)),
      ==, 4);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));

  /* the example CM's fake contacts accept requests that contain "please" */
  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_add_members (test->subscribe,
      -1, test->arr, "Please may I see your presence?", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->stored),
        test->ninja));

  /* after a short delay, the contact accepts our request */
  while (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja))
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));

  /* the contact also requests our presence after a short delay */
  while (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja) ||
      test->log->len < 3)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja));

  g_assert_cmpuint (test->log->len, ==, 3);
  test_assert_one_contact_changed (test, 0, test->ninja,
      TP_PRESENCE_STATE_ASK, TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja,
      TP_PRESENCE_STATE_YES, TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 2, test->ninja,
      TP_PRESENCE_STATE_YES, TP_PRESENCE_STATE_ASK,
      "May I see your presence, please?");
}

static void
test_reject_subscribe_request (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->subscribe)),
      ==, 4);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));

  /* the example CM's fake contacts reject requests that don't contain
   * "please" */
  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_add_members (test->subscribe,
      -1, test->arr, "I demand to see your presence", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->stored),
        test->ninja));

  /* after a short delay, the contact rejects our request. Say please! */
  while (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja) ||
      test->log->len < 2)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->ninja));

  /* the ninja is still on the stored list */
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->stored),
        test->ninja));

  g_assert_cmpuint (test->log->len, ==, 2);
  test_assert_one_contact_changed (test, 0, test->ninja,
      TP_PRESENCE_STATE_ASK, TP_PRESENCE_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja,
      TP_PRESENCE_STATE_NO, TP_PRESENCE_STATE_NO, "");
}

static void
test_remove_from_subscribe (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->subscribe)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_remove_members (test->subscribe,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->sjoerd));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_changed (test, 0, test->sjoerd,
      TP_PRESENCE_STATE_NO, TP_PRESENCE_STATE_YES, "");
}

static void
test_remove_from_subscribe_pending (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_remote_pending (test->subscribe)),
      ==, 2);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->helen));

  g_array_append_val (test->arr, test->helen);
  tp_cli_channel_interface_group_run_remove_members (test->subscribe,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->helen));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->helen));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->helen));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_changed (test, 0, test->helen,
      TP_PRESENCE_STATE_NO, TP_PRESENCE_STATE_NO, "");
}

static void
test_remove_from_subscribe_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "subscribe");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->subscribe)),
      ==, 4);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_remove_members (test->subscribe,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_add_to_group (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST,
      "subscribe");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_add_members (test->group,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 5);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->ninja));

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->subscribe),
        test->ninja));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->ninja));
}

static void
test_add_to_group_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_add_members (test->group,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);
}

static void
test_remove_from_group (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  tp_cli_channel_interface_group_run_remove_members (test->group,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));
}

static void
test_remove_from_group_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_remove_members (test->group,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);
}

static void
test_remove_group (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (!tp_intset_is_empty (
        tp_channel_group_get_members (test->group)));

  tp_cli_channel_run_close (test->group, -1, &error, NULL);
  g_assert_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE);
}

static void
test_remove_group_empty (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "people who understand const in C");

  g_assert (tp_intset_is_empty (
        tp_channel_group_get_members (test->group)));

  tp_cli_channel_run_close (test->group, -1, &error, NULL);
  g_assert_no_error (error);
}

static void
test_add_to_deny (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");
  test->stored = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "stored");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->deny)),
      ==, 2);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_add_members (test->deny,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->deny)),
      ==, 3);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
}

static void
test_add_to_deny_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));

  g_array_append_val (test->arr, test->bill);
  tp_cli_channel_interface_group_run_add_members (test->deny,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));
}

static void
test_remove_from_deny (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");
  test->subscribe = test_ensure_channel (test, TP_HANDLE_TYPE_LIST,
      "subscribe");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));

  g_array_append_val (test->arr, test->bill);
  tp_cli_channel_interface_group_run_remove_members (test->deny,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));
}

static void
test_remove_from_deny_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  tp_cli_channel_interface_group_run_remove_members (test->deny,
      -1, test->arr, "", &error, NULL);
  g_assert_no_error (error);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/contact-lists/nothing",
      Test, NULL, setup, test_nothing, teardown);

  g_test_add ("/contact-lists/initial-channels",
      Test, NULL, setup, test_initial_channels, teardown);
  g_test_add ("/contact-lists/accept-publish-request",
      Test, NULL, setup, test_accept_publish_request, teardown);
  g_test_add ("/contact-lists/reject-publish-request",
      Test, NULL, setup, test_reject_publish_request, teardown);
  g_test_add ("/contact-lists/add-to-publish/pre-approve",
      Test, NULL, setup, test_add_to_publish_pre_approve, teardown);
  g_test_add ("/contact-lists/add-to-publish/no-op",
      Test, NULL, setup, test_add_to_publish_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-publish",
      Test, NULL, setup, test_remove_from_publish, teardown);
  g_test_add ("/contact-lists/remove-from-publish/no-op",
      Test, NULL, setup, test_remove_from_publish_no_op, teardown);
  g_test_add ("/contact-lists/add-to-stored",
      Test, NULL, setup, test_add_to_stored, teardown);
  g_test_add ("/contact-lists/add-to-stored/no-op",
      Test, NULL, setup, test_add_to_stored_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-stored",
      Test, NULL, setup, test_remove_from_stored, teardown);
  g_test_add ("/contact-lists/remove-from-stored/no-op",
      Test, NULL, setup, test_remove_from_stored_no_op, teardown);
  g_test_add ("/contact-lists/accept-subscribe-request",
      Test, NULL, setup, test_accept_subscribe_request, teardown);
  g_test_add ("/contact-lists/reject-subscribe-request",
      Test, NULL, setup, test_reject_subscribe_request, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe",
      Test, NULL, setup, test_remove_from_subscribe, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe/pending",
      Test, NULL, setup, test_remove_from_subscribe_pending, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe/no-op",
      Test, NULL, setup, test_remove_from_subscribe_no_op, teardown);

  g_test_add ("/contact-lists/add-to-group",
      Test, NULL, setup, test_add_to_group, teardown);
  g_test_add ("/contact-lists/add-to-group/no-op",
      Test, NULL, setup, test_add_to_group_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-group",
      Test, NULL, setup, test_remove_from_group, teardown);
  g_test_add ("/contact-lists/remove-from-group/no-op",
      Test, NULL, setup, test_remove_from_group_no_op, teardown);
  g_test_add ("/contact-lists/remove-group",
      Test, NULL, setup, test_remove_group, teardown);
  g_test_add ("/contact-lists/remove-group/empty",
      Test, NULL, setup, test_remove_group_empty, teardown);

  g_test_add ("/contact-lists/add-to-deny",
      Test, NULL, setup, test_add_to_deny, teardown);
  g_test_add ("/contact-lists/add-to-deny/no-op",
      Test, NULL, setup, test_add_to_deny_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-deny",
      Test, NULL, setup, test_remove_from_deny, teardown);
  g_test_add ("/contact-lists/remove-from-deny/no-op",
      Test, NULL, setup, test_remove_from_deny_no_op, teardown);

  return g_test_run ();
}
