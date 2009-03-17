/* Test TpGroupMixin
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/util.h"

#define IDENTIFIER "them@example.org"

static GMainLoop *mainloop;
TestTextChannelGroup *service_chan;
TpChannel *chan = NULL;
TpHandleRepoIface *contact_repo;
TpHandle self_handle, camel, camel2;

typedef void (*diff_checker) (const GArray *added, const GArray *removed,
    const GArray *local_pending, const GArray *remote_pending,
    const GHashTable *details);

static gboolean expecting_members_changed = FALSE;
static gboolean expecting_members_changed_detailed = FALSE;
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
  expecting_members_changed_detailed = TRUE;

  expected_message = message;
  expected_actor = actor;
  expected_reason = reason;
  expected_diffs = check_diffs;
}

static gboolean
outstanding_signals (void)
{
  return (expecting_members_changed || expecting_members_changed_detailed);
}

static void
wait_for_outstanding_signals (void)
{
  if (outstanding_signals ())
    g_main_loop_run (mainloop);
}

static void
on_members_changed (TpChannel *proxy,
                    const gchar *arg_Message,
                    const GArray *arg_Added,
                    const GArray *arg_Removed,
                    const GArray *arg_Local_Pending,
                    const GArray *arg_Remote_Pending,
                    guint arg_Actor,
                    guint arg_Reason,
                    gpointer user_data,
                    GObject *weak_object)
{
  MYASSERT (expecting_members_changed, ": got unexpected MembersChanged");
  expecting_members_changed = FALSE;

  MYASSERT_SAME_STRING (arg_Message, expected_message);
  MYASSERT_SAME_UINT (arg_Actor, expected_actor);
  MYASSERT_SAME_UINT (arg_Reason, expected_reason);

  expected_diffs (arg_Added, arg_Removed, arg_Local_Pending,
      arg_Remote_Pending, NULL);

  if (!outstanding_signals ())
    g_main_loop_quit (mainloop);
}

static void
on_members_changed_detailed (TpChannel *proxy,
                             const GArray *arg_Added,
                             const GArray *arg_Removed,
                             const GArray *arg_Local_Pending,
                             const GArray *arg_Remote_Pending,
                             GHashTable *arg_Details,
                             gpointer user_data,
                             GObject *weak_object)
{
  const gchar *message;
  TpHandle actor;
  TpChannelGroupChangeReason reason;
  gboolean valid;

  MYASSERT (expecting_members_changed_detailed,
      ": got unexpected MembersChangedDetailed");
  expecting_members_changed_detailed = FALSE;

  message = tp_asv_get_string (arg_Details, "message");

  if (message == NULL)
    message = "";

  MYASSERT_SAME_STRING (message, expected_message);

  actor = tp_asv_get_uint32 (arg_Details, "actor", &valid);
  if (valid)
    {
      MYASSERT_SAME_UINT (actor, expected_actor);
    }
  else
    {
      MYASSERT_SAME_UINT (expected_actor, 0);
      MYASSERT (tp_asv_lookup (arg_Details, "actor") == NULL,
          ": wanted an actor, not an imposter");
    }

  reason = tp_asv_get_uint32 (arg_Details, "change-reason", &valid);
  if (valid)
    {
      MYASSERT_SAME_UINT (reason, expected_reason);
    }
  else
    {
      MYASSERT_SAME_UINT (expected_reason,
          TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
      MYASSERT (tp_asv_lookup (arg_Details, "reason") == NULL,
          ": utterly unreasonable");
    }

  expected_diffs (arg_Added, arg_Removed, arg_Local_Pending,
      arg_Remote_Pending, arg_Details);

  if (!outstanding_signals ())
    g_main_loop_quit (mainloop);
}

static void
check_initial_properties (void)
{
  GHashTable *props = NULL;
  GArray *members;
  TpHandle h;
  gboolean valid;
  GError *error = NULL;
  TpChannelGroupFlags flags;

  MYASSERT (tp_cli_dbus_properties_run_get_all (chan, -1,
      TP_IFACE_CHANNEL_INTERFACE_GROUP, &props, &error, NULL), "");
  test_assert_no_error (error);

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

  h = tp_asv_get_uint32 (props, "SelfHandle", &valid);
  MYASSERT (valid, ": SelfHandle property should be defined");

  flags = tp_asv_get_uint32 (props, "GroupFlags", &valid);
  MYASSERT (flags, ": GroupFlags property should be defined");
  MYASSERT_SAME_UINT (flags,
      TP_CHANNEL_GROUP_FLAG_PROPERTIES |
      TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED);

  g_hash_table_unref (props);
}

static void
details_contains_ids_for (const GHashTable *details,
                          TpHandle *hs)
{
  const GValue *member_ids_v;
  GHashTable *member_ids;
  const gchar *id;
  guint n = 0;
  TpHandle *h;

  if (details == NULL)
    return;

  member_ids_v = tp_asv_lookup (details, "member-ids");
  member_ids = g_value_get_boxed (member_ids_v);

  for (h = hs; *h != 0; h++)
    {
      n++;

      id = g_hash_table_lookup (member_ids, GUINT_TO_POINTER (*h));
      MYASSERT (id != NULL, ": id for %u in map", *h);
      MYASSERT_SAME_STRING (id, tp_handle_inspect (contact_repo, *h));
    }

  MYASSERT (g_hash_table_size (member_ids) == n, ": %u member IDs", n);
}

static void
self_added_to_lp (const GArray *added,
                  const GArray *removed,
                  const GArray *local_pending,
                  const GArray *remote_pending,
                  const GHashTable *details)
{
  TpHandle h;
  TpHandle hs[] = { self_handle, 0 };

  MYASSERT (added->len == 0, ": no new added to members");
  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
  MYASSERT (local_pending->len == 1, ": one local pending...");

  /* ...which is us */
  h = g_array_index (local_pending, TpHandle, 0);
  MYASSERT_SAME_UINT (h, self_handle);

  details_contains_ids_for (details, hs);
}

