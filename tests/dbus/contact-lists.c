/* Feature test for contact lists
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/connection.h>

#include "examples/cm/contactlist/conn.h"
#include "tests/lib/util.h"

typedef enum {
    CONTACTS_CHANGED,
    GROUPS_CHANGED,
    GROUPS_CREATED,
    GROUPS_REMOVED,
    GROUP_RENAMED,
    BLOCKED_CONTACTS_CHANGED
} LogEntryType;

typedef struct {
    LogEntryType type;
    /* ContactsChanged */
    GHashTable *contacts_changed;
    TpIntset *contacts_removed;
    /* GroupsChanged */
    GArray *contacts;
    /* GroupsChanged, GroupsCreated, GroupRenamed */
    GStrv groups_added;
    /* GroupsChanged, GroupsRemoved, GroupRenamed */
    GStrv groups_removed;
    /* BlockedContactsChanged */
    GHashTable *blocked_contacts;
    GHashTable *unblocked_contacts;
} LogEntry;

static void
log_entry_free (LogEntry *le)
{
  if (le->contacts_changed != NULL)
    g_hash_table_unref (le->contacts_changed);

  if (le->contacts_removed != NULL)
    tp_intset_destroy (le->contacts_removed);

  if (le->contacts != NULL)
    g_array_unref (le->contacts);

  g_strfreev (le->groups_added);
  g_strfreev (le->groups_removed);

  if (le->blocked_contacts != NULL)
    g_hash_table_unref (le->blocked_contacts);

  if (le->unblocked_contacts != NULL)
    g_hash_table_unref (le->unblocked_contacts);

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
    TpHandle canceller;

    GArray *arr;

    /* list of LogEntry */
    GPtrArray *log;

    GAsyncResult *prepare_result;
    GHashTable *contact_attributes;

    GMainLoop *main_loop;
} Test;

static void
test_quit_loop (gpointer p)
{
  Test *test = p;

  g_main_loop_quit (test->main_loop);
}

static void
contacts_changed_with_id_cb (TpConnection *connection,
    GHashTable *changes,
    GHashTable *identifiers,
    GHashTable *removals,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);
  GHashTableIter i;
  gpointer key, value;

  if (g_hash_table_size (changes) > 0)
    g_assert_cmpuint (g_hash_table_size (changes), ==,
        g_hash_table_size (identifiers));
  else
    g_assert_cmpuint (g_hash_table_size (removals), >, 0);

  le->type = CONTACTS_CHANGED;
  le->contacts_changed = g_boxed_copy (TP_HASH_TYPE_CONTACT_SUBSCRIPTION_MAP,
      changes);

  /* We asserted above that we have as many identifiers as we have changes. */
  g_hash_table_iter_init (&i, identifiers);
  while (g_hash_table_iter_next (&i, &key, &value))
    {
      TpHandle handle = GPOINTER_TO_UINT (key);

      g_assert_cmpstr (value, ==,
          tp_handle_inspect (test->contact_repo, handle));
    }

  le->contacts_removed = tp_intset_new ();

  g_hash_table_iter_init (&i, removals);
  while (g_hash_table_iter_next (&i, &key, &value))
    {
      TpHandle handle = GPOINTER_TO_UINT (key);

      g_assert_cmpstr (value, ==,
          tp_handle_inspect (test->contact_repo, handle));
      tp_intset_add (le->contacts_removed, handle);
    }

  g_ptr_array_add (test->log, le);
}

static void
contacts_changed_cb (TpConnection *connection,
    GHashTable *changes,
    const GArray *removals,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le;
  GHashTableIter i;
  gpointer key, value;
  TpIntset *removal_set;

  g_assert (g_hash_table_size (changes) > 0 || removals->len > 0);

  /* We should have had a ContactsChangedByID signal immediately before this
   * signal */
  g_assert_cmpuint (test->log->len, >, 0);

  le = g_ptr_array_index (test->log, test->log->len - 1);
  g_assert_cmpuint (le->type, ==, CONTACTS_CHANGED);

  /* The changes should all have been the same as in the previous signal */
  g_assert_cmpuint (g_hash_table_size (changes), ==,
      g_hash_table_size (le->contacts_changed));

  g_hash_table_iter_init (&i, changes);
  while (g_hash_table_iter_next (&i, &key, &value))
    {
      GValueArray *existing = g_hash_table_lookup (le->contacts_changed, key);
      GValueArray *emitted = value;
      guint existing_sub, existing_pub, emitted_sub, emitted_pub;
      const gchar *existing_req, *emitted_req;

      g_assert (existing != NULL);

      tp_value_array_unpack (existing, 3, &existing_sub, &existing_pub,
          &existing_req);
      tp_value_array_unpack (emitted, 3, &emitted_sub, &emitted_pub,
          &emitted_req);

      g_assert_cmpuint (existing_sub, ==, emitted_sub);
      g_assert_cmpuint (existing_pub, ==, emitted_pub);
      g_assert_cmpstr (existing_req, ==, emitted_req);
    }

  removal_set = tp_intset_from_array (removals);

  if (!tp_intset_is_equal (removal_set, le->contacts_removed))
    g_error ("Removals from ContactsChangedById (%s) != "
        "Removals from ContactsChanged (%s)",
        tp_intset_dump (le->contacts_removed),
        tp_intset_dump (removal_set));

  tp_intset_destroy (removal_set);
}

static void
groups_changed_cb (TpConnection *connection,
    const GArray *contacts,
    const gchar **groups_added,
    const gchar **groups_removed,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  g_assert (contacts->len > 0);
  g_assert ((groups_added != NULL && groups_added[0] != NULL) ||
      (groups_removed != NULL && groups_removed[0] != NULL));

  le->type = GROUPS_CHANGED;
  le->contacts = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      contacts->len);
  g_array_append_vals (le->contacts, contacts->data, contacts->len);
  le->groups_added = g_strdupv ((GStrv) groups_added);
  le->groups_removed = g_strdupv ((GStrv) groups_removed);

  g_ptr_array_add (test->log, le);
}

static void
groups_created_cb (TpConnection *connection,
    const gchar **groups_added,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  g_assert (groups_added != NULL);
  g_assert (groups_added[0] != NULL);

  le->type = GROUPS_CREATED;
  le->groups_added = g_strdupv ((GStrv) groups_added);

  g_ptr_array_add (test->log, le);
}

static void
groups_removed_cb (TpConnection *connection,
    const gchar **groups_removed,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  g_assert (groups_removed != NULL);
  g_assert (groups_removed[0] != NULL);

  le->type = GROUPS_REMOVED;
  le->groups_removed = g_strdupv ((GStrv) groups_removed);

  g_ptr_array_add (test->log, le);
}

