/* Test TpChannel's group code.
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <string.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/util.h"

static GMainLoop *mainloop;
SimpleConnection *service_conn;
gchar *conn_path;
TpConnection *conn;
TpHandleRepoIface *contact_repo;
TpHandle self_handle, h1, h2, h3;

gboolean expecting_group_members_changed = FALSE;
gboolean expecting_group_members_changed_detailed = FALSE;
TpChannelGroupChangeReason expected_reason = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
gboolean expecting_invalidated = FALSE;

static void
group_members_changed_cb (TpChannel *chan_,
                          gchar *message,
                          GArray *added,
                          GArray *removed,
                          GArray *local_pending,
                          GArray *remote_pending,
                          guint actor,
                          guint reason,
                          gpointer user_data)
{
  DEBUG ("\"%s\", %u, %u, %u, %u, %u, %u", message, added->len, removed->len,
      local_pending->len, remote_pending->len, actor, reason);

  MYASSERT (expecting_group_members_changed, "");
  MYASSERT_SAME_UINT (reason, expected_reason);

  expecting_group_members_changed = FALSE;
}

static void
group_members_changed_detailed_cb (TpChannel *chan_,
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

  MYASSERT (expecting_group_members_changed_detailed, "");
  MYASSERT_SAME_UINT (reason, expected_reason);

  expecting_group_members_changed_detailed = FALSE;
}


static void
test_channel_proxy (TestTextChannelGroup *service_chan,
                    TpChannel *chan,
                    gboolean detailed,
                    gboolean properties)
{
  TpIntSet *add, *rem, *expected_members;
  GArray *arr, *yarr;
  GError *error = NULL;
  TpChannelGroupFlags flags;
  gboolean has_detailed_flag, has_properties_flag;

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  test_assert_no_error (error);

  /* We want to ensure that each of these signals fires exactly once per
   * change.  The channel emits both MembersChanged and MembersChangedDetailed,
   * but TpChannel should only be reacting to one of them, based on whether the
   * Members_Changed_Detailed flag is set.  So, each signal's handler has a
   * corresponding "expected" flag, which it asserts on then sets back to FALSE.
   */
  g_signal_connect (chan, "group-members-changed",
      (GCallback) group_members_changed_cb, NULL);
  g_signal_connect (chan, "group-members-changed-detailed",
      (GCallback) group_members_changed_detailed_cb, NULL);

  flags = tp_channel_group_get_flags (chan);
  has_detailed_flag = !!(flags & TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED);
  MYASSERT (detailed == has_detailed_flag,
      ": expected Members_Changed_Detailed to be %sset",
      (detailed ? "" : "un"));
  has_properties_flag = !!(flags & TP_CHANNEL_GROUP_FLAG_PROPERTIES);
  MYASSERT (properties == has_properties_flag,
      ": expected Properties to be %sset",
      (detailed ? "" : "un"));

  /* Add a couple of members. */
  add = tp_intset_new ();
  tp_intset_add (add, h1);
  tp_intset_add (add, h2);

  expecting_group_members_changed = TRUE;
  expecting_group_members_changed_detailed = TRUE;
  expected_reason++;
  tp_group_mixin_change_members ((GObject *) service_chan,
      "quantum tunnelling", add, NULL, NULL, NULL, 0, expected_reason);

  /* Clear the queue to ensure that there aren't any more
   * MembersChanged[Detailed] signals waiting for us.
   */
  test_connection_run_until_dbus_queue_processed (conn);

  expected_members = add;
  MYASSERT (tp_intset_is_equal (expected_members,
      tp_channel_group_get_members (chan)), "");

  /* Add one, remove one. Check that the cache is properly updated. */
  add = tp_intset_new ();
  tp_intset_add (add, h3);
  rem = tp_intset_new ();
  tp_intset_add (rem, h1);

  expecting_group_members_changed = TRUE;
  expecting_group_members_changed_detailed = TRUE;
  expected_reason++;
  tp_group_mixin_change_members ((GObject *) service_chan,
      "goat", add, rem, NULL, NULL, 0, expected_reason);
  tp_intset_destroy (add);
  tp_intset_destroy (rem);

  test_connection_run_until_dbus_queue_processed (conn);

  tp_intset_add (expected_members, h3);
  tp_intset_remove (expected_members, h1);

  MYASSERT (tp_intset_is_equal (expected_members,
      tp_channel_group_get_members (chan)), "");

  /* Now, emit a spurious instance of whichever DBus signal the proxy should
   * not be listening to to check it's really not listening to it.  If the
   * TpChannel is reacting to the wrong DBus signal, it'll trigger an assertion
   * in the GObject signal handlers.
   */
  yarr = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  arr = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
  g_array_insert_val (arr, 0, h1);

  expecting_group_members_changed = FALSE;
  expecting_group_members_changed_detailed = FALSE;

  if (detailed)
    {
      tp_svc_channel_interface_group_emit_members_changed (service_chan,
          "whee", arr, yarr, yarr, yarr, 0,
          TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
    }
  else
    {
      GHashTable *details = g_hash_table_new (NULL, NULL);

      tp_svc_channel_interface_group_emit_members_changed_detailed (
          service_chan, arr, yarr, yarr, yarr, details);
      g_hash_table_unref (details);
    }

  g_array_free (yarr, TRUE);
  g_array_free (arr, TRUE);

  test_connection_run_until_dbus_queue_processed (conn);

  /* And, the cache of group members should be unaltered, since the signal the
   * TpChannel cares about was not fired.
   */
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
  MYASSERT_SAME_UINT (domain, TP_DBUS_ERRORS);
  MYASSERT (code == TP_DBUS_ERROR_INCONSISTENT, ": was %i", code);

  expecting_invalidated = FALSE;
}

