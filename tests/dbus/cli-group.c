/* Test TpChannel's group code.
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <string.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/proxy-subclass.h>

#include "telepathy-glib/reentrants.h"

#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/util.h"

static GMainLoop *mainloop;
TpTestsSimpleConnection *service_conn;
TpConnection *conn;
TpHandleRepoIface *contact_repo;
TpHandle self_handle, h1, h2, h3;

gboolean expecting_group_members_changed = FALSE;
TpChannelGroupChangeReason expected_reason = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
gboolean expecting_invalidated = FALSE;

static void
group_members_changed_cb (TpChannel *chan_,
                          GArray *added,
                          GArray *removed,
                          GArray *local_pending,
                          GArray *remote_pending,
                          GHashTable *details,
                          gpointer user_data)
{
  guint reason = tp_asv_get_uint32 (details, "change-reason", NULL);

  DEBUG ("%u, %u, %u, %u, %u details", added->len, removed->len,
      local_pending->len, remote_pending->len, g_hash_table_size (details));

  MYASSERT (expecting_group_members_changed, "");
  g_assert_cmpuint (reason, ==, expected_reason);

  expecting_group_members_changed = FALSE;
}


static void
test_channel_proxy (TpTestsTextChannelGroup *service_chan,
                    TpChannel *chan)
{
  TpIntset *add, *rem, *expected_members;
  GHashTable *details;

  tp_tests_proxy_run_until_prepared (chan, NULL);

  g_signal_connect (chan, "group-members-changed",
      (GCallback) group_members_changed_cb, NULL);

  /* Add a couple of members. */
  add = tp_intset_new ();
  tp_intset_add (add, h1);
  tp_intset_add (add, h2);

  expecting_group_members_changed = TRUE;

  expected_reason++;

  details = tp_asv_new (
      "message", G_TYPE_STRING, "quantum tunnelling",
      "change-reason", G_TYPE_UINT, expected_reason,
      "actor", G_TYPE_UINT, 0,
      NULL);

  tp_group_mixin_change_members ((GObject *) service_chan,
      add, NULL, NULL, NULL, details);

  tp_clear_pointer (&details, g_hash_table_unref);

  /* Clear the queue to ensure that there aren't any more
   * MembersChanged signals waiting for us.
   */
  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  expected_members = add;
  MYASSERT (tp_intset_is_equal (expected_members,
      tp_channel_group_get_members (chan)), "");

  /* Add one, remove one. Check that the cache is properly updated. */
  add = tp_intset_new ();
  tp_intset_add (add, h3);
  rem = tp_intset_new ();
  tp_intset_add (rem, h1);

  expecting_group_members_changed = TRUE;
  expected_reason++;

  details = tp_asv_new (
      "message", G_TYPE_STRING, "goat",
      "change-reason", G_TYPE_UINT, expected_reason,
      "actor", G_TYPE_UINT, 0,
      NULL);

  tp_group_mixin_change_members ((GObject *) service_chan,
      add, rem, NULL, NULL, details);

  tp_clear_pointer (&details, g_hash_table_unref);
  tp_intset_destroy (add);
  tp_intset_destroy (rem);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  tp_intset_add (expected_members, h3);
  tp_intset_remove (expected_members, h1);

  MYASSERT (tp_intset_is_equal (expected_members,
      tp_channel_group_get_members (chan)), "");

  tp_intset_destroy (expected_members);
}

static void
channel_invalidated_cb (TpProxy *proxy,
                        guint domain,
                        gint code,
                        gchar *message,
                        gpointer user_data)
{
  DEBUG ("called");
  MYASSERT (expecting_invalidated, ": I've been EXPECTING YOU");
  expecting_invalidated = FALSE;
}

static void
run_membership_test (void)
{
  gchar *chan_path;
  TpTestsTextChannelGroup *service_chan;
  TpChannel *chan;
  GError *error = NULL;
  guint invalidated_id = 0;

  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (conn));
  service_chan = TP_TESTS_TEXT_CHANNEL_GROUP (
      tp_tests_object_new_static_class (
      TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
      "connection", service_conn,
      "object-path", chan_path,
      NULL));
  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);

  g_assert_no_error (error);

  expecting_invalidated = FALSE;
  invalidated_id = g_signal_connect (chan, "invalidated",
      (GCallback) channel_invalidated_cb, NULL);

  test_channel_proxy (service_chan, chan);

  g_signal_handler_disconnect (chan, invalidated_id);

  g_object_unref (chan);
  g_object_unref (service_chan);
  g_free (chan_path);
}

#define REMOVED_REASON TP_CHANNEL_GROUP_CHANGE_REASON_NO_ANSWER
#define REMOVED_KNOWN_ERROR_CODE TP_ERROR_CONNECTION_REFUSED
#define REMOVED_KNOWN_ERROR_STR TP_ERROR_STR_CONNECTION_REFUSED
#define REMOVED_UNKNOWN_ERROR "if.bob.dylan.were.hiding.at.the.bottom.of.a.well"
#define REMOVED_MESSAGE \
  "I'm just sittin' here, hidin' inside of a well, and I ain't comin' out!"

static void
check_invalidated_unknown_error_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  gboolean *invalidated = user_data;
  GError e = { domain, code, message };
  GError *error = &e;

  MYASSERT (!*invalidated, "");
  *invalidated = TRUE;

  /* Because we didn't understand the D-Bus error string, the Telepathy error
   * is derived from the Channel_Group_Change_Reason; since 0.11.5
   * it's remapped into the TP_ERROR domain if possible */
  g_assert_error (error, TP_ERROR, TP_ERROR_NO_ANSWER);
  MYASSERT (strstr (message, REMOVED_UNKNOWN_ERROR) != NULL, " (%s, %s)",
      message, REMOVED_UNKNOWN_ERROR);
  MYASSERT (strstr (message, REMOVED_MESSAGE) != NULL, " (%s, %s)", message,
      REMOVED_MESSAGE);
}