static void
group_renamed_cb (TpConnection *connection,
    const gchar *old_name,
    const gchar *new_name,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  le->type = GROUP_RENAMED;
  le->groups_added = g_new0 (gchar *, 2);
  le->groups_added[0] = g_strdup (new_name);
  le->groups_removed = g_new0 (gchar *, 2);
  le->groups_removed[0] = g_strdup (old_name);

  g_ptr_array_add (test->log, le);
}

static void
blocked_contacts_changed_cb (TpConnection *connection,
    GHashTable *blocked_contacts,
    GHashTable *unblocked_contacts,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  LogEntry *le = g_slice_new0 (LogEntry);

  le->type = BLOCKED_CONTACTS_CHANGED;
  le->blocked_contacts = g_hash_table_ref (blocked_contacts);
  le->unblocked_contacts = g_hash_table_ref (unblocked_contacts);

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
setup_pre_connect (
    Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  const gchar *account;

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  test->main_loop = g_main_loop_new (NULL, FALSE);

  /* Some tests want 'account' to be an invalid identifier, so that Connect()
   * will fail (and the status will change to Disconnected).
   */
  if (!tp_strdiff (data, "break-account-parameter"))
    account = "";
  else
    account = "me@example.com";

  test->service_conn = tp_tests_object_new_static_class (
        EXAMPLE_TYPE_CONTACT_LIST_CONNECTION,
        "account", account,
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

  /* Prepare the connection far enough to know its own interfaces. */
  tp_tests_proxy_run_until_prepared (test->conn, NULL);
}

static void
setup (Test *test,
    gconstpointer data)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  setup_pre_connect (test, data);

  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (test->conn, features);

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  test->log = g_ptr_array_new ();

  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_list_connect_to_contacts_changed_with_id (
        test->conn, contacts_changed_with_id_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_list_connect_to_contacts_changed (
        test->conn, contacts_changed_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_groups_connect_to_groups_changed (
        test->conn, groups_changed_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_groups_connect_to_groups_created (
        test->conn, groups_created_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_groups_connect_to_groups_removed (
        test->conn, groups_removed_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_groups_connect_to_group_renamed (
        test->conn, group_renamed_cb, test, NULL, NULL, NULL));
  maybe_queue_disconnect (
      tp_cli_connection_interface_contact_blocking_connect_to_blocked_contacts_changed (
        test->conn, blocked_contacts_changed_cb, test, NULL, NULL, NULL));

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
  test->canceller = tp_handle_ensure (test->contact_repo,
      "canceller@cancel.example.com", NULL, NULL);
  g_assert (test->canceller != 0);

  test->arr = g_array_new (FALSE, FALSE, sizeof (TpHandle));
}

static void
test_clear_log (Test *test)
{
  g_ptr_array_foreach (test->log, (GFunc) log_entry_free, NULL);
  g_ptr_array_set_size (test->log, 0);
}

static void
teardown_pre_connect (
    Test *test,
    gconstpointer data)
{
  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);
  tp_clear_object (&test->conn);
  tp_clear_object (&test->dbus);
  tp_clear_pointer (&test->main_loop, g_main_loop_unref);
}

static void
teardown (Test *test,
    gconstpointer data)
{
  TpConnection *conn;
  GError *error = NULL;

  g_array_unref (test->arr);

  test_clear_log (test);
  g_ptr_array_unref (test->log);

  tp_handle_unref (test->contact_repo, test->sjoerd);
  tp_handle_unref (test->contact_repo, test->helen);
  tp_handle_unref (test->contact_repo, test->wim);
  tp_handle_unref (test->contact_repo, test->bill);
  tp_handle_unref (test->contact_repo, test->ninja);
  tp_handle_unref (test->contact_repo, test->canceller);

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
  tp_tests_connection_assert_disconnect_succeeds (conn);
  g_assert (!tp_connection_run_until_ready (conn, FALSE, &error, NULL));
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  tp_clear_pointer (&test->contact_attributes, g_hash_table_unref);

  teardown_pre_connect (test, data);
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

  tp_tests_proxy_run_until_prepared (ret, NULL);
  return ret;
}

static void
test_assert_one_contact_changed (Test *test,
    guint index,
    TpHandle handle,
    TpSubscriptionState expected_sub_state,
    TpSubscriptionState expected_pub_state,
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

  g_assert_cmpuint (tp_intset_size (le->contacts_removed), ==, 0);
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
  g_assert_cmpuint (tp_intset_size (le->contacts_removed), ==, 1);
  g_assert (tp_intset_is_member (le->contacts_removed, handle));
}

static void
test_assert_one_group_joined (Test *test,
    guint index,
    TpHandle handle,
    const gchar *group)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
  g_assert_cmpuint (le->contacts->len, ==, 1);
  g_assert_cmpuint (g_array_index (le->contacts, guint, 0), ==, handle);
  g_assert (le->groups_added != NULL);
  g_assert_cmpstr (le->groups_added[0], ==, group);
  g_assert_cmpstr (le->groups_added[1], ==, NULL);
  g_assert (le->groups_removed == NULL || le->groups_removed[0] == NULL);
}

static void
test_assert_one_group_left (Test *test,
    guint index,
    TpHandle handle,
    const gchar *group)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
  g_assert_cmpuint (le->contacts->len, ==, 1);
  g_assert_cmpuint (g_array_index (le->contacts, guint, 0), ==, handle);
  g_assert (le->groups_added == NULL || le->groups_added[0] == NULL);
  g_assert (le->groups_removed != NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, group);
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);
}

static void
test_assert_one_group_created (Test *test,
    guint index,
    const gchar *group)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, GROUPS_CREATED);
  g_assert (le->groups_added != NULL);
  g_assert_cmpstr (le->groups_added[0], ==, group);
  g_assert_cmpstr (le->groups_added[1], ==, NULL);
}

static void
test_assert_one_group_removed (Test *test,
    guint index,
    const gchar *group)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, GROUPS_REMOVED);
  g_assert (le->groups_removed != NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, group);
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);
}

static void
test_assert_one_contact_blocked (Test *test,
    guint index,
    TpHandle handle,
    const gchar *id)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, BLOCKED_CONTACTS_CHANGED);

  g_assert (le->blocked_contacts != NULL);
  g_assert_cmpuint (g_hash_table_size (le->blocked_contacts), ==, 1);
  g_assert_cmpstr (g_hash_table_lookup (le->blocked_contacts, GUINT_TO_POINTER (handle)),
      ==, id);

  g_assert (le->unblocked_contacts != NULL);
  g_assert_cmpuint (g_hash_table_size (le->unblocked_contacts), ==, 0);
}

