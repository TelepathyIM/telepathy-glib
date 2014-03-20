/* Test TpGroupMixin
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

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

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/util.h"

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
}

#define IDENTIFIER "them@example.org"

static GMainLoop *mainloop;
TpTestsTextChannelGroup *service_chan;
TpChannel *chan = NULL;
TpHandleRepoIface *contact_repo;
TpHandle self_handle, camel, camel2;

typedef void (*diff_checker) (const GPtrArray *added, const GPtrArray *removed,
    const GPtrArray *local_pending, const GPtrArray *remote_pending,
    const GHashTable *details);

static gboolean expecting_members_changed = FALSE;
static const gchar *expected_message;
static TpHandle expected_actor;
static TpChannelGroupChangeReason expected_reason;
static diff_checker expected_diffs;

static void
expect_signals (const gchar *message,
                TpHandle actor,
                TpChannelGroupChangeReason reason,
                diff_checker check_diffs)
{
  expecting_members_changed = TRUE;

  expected_message = message;
  expected_actor = actor;
  expected_reason = reason;
  expected_diffs = check_diffs;
}

static gboolean
outstanding_signals (void)
{
  return expecting_members_changed;
}

static void
wait_for_outstanding_signals (void)
{
  if (outstanding_signals ())
    g_main_loop_run (mainloop);
}

static void
on_members_changed (TpChannel *proxy,
                    GPtrArray *added,
                    GPtrArray *removed,
                    GPtrArray *local_pending,
                    GPtrArray *remote_pending,
                    TpContact *actor,
                    GHashTable *details,
                    gpointer user_data)
{
  const gchar *message;
  TpChannelGroupChangeReason reason;
  gboolean valid;

  MYASSERT (expecting_members_changed,
      ": got unexpected MembersChanged");
  expecting_members_changed = FALSE;

  message = tp_asv_get_string (details, "message");

  if (message == NULL)
    message = "";

  g_assert_cmpstr (message, ==, expected_message);

  if (actor != NULL)
    {
      g_assert_cmpuint (tp_contact_get_handle (actor), ==, expected_actor);
    }
  else
    {
      g_assert_cmpuint (expected_actor, ==, 0);
    }

  reason = tp_asv_get_uint32 (details, "change-reason", &valid);
  if (valid)
    {
      g_assert_cmpuint (reason, ==, expected_reason);
    }
  else
    {
      g_assert_cmpuint (expected_reason, ==,
          TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
      MYASSERT (tp_asv_lookup (details, "reason") == NULL,
          ": utterly unreasonable");
    }

  expected_diffs (added, removed, local_pending,
      remote_pending, details);

  if (!outstanding_signals ())
    g_main_loop_quit (mainloop);
}

static void
check_initial_properties (void)
{
  GHashTable *props = NULL;
  GArray *members;
  gboolean valid;
  GError *error = NULL;
  TpChannelGroupFlags flags;

  MYASSERT (tp_cli_dbus_properties_run_get_all (chan, -1,
      TP_IFACE_CHANNEL_INTERFACE_GROUP1, &props, &error, NULL), "");
  g_assert_no_error (error);

  members = tp_asv_get_boxed (props, "Members", DBUS_TYPE_G_UINT_ARRAY);
  MYASSERT (members != NULL, ": Members should be defined"); \
  MYASSERT (members->len == 0, ": Members should be empty initally");

  members = tp_asv_get_boxed (props, "RemotePendingMembers",
      DBUS_TYPE_G_UINT_ARRAY);
  MYASSERT (members != NULL, ": RemotePendingMembers should be defined"); \
  MYASSERT (members->len == 0, ": RemotePendingMembers should be empty initally");

  members = tp_asv_get_boxed (props, "LocalPendingMembers",
      TP_ARRAY_TYPE_LOCAL_PENDING_INFO_LIST);
  MYASSERT (members != NULL, ": LocalPendingMembers should be defined"); \
  MYASSERT (members->len == 0, ": LocalPendingMembers should be empty initally");

  tp_asv_get_uint32 (props, "SelfHandle", &valid);
  MYASSERT (valid, ": SelfHandle property should be defined");

  flags = tp_asv_get_uint32 (props, "GroupFlags", &valid);
  MYASSERT (flags, ": GroupFlags property should be defined");
  g_assert_cmpuint (flags, ==, TP_CHANNEL_GROUP_FLAG_CAN_ADD);

  g_hash_table_unref (props);
}

static void
details_contains_ids_for (const GHashTable *details,
                          TpHandle *hs)
{
  GHashTable *contact_ids;
  const gchar *id;
  guint n = 0;
  TpHandle *h;

  if (details == NULL)
    return;

  contact_ids = tp_asv_get_boxed (details, "contact-ids",
      TP_HASH_TYPE_HANDLE_IDENTIFIER_MAP);
  g_assert (contact_ids != NULL);

  for (h = hs; *h != 0; h++)
    {
      n++;

      id = g_hash_table_lookup (contact_ids, GUINT_TO_POINTER (*h));
      MYASSERT (id != NULL, ": id for %u in map", *h);
      g_assert_cmpstr (id, ==, tp_handle_inspect (contact_repo, *h));
    }

  MYASSERT (g_hash_table_size (contact_ids) == n, ": %u contact IDs", n);
}

static void
self_added_to_lp (const GPtrArray *added,
                  const GPtrArray *removed,
                  const GPtrArray *local_pending,
                  const GPtrArray *remote_pending,
                  const GHashTable *details)
{
  TpContact *c;
  TpHandle hs[] = { self_handle, 0 };

  MYASSERT (added->len == 0, ": no new added to members");
  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
  MYASSERT (local_pending->len == 1, ": one local pending...");

  /* ...which is us */
  c = g_ptr_array_index (local_pending, 0);
  g_assert_cmpuint (tp_contact_get_handle (c), ==, self_handle);

  details_contains_ids_for (details, hs);
}