static void
check_removed_unknown_error_in_invalidated (void)
{
  gchar *chan_path;
  TpTestsTextChannelGroup *service_chan;
  TpChannel *chan;
  TpIntset *self_handle_singleton = tp_intset_new ();
  gboolean invalidated = FALSE;
  GError *error = NULL;
  GHashTable *details;

  chan_path = g_strdup_printf ("%s/Channel_1_6180339887",
      tp_proxy_get_object_path (conn));
  service_chan = TP_TESTS_TEXT_CHANNEL_GROUP (
      tp_tests_object_new_static_class (
      TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
      "connection", service_conn,
      "object-path", chan_path,
      NULL));
  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);

  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);
  DEBUG ("ready!");

  g_signal_connect (chan, "invalidated",
      (GCallback) check_invalidated_unknown_error_cb, &invalidated);

  details = tp_asv_new (
      "message", G_TYPE_STRING, "hello",
      "change-reason", G_TYPE_UINT, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      "actor", G_TYPE_UINT, 0,
      NULL);

  tp_intset_add (self_handle_singleton, self_handle);
  tp_group_mixin_change_members ((GObject *) service_chan,
      self_handle_singleton, NULL, NULL, NULL, details);

  tp_clear_pointer (&details, g_hash_table_unref);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  details = tp_asv_new (
      "message", G_TYPE_STRING, REMOVED_MESSAGE,
      "change-reason", G_TYPE_UINT, REMOVED_REASON,
      "error", G_TYPE_STRING, REMOVED_UNKNOWN_ERROR,
      NULL);

  tp_group_mixin_change_members ((GObject *) service_chan, NULL,
      self_handle_singleton, NULL, NULL, details);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  tp_cli_channel_call_close (chan, -1, NULL, NULL, NULL, NULL);

  tp_clear_pointer (&details, g_hash_table_unref);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  MYASSERT (invalidated, "");

  g_object_unref (chan);
  g_object_unref (service_chan);
  g_free (chan_path);
  tp_intset_destroy (self_handle_singleton);
}

static void
check_invalidated_known_error_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  gboolean *invalidated = user_data;
  GError e = { domain, code, message };
  GError *error = &e;

  MYASSERT (!*invalidated, "");
  *invalidated = TRUE;

  g_assert_error (error, TP_ERROR, REMOVED_KNOWN_ERROR_CODE);
  MYASSERT (strstr (message, REMOVED_KNOWN_ERROR_STR) == NULL, " (%s, %s)",
      message, REMOVED_KNOWN_ERROR_STR);
  MYASSERT (strstr (message, REMOVED_MESSAGE) != NULL, " (%s, %s)", message,
      REMOVED_MESSAGE);
}


static void
check_removed_known_error_in_invalidated (void)
{
  gchar *chan_path;
  TpTestsTextChannelGroup *service_chan;
  TpChannel *chan;
  TpIntset *self_handle_singleton = tp_intset_new ();
  GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  gboolean invalidated = FALSE;
  GError *error = NULL;

  chan_path = g_strdup_printf ("%s/Channel_1_6180339887",
      tp_proxy_get_object_path (conn));
  service_chan = TP_TESTS_TEXT_CHANNEL_GROUP (g_object_new (
      TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
      "connection", service_conn,
      "object-path", chan_path,
      NULL));
  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);

  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);
  DEBUG ("ready!");

  g_signal_connect (chan, "invalidated",
      (GCallback) check_invalidated_known_error_cb, &invalidated);

  details = tp_asv_new (
      "message", G_TYPE_STRING, "hello",
      "change-reason", G_TYPE_UINT, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
      "actor", G_TYPE_UINT, 0,
      NULL);

  tp_intset_add (self_handle_singleton, self_handle);
  tp_group_mixin_change_members ((GObject *) service_chan,
      self_handle_singleton, NULL, NULL, NULL, details);

  tp_clear_pointer (&details, g_hash_table_unref);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  details = tp_asv_new (
      "message", G_TYPE_STRING, REMOVED_MESSAGE,
      "change-reason", G_TYPE_UINT, REMOVED_REASON,
      "error", G_TYPE_STRING, REMOVED_KNOWN_ERROR_STR,
      NULL);

  tp_group_mixin_change_members ((GObject *) service_chan, NULL,
      self_handle_singleton, NULL, NULL, details);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  tp_cli_channel_call_close (chan, -1, NULL, NULL, NULL, NULL);

  tp_clear_pointer (&details, g_hash_table_unref);

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  MYASSERT (invalidated, "");

  g_object_unref (chan);
  g_object_unref (service_chan);
  g_free (chan_path);
  tp_intset_destroy (self_handle_singleton);
}

int
main (int argc,
      char **argv)
{
  TpBaseConnection *service_conn_as_base;
  GError *error = NULL;

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &conn);
  service_conn = TP_TESTS_SIMPLE_CONNECTION (service_conn_as_base);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  self_handle = tp_handle_ensure (contact_repo, "me@example.com", NULL, NULL);
  h1 = tp_handle_ensure (contact_repo, "h1", NULL, NULL);
  h2 = tp_handle_ensure (contact_repo, "h2", NULL, NULL);
  h3 = tp_handle_ensure (contact_repo, "h3", NULL, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  g_assert_no_error (error);

  run_membership_test ();
  check_removed_unknown_error_in_invalidated ();
  check_removed_known_error_in_invalidated ();

  tp_tests_connection_assert_disconnect_succeeds (conn);

  /* clean up */

  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);

  return 0;
}