static void
test_assert_one_contact_unblocked (Test *test,
    guint index,
    TpHandle handle,
    const gchar *id)
{
  LogEntry *le;

  le = g_ptr_array_index (test->log, index);
  g_assert_cmpint (le->type, ==, BLOCKED_CONTACTS_CHANGED);

  g_assert (le->blocked_contacts != NULL);
  g_assert_cmpuint (g_hash_table_size (le->blocked_contacts), ==, 0);

  g_assert (le->unblocked_contacts != NULL);
  g_assert_cmpuint (g_hash_table_size (le->unblocked_contacts), ==, 1);
  g_assert_cmpstr (g_hash_table_lookup (le->unblocked_contacts, GUINT_TO_POINTER (handle)),
      ==, id);
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
}

static void
test_properties (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GHashTable *asv;
  GError *error = NULL;
  guint32 blocking_caps;
  gboolean valid;

  tp_cli_dbus_properties_run_get_all (test->conn, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST, &asv, &error, NULL);
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (asv), >=, 3);
  g_assert (tp_asv_get_boolean (asv, "ContactListPersists", NULL));
  g_assert (tp_asv_get_boolean (asv, "CanChangeContactList", NULL));
  g_assert (tp_asv_get_boolean (asv, "RequestUsesMessage", NULL));
  g_hash_table_unref (asv);

  tp_cli_dbus_properties_run_get_all (test->conn, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS, &asv, &error, NULL);
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (asv), >=, 3);
  g_assert (G_VALUE_HOLDS_BOOLEAN (tp_asv_lookup (asv, "DisjointGroups")));
  g_assert (!tp_asv_get_boolean (asv, "DisjointGroups", NULL));
  g_assert (G_VALUE_HOLDS_UINT (tp_asv_lookup (asv, "GroupStorage")));
  g_assert_cmpuint (tp_asv_get_uint32 (asv, "GroupStorage", NULL), ==,
      TP_CONTACT_METADATA_STORAGE_TYPE_ANYONE);
  /* Don't assert about the contents yet - we might not have received the
   * contact list yet */
  g_assert (G_VALUE_HOLDS (tp_asv_lookup (asv, "Groups"), G_TYPE_STRV));
  g_hash_table_unref (asv);

  /* this has the side-effect of waiting for the contact list to be received */
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  tp_cli_dbus_properties_run_get_all (test->conn, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST, &asv, &error, NULL);
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (asv), >=, 3);
  g_assert (tp_asv_get_boolean (asv, "ContactListPersists", NULL));
  g_assert (tp_asv_get_boolean (asv, "CanChangeContactList", NULL));
  g_assert (tp_asv_get_boolean (asv, "RequestUsesMessage", NULL));
  g_hash_table_unref (asv);

  tp_cli_dbus_properties_run_get_all (test->conn, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS, &asv, &error, NULL);
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (asv), >=, 3);
  g_assert (G_VALUE_HOLDS_BOOLEAN (tp_asv_lookup (asv, "DisjointGroups")));
  g_assert (G_VALUE_HOLDS_UINT (tp_asv_lookup (asv, "GroupStorage")));
  g_assert (tp_asv_get_strv (asv, "Groups") != NULL);
  g_assert (tp_strv_contains (tp_asv_get_strv (asv, "Groups"), "Cambridge"));
  g_assert (tp_strv_contains (tp_asv_get_strv (asv, "Groups"), "Montreal"));
  g_assert (tp_strv_contains (tp_asv_get_strv (asv, "Groups"),
        "Francophones"));
  g_hash_table_unref (asv);

  tp_cli_dbus_properties_run_get_all (test->conn, -1,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_BLOCKING, &asv, &error, NULL);
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (asv), ==, 1);
  blocking_caps = tp_asv_get_uint32 (asv, "ContactBlockingCapabilities",
      &valid);
  g_assert (valid);
  g_assert_cmpuint (blocking_caps, ==,
      TP_CONTACT_BLOCKING_CAPABILITY_CAN_REPORT_ABUSIVE);
  g_hash_table_unref (asv);

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
contact_attrs_cb (TpConnection *conn G_GNUC_UNUSED,
    GHashTable *attributes,
    const GError *error,
    gpointer user_data,
    GObject *object G_GNUC_UNUSED)
{
  Test *test = user_data;

  g_assert_no_error ((GError *) error);
  tp_clear_pointer (&test->contact_attributes, g_hash_table_unref);
  test->contact_attributes = g_boxed_copy (TP_HASH_TYPE_CONTACT_ATTRIBUTES_MAP,
      attributes);
}

static void
test_assert_contact_list_attrs (Test *test,
    TpHandle handle,
    TpSubscriptionState expected_sub_state,
    TpSubscriptionState expected_pub_state,
    const gchar *expected_pub_request)
{
  GHashTable *asv;
  gboolean valid;

  g_assert_cmpuint (g_hash_table_size (test->contact_attributes), >=, 1);
  asv = g_hash_table_lookup (test->contact_attributes,
      GUINT_TO_POINTER (handle));
  g_assert (asv != NULL);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_SUBSCRIBE, &valid), ==,
      expected_sub_state);
  g_assert (valid);
  g_assert_cmpuint (tp_asv_get_uint32 (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_PUBLISH, &valid), ==,
      expected_pub_state);
  g_assert (valid);
  g_assert_cmpstr (tp_asv_get_string (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_LIST_PUBLISH_REQUEST), ==,
      expected_pub_request);
  g_assert (valid);
}

/* We simplify here by assuming that contacts are in at most one group,
 * which happens to be true for all of these tests. */
static void
test_assert_contact_groups_attr (Test *test,
    TpHandle handle,
    const gchar *group)
{
  GHashTable *asv;
  const gchar * const *strv;

  g_assert_cmpuint (g_hash_table_size (test->contact_attributes), >=, 1);
  asv = g_hash_table_lookup (test->contact_attributes,
      GUINT_TO_POINTER (handle));
  g_assert (asv != NULL);
  tp_asv_dump (asv);
  g_assert (tp_asv_lookup (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_GROUPS_GROUPS) != NULL);
  g_assert (G_VALUE_HOLDS (tp_asv_lookup (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_GROUPS_GROUPS), G_TYPE_STRV));
  strv = tp_asv_get_strv (asv,
        TP_TOKEN_CONNECTION_INTERFACE_CONTACT_GROUPS_GROUPS);

  if (group == NULL)
    {
      if (strv != NULL)
        g_assert_cmpstr (strv[0], ==, NULL);
    }
  else
    {
      g_assert (strv != NULL);
      g_assert_cmpstr (strv[0], ==, group);
      g_assert_cmpstr (strv[1], ==, NULL);
    }
}

static void
test_assert_contact_state (Test *test,
    TpHandle handle,
    TpSubscriptionState expected_sub_state,
    TpSubscriptionState expected_pub_state,
    const gchar *expected_pub_request,
    const gchar *expected_group)
{
  const gchar * const interfaces[] = {
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST,
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS,
      NULL };

  tp_connection_get_contact_attributes (test->conn, -1,
      1, &handle, interfaces, FALSE, contact_attrs_cb,
      test, test_quit_loop, NULL);
  g_main_loop_run (test->main_loop);

  g_assert_cmpuint (g_hash_table_size (test->contact_attributes), ==, 1);
  test_assert_contact_list_attrs (test, handle, expected_sub_state,
      expected_pub_state, expected_pub_request);
  test_assert_contact_groups_attr (test, handle, expected_group);
}