static void
self_added_to_members (const GArray *added,
                       const GArray *removed,
                       const GArray *local_pending,
                       const GArray *remote_pending,
                       const GHashTable *details)
{
  TpHandle h;
  TpHandle hs[] = { self_handle, 0 };

  MYASSERT (added->len == 1, ": one added");

  h = g_array_index (added, TpHandle, 0);
  MYASSERT_SAME_UINT (h, self_handle);

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
    TpIntSet *add_local_pending = tp_intset_new ();
    tp_intset_add (add_local_pending, self_handle);

    expect_signals ("HELLO THAR", 0, TP_CHANNEL_GROUP_CHANGE_REASON_INVITED,
        self_added_to_lp);
    tp_group_mixin_change_members ((GObject *) service_chan, "HELLO THAR", NULL,
        NULL, add_local_pending, NULL, 0,
        TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged and MembersChangedDetailed should have fired once");

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
    MYASSERT (tp_cli_channel_interface_group_run_add_members (chan, -1,
        contacts, "", &error, NULL), "");
    test_assert_no_error (error);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged and MembersChangedDetailed should have fired once");

    g_array_free (contacts, TRUE);
  }
}

static void
camel_added (const GArray *added,
             const GArray *removed,
             const GArray *local_pending,
             const GArray *remote_pending,
             const GHashTable *details)
{
  TpHandle h;
  TpHandle hs[] = { camel, 0 };

  MYASSERT (added->len == 1, ": one added");

  h = g_array_index (added, TpHandle, 0);
  MYASSERT_SAME_UINT (h, camel);

  details_contains_ids_for (details, hs);

  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
}

static void
camel2_added (const GArray *added,
              const GArray *removed,
              const GArray *local_pending,
              const GArray *remote_pending,
              const GHashTable *details)
{
  TpHandle h;
  /* camel is the actor */
  TpHandle hs[] = { camel, camel2, 0 };

  MYASSERT (added->len == 1, ": one added");

  h = g_array_index (added, TpHandle, 0);
  MYASSERT_SAME_UINT (h, camel2);

  details_contains_ids_for (details, hs);

  MYASSERT (removed->len == 0, ": no-one removed");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");
}

static void
camel_removed (const GArray *added,
               const GArray *removed,
               const GArray *local_pending,
               const GArray *remote_pending,
               const GHashTable *details)
{
  TpHandle h;
  /* camel2 is the actor. camel shouldn't be in the ids, because they were
   * removed and the spec says that you can leave those out, and we want
   * tp-glib's automatic construction of member-ids to work in the #ubuntu
   * case.
   */
  TpHandle hs[] = { camel2, 0 };

  MYASSERT (removed->len == 1, ": one removed");

  h = g_array_index (removed, TpHandle, 0);
  MYASSERT_SAME_UINT (h, camel);

  MYASSERT (added->len == 0, ": no-one added");
  MYASSERT (local_pending->len == 0, ": no new local pending");
  MYASSERT (remote_pending->len == 0, ": no new remote pending");

  details_contains_ids_for (details, hs);
}