static void
test_invalidated_on_illegal_change (TestTextChannelGroup *serv_chan,
                                    TpChannel *chan,
                                    gboolean detailed,
                                    gboolean properties)
{
  TpChannelGroupFlags add, del;

  DEBUG ("This channel has detailed %sset and properties %sset",
    (detailed ? "" : "un"), (properties ? "" : "un"));

  /* If we re-set or -unset the flags the channel already has, then the
   * TpChannel shouldn't care. This emits the signal directly rather than going
   * through the mixin, because the mixin helpfully optimizes out the spurious
   * change notification.
   */
  expecting_invalidated = FALSE;
  add = del = 0;
  *(detailed ? &add : &del) |= TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED;
  *(properties ? &add : &del) |= TP_CHANNEL_GROUP_FLAG_PROPERTIES;
  DEBUG ("Changing flags: add %u, del %u", add, del);
  tp_svc_channel_interface_group_emit_group_flags_changed (serv_chan, add, del);
  test_connection_run_until_dbus_queue_processed (conn);

  /* Now, let's flip the Detailed and Properties flags, and check that the
   * proxy gets invalidated due to inconsistency on the part of the service.
   */
  expecting_invalidated = TRUE;
  DEBUG ("Changing flags: add %u, del %u", del, add);
  tp_group_mixin_change_flags ((GObject *) serv_chan, del, add);
  test_connection_run_until_dbus_queue_processed (conn);

  MYASSERT (!expecting_invalidated, ": invalidated should have fired");
}

static void
run_membership_test (guint channel_number,
                     gboolean detailed,
                     gboolean properties)
{
  gchar *chan_path;
  TestTextChannelGroup *service_chan;
  TpChannel *chan;
  GError *error = NULL;

  chan_path = g_strdup_printf ("%s/Channel%u", conn_path, channel_number);
  service_chan = TEST_TEXT_CHANNEL_GROUP (g_object_new (
      TEST_TYPE_TEXT_CHANNEL_GROUP,
      "connection", service_conn,
      "object-path", chan_path,
      "detailed", detailed,
      "properties", properties,
      NULL));
  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);

  test_assert_no_error (error);

  expecting_invalidated = FALSE;
  g_signal_connect (chan, "invalidated", (GCallback) channel_invalidated_cb,
      NULL);

  test_channel_proxy (service_chan, chan, detailed, properties);
  test_invalidated_on_illegal_change (service_chan, chan, detailed, properties);

  g_object_unref (chan);
  g_object_unref (service_chan);
  g_free (chan_path);
}

