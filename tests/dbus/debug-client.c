/* Tests of TpTextChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/debug-sender.h>

#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side object */
    TpDebugSender *sender;

    /* Client side object */
    TpDebugClient *client;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  test->sender = tp_debug_sender_dup ();
  g_assert (test->sender != NULL);

  test->client = tp_debug_client_new (test->dbus,
      tp_dbus_daemon_get_unique_name (test->dbus), &test->error);
  g_assert_no_error (test->error);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->sender);
  tp_clear_object (&test->client);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (TP_IS_DEBUG_CLIENT (test->client));
}

static void
invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    Test *test)
{
  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_invalidated (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_signal_connect (test->client, "invalidated",
      G_CALLBACK (invalidated_cb), test);

  tp_clear_object (&test->sender);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

static void
proxy_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (source, result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_core_feature (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_DEBUG_CLIENT_FEATURE_CORE, 0 };

  g_object_set (test->sender, "enabled", TRUE, NULL);

  /* feature is not prepared yet */
  g_assert (!tp_debug_client_is_enabled (test->client));

  tp_proxy_prepare_async (test->client, features, proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (tp_debug_client_is_enabled (test->client));
}

static void
set_enabled_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_debug_client_set_enabled_finish (TP_DEBUG_CLIENT (source),
      result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_set_enabled (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean enabled;

  g_object_get (test->sender, "enabled", &enabled, NULL);
  g_assert (!enabled);

  /* Enable */
  tp_debug_client_set_enabled_async (test->client, TRUE, set_enabled_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_object_get (test->sender, "enabled", &enabled, NULL);
  g_assert (enabled);

  /* Disable */
  tp_debug_client_set_enabled_async (test->client, FALSE, set_enabled_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_object_get (test->sender, "enabled", &enabled, NULL);
  g_assert (!enabled);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/debug-client/creation", Test, NULL, setup,
      test_creation, teardown);
  g_test_add ("/debug-client/invalidated", Test, NULL, setup,
      test_invalidated, teardown);
  g_test_add ("/debug-client/core-feature", Test, NULL, setup,
      test_core_feature, teardown);
  g_test_add ("/debug-client/set-enabled", Test, NULL, setup,
      test_set_enabled, teardown);

  return g_test_run ();
}