static void
self_added_to_members (const GPtrArray *added,
                       const GPtrArray *removed,
                       const GPtrArray *local_pending,
                       const GPtrArray *remote_pending,
                       const GHashTable *details)
{
  TpContact *c;
  TpHandle hs[] = { self_handle, 0 };

  MYASSERT (added->len == 1, ": one added");

  c = g_ptr_array_index (added, 0);
  g_assert_cmpuint (tp_contact_get_handle (c), ==, self_handle);

  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");

  details_contains_ids_for (details, hs);
}

static void
check_incoming_invitation (void)
{
  GError *error = NULL;

  /* We get an invitation to the channel */
  {
    TpIntset *add_local_pending = tp_intset_new ();
    GHashTable *details = tp_asv_new (
        "message", G_TYPE_STRING, "HELLO THAR",
        "actor", G_TYPE_UINT, 0,
        "change-reason", G_TYPE_UINT, TP_CHANNEL_GROUP_CHANGE_REASON_INVITED,
        NULL);
    tp_intset_add (add_local_pending, self_handle);

    expect_signals ("HELLO THAR", 0, TP_CHANNEL_GROUP_CHANGE_REASON_INVITED,
        self_added_to_lp);
    tp_group_mixin_change_members ((GObject *) service_chan, NULL,
        NULL, add_local_pending, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged should have fired once");

    g_hash_table_unref (details);
    tp_intset_destroy (add_local_pending);
  }

  /* We accept the invitation; even though the channel lacks CanAdd we should
   * be able to move someone from local pending to members by calling Add().
   */
  {
    GArray *contacts = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
    g_array_append_val (contacts, self_handle);

    expect_signals ("", self_handle, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        self_added_to_members);
    MYASSERT (tp_cli_channel_interface_group1_run_add_members (chan, -1,
        contacts, "", &error, NULL), "");
    g_assert_no_error (error);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged should have fired once");

    g_array_unref (contacts);
  }
}

static void
camel_added (const GPtrArray *added,
             const GPtrArray *removed,
             const GPtrArray *local_pending,
             const GPtrArray *remote_pending,
             const GHashTable *details)
{
  TpContact *c;
  TpHandle hs[] = { camel, 0 };

  MYASSERT (added->len == 1, ": one added");

  c = g_ptr_array_index (added, 0);
  g_assert_cmpuint (tp_contact_get_handle (c), ==, camel);

  details_contains_ids_for (details, hs);

  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
}

static void
camel2_added (const GPtrArray *added,
              const GPtrArray *removed,
              const GPtrArray *local_pending,
              const GPtrArray *remote_pending,
              const GHashTable *details)
{
  TpContact *c;
  /* camel is the actor */
  TpHandle hs[] = { camel, camel2, 0 };

  MYASSERT (added->len == 1, ": one added");

  c = g_ptr_array_index (added, 0);
  g_assert_cmpuint (tp_contact_get_handle (c), ==, camel2);

  details_contains_ids_for (details, hs);

  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
}

static void
camel_removed (const GPtrArray *added,
               const GPtrArray *removed,
               const GPtrArray *local_pending,
               const GPtrArray *remote_pending,
               const GHashTable *details)
{
  TpContact *c;
  /* camel2 is the actor. camel shouldn't be in the ids, because they were
   * removed and the spec says that you can leave those out, and we want
   * tp-glib's automatic construction of contact-ids to work in the #ubuntu
   * case.
   */
  TpHandle hs[] = { camel2, 0 };

  MYASSERT (removed->len == 1, ": one removed");

  c = g_ptr_array_index (removed, 0);
  g_assert_cmpuint (tp_contact_get_handle (c), ==, camel);

  MYASSERT (added->len == 0, ": no-one added");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");

  details_contains_ids_for (details, hs);
}

static void
in_the_desert (void)
{
  TpIntset *expected_members;

  expected_members = tp_intset_new ();
  tp_intset_add (expected_members, self_handle);

  camel  = tp_handle_ensure (contact_repo, "camel", NULL, NULL);
  camel2 = tp_handle_ensure (contact_repo, "camel2", NULL, NULL);

  /* A camel is approaching */
  {
    TpIntset *add = tp_intset_new ();
    GHashTable *details = tp_asv_new (
        "message", G_TYPE_STRING, "",
        "actor", G_TYPE_UINT, camel,
        "change-reason", G_TYPE_UINT, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        NULL);

    tp_intset_add (add, camel);
    tp_intset_add (expected_members, camel);
    expect_signals ("", camel, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        camel_added);
    tp_group_mixin_change_members ((GObject *) service_chan, add, NULL,
        NULL, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged should have fired once");

    tp_intset_destroy (add);
  }

  /* A second camel is approaching (invited by the first camel) */
  {
    TpIntset *add = tp_intset_new ();
    GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal,
        NULL, (GDestroyNotify) tp_g_value_slice_free);

    tp_intset_add (add, camel2);
    tp_intset_add (expected_members, camel2);

    g_hash_table_insert (details, "actor", tp_g_value_slice_new_uint (camel));

    expect_signals ("", camel, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        camel2_added);
    tp_group_mixin_change_members ((GObject *) service_chan, add,
        NULL, NULL, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged should have fired once");

    tp_intset_destroy (add);
    g_hash_table_unref (details);
  }

  {
    TpIntset *del = tp_intset_new ();
    GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal,
        NULL, (GDestroyNotify) tp_g_value_slice_free);

    tp_intset_add (del, camel);
    tp_intset_remove (expected_members, camel);

    g_hash_table_insert (details, "actor", tp_g_value_slice_new_uint (camel2));

    /* It turns out that spitting was not included in the GroupChangeReason
     * enum.
     */
    g_hash_table_insert (details, "error",
        tp_g_value_slice_new_static_string ("le.mac.Spat"));
    g_hash_table_insert (details, "saliva-consistency",
        tp_g_value_slice_new_static_string ("fluid"));

    /* Kicking is the closest we have to this .. unsavory act. */
    g_hash_table_insert (details, "change-reason",
        tp_g_value_slice_new_uint (TP_CHANNEL_GROUP_CHANGE_REASON_KICKED));
    g_hash_table_insert (details, "message",
        tp_g_value_slice_new_static_string ("*ptooey*"));

    /* Check that all the right information was extracted from the dict. */
    expect_signals ("*ptooey*", camel2,
        TP_CHANNEL_GROUP_CHANGE_REASON_KICKED, camel_removed);
    tp_group_mixin_change_members ((GObject *) service_chan, NULL,
        del, NULL, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged should have fired once");

    tp_intset_destroy (del);
    g_hash_table_unref (details);
  }

  /* We and the second camel should be left in the channel */
  {
    GArray *service_members;
    TpIntset *service_members_intset;

    tp_tests_channel_assert_expect_members (chan, expected_members);

    /* And let's check that the group mixin agrees, in case that's just the
     * client binding being wrong.
     */
    tp_group_mixin_get_members ((GObject *) service_chan, &service_members,
        NULL);
    service_members_intset = tp_intset_from_array (service_members);
    g_assert (tp_intset_is_equal (service_members_intset, expected_members));

    g_array_unref (service_members);
    tp_intset_destroy (service_members_intset);
  }

  tp_intset_destroy (expected_members);
}

static void
test_group_mixin (void)
{
  GQuark features[] = { TP_CHANNEL_FEATURE_GROUP, 0 };
  tp_tests_proxy_run_until_prepared (chan, features);

  MYASSERT (tp_proxy_has_interface (chan, TP_IFACE_CHANNEL_INTERFACE_GROUP1),
      "");

  g_signal_connect (chan, "group-members-changed",
      G_CALLBACK (on_members_changed), NULL);

  check_initial_properties ();

  check_incoming_invitation ();

  in_the_desert ();
}

static void
test (Fixture *f,
    gconstpointer data)
{
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *conn;
  GError *error = NULL;
  gchar *chan_path;
  GTestDBus *test_dbus;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_dbus_unset ();
  test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (test_dbus);

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &conn);
  service_conn = TP_TESTS_SIMPLE_CONNECTION (service_conn_as_base);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_ENTITY_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");
  self_handle = tp_handle_ensure (contact_repo, "me@example.com", NULL, NULL);

  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (conn));

  service_chan = TP_TESTS_TEXT_CHANNEL_GROUP (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", chan_path,
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  g_assert_no_error (error);

  chan = tp_tests_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  test_group_mixin ();

  tp_tests_connection_assert_disconnect_succeeds (conn);

  /* clean up */

  g_object_unref (chan);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);
  g_object_unref (service_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (chan_path);

  g_test_dbus_down (test_dbus);
  tp_tests_assert_last_unref (&test_dbus);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/group-mixin", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
