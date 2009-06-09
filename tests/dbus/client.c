/* A very basic feature test for TpClient
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpClient *client;
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

  test->client = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->client != NULL)
    {
      g_object_unref (test->client);
      test->client = NULL;
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
  test->client = g_object_new (TP_TYPE_CLIENT,
      "dbus-daemon", test->dbus,
      "object-path", "/org/freedesktop/Telepathy/Client/whatever",
      "bus-name", "org.freedesktop.Telepathy.Client.whatever",
      NULL);
  g_assert (test->client != NULL);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/client/new", Test, NULL, setup, test_new, teardown);

  return g_test_run ();
}
