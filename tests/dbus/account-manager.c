/* A very basic feature test for TpAccountManager
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

#include "tests/lib/simple-account-manager.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    DBusGConnection *bus;

    SimpleAccountManager *service /* initialized in prepare_service */;
    TpAccountManager *am;
    gboolean prepared /* The result of prepare_finish */;
    guint timeout_id;

    GError *error /* initialized where needed */;
} Test;

static gboolean
test_timed_out (gpointer data)
{
  Test *test = (Test *) data;
  g_assert_not_reached ();
  test->prepared = FALSE;
  /* Note that this is a completely bogus error, but it only gets returned if
   * you comment out the g_assert_not_reached() above. */
  test->error = g_error_new_literal (TP_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
                                     "timeout");
  g_print ("about to quit");
  g_main_loop_quit (test->mainloop);
  g_print ("just quit");
  return FALSE;
}

static void
setup (Test *test,
       gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);

  g_main_loop_ref (test->mainloop);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->am = NULL;
  test->timeout_id = 0;
}

static void
setup_service (Test *test,
       gconstpointer data)
{

  setup (test, data);

  test->bus = tp_get_bus ();

  g_assert (tp_dbus_daemon_request_name (test->dbus,
          TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error));

  test->service = g_object_new (SIMPLE_TYPE_ACCOUNT_MANAGER, NULL);
  dbus_g_connection_register_g_object (test->bus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      (GObject *) test->service);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->am != NULL)
    {
      g_object_unref (test->am);
      test->am = NULL;
    }
  if (test->timeout_id != 0)
    {
      g_source_remove (test->timeout_id);
      test->timeout_id = 0;
    }
  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
teardown_service (Test *test,
          gconstpointer data)
{
  g_assert (
      tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
                               &test->error));
  g_object_unref (test->service);
  test->service = NULL;
  teardown (test, data);
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test->am = tp_account_manager_new (test->dbus);
}

static void
test_dup (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  TpAccountManager *one, *two;
  TpDBusDaemon *dbus_one, *dbus_two;

  one = tp_account_manager_dup ();
  two = tp_account_manager_dup ();

  g_assert (one == two);

  dbus_one = tp_dbus_daemon_dup (NULL);
  dbus_two = tp_proxy_get_dbus_daemon (one);

  g_assert (dbus_one == dbus_two);

  g_object_unref (dbus_one);
  g_object_unref (two);
  g_object_unref (one);
}

/** Tests which use the bus follow this comment. */

/**
 * Used by test_prepare.
 */
static void am_prepare_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  Test *test = (Test *) user_data;
  TpAccountManager *am = TP_ACCOUNT_MANAGER (source_object);
  g_assert (test->am == am);
  test->prepared = tp_account_manager_prepare_finish (am, res, &test->error);
  g_main_loop_quit (test->mainloop);
}

/**
 * Helper function for testing the functionality of prepare. Calls prepare
 * and then starts the mainloop until it finishes or times out after a second.
 * Not actually run as a test on its own even though it looks like one.
 * Note that test_prepare doesn't assert that the prepare
 * succeeded. Only that it didn't time out. Use test_prepare_success or
 * test_prepare_fail for this.
 */
static void
test_prepare (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test->am = tp_account_manager_new (test->dbus);
  tp_account_manager_prepare_async (test->am, NULL, am_prepare_cb, test);
  test->timeout_id = g_timeout_add (1000, test_timed_out, test);
  g_main_loop_run (test->mainloop);
}

/**
 * Tests the usual case where prepare succeeds.
 */
static void
test_prepare_success (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);
  g_assert_no_error (test->error);
  g_assert (test->prepared);
}

/**
 * Tests the case where the well-known name is not provided.
 * This should be run with setup rather than setup_service to make this the case.
 * TODO: use g_assert_error (err, dom, c) to fix the domain and code.
 */
static void
test_prepare_no_name (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);
  g_assert (test->error != NULL);
  test->error = NULL;
  g_assert (!test->prepared);
}

/**
 * Tests the case where the object has been destroyed.
 * TODO: use g_assert_error (err, dom, c) to fix the domain and code.
 */
static void
test_prepare_destroyed (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  dbus_g_connection_unregister_g_object (test->bus, G_OBJECT (test->service));
  test_prepare (test, data);
  g_assert (test->error != NULL);
  test->error = NULL;
  g_assert (!test->prepared);
  dbus_g_connection_register_g_object (test->bus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      (GObject *) test->service);
}

static void
test_create_success (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);
  g_assert_no_error (test->error);
  g_assert (test->prepared);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/am/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/am/dup", Test, NULL, setup, test_dup, teardown);

  g_test_add ("/am/prepare/success", Test, NULL, setup_service,
              test_prepare_success, teardown_service);
  g_test_add ("/am/prepare/destroyed", Test, NULL, setup_service,
              test_prepare_destroyed, teardown_service);
  g_test_add ("/am/prepare/name-not-provided", Test, NULL, setup,
              test_prepare_no_name, teardown);

  g_test_add ("/am/create/success", Test, NULL, setup_service,
              test_create_success, teardown_service);
  return g_test_run ();
}