static void
test_contacts (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  /* ensure the contact list has been received */
  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, "Cambridge");
  test_assert_contact_state (test, test->wim,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_ASK,
      "I'm more metal than you!", NULL);
  test_assert_contact_state (test, test->helen,
      TP_SUBSCRIPTION_STATE_ASK, TP_SUBSCRIPTION_STATE_NO, NULL, "Cambridge");
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
  test_assert_contact_state (test, test->bill,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_contact_list_attrs (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  const gchar * const interfaces[] = {
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS,
      NULL };

  tp_connection_get_contact_list_attributes (test->conn, -1,
      interfaces, FALSE, contact_attrs_cb, test, test_quit_loop, NULL);
  g_main_loop_run (test->main_loop);

  test_assert_contact_list_attrs (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL);
  test_assert_contact_list_attrs (test, test->wim,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_ASK,
      "I'm more metal than you!");
  test_assert_contact_list_attrs (test, test->helen,
      TP_SUBSCRIPTION_STATE_ASK, TP_SUBSCRIPTION_STATE_NO, NULL);

  test_assert_contact_groups_attr (test, test->sjoerd, "Cambridge");
  test_assert_contact_groups_attr (test, test->wim, NULL);
  test_assert_contact_groups_attr (test, test->helen, "Cambridge");

  /* bill is blocked, but is not on the contact list as such; the ninja isn't
   * in the initial state at all */
  g_assert (g_hash_table_lookup (test->contact_attributes,
        GUINT_TO_POINTER (test->bill)) == NULL);
  g_assert (g_hash_table_lookup (test->contact_attributes,
        GUINT_TO_POINTER (test->ninja)) == NULL);
}

static void
test_assert_contact_blocking_attrs (Test *test,
    TpHandle handle,
    gboolean expected_blocked)
{
  GHashTable *asv;
  gboolean blocked, valid;

  g_assert_cmpuint (g_hash_table_size (test->contact_attributes), >=, 1);
  asv = g_hash_table_lookup (test->contact_attributes,
      GUINT_TO_POINTER (handle));
  g_assert (asv != NULL);
  tp_asv_dump (asv);

  blocked = tp_asv_get_boolean (asv,
      TP_TOKEN_CONNECTION_INTERFACE_CONTACT_BLOCKING_BLOCKED, &valid);
  g_assert (valid);
  g_assert (blocked == expected_blocked);
}

static void
test_contact_blocking_attrs (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  const gchar * const interfaces[] = {
      TP_IFACE_CONNECTION_INTERFACE_CONTACT_BLOCKING,
      NULL };
  TpHandle handles[] = { test->sjoerd, test->bill };

  tp_connection_get_contact_attributes (test->conn, -1,
      G_N_ELEMENTS (handles), handles,
      interfaces, FALSE, contact_attrs_cb, test, test_quit_loop, NULL);
  g_main_loop_run (test->main_loop);

  test_assert_contact_blocking_attrs (test, test->sjoerd, FALSE);
  test_assert_contact_blocking_attrs (test, test->bill, TRUE);
}

static void
test_accept_publish_request (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->publish,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_authorize_publication (
        test->conn, -1, test->arr, &error, NULL);

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
  test_assert_one_contact_changed (test, 0, test->wim, TP_SUBSCRIPTION_STATE_NO,
      TP_SUBSCRIPTION_STATE_YES, "");
  test_assert_contact_state (test, test->wim,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_YES, NULL, NULL);
}

static void
test_reject_publish_request (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    {
      tp_cli_channel_interface_group_run_remove_members (test->publish,
          -1, test->arr, "", &error, NULL);
    }
  else if (!tp_strdiff (mode, "unpublish"))
    {
      /* directly equivalent, but in practice people won't do this */
      tp_cli_connection_interface_contact_list_run_unpublish (
          test->conn, -1, test->arr, &error, NULL);
    }
  else
    {
      /* this isn't directly equivalent, but in practice it's what people
       * will do */
      tp_cli_connection_interface_contact_list_run_remove_contacts (
          test->conn, -1, test->arr, &error, NULL);
    }

  g_assert_no_error (error);

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

  /* because Wim wasn't really on our contact list, he's removed as a
   * side-effect, even if we only unpublished */
  test_assert_one_contact_removed (test, 0, test->wim);

  test_assert_contact_state (test, test->wim,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_add_to_publish_pre_approve (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->publish,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_authorize_publication (
        test->conn, -1, test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->ninja));

  /* the example CM's fake contacts accept requests that contain "please" */
  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->subscribe,
        -1, test->arr, "Please may I see your presence?", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_request_subscription (
        test->conn, -1, test->arr, "Please may I see your presence?", &error,
        NULL);

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
  test_assert_one_contact_changed (test, 0, test->ninja, TP_SUBSCRIPTION_STATE_ASK,
      TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja, TP_SUBSCRIPTION_STATE_YES,
      TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 2, test->ninja, TP_SUBSCRIPTION_STATE_YES,
      TP_SUBSCRIPTION_STATE_YES, "");

  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, NULL);
}

static void
test_add_to_publish_no_op (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  /* Adding a member to the publish channel when they're already there is
   * valid. */

  test->publish = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "publish");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->publish,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_authorize_publication (
        test->conn, -1, test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, "Cambridge");
}

static void
test_remove_from_publish (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->publish,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unpublish (
        test->conn, -1, test->arr, &error, NULL);

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
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_ASK,
      "May I see your presence, please?");
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_ASK,
      "May I see your presence, please?", "Cambridge");
}

