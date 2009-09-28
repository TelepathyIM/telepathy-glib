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

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpAccountManager *am;
    GError *error /* initialized where needed */;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->am = NULL;
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

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
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

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/am/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/am/dup", Test, NULL, setup, test_dup, teardown);

  return g_test_run ();
}
