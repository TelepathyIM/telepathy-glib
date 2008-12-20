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

static int fail = 0;
static GMainLoop *mainloop;
static guint expecting_members_changed = 0;

static void
myassert_failed (void)
{
  fail = 1;
}

static gboolean
outstanding_signals (void)
{
  return expecting_members_changed > 0;
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
  MYASSERT (expecting_members_changed > 0, "got unexpected MembersChanged");

  expecting_members_changed--;

  if (!outstanding_signals ())
    g_main_loop_quit (mainloop);
}

static void
check_initial_properties (TpChannel *chan)
{
  GHashTable *props = NULL;
  GArray *members;
  TpHandle h;
  gboolean valid;
  GError *error = NULL;
  TpChannelGroupFlags flags;

  MYASSERT (tp_cli_dbus_properties_run_get_all (chan, -1,
      TP_IFACE_CHANNEL_INTERFACE_GROUP, &props, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

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
  MYASSERT_SAME_UINT (flags, TP_CHANNEL_GROUP_FLAG_PROPERTIES);

  g_hash_table_unref (props);
}

static void
check_incoming_invitation (TestTextChannelGroup *service_chan,
                           TpChannel *chan)
{
  GError *error = NULL;

  /* We get an invitation to the channel */
  {
    TpIntSet *add_local_pending = tp_intset_new ();
    tp_intset_add (add_local_pending, service_chan->conn->self_handle);

    expecting_members_changed = 1;
    tp_group_mixin_change_members ((GObject *) service_chan, "HELLO THAR", NULL,
        NULL, add_local_pending, NULL, 0,
        TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
    wait_for_outstanding_signals ();
    MYASSERT (expecting_members_changed == 0,
        ": MembersChanged should have fired once");

    tp_intset_destroy (add_local_pending);
  }

  /* We accept the invitation; even though the channel lacks CanAdd we should
   * be able to move someone from local pending to members by calling Add().
   */
  {
    GArray *contacts = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
    g_array_append_val (contacts, service_chan->conn->self_handle);

    expecting_members_changed = 1;
    MYASSERT (tp_cli_channel_interface_group_run_add_members (chan, -1,
        contacts, "", &error, NULL), "");
    MYASSERT_NO_ERROR (error);
      wait_for_outstanding_signals ();
    MYASSERT (expecting_members_changed == 0,
        ": MembersChanged should have fired once");

    g_array_free (contacts, TRUE);
  }
}

static void
test_group_mixin (TestTextChannelGroup *service_chan,
    TpChannel *chan)
{
  GError *error = NULL;

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_proxy_has_interface (chan, TP_IFACE_CHANNEL_INTERFACE_GROUP),
      "");

  tp_cli_channel_interface_group_connect_to_members_changed (chan,
      on_members_changed, NULL, NULL, NULL, NULL);

  check_initial_properties (chan);

  check_incoming_invitation (service_chan, chan);
}

int
main (int argc,
      char **argv)
{
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TestTextChannelGroup *service_chan;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  TpChannel *chan = NULL;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;

  g_type_init ();
  tp_debug_set_flags ("all");

  service_conn = SIMPLE_CONNECTION (g_object_new (SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  MYASSERT_NO_ERROR (error);

  dbus = tp_dbus_daemon_new (tp_get_bus ());
  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  chan_path = g_strdup_printf ("%s/Channel", conn_path);

  service_chan = TEST_TEXT_CHANNEL_GROUP (g_object_new (
        TEST_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", chan_path,
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_connection_run_connect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  chan = tp_channel_new (conn, chan_path, NULL, TP_UNKNOWN_HANDLE_TYPE, 0,
      &error);
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  test_group_mixin (service_chan, chan);

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

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

  return fail;
}