static void
test_remove_from_publish_no_op (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->publish,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unpublish (
        test->conn, -1, test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_cancelled_publish_request (Test *test,
    gconstpointer mode)
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
        test->canceller));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_remote_pending (test->subscribe),
        test->canceller));

  /* the example CM's fake contacts accept requests that contain "please" */
  g_array_append_val (test->arr, test->canceller);

  tp_cli_connection_interface_contact_list_run_request_subscription (
      test->conn, -1, test->arr, "Please may I see your presence?",
      &error, NULL);

  /* It starts off the same as test_accept_subscribe_request, but because
   * we're using an identifier with special significance, the contact cancels
   * the request immediately after */
  while (tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->canceller) ||
      test->log->len < 4)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->publish),
        test->canceller));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_local_pending (test->publish),
        test->canceller));

  g_assert_cmpuint (test->log->len, ==, 4);
  test_assert_one_contact_changed (test, 0, test->canceller,
      TP_SUBSCRIPTION_STATE_ASK, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->canceller,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 2, test->canceller,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_ASK,
      "May I see your presence, please?");
  test_assert_one_contact_changed (test, 3, test->canceller,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_REMOVED_REMOTELY, "");
  test_assert_contact_state (test, test->canceller,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_REMOVED_REMOTELY,
      NULL, NULL);

  test_clear_log (test);

  /* We can acknowledge the cancellation with Unpublish() or
   * RemoveContacts(). We can't use the old API here, because in the old API,
   * the contact has already vanished from the Group */
  if (!tp_strdiff (mode, "remove-after"))
    tp_cli_connection_interface_contact_list_run_remove_contacts (test->conn,
        -1, test->arr, &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unpublish (
        test->conn, -1, test->arr, &error, NULL);

  while (test->log->len < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (test->log->len, ==, 1);

  if (!tp_strdiff (mode, "remove-after"))
    test_assert_one_contact_removed (test, 0, test->canceller);
  else
    test_assert_one_contact_changed (test, 0, test->canceller,
        TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_NO, "");
}

static void
test_add_to_stored (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    {
      tp_cli_channel_interface_group_run_add_members (test->stored,
          -1, test->arr, "", &error, NULL);
    }
  else
    {
      /* there's no specific API for adding contacts to stored (it's not a
       * very useful action in general), but setting an alias has it as a
       * side-effect */
      GHashTable *table = g_hash_table_new (NULL, NULL);

      g_hash_table_insert (table, GUINT_TO_POINTER (test->ninja),
          "The Wee Ninja");
      tp_cli_connection_interface_aliasing_run_set_aliases (test->conn,
          -1, table, &error, NULL);
      g_hash_table_unref (table);
    }

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
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_add_to_stored_no_op (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    {
      tp_cli_channel_interface_group_run_add_members (test->stored,
          -1, test->arr, "", &error, NULL);
    }
  else
    {
      /* there's no specific API for adding contacts to stored (it's not a
       * very useful action in general), but setting an alias has it as a
       * side-effect */
      GHashTable *table = g_hash_table_new (NULL, NULL);

      g_hash_table_insert (table, GUINT_TO_POINTER (test->sjoerd),
          "Sjoerd");
      tp_cli_connection_interface_aliasing_run_set_aliases (test->conn,
          -1, table, &error, NULL);
      g_hash_table_unref (table);
    }

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, "Cambridge");
}

static void
test_remove_from_stored (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->stored,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_remove_contacts (test->conn,
        -1, test->arr, &error, NULL);

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
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_remove_from_stored_no_op (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->stored,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_remove_contacts (test->conn,
        -1, test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_accept_subscribe_request (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->subscribe,
        -1, test->arr, "Please may I see your presence?", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_request_subscription (
        test->conn, -1, test->arr, "Please may I see your presence?",
        &error, NULL);

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
      TP_SUBSCRIPTION_STATE_ASK, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 2, test->ninja,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_ASK,
      "May I see your presence, please?");
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_ASK,
      "May I see your presence, please?", NULL);
}

static void
test_reject_subscribe_request (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->subscribe,
        -1, test->arr, "I demand to see your presence?", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_request_subscription (
        test->conn, -1, test->arr, "I demand to see your presence?",
        &error, NULL);

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
      TP_SUBSCRIPTION_STATE_ASK, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_one_contact_changed (test, 1, test->ninja,
      TP_SUBSCRIPTION_STATE_REMOVED_REMOTELY, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_REMOVED_REMOTELY, TP_SUBSCRIPTION_STATE_NO, NULL,
      NULL);

  test_clear_log (test);

  /* We can acknowledge the failure to subscribe with Unsubscribe() or
   * RemoveContacts(). We can't use the old API here, because in the old API,
   * the contact has already vanished from the Group */
  if (!tp_strdiff (mode, "remove-after"))
    tp_cli_connection_interface_contact_list_run_remove_contacts (test->conn,
        -1, test->arr, &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unsubscribe (
        test->conn, -1, test->arr, &error, NULL);

  /* the ninja falls off our subscribe list */
  while (test->log->len < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (test->log->len, ==, 1);

  if (!tp_strdiff (mode, "remove-after"))
    test_assert_one_contact_removed (test, 0, test->ninja);
  else
    test_assert_one_contact_changed (test, 0, test->ninja,
        TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, "");
}

static void
test_remove_from_subscribe (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->subscribe,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unsubscribe (
        test->conn, -1, test->arr, &error, NULL);

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
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_YES, "");
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_YES, NULL, "Cambridge");
}

static void
test_remove_from_subscribe_pending (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->subscribe,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unsubscribe (
        test->conn, -1, test->arr, &error, NULL);

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
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, "");
  test_assert_contact_state (test, test->helen,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, "Cambridge");
}

static void
test_remove_from_subscribe_no_op (Test *test,
    gconstpointer mode)
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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->subscribe,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_list_run_unsubscribe (
        test->conn, -1, test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_add_to_group (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;
  LogEntry *le;
  guint i;

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

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->group,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_groups_run_add_to_group (test->conn,
        -1, "Cambridge", test->arr, &error, NULL);

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

  g_assert_cmpuint (test->log->len, ==, 2);

  le = g_ptr_array_index (test->log, 0);

  if (le->type == CONTACTS_CHANGED)
    {
      test_assert_one_contact_changed (test, 0, test->ninja,
          TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, "");
      i = 1;
    }
  else
    {
      test_assert_one_contact_changed (test, 1, test->ninja,
          TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, "");
      i = 0;
    }

  /* either way, the i'th entry is now the GroupsChanged signal */
  test_assert_one_group_joined (test, i, test->ninja, "Cambridge");

  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, "Cambridge");
}

static void
test_add_to_group_no_op (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_add_members (test->group,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_groups_run_add_to_group (test->conn,
        -1, "Cambridge", test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, "Cambridge");
}

static void
test_remove_from_group (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->group,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_groups_run_remove_from_group (
        test->conn, -1, "Cambridge", test->arr, &error, NULL);

  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_group_left (test, 0, test->sjoerd, "Cambridge");
  test_assert_contact_state (test, test->sjoerd,
      TP_SUBSCRIPTION_STATE_YES, TP_SUBSCRIPTION_STATE_YES, NULL, NULL);
}

static void
test_remove_from_group_no_op (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);

  if (!tp_strdiff (mode, "old"))
    tp_cli_channel_interface_group_run_remove_members (test->group,
        -1, test->arr, "", &error, NULL);
  else
    tp_cli_connection_interface_contact_groups_run_remove_from_group (
        test->conn, -1, "Cambridge", test->arr, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 0);
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);
}

