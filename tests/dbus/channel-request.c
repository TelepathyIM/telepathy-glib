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
#include <telepathy-glib/client-factory-internal.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel-request.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/contacts-conn.h"
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
    GDBusConnection *dbus;

    GDBusConnection *private_dbus;
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
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->private_dbus = tp_tests_get_private_bus ();
  g_assert (test->private_dbus != NULL);

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);
  test->cr = NULL;

  test->cr_service = tp_tests_object_new_static_class (test_simple_cr_get_type (),
      NULL);
  tp_dbus_connection_register_object (test->private_dbus, "/whatever",
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
      tp_dbus_connection_release_name (test->private_dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

      g_dbus_connection_close_sync (test->private_dbus, NULL, NULL);

      g_object_unref (test->private_dbus);
      test->private_dbus = NULL;
    }

  g_object_unref (test->cr_service);
  test->cr_service = NULL;

  /* make sure any pending things have happened */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_object_unref (test->dbus);
  test->dbus = NULL;

  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static TpChannelRequest *
channel_request_new (GDBusConnection *bus_connection,
    const gchar *object_path,
    GHashTable *immutable_properties,
    GError **error)
{
  TpChannelRequest *self;
  TpClientFactory *factory;

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  if (immutable_properties == NULL)
    immutable_properties = tp_asv_new (NULL, NULL);
  else
    g_hash_table_ref (immutable_properties);

  factory = tp_client_factory_new (bus_connection);
  self = _tp_client_factory_ensure_channel_request (factory, object_path,
      immutable_properties, error);

  g_object_unref (factory);
  g_hash_table_unref (immutable_properties);

  return self;
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  /* CD not running */
  test->cr = channel_request_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cr == NULL);

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus,
      "not even syntactically valid", NULL, NULL);
  g_assert (test->cr == NULL);

  test->cr = channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
}

static void
test_crash (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  tp_dbus_connection_release_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_dbus_connection_close_sync (test->private_dbus, NULL, NULL);
  g_clear_object (&test->private_dbus);

  while (tp_proxy_get_invalidated (test->cr) == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_NAME_OWNER_LOST);
}

static void
succeeded_cb (TpChannelRequest *request,
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

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_signal_connect (test->cr, "succeeded",
      G_CALLBACK (succeeded_cb), test);

  /* sync up both sockets to ensure that the match rules are in place */
  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  props = g_hash_table_new (NULL, NULL);

  tp_svc_channel_request_emit_succeeded (test->cr_service,
      tp_base_connection_get_object_path (test->base_connection),
      props, "/Channel", props);

  g_hash_table_unref (props);

  tp_tests_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_OBJECT_REMOVED);
  g_assert_cmpuint (test->succeeded, ==, 1);

  g_signal_handlers_disconnect_by_func (test->cr, G_CALLBACK (succeeded_cb),
      test);
}

static void
test_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus, "/whatever", NULL, NULL);
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

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus, "/whatever", props, NULL);
  g_assert (test->cr != NULL);

  g_hash_table_unref (props);

  vardict = tp_channel_request_dup_immutable_properties (test->cr);
  g_assert_cmpuint (tp_vardict_get_uint32 (vardict, "badger", NULL), ==, 42);
  g_variant_unref (vardict);

  g_object_get (test->cr,
      "immutable-properties", &vardict, NULL);
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

  ok = tp_dbus_connection_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = channel_request_new (test->dbus, "/whatever", props, NULL);
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
  vardict = tp_channel_request_dup_hints (test->cr);
  g_assert_cmpstr (tp_vardict_get_string (vardict, "test"), ==, "hi");
  g_variant_unref (vardict);

  g_object_get (test->cr,
      "hints", &vardict,
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

  return tp_tests_run_with_bus ();
}
