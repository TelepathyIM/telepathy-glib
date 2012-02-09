/* A very basic feature test for TpChannelDispatcher
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/channel-dispatcher.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpChannelDispatcher *cd;
    GError *error /* initialized where needed */;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->cd = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->cd != NULL)
    {
      g_object_unref (test->cd);
      test->cd = NULL;
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
  test->cd = tp_channel_dispatcher_new (test->dbus);
  g_assert (test->cd != NULL);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cd/new", Test, NULL, setup, test_new, teardown);
  /* tp_channel_dispatcher_present_channel_async() is tested in
   * test-base-client */

  return g_test_run ();
}
