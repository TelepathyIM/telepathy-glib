/* A very basic feature test for TpChannelRequest
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/svc-channel-request.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/simple-conn.h"
#include "tests/lib/util.h"

/* This object implements no methods and no properties - TpChannelRequest
 * doesn't actually use them yet */

static GType test_simple_cr_get_type (void);

typedef struct {
    GObject parent;
} TestSimpleCR;

typedef struct {
    GObjectClass parent;
} TestSimpleCRClass;

G_DEFINE_TYPE_WITH_CODE (TestSimpleCR, test_simple_cr, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_REQUEST, NULL));

static void
test_simple_cr_init (TestSimpleCR *self)
{
}

static void
test_simple_cr_class_init (TestSimpleCRClass *klass)
{
}

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    DBusGConnection *private_conn;
    TpDBusDaemon *private_dbus;
    GObject *cr_service;

    /* Service side objects */
    TpBaseConnection *base_connection;

    /* Client side objects */
    TpConnection *connection;
    TpChannel *channel;
    TpChannelRequest *cr;

    GError *error /* initialized where needed */;

    guint succeeded;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  DBusConnection *libdbus;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  libdbus = dbus_bus_get_private (DBUS_BUS_STARTER, NULL);
  g_assert (libdbus != NULL);
  dbus_connection_setup_with_g_main (libdbus, NULL);
  dbus_connection_set_exit_on_disconnect (libdbus, FALSE);
  test->private_conn = dbus_connection_get_g_connection (libdbus);
  /* transfer ref */
  dbus_g_connection_ref (test->private_conn);
  dbus_connection_unref (libdbus);
  g_assert (test->private_conn != NULL);
  test->private_dbus = tp_dbus_daemon_new (test->private_conn);
  g_assert (test->private_dbus != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_SIMPLE_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);
  test->cr = NULL;

  test->cr_service = tp_tests_object_new_static_class (test_simple_cr_get_type (),
      NULL);
  tp_dbus_daemon_register_object (test->private_dbus, "/whatever",
      test->cr_service);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);

  if (test->cr != NULL)
    {
      g_object_unref (test->cr);
      test->cr = NULL;
    }

  if (test->private_dbus != NULL)
    {
      tp_dbus_daemon_release_name (test->private_dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

      g_object_unref (test->private_dbus);
      test->private_dbus = NULL;
    }

  g_object_unref (test->cr_service);
  test->cr_service = NULL;

  if (test->private_conn != NULL)
    {
      dbus_connection_close (dbus_g_connection_get_connection (
            test->private_conn));

      dbus_g_connection_unref (test->private_conn);
      test->private_conn = NULL;
    }

  /* make sure any pending things have happened */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_object_unref (test->dbus);
  test->dbus = NULL;

  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  /* CD not running */
  test->cr = tp_channel_request_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cr == NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus,
      "not even syntactically valid", NULL, NULL);
  g_assert (test->cr == NULL);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
}

static void
test_crash (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  tp_dbus_daemon_release_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  dbus_connection_close (dbus_g_connection_get_connection (
        test->private_conn));
  dbus_g_connection_unref (test->private_conn);
  test->private_conn = NULL;

  while (tp_proxy_get_invalidated (test->cr) == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_NAME_OWNER_LOST);
}

static void
succeeded_cb (Test *test)
{
  test->succeeded++;
}

static void
succeeded_with_channel_cb (TpChannelRequest *request,
    TpConnection *connection,
    TpChannel *channel,
    Test *test)
{
  g_assert (TP_IS_CONNECTION (connection));
  g_assert (TP_IS_CHANNEL (channel));

  g_assert_cmpstr (tp_proxy_get_object_path (connection), ==,
      tp_base_connection_get_object_path (test->base_connection));
  g_assert_cmpstr (tp_proxy_get_object_path (channel), ==,
      "/Channel");

  test->succeeded++;
}

static void
test_succeeded (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;
  GHashTable *props;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_signal_connect_swapped (test->cr, "succeeded", G_CALLBACK (succeeded_cb),
      test);
  g_signal_connect (test->cr, "succeeded-with-channel",
      G_CALLBACK (succeeded_with_channel_cb), test);

  /* sync up both sockets to ensure that the match rules are in place */
  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  props = g_hash_table_new (NULL, NULL);

  tp_svc_channel_request_emit_succeeded_with_channel (test->cr_service,
      tp_base_connection_get_object_path (test->base_connection),
      props, "/Channel", props);

  g_hash_table_unref (props);

  tp_svc_channel_request_emit_succeeded (test->cr_service);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_OBJECT_REMOVED);
  g_assert_cmpuint (test->succeeded, ==, 2);

  g_signal_handlers_disconnect_by_func (test->cr, G_CALLBACK (succeeded_cb),
      test);
}

