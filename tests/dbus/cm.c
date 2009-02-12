/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=18291
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>

#include "examples/cm/echo/connection-manager.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    ExampleEchoConnectionManager *service_cm;

    TpConnectionManager *cm;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->service_cm = EXAMPLE_ECHO_CONNECTION_MANAGER (g_object_new (
        EXAMPLE_TYPE_ECHO_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_object_unref (test->service_cm);
  test->service_cm = NULL;
  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_valid_name (void)
{
  GError *error = NULL;

  g_assert (tp_connection_manager_check_valid_name ("gabble", NULL));

  g_assert (tp_connection_manager_check_valid_name ("l33t_cm", NULL));

  g_assert (!tp_connection_manager_check_valid_name ("wtf tbh", &error));
  g_assert (error != NULL);
  g_clear_error (&error);

  g_assert (!tp_connection_manager_check_valid_name ("0pointer", &error));
  g_assert (error != NULL);
  g_clear_error (&error);
}

static void
on_got_info_expect_none (TpConnectionManager *self,
                         guint info_source,
                         gpointer p)
{
  Test *test = p;

  g_assert (self == test->cm);
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_NONE);
  g_assert_cmpuint (info_source, ==, test->cm->info_source);

  g_main_loop_quit (test->mainloop);
}

static void
test_nothing_got_info (Test *test,
                       gconstpointer data)
{
  GError *error = NULL;
  gulong id;

  test->cm = tp_connection_manager_new (test->dbus, "not_actually_there",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);

  /* Spin the mainloop until we get the got-info signal. This API is rubbish.
   * In particular, got-info isn't guaranteed to be emitted at all,
   * unless you call tp_connection_manager_activate (#18207). As a
   * workaround for that, you can call tp_connection_manager_activate. */
  g_test_bug ("18207");
  tp_connection_manager_activate (test->cm);
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_none), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);

  g_assert_cmpstr (test->cm->name, ==, "not_actually_there");
  g_assert_cmpuint (test->cm->running, ==, FALSE);
  g_assert_cmpuint (test->cm->info_source, ==, TP_CM_INFO_SOURCE_NONE);
  g_assert (test->cm->protocols == NULL);
}

static void
on_got_info_expect_file (TpConnectionManager *self,
                         guint info_source,
                         gpointer p)
{
  Test *test = p;

  g_assert (self == test->cm);
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_assert_cmpuint (info_source, ==, test->cm->info_source);

  g_main_loop_quit (test->mainloop);

  g_assert_cmpstr (test->cm->name, ==, "spurious");
  g_assert_cmpuint (test->cm->running, ==, FALSE);
  g_assert_cmpuint (test->cm->info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_assert (test->cm->protocols != NULL);
  g_assert (test->cm->protocols[0] != NULL);
  g_assert (test->cm->protocols[1] != NULL);
  g_assert (test->cm->protocols[2] == NULL);

  /* FIXME: it's not technically an API guarantee that protocols and params
   * come out in this order... */

  g_assert_cmpstr (test->cm->protocols[0]->name, ==, "normal");
  g_assert_cmpstr (test->cm->protocols[0]->params[0].name, ==, "account");
  g_assert_cmpstr (test->cm->protocols[0]->params[0].dbus_signature, ==, "s");
  g_assert_cmpuint (test->cm->protocols[0]->params[0].flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER);
  g_assert_cmpstr (test->cm->protocols[0]->params[1].name, ==, "password");
  g_assert_cmpstr (test->cm->protocols[0]->params[1].dbus_signature, ==, "s");
  g_assert_cmpuint (test->cm->protocols[0]->params[1].flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER |
      TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (test->cm->protocols[0]->params[2].name, ==, "register");
  g_assert_cmpstr (test->cm->protocols[0]->params[2].dbus_signature, ==, "b");
  g_assert_cmpuint (test->cm->protocols[0]->params[2].flags, ==,
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (test->cm->protocols[0]->params[3].name == NULL);

  g_assert_cmpstr (test->cm->protocols[1]->name, ==, "weird");
  g_assert_cmpstr (test->cm->protocols[1]->params[0].name, ==,
      "com.example.Bork.Bork.Bork");
  g_assert_cmpuint (test->cm->protocols[1]->params[0].flags, ==,
      TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY |
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (test->cm->protocols[1]->params[0].dbus_signature, ==, "u");
  g_assert (test->cm->protocols[1]->params[1].name == NULL);
}

static void
test_file_got_info (Test *test,
                    gconstpointer data)
{
  GError *error = NULL;
  gulong id;

  test->cm = tp_connection_manager_new (test->dbus, "spurious",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_file), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);
}

static void
on_got_info_expect_live (TpConnectionManager *self,
                         guint info_source,
                         gpointer p)
{
  Test *test = p;

  g_assert (self == test->cm);
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_LIVE);
  g_assert_cmpuint (info_source, ==, test->cm->info_source);

  g_main_loop_quit (test->mainloop);
}

static void
test_dbus_got_info (Test *test,
                    gconstpointer data)
{
  GError *error = NULL;
  gulong id;

  test->cm = tp_connection_manager_new (test->dbus,
      TP_BASE_CONNECTION_MANAGER_GET_CLASS (test->service_cm)->cm_dbus_name,
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_live), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_func ("/cm/valid-name", test_valid_name);
  g_test_add ("/cm/nothing", Test, NULL, setup, test_nothing_got_info,
      teardown);
  g_test_add ("/cm/file", Test, NULL, setup, test_file_got_info, teardown);
  g_test_add ("/cm/dbus", Test, NULL, setup, test_dbus_got_info, teardown);

  return g_test_run ();
}