static void
test_remove_group (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert (!tp_intset_is_empty (
        tp_channel_group_get_members (test->group)));

  if (!tp_strdiff (mode, "old"))
    {
      /* The old API can't remove non-empty groups... */
      tp_cli_channel_run_close (test->group, -1, &error, NULL);
      g_assert_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);

      g_assert_cmpuint (test->log->len, ==, 0);
    }
  else
    {
      /* ... but the new API can */
      LogEntry *le;

      tp_cli_connection_interface_contact_groups_run_remove_group (test->conn,
          -1, "Cambridge", &error, NULL);
      g_assert_no_error (error);

      g_assert (tp_proxy_get_invalidated (test->group) != NULL);
      g_assert_cmpuint (test->log->len, ==, 2);
      test_assert_one_group_removed (test, 0, "Cambridge");

      le = g_ptr_array_index (test->log, 1);
      g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
      g_assert_cmpuint (le->contacts->len, ==, 4);
      g_assert (le->groups_added == NULL || le->groups_added[0] == NULL);
      g_assert (le->groups_removed != NULL);
      g_assert_cmpstr (le->groups_removed[0], ==, "Cambridge");
      g_assert_cmpstr (le->groups_removed[1], ==, NULL);
    }
}

static void
test_remove_group_empty (Test *test,
    gconstpointer mode)
{
  GError *error = NULL;

  g_assert_cmpuint (test->log->len, ==, 0);
  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "people who understand const in C");

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_group_created (test, 0, "people who understand const in C");

  g_assert (tp_intset_is_empty (
        tp_channel_group_get_members (test->group)));

  tp_cli_channel_run_close (test->group, -1, &error, NULL);
  g_assert_no_error (error);

  g_assert_cmpuint (test->log->len, ==, 2);
  test_assert_one_group_removed (test, 1, "people who understand const in C");
}

static void
test_set_contact_groups (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  LogEntry *le;
  const gchar *montreal_strv[] = { "Montreal", NULL };

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  g_array_append_val (test->arr, test->wim);

  tp_cli_connection_interface_contact_groups_run_set_contact_groups (
      test->conn, -1, test->sjoerd, montreal_strv, &error, NULL);

  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 3);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 1);

  le = g_ptr_array_index (test->log, 0);
  g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
  g_assert_cmpuint (le->contacts->len, ==, 1);
  g_assert_cmpuint (g_array_index (le->contacts, guint, 0), ==, test->sjoerd);
  g_assert (le->groups_added != NULL);
  g_assert_cmpstr (le->groups_added[0], ==, "Montreal");
  g_assert_cmpstr (le->groups_added[1], ==, NULL);
  g_assert (le->groups_removed != NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, "Cambridge");
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);
}

static void
test_set_contact_groups_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  const gchar *cambridge_strv[] = { "Cambridge", NULL };

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_array_append_val (test->arr, test->sjoerd);
  g_array_append_val (test->arr, test->wim);

  tp_cli_connection_interface_contact_groups_run_set_contact_groups (
      test->conn, -1, test->sjoerd, cambridge_strv, &error, NULL);

  g_assert_no_error (error);

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));

  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
test_set_group_members (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  LogEntry *le;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->helen));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->wim));

  g_array_append_val (test->arr, test->sjoerd);
  g_array_append_val (test->arr, test->wim);

  tp_cli_connection_interface_contact_groups_run_set_group_members (test->conn,
      -1, "Cambridge", test->arr, &error, NULL);

  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, too */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 2);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->wim));
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->sjoerd));
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->group),
        test->helen));

  g_assert_cmpuint (test->log->len, ==, 2);

  /* Wim was added */
  test_assert_one_group_joined (test, 0, test->wim, "Cambridge");

  /* The three other members, other than Sjoerd, left */
  le = g_ptr_array_index (test->log, 1);
  g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
  g_assert_cmpuint (le->contacts->len, ==, 3);
  g_assert (le->groups_added == NULL || le->groups_added[0] == NULL);
  g_assert (le->groups_removed != NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, "Cambridge");
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);
}

static void
test_rename_group (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  LogEntry *le;
  GError *error = NULL;

  test->group = test_ensure_channel (test, TP_HANDLE_TYPE_GROUP,
      "Cambridge");

  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->group)),
      ==, 4);

  tp_cli_connection_interface_contact_groups_run_rename_group (test->conn,
      -1, "Cambridge", "Grantabrugge", &error, NULL);
  g_assert_no_error (error);

  g_assert (tp_proxy_get_invalidated (test->group) != NULL);
  g_assert_cmpuint (test->log->len, ==, 4);

  le = g_ptr_array_index (test->log, 0);
  g_assert_cmpint (le->type, ==, GROUP_RENAMED);
  g_assert (le->groups_added != NULL);
  g_assert_cmpstr (le->groups_added[0], ==, "Grantabrugge");
  g_assert_cmpstr (le->groups_added[1], ==, NULL);
  g_assert (le->groups_removed != NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, "Cambridge");
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);

  test_assert_one_group_created (test, 1, "Grantabrugge");

  test_assert_one_group_removed (test, 2, "Cambridge");

  le = g_ptr_array_index (test->log, 3);
  g_assert_cmpint (le->type, ==, GROUPS_CHANGED);
  g_assert_cmpuint (le->contacts->len, ==, 4);
  g_assert (le->groups_added != NULL);
  g_assert_cmpstr (le->groups_added[0], ==, "Grantabrugge");
  g_assert_cmpstr (le->groups_added[1], ==, NULL);
  g_assert_cmpstr (le->groups_removed[0], ==, "Cambridge");
  g_assert_cmpstr (le->groups_removed[1], ==, NULL);
}

static void
test_rename_group_overwrite (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  tp_cli_connection_interface_contact_groups_run_rename_group (test->conn,
      -1, "Cambridge", "Montreal", &error, NULL);
  g_assert_error (error, TP_ERROR, TP_ERROR_NOT_AVAILABLE);
  g_assert_cmpuint (test->log->len, ==, 0);
  g_clear_error (&error);
}

static void
test_rename_group_absent (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  tp_cli_connection_interface_contact_groups_run_rename_group (test->conn,
      -1, "Badgers", "Mushrooms", &error, NULL);
  g_assert_error (error, TP_ERROR, TP_ERROR_DOES_NOT_EXIST);
  g_assert_cmpuint (test->log->len, ==, 0);
  g_clear_error (&error);
}

/* Signature of a function which does something with test->arr */
typedef void (*ManipulateContactsFunc) (
    Test *test,
    GError **error);

static void
block_contacts (Test *test,
    ManipulateContactsFunc func)
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
  func (test, &error);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * change-notification, on both the deny channel and the ContactBlocking
   * connection interface */
  g_assert_cmpuint (
      tp_intset_size (tp_channel_group_get_members (test->deny)),
      ==, 3);
  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->stored),
        test->ninja));
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_blocked (test, 0, test->ninja,
      tp_handle_inspect (test->contact_repo, test->ninja));
}

