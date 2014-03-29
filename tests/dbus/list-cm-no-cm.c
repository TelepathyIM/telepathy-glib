/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=68892
 *
 * Copyright (C) 2014 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib/gstdio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/util.h"

typedef struct {
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  TpClientFactory *factory;
  GError *error;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  test->factory = tp_client_factory_new (test->dbus);

  test->error = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_object (&test->dbus);
  g_clear_object (&test->factory);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_list_cm_no_cm (Test *test,
    gconstpointer data)
{
  GAsyncResult *res = NULL;
  GList *cms;

  tp_list_connection_managers_async (test->factory, tp_tests_result_ready_cb,
      &res);
  tp_tests_run_until_result (&res);
  cms = tp_list_connection_managers_finish (res, &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpuint (g_list_length (cms), ==, 0);

  g_object_unref (res);
  g_list_free (cms);
}

int
main (int argc,
      char **argv)
{
  gchar *dir;
  GError *error = NULL;
  int result;

  /* This test relies on D-Bus not finding any service file so tweak
   * TP_TESTS_SERVICES_DIR to point to an empty directory. */
  dir = g_dir_make_tmp ("tp-glib-tests.XXXXXX", &error);
  g_assert_no_error (error);
  g_setenv ("TP_TESTS_SERVICES_DIR", dir, TRUE);

  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cm/list-cm-no-cm", Test, NULL, setup, test_list_cm_no_cm,
      teardown);

  result = tp_tests_run_with_bus ();

  g_rmdir (dir);
  g_free (dir);

  return result;
}