static void
run_membership_tests (void)
{
  /* Run a set of sanity checks on a series of channels, with all 4
   * combinations of states of the of the Members_Changed_Detailed and
   * Properties group flags.
   */
  run_membership_test (1, FALSE, FALSE);
  run_membership_test (2, FALSE, TRUE);
  run_membership_test (3, TRUE, FALSE);
  run_membership_test (4, TRUE, TRUE);
}

#define REMOVED_REASON TP_CHANNEL_GROUP_CHANGE_REASON_NO_ANSWER
#define REMOVED_ERROR "if.bob.dylan.were.hiding.at.the.bottom.of.a.well"
#define REMOVED_MESSAGE \
  "I'm just sittin' here, hidin' inside of a well, and I ain't comin' out!"

static void
check_invalidated_cb (TpProxy *proxy,
                      guint domain,
                      gint code,
                      gchar *message,
                      gpointer user_data)
{
  gboolean *invalidated = user_data;

  MYASSERT (!*invalidated, "");
  *invalidated = TRUE;

  MYASSERT (domain == TP_ERRORS_REMOVED_FROM_GROUP, ": %u (%s) != %u (%s)",
      domain, g_quark_to_string (domain),
      TP_ERRORS_REMOVED_FROM_GROUP, g_quark_to_string
      (TP_ERRORS_REMOVED_FROM_GROUP));
  MYASSERT (code == REMOVED_REASON, ": %i != %i", code, REMOVED_REASON);
  MYASSERT (strstr (message, REMOVED_ERROR) != NULL, " (%s, %s)", message,
      REMOVED_ERROR);
  MYASSERT (strstr (message, REMOVED_MESSAGE) != NULL, " (%s, %s)", message,
      REMOVED_MESSAGE);
}


static void
check_removed_error_in_invalidated (void)
{
  gchar *chan_path;
  TestTextChannelGroup *service_chan;
  TpChannel *chan;
  TpIntSet *self_handle_singleton = tp_intset_new ();
  GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  gboolean invalidated = FALSE;
  GError *error = NULL;

  chan_path = g_strdup_printf ("%s/Channel_1_6180339887", conn_path);
  service_chan = TEST_TEXT_CHANNEL_GROUP (g_object_new (
      TEST_TYPE_TEXT_CHANNEL_GROUP,
      "connection", service_conn,
      "object-path", chan_path,
      "detailed", TRUE,
      "properties", TRUE,
      NULL));
  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);

  test_assert_no_error (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  test_assert_no_error (error);
  DEBUG ("ready!");

  g_signal_connect (chan, "invalidated", (GCallback) check_invalidated_cb,
      &invalidated);

  tp_intset_add (self_handle_singleton, self_handle);
  tp_group_mixin_change_members ((GObject *) service_chan, "hello",
      self_handle_singleton, NULL, NULL, NULL, 0,
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  test_connection_run_until_dbus_queue_processed (conn);

  g_hash_table_insert (details, "change-reason",
      tp_g_value_slice_new_uint (REMOVED_REASON));

  g_hash_table_insert (details, "message",
      tp_g_value_slice_new_static_string (REMOVED_MESSAGE));

  g_hash_table_insert (details, "error",
      tp_g_value_slice_new_static_string (REMOVED_ERROR));

  tp_group_mixin_change_members_detailed ((GObject *) service_chan, NULL,
      self_handle_singleton, NULL, NULL, details);

  test_connection_run_until_dbus_queue_processed (conn);

  tp_cli_channel_call_close (chan, -1, NULL, NULL, NULL, NULL);

  g_hash_table_unref (details);

  test_connection_run_until_dbus_queue_processed (conn);

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
  TpDBusDaemon *dbus;
  GError *error = NULL;
  gchar *name;

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_dbus_daemon_new (tp_get_bus ());

  service_conn = SIMPLE_CONNECTION (g_object_new (SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  test_assert_no_error (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  self_handle = tp_handle_ensure (contact_repo, "me@example.com", NULL, NULL);
  h1 = tp_handle_ensure (contact_repo, "h1", NULL, NULL);
  h2 = tp_handle_ensure (contact_repo, "h2", NULL, NULL);
  h3 = tp_handle_ensure (contact_repo, "h3", NULL, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  test_assert_no_error (error);

  run_membership_tests ();
  check_removed_error_in_invalidated ();

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  test_assert_no_error (error);

  /* clean up */

  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);

  return 0;
}
