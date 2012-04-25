/* A very basic feature test for TpFutureAccount
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/future-account.h>

#include "tests/lib/util.h"

typedef struct {
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;

  TpAccountManager *account_manager;
  TpFutureAccount *account;

  GAsyncResult *result;
  GError *error /* initialized where needed */;
} Test;

static void
setup (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->account_manager = tp_account_manager_dup ();
  g_assert (test->account_manager != NULL);

  test->account = NULL;
}

static void
teardown (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_clear_object (&test->account);

  /* make sure any pending calls on the account have happened, so it can die */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_clear_object (&test->dbus);
  tp_clear_pointer (&test->mainloop, g_main_loop_unref);

  g_clear_error (&test->error);
  tp_clear_object (&test->result);
}

static void
test_new (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");
  g_assert (TP_IS_FUTURE_ACCOUNT (test->account));
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccountManager *am;
  gchar *manager, *protocol;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  g_object_get (test->account,
      "account-manager", &am,
      "connection-manager", &manager,
      "protocol", &protocol,
      NULL);

  g_assert (am == test->account_manager);
  g_assert_cmpstr (manager, ==, "gabble");
  g_assert_cmpstr (protocol, ==, "jabber");

  g_object_unref (am);
  g_free (manager);
  g_free (protocol);
}

int
main (int argc,
    char **argv)
{
  g_type_init ();
  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/future-account/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/future-account/properties", Test, NULL, setup,
      test_properties, teardown);

  return g_test_run ();
}