static void
block_contacts_no_op (Test *test,
    ManipulateContactsFunc func)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));

  g_array_append_val (test->arr, test->bill);
  func (test, &error);
  g_assert_no_error (error);

  g_assert (tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));
  test_assert_contact_state (test, test->bill,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);

  /* We shouldn't emit spurious empty BlockedContactsChanged signals. */
  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
unblock_contacts (Test *test,
    ManipulateContactsFunc func)
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
  func (test, &error);
  g_assert_no_error (error);

  /* by the time the method returns, we should have had the
   * removal-notification, too */
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->bill));
  test_assert_contact_state (test, test->bill,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);

  g_assert_cmpuint (test->log->len, ==, 1);
  test_assert_one_contact_unblocked (test, 0, test->bill,
      tp_handle_inspect (test->contact_repo, test->bill));
}

static void
unblock_contacts_no_op (Test *test,
    ManipulateContactsFunc func)
{
  GError *error = NULL;

  test->deny = test_ensure_channel (test, TP_HANDLE_TYPE_LIST, "deny");

  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));

  g_array_append_val (test->arr, test->ninja);
  func (test, &error);
  g_assert_no_error (error);
  g_assert (!tp_intset_is_member (
        tp_channel_group_get_members (test->deny),
        test->ninja));
  test_assert_contact_state (test, test->ninja,
      TP_SUBSCRIPTION_STATE_NO, TP_SUBSCRIPTION_STATE_NO, NULL, NULL);

  /* We shouldn't emit spurious empty BlockedContactsChanged signals. */
  g_assert_cmpuint (test->log->len, ==, 0);
}

static void
add_to_deny (Test *test,
    GError **error)
{
  tp_cli_channel_interface_group_run_add_members (test->deny,
      -1, test->arr, "", error, NULL);
}

static void
test_add_to_deny (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  block_contacts (test, add_to_deny);
}

static void
test_add_to_deny_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  block_contacts_no_op (test, add_to_deny);
}

static void
remove_from_deny (Test *test,
    GError **error)
{
  tp_cli_channel_interface_group_run_remove_members (test->deny,
      -1, test->arr, "", error, NULL);
}

static void
test_remove_from_deny (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  unblock_contacts (test, remove_from_deny);
}

static void
test_remove_from_deny_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  unblock_contacts_no_op (test, remove_from_deny);
}

static void
test_request_blocked_contacts (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GHashTable *blocked_contacts;
  GError *error = NULL;

  tp_cli_connection_interface_contact_blocking_run_request_blocked_contacts (
      test->conn, -1, &blocked_contacts, &error, NULL);
  g_assert_no_error (error);
  g_assert (blocked_contacts != NULL);

  /* Both Bill and the shadowy Steve are blocked; Steve does not appear in this
   * test, as he is in poor health.
   */
  g_assert_cmpuint (g_hash_table_size (blocked_contacts), ==, 2);
  g_assert_cmpstr (tp_handle_inspect (test->contact_repo, test->bill), ==,
      g_hash_table_lookup (blocked_contacts, GUINT_TO_POINTER (test->bill)));
  g_hash_table_unref (blocked_contacts);
}

static void
request_blocked_contacts_succeeded_cb (
    TpConnection *conn,
    GHashTable *blocked_contacts,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  g_assert_no_error (error);

  /* As above. */
  g_assert_cmpuint (g_hash_table_size (blocked_contacts), ==, 2);
}

static void
test_request_blocked_contacts_pre_connect (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  /* This verifies that calling RequestBlockedContacts()
   * before Connect(), when Connect() ultimately succeeds, returns correctly.
   */
  tp_cli_connection_interface_contact_blocking_call_request_blocked_contacts (
      test->conn, -1, request_blocked_contacts_succeeded_cb,
      test, test_quit_loop, NULL);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  g_main_loop_run (test->main_loop);

  tp_tests_connection_assert_disconnect_succeeds (test->conn);
}

static void
request_blocked_contacts_failed_cb (
    TpConnection *conn,
    GHashTable *blocked_contacts,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  g_assert_error (error, TP_ERROR, TP_ERROR_DISCONNECTED);
}

static void
test_request_blocked_contacts_connect_failed (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  /* This verifies that calling RequestBlockedContacts() (twice, no less)
   * before Connect(), when Connect() ultimately fails, returns an appropriate
   * error.
   */
  tp_cli_connection_interface_contact_blocking_call_request_blocked_contacts (
      test->conn, -1, request_blocked_contacts_failed_cb,
      test, test_quit_loop, NULL);
  tp_cli_connection_interface_contact_blocking_call_request_blocked_contacts (
      test->conn, -1, request_blocked_contacts_failed_cb,
      test, test_quit_loop, NULL);

  /* We expect calling Connect() to fail because the handle was invalid, but
   * don't wait around for it.
   */
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  /* Spin the mainloop twice, once for each outstanding call. */
  g_main_loop_run (test->main_loop);
  g_main_loop_run (test->main_loop);
}

static void
call_block_contacts (Test *test,
    GError **error)
{
  tp_cli_connection_interface_contact_blocking_run_block_contacts (test->conn,
      -1, test->arr, FALSE, error, NULL);
}

static void
test_block_contacts (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  block_contacts (test, call_block_contacts);
}

static void
test_block_contacts_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  block_contacts_no_op (test, call_block_contacts);
}

static void
call_unblock_contacts (Test *test,
    GError **error)
{
  tp_cli_connection_interface_contact_blocking_run_unblock_contacts (
      test->conn, -1, test->arr, error, NULL);
}

static void
test_unblock_contacts (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  unblock_contacts (test, call_unblock_contacts);
}

static void
test_unblock_contacts_no_op (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  unblock_contacts_no_op (test, call_unblock_contacts);
}

static void
download_contacts_cb (
    TpConnection *conn,
    const GError *error, gpointer user_data,
    GObject *weak_object)
{
  g_assert_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED);
}