static void
in_the_desert (void)
{
  camel  = tp_handle_ensure (contact_repo, "camel", NULL, NULL);
  camel2 = tp_handle_ensure (contact_repo, "camel2", NULL, NULL);

  /* A camel is approaching */
  {
    TpIntSet *add = tp_intset_new ();

    tp_intset_add (add, camel);
    expect_signals ("", camel, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        camel_added);
    tp_group_mixin_change_members ((GObject *) service_chan, NULL, add, NULL,
        NULL, NULL, camel, TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged and MembersChangedDetailed should have fired once");

    tp_intset_destroy (add);
  }

  /* A second camel is approaching (invited by the first camel) */
  {
    TpIntSet *add = tp_intset_new ();
    GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal,
        NULL, (GDestroyNotify) tp_g_value_slice_free);

    tp_intset_add (add, camel2);

    g_hash_table_insert (details, "actor", tp_g_value_slice_new_uint (camel));

    expect_signals ("", camel, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
        camel2_added);
    tp_group_mixin_change_members_detailed ((GObject *) service_chan, add,
        NULL, NULL, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged and MembersChangedDetailed should have fired once");

    tp_intset_destroy (add);
    g_hash_table_unref (details);
  }

  {
    TpIntSet *del = tp_intset_new ();
    GHashTable *details = g_hash_table_new_full (g_str_hash, g_str_equal,
        NULL, (GDestroyNotify) tp_g_value_slice_free);

    tp_intset_add (del, camel);

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
    tp_group_mixin_change_members_detailed ((GObject *) service_chan, NULL,
        del, NULL, NULL, details);
    wait_for_outstanding_signals ();
    MYASSERT (!outstanding_signals (),
        ": MembersChanged and MembersChangedDetailed should have fired once");

    tp_intset_destroy (del);
    g_hash_table_unref (details);
  }

  /* We and the second camel should be left in the channel */
  {
    const TpIntSet *members = tp_channel_group_get_members (chan);
    GArray *service_members;
    TpHandle a, b;

    MYASSERT_SAME_UINT (tp_intset_size (members), 2);
    MYASSERT (tp_intset_is_member (members, self_handle), "");
    MYASSERT (tp_intset_is_member (members, camel2), ": what a pity");

    /* And let's check that the group mixin agrees, in case that's just the
     * client binding being wrong.
     */
    tp_group_mixin_get_members ((GObject *) service_chan, &service_members,
        NULL);
    MYASSERT_SAME_UINT (service_members->len, 2);
    a = g_array_index (service_members, TpHandle, 0);
    b = g_array_index (service_members, TpHandle, 1);
    MYASSERT (a != b, "");
    MYASSERT (a == self_handle || b == self_handle, "");
    MYASSERT (a == camel2 || b == camel2, "");

    g_array_free (service_members, TRUE);
  }

  tp_handle_unref (contact_repo, camel);
  tp_handle_unref (contact_repo, camel2);
}

static void
test_group_mixin (void)
{
  GError *error = NULL;

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  test_assert_no_error (error);

  MYASSERT (tp_proxy_has_interface (chan, TP_IFACE_CHANNEL_INTERFACE_GROUP),
      "");

  tp_cli_channel_interface_group_connect_to_members_changed (chan,
      on_members_changed, NULL, NULL, NULL, NULL);
  tp_cli_channel_interface_group_connect_to_members_changed_detailed (chan,
      on_members_changed_detailed, NULL, NULL, NULL, NULL);

  check_initial_properties ();

  check_incoming_invitation ();

  in_the_desert ();
}

int
main (int argc,
      char **argv)
{
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;

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

  chan_path = g_strdup_printf ("%s/Channel", conn_path);

  service_chan = TEST_TEXT_CHANNEL_GROUP (g_object_new (
        TEST_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", chan_path,
        "detailed", TRUE,
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  test_assert_no_error (error);

  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);
  test_assert_no_error (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  test_assert_no_error (error);

  test_group_mixin ();

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  test_assert_no_error (error);

  /* clean up */

  g_object_unref (chan);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);
  g_object_unref (service_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);

  return 0;
}