static void
test_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_signal_connect_swapped (test->cr, "succeeded", G_CALLBACK (succeeded_cb),
      test);

  /* sync up both sockets to ensure that the match rules are in place */
  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  tp_svc_channel_request_emit_failed (test->cr_service,
      TP_ERROR_STR_NOT_YOURS, "lalala");

  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_ERROR);
  g_assert (tp_proxy_get_invalidated (test->cr)->code == TP_ERROR_NOT_YOURS);
  g_assert_cmpstr (tp_proxy_get_invalidated (test->cr)->message, ==,
      "lalala");
  g_assert_cmpuint (test->succeeded, ==, 0);

  g_signal_handlers_disconnect_by_func (test->cr, G_CALLBACK (succeeded_cb),
      test);
}

static void
test_immutable_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;
  GHashTable *props;
  GVariant *vardict;

  props = tp_asv_new ("badger", G_TYPE_UINT, 42,
      NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", props, NULL);
  g_assert (test->cr != NULL);

  g_hash_table_unref (props);

  props = (GHashTable *) tp_channel_request_get_immutable_properties (test->cr);
  g_assert_cmpuint (tp_asv_get_uint32 (props, "badger", NULL), ==, 42);

  g_object_get (test->cr, "immutable-properties", &props, NULL);
  g_assert_cmpuint (tp_asv_get_uint32 (props, "badger", NULL), ==, 42);

  g_hash_table_unref (props);

  vardict = tp_channel_request_dup_immutable_properties (test->cr);
  g_assert_cmpuint (tp_vardict_get_uint32 (vardict, "badger", NULL), ==, 42);
  g_variant_unref (vardict);

  g_object_get (test->cr,
      "immutable-properties-vardict", &vardict, NULL);
  g_assert_cmpuint (tp_vardict_get_uint32 (vardict, "badger", NULL), ==, 42);
  g_variant_unref (vardict);
}

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "a/b/c"

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;
  GHashTable *props, *hints;
  TpAccount *account;
  gint64 user_action_time;
  const gchar *handler;
  GVariant *vardict;

  hints = tp_asv_new ("test", G_TYPE_STRING, "hi", NULL);

  props = tp_asv_new (
      TP_PROP_CHANNEL_REQUEST_ACCOUNT, DBUS_TYPE_G_OBJECT_PATH, ACCOUNT_PATH,
      TP_PROP_CHANNEL_REQUEST_USER_ACTION_TIME, G_TYPE_INT64, (gint64) 12345,
      TP_PROP_CHANNEL_REQUEST_PREFERRED_HANDLER, G_TYPE_STRING, "Badger",
      TP_PROP_CHANNEL_REQUEST_HINTS, TP_HASH_TYPE_STRING_VARIANT_MAP, hints,
      NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", props, NULL);
  g_assert (test->cr != NULL);

  g_hash_table_unref (props);
  g_hash_table_unref (hints);

  /* Account */
  account = tp_channel_request_get_account (test->cr);
  g_assert (TP_IS_ACCOUNT (account));
  g_assert_cmpstr (tp_proxy_get_object_path (account), ==, ACCOUNT_PATH);

  g_object_get (test->cr, "account", &account, NULL);
  g_assert (TP_IS_ACCOUNT (account));
  g_assert_cmpstr (tp_proxy_get_object_path (account), ==, ACCOUNT_PATH);
  g_object_unref (account);

  /* UserActionTime */
  user_action_time = tp_channel_request_get_user_action_time (test->cr);
  g_assert_cmpint (user_action_time, ==, 12345);

  g_object_get (test->cr, "user-action-time", &user_action_time, NULL);
  g_assert_cmpint (user_action_time, ==, 12345);

  /* PreferredHandler */
  handler = tp_channel_request_get_preferred_handler (test->cr);
  g_assert_cmpstr (handler, ==, "Badger");

  g_object_get (test->cr, "preferred-handler", &handler, NULL);
  g_assert_cmpstr (handler, ==, "Badger");

  /* Hints */
  hints = (GHashTable *) tp_channel_request_get_hints (test->cr);
  g_assert_cmpstr (tp_asv_get_string (hints, "test"), ==, "hi");

  g_object_get (test->cr, "hints", &hints, NULL);
  g_assert_cmpstr (tp_asv_get_string (hints, "test"), ==, "hi");

  g_hash_table_unref (hints);

  vardict = tp_channel_request_dup_hints (test->cr);
  g_assert_cmpstr (tp_vardict_get_string (vardict, "test"), ==, "hi");
  g_variant_unref (vardict);

  g_object_get (test->cr,
      "hints-vardict", &vardict,
      NULL);
  g_assert_cmpstr (tp_vardict_get_string (vardict, "test"), ==, "hi");
  g_variant_unref (vardict);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cr/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/cr/crash", Test, NULL, setup, test_crash, teardown);
  g_test_add ("/cr/succeeded", Test, NULL, setup, test_succeeded, teardown);
  g_test_add ("/cr/failed", Test, NULL, setup, test_failed, teardown);
  g_test_add ("/cr/immutable-properties", Test, NULL, setup,
      test_immutable_properties, teardown);
  g_test_add ("/cr/properties", Test, NULL, setup, test_properties, teardown);

  return g_test_run ();
}