static void
test_download_contacts (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  tp_cli_connection_interface_contact_list_call_download (
    test->conn, -1, download_contacts_cb, test, test_quit_loop, NULL);

  g_main_loop_run (test->main_loop);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_tests_abort_after (30);
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/contact-lists/nothing",
      Test, NULL, setup, test_nothing, teardown);

  g_test_add ("/contact-lists/initial-channels",
      Test, NULL, setup, test_initial_channels, teardown);
  g_test_add ("/contact-lists/properties",
      Test, NULL, setup, test_properties, teardown);
  g_test_add ("/contact-lists/contacts",
      Test, NULL, setup, test_contacts, teardown);
  g_test_add ("/contact-lists/contact-list-attrs",
      Test, NULL, setup, test_contact_list_attrs, teardown);
  g_test_add ("/contact-lists/contact-blocking-attrs",
      Test, NULL, setup, test_contact_blocking_attrs, teardown);

  g_test_add ("/contact-lists/accept-publish-request",
      Test, NULL, setup, test_accept_publish_request, teardown);
  g_test_add ("/contact-lists/reject-publish-request",
      Test, NULL, setup, test_reject_publish_request, teardown);
  g_test_add ("/contact-lists/reject-publish-request/unpublish",
      Test, "unpublish", setup, test_reject_publish_request, teardown);
  g_test_add ("/contact-lists/add-to-publish/pre-approve",
      Test, NULL, setup, test_add_to_publish_pre_approve, teardown);
  g_test_add ("/contact-lists/add-to-publish/no-op",
      Test, NULL, setup, test_add_to_publish_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-publish",
      Test, NULL, setup, test_remove_from_publish, teardown);
  g_test_add ("/contact-lists/remove-from-publish/no-op",
      Test, NULL, setup, test_remove_from_publish_no_op, teardown);

  g_test_add ("/contact-lists/accept-publish-request/old",
      Test, "old", setup, test_accept_publish_request, teardown);
  g_test_add ("/contact-lists/reject-publish-request/old",
      Test, "old", setup, test_reject_publish_request, teardown);
  g_test_add ("/contact-lists/add-to-publish/pre-approve/old",
      Test, "old", setup, test_add_to_publish_pre_approve, teardown);
  g_test_add ("/contact-lists/add-to-publish/no-op/old",
      Test, "old", setup, test_add_to_publish_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-publish/old",
      Test, "old", setup, test_remove_from_publish, teardown);
  g_test_add ("/contact-lists/remove-from-publish/no-op/old",
      Test, "old", setup, test_remove_from_publish_no_op, teardown);

  g_test_add ("/contact-lists/cancelled-publish-request",
      Test, NULL, setup, test_cancelled_publish_request, teardown);
  g_test_add ("/contact-lists/cancelled-publish-request",
      Test, "remove-after", setup, test_cancelled_publish_request, teardown);

  g_test_add ("/contact-lists/add-to-stored",
      Test, NULL, setup, test_add_to_stored, teardown);
  g_test_add ("/contact-lists/add-to-stored/no-op",
      Test, NULL, setup, test_add_to_stored_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-stored",
      Test, NULL, setup, test_remove_from_stored, teardown);
  g_test_add ("/contact-lists/remove-from-stored/no-op",
      Test, NULL, setup, test_remove_from_stored_no_op, teardown);

  g_test_add ("/contact-lists/add-to-stored/old",
      Test, "old", setup, test_add_to_stored, teardown);
  g_test_add ("/contact-lists/add-to-stored/no-op/old",
      Test, "old", setup, test_add_to_stored_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-stored/old",
      Test, "old", setup, test_remove_from_stored, teardown);
  g_test_add ("/contact-lists/remove-from-stored/no-op/old",
      Test, "old", setup, test_remove_from_stored_no_op, teardown);

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

  g_test_add ("/contact-lists/accept-subscribe-request/old",
      Test, "old", setup, test_accept_subscribe_request, teardown);
  g_test_add ("/contact-lists/reject-subscribe-request/old",
      Test, "old", setup, test_reject_subscribe_request, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe/old",
      Test, "old", setup, test_remove_from_subscribe, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe/pending/old",
      Test, "old", setup, test_remove_from_subscribe_pending, teardown);
  g_test_add ("/contact-lists/remove-from-subscribe/no-op/old",
      Test, "old", setup, test_remove_from_subscribe_no_op, teardown);

  g_test_add ("/contact-lists/reject-subscribe-request/remove-after",
      Test, "remove-after", setup, test_reject_subscribe_request, teardown);

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

  g_test_add ("/contact-lists/add-to-group/old",
      Test, "old", setup, test_add_to_group, teardown);
  g_test_add ("/contact-lists/add-to-group/no-op/old",
      Test, "old", setup, test_add_to_group_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-group/old",
      Test, "old", setup, test_remove_from_group, teardown);
  g_test_add ("/contact-lists/remove-from-group/no-op/old",
      Test, "old", setup, test_remove_from_group_no_op, teardown);
  g_test_add ("/contact-lists/remove-group/old",
      Test, "old", setup, test_remove_group, teardown);
  g_test_add ("/contact-lists/remove-group/empty/old",
      Test, "old", setup, test_remove_group_empty, teardown);

  g_test_add ("/contact-lists/set_contact_groups",
      Test, NULL, setup, test_set_contact_groups, teardown);
  g_test_add ("/contact-lists/set_contact_groups/no-op",
      Test, NULL, setup, test_set_contact_groups_no_op, teardown);
  g_test_add ("/contact-lists/set_group_members",
      Test, NULL, setup, test_set_group_members, teardown);

  g_test_add ("/contact-lists/rename_group",
      Test, NULL, setup, test_rename_group, teardown);
  g_test_add ("/contact-lists/rename_group/absent",
      Test, NULL, setup, test_rename_group_absent, teardown);
  g_test_add ("/contact-lists/rename_group/overwrite",
      Test, NULL, setup, test_rename_group_overwrite, teardown);

  g_test_add ("/contact-lists/add-to-deny",
      Test, NULL, setup, test_add_to_deny, teardown);
  g_test_add ("/contact-lists/add-to-deny/no-op",
      Test, NULL, setup, test_add_to_deny_no_op, teardown);
  g_test_add ("/contact-lists/remove-from-deny",
      Test, NULL, setup, test_remove_from_deny, teardown);
  g_test_add ("/contact-lists/remove-from-deny/no-op",
      Test, NULL, setup, test_remove_from_deny_no_op, teardown);

  g_test_add ("/contact-lists/request-blocked-contacts",
      Test, NULL, setup, test_request_blocked_contacts, teardown);
  g_test_add ("/contact-lists/request-blocked-contacts-before-connect",
      Test, NULL, setup_pre_connect,
      test_request_blocked_contacts_pre_connect, teardown_pre_connect);
  g_test_add ("/contact-lists/request-blocked-contacts-connect-failed",
      Test, "break-account-parameter", setup_pre_connect,
      test_request_blocked_contacts_connect_failed,
      teardown_pre_connect);
  g_test_add ("/contact-lists/block-contacts",
      Test, NULL, setup, test_block_contacts, teardown);
  g_test_add ("/contact-lists/block-contacts/no-op",
      Test, NULL, setup, test_block_contacts_no_op, teardown);
  g_test_add ("/contact-lists/unblock-contacts",
      Test, NULL, setup, test_unblock_contacts, teardown);
  g_test_add ("/contact-lists/unblock-contacts/no-op",
      Test, NULL, setup, test_unblock_contacts_no_op, teardown);

  g_test_add ("/contact-lists/download",
      Test, NULL, setup, test_download_contacts, teardown);

  return g_test_run ();
}
