/* Tests of TpDebugClient
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

    GPtrArray *messages;
    TpDebugMessage *message;
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

  tp_clear_pointer (&test->messages, g_ptr_array_unref);
  tp_clear_object (&test->message);
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

static void
get_messages_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_clear_pointer (&test->messages, g_ptr_array_unref);

  test->messages = tp_debug_client_get_messages_finish (
      TP_DEBUG_CLIENT (source), result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_get_messages (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GDateTime *time1, *time2, *t;
  GTimeVal time_val;
  TpDebugMessage *msg;

  time1 = g_date_time_new_now_utc ();
  g_date_time_to_timeval (time1, &time_val);

  tp_debug_sender_add_message (test->sender, &time_val, "domain1",
      G_LOG_LEVEL_MESSAGE, "message1\n");

  time2 = g_date_time_new_now_local ();
  g_date_time_to_timeval (time2, &time_val);

  tp_debug_sender_add_message (test->sender, &time_val, "domain2/category",
      G_LOG_LEVEL_DEBUG, "message2");

  tp_debug_client_get_messages_async (test->client, get_messages_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (test->messages != NULL);
  g_assert_cmpuint (test->messages->len, ==, 2);

  /* first message */
  msg = g_ptr_array_index (test->messages, 0);
  g_assert (TP_IS_DEBUG_MESSAGE (msg));

  t = tp_debug_message_get_time (msg);
  g_assert (t != NULL);
  /* Don't use g_date_time_equal() as the gouble -> GDateTime conversion in
   * _tp_debug_message_new() may result in a difference of one (!)
   * millisecond */
  g_assert_cmpuint (g_date_time_to_unix (t), ==, g_date_time_to_unix (time1));

  g_assert_cmpstr (tp_debug_message_get_domain (msg), ==, "domain1");
  g_assert (tp_debug_message_get_category (msg) == NULL);
  g_assert_cmpuint (tp_debug_message_get_level (msg), ==, G_LOG_LEVEL_MESSAGE);
  g_assert_cmpstr (tp_debug_message_get_message (msg), ==, "message1");

  /* second message */
  msg = g_ptr_array_index (test->messages, 1);
  g_assert (TP_IS_DEBUG_MESSAGE (msg));

  t = tp_debug_message_get_time (msg);
  g_assert (t != NULL);
  g_assert_cmpuint (g_date_time_to_unix (t), ==, g_date_time_to_unix (time2));

  g_assert_cmpstr (tp_debug_message_get_domain (msg), ==, "domain2");
  g_assert_cmpstr (tp_debug_message_get_category (msg), ==, "category");
  g_assert_cmpuint (tp_debug_message_get_level (msg), ==, G_LOG_LEVEL_DEBUG);
  g_assert_cmpstr (tp_debug_message_get_message (msg), ==, "message2");
}

static void
new_debug_message_cb (TpDebugClient *client,
    TpDebugMessage *message,
    Test *test)
{
  tp_clear_object (&test->message);

  test->message = g_object_ref (message);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);

}

static void
test_new_debug_message (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_signal_connect (test->client, "new-debug-message",
      G_CALLBACK (new_debug_message_cb), test);

  g_object_set (test->sender, "enabled", TRUE, NULL);

  tp_debug_sender_add_message (test->sender, NULL, "domain",
      G_LOG_LEVEL_DEBUG, "new message");

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert (TP_IS_DEBUG_MESSAGE (test->message));

  g_assert_cmpstr (tp_debug_message_get_domain (test->message), ==, "domain");
  g_assert_cmpuint (tp_debug_message_get_level (test->message), ==,
      G_LOG_LEVEL_DEBUG);
  g_assert_cmpstr (tp_debug_message_get_message (test->message), ==,
      "new message");
}

static void
test_get_messages_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  /* Remove debug service */
  tp_clear_object (&test->sender);

  tp_debug_client_get_messages_async (test->client, get_messages_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_error (test->error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD);

  g_assert (test->messages == NULL);
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
  g_test_add ("/debug-client/get-messages", Test, NULL, setup,
      test_get_messages, teardown);
  g_test_add ("/debug-client/new-debug-message", Test, NULL, setup,
      test_new_debug_message, teardown);
  g_test_add ("/debug-client/get-messages-failed", Test, NULL, setup,
      test_get_messages_failed, teardown);

  return g_test_run ();
}
