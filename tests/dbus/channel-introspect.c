/* Basic introspection on a channel (template for further regression tests)
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
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/textchan-group.h"
#include "tests/lib/echo-chan.h"
#include "tests/lib/util.h"

#define IDENTIFIER "them@example.org"

static GMainLoop *mainloop;

static void
channel_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  GAsyncResult **output = user_data;

  g_message ("channel %p prepared", object);
  *output = g_object_ref (res);

  if (mainloop != NULL)
    g_main_loop_quit (mainloop);
}

static void
assert_chan_sane (TpChannel *chan,
    TpHandle handle,
    gboolean requested,
    TpHandle initiator_handle,
    const gchar *initiator_id)
{
  GVariant *variant;
  TpEntityType type;
  TpContact *contact;

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);
  g_assert_cmpuint (tp_channel_get_handle (chan, NULL), ==, handle);
  g_assert_cmpuint (tp_channel_get_handle (chan, &type), ==, handle);
  g_assert_cmpuint (type, ==,
      handle == 0 ? TP_ENTITY_TYPE_NONE : TP_ENTITY_TYPE_CONTACT);
  g_assert_cmpstr (tp_channel_get_channel_type (chan), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpuint (tp_channel_get_channel_type_id (chan), ==,
      TP_IFACE_QUARK_CHANNEL_TYPE_TEXT);
  g_assert (TP_IS_CONNECTION (tp_channel_get_connection (chan)));
  g_assert_cmpstr (tp_channel_get_identifier (chan), ==, IDENTIFIER);
  g_assert (tp_channel_get_requested (chan) == requested);

  contact = tp_channel_get_initiator_contact (chan);
  g_assert (contact != NULL);
  g_assert_cmpuint (tp_contact_get_handle (contact), ==, initiator_handle);
  g_assert_cmpstr (tp_contact_get_identifier (contact), ==, initiator_id);

  contact = tp_channel_get_target_contact (chan);
  if (handle != 0)
    {
      g_assert (contact != NULL);
      g_assert_cmpuint (tp_contact_get_handle (contact), ==, handle);
    }
  else
    {
      g_assert (contact == NULL);
    }

  variant = tp_channel_dup_immutable_properties (chan);
  g_assert (variant != NULL);
  g_assert_cmpstr (
      tp_vardict_get_string (variant, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpuint (
      tp_vardict_get_uint32 (variant, TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, NULL), ==,
      handle == 0 ? TP_ENTITY_TYPE_NONE : TP_ENTITY_TYPE_CONTACT);
  g_assert_cmpuint (
      tp_vardict_get_uint32 (variant, TP_PROP_CHANNEL_TARGET_HANDLE, NULL), ==,
      handle);
  g_assert_cmpstr (
      tp_vardict_get_string (variant, TP_PROP_CHANNEL_TARGET_ID), ==,
      IDENTIFIER);
}

int
main (int argc,
      char **argv)
{
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TpTestsEchoChannel *service_props_chan;
  TpTestsTextChannelGroup *service_props_group_chan;
  TpDBusDaemon *dbus;
  TpConnection *conn, *conn2;
  TpChannel *chan, *chan2;
  GError *error = NULL;
  gchar *props_chan_path;
  gchar *props_group_chan_path;
  gchar *bad_chan_path;
  TpHandle handle;
  GHashTable *asv;
  GAsyncResult *prepare_result;
  GQuark group_features[] = { TP_CHANNEL_FEATURE_GROUP, 0 };
  const gchar * const empty[] = { NULL };
  GTestDBus *test_dbus;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_dbus_unset ();
  test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (test_dbus);

  dbus = tp_tests_dbus_daemon_dup_or_die ();

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &conn);
  service_conn = TP_TESTS_SIMPLE_CONNECTION (service_conn_as_base);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_ENTITY_TYPE_CONTACT);
  g_assert (contact_repo != NULL);

  handle = tp_handle_ensure (contact_repo, IDENTIFIER, NULL, &error);
  g_assert_no_error (error);

  props_chan_path = g_strdup_printf ("%s/PropertiesChannel",
      tp_proxy_get_object_path (conn));

  service_props_chan = TP_TESTS_ECHO_CHANNEL (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CHANNEL,
        "connection", service_conn,
        "object-path", props_chan_path,
        "handle", handle,
        "requested", TRUE,
        "initiator-handle",
            tp_base_connection_get_self_handle (service_conn_as_base),
        NULL));

  props_group_chan_path = g_strdup_printf ("%s/PropsGroupChannel",
      tp_proxy_get_object_path (conn));

  service_props_group_chan = TP_TESTS_TEXT_CHANNEL_GROUP (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_TEXT_CHANNEL_GROUP,
        "connection", service_conn,
        "object-path", props_group_chan_path,
        "requested", TRUE,
        "initiator-handle",
            tp_base_connection_get_self_handle (service_conn_as_base),
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  g_message ("Channel becomes ready while we wait (the version with "
      "Properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  chan = tp_tests_channel_new (conn, props_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);

  tp_tests_proxy_run_until_prepared (chan, NULL);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  assert_chan_sane (chan, handle, TRUE,
      tp_base_connection_get_self_handle (service_conn_as_base),
      tp_handle_inspect (contact_repo,
        tp_base_connection_get_self_handle (service_conn_as_base)));

  /* no way to see what this is doing - just make sure it doesn't crash */
  tp_proxy_prepare_async (chan, NULL, NULL, NULL);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (preloading immutable "
      "properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  asv = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT, TP_ENTITY_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
      TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
      TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
      TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, empty,
      TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
      NULL);

  chan = tp_tests_channel_new_from_properties (conn, props_chan_path, asv, &error);
  g_assert_no_error (error);

  g_hash_table_unref (asv);
  asv = NULL;

  tp_tests_proxy_run_until_prepared (chan, NULL);
  assert_chan_sane (chan, handle, TRUE,
      tp_base_connection_get_self_handle (service_conn_as_base),
      tp_handle_inspect (contact_repo,
          tp_base_connection_get_self_handle (service_conn_as_base)));

  g_object_unref (chan);
  chan = NULL;

  g_message ("Group channel becomes ready while we wait (preloading immutable "
      "properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  {
    const gchar *interfaces[] = {
        TP_IFACE_CHANNEL_INTERFACE_GROUP1,
        NULL
    };

    asv = tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
            TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT,
            TP_ENTITY_TYPE_CONTACT,
        TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, interfaces,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
        NULL);
  }

  chan = tp_tests_channel_new_from_properties (conn, props_group_chan_path, asv, &error);
  g_assert_no_error (error);

  g_hash_table_unref (asv);
  asv = NULL;

  tp_tests_proxy_run_until_prepared (chan, group_features);
  assert_chan_sane (chan, 0, TRUE,
      tp_base_connection_get_self_handle (service_conn_as_base),
      tp_handle_inspect (contact_repo,
          tp_base_connection_get_self_handle (service_conn_as_base)));

  g_object_unref (chan);
  chan = NULL;

  g_message ("channel does not, in fact, exist");

  bad_chan_path = g_strdup_printf ("%s/Does/Not/Actually/Exist",
      tp_proxy_get_object_path (conn));
  chan = tp_tests_channel_new (conn, bad_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  tp_tests_proxy_run_until_prepared_or_failed (chan, NULL, &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_error_free (error);
  error = NULL;

  g_object_unref (chan);
  chan = NULL;
  g_free (bad_chan_path);
  bad_chan_path = NULL;

  /* regression test for fdo#41729
   *
   * tp-glib uses to rely on its introspection queue to add the interface ID
   * of its channel type even when the type was already known during
   * construction.
   *
   * This test create new proxies, ensuring that the TpConnection of the
   * TpChannel isn't prepared yet, and check that the interface is added right
   * away after its construction.
   * */

  g_message ("Regression test for fdo#41729");

  conn2 = tp_tests_connection_new (dbus, tp_proxy_get_bus_name (conn),
      tp_proxy_get_object_path (conn),
      &error);
  g_assert_no_error (error);

  {
    const gchar *interfaces[] = {
        TP_IFACE_CHANNEL_INTERFACE_GROUP1,
        NULL
    };

    asv = tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
            TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, G_TYPE_UINT,
            TP_ENTITY_TYPE_CONTACT,
        TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, interfaces,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
        NULL);
  }

  chan2 = tp_tests_channel_new_from_properties (conn2, props_group_chan_path, asv,
      &error);
  g_assert_no_error (error);

  g_assert (tp_proxy_has_interface_by_id (chan2,
        TP_IFACE_QUARK_CHANNEL_TYPE_TEXT));
  g_assert (tp_proxy_has_interface_by_id (chan2,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP1));

  g_hash_table_unref (asv);

  g_clear_object (&chan2);
  g_clear_object (&conn2);

  g_message ("Channel already dead");

  chan = tp_tests_channel_new (conn, props_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);
  g_assert (prepare_result == NULL);
  g_main_loop_run (mainloop);
  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);
  g_object_unref (prepare_result);
  prepare_result = NULL;

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);

  tp_tests_connection_assert_disconnect_succeeds (conn);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  /* is_prepared becomes FALSE because the channel broke */
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_error (tp_proxy_get_invalidated (chan),
      TP_ERROR, TP_ERROR_CANCELLED);

  /* ... but prepare_async still hasn't finished until we run the main loop */
  g_assert (prepare_result == NULL);
  g_main_loop_run (mainloop);
  g_assert (prepare_result != NULL);
  MYASSERT (!tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_assert_cmpstr (error->message, ==,
      tp_proxy_get_invalidated (chan)->message);
  tp_clear_object (&prepare_result);

  g_clear_error (&error);

  g_object_unref (chan);
  chan = NULL;

  /* clean up */

  g_assert (chan == NULL);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  g_object_unref (conn);
  g_object_unref (service_props_chan);
  g_object_unref (service_props_group_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (props_chan_path);
  g_free (props_group_chan_path);

  g_test_dbus_down (test_dbus);
  tp_tests_assert_last_unref (&test_dbus);

  return 0;
}
