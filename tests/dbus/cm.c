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
}

static void
test_file_got_info (Test *test,
                    gconstpointer data)
{
  GError *error = NULL;
  gulong id;
  const TpConnectionManagerParam *param;
  const TpConnectionManagerProtocol *protocol;

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

  g_assert_cmpstr (test->cm->name, ==, "spurious");
  g_assert_cmpuint (test->cm->running, ==, FALSE);
  g_assert_cmpuint (test->cm->info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_assert (test->cm->protocols != NULL);
  g_assert (test->cm->protocols[0] != NULL);
  g_assert (test->cm->protocols[1] != NULL);
  g_assert (test->cm->protocols[2] == NULL);

  /* FIXME: it's not technically an API guarantee that protocols and params
   * come out in this order... */

  protocol = test->cm->protocols[0];
  g_assert_cmpstr (protocol->name, ==, "normal");

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "account");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER);

  param = &protocol->params[1];
  g_assert_cmpstr (param->name, ==, "password");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER |
      TP_CONN_MGR_PARAM_FLAG_SECRET);

  param = &protocol->params[2];
  g_assert_cmpstr (param->name, ==, "register");
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);

  param = &protocol->params[3];
  g_assert (param->name == NULL);

  protocol = test->cm->protocols[1];
  g_assert_cmpstr (protocol->name, ==, "weird");

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "com.example.Bork.Bork.Bork");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY |
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "u");

  param = &protocol->params[1];
  g_assert (param->name == NULL);

  protocol = test->cm->protocols[2];
  g_assert (protocol == NULL);
}

static void
test_complex_file_got_info (Test *test,
                            gconstpointer data)
{
  GError *error = NULL;
  gulong id;
  const TpConnectionManagerParam *param;
  const TpConnectionManagerProtocol *protocol;
  gchar **strv;

  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert (error == NULL);
  g_test_queue_unref (test->cm);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_file), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);

  g_assert_cmpstr (test->cm->name, ==, "test_manager_file");
  g_assert_cmpuint (test->cm->running, ==, FALSE);
  g_assert_cmpuint (test->cm->info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_assert (test->cm->protocols != NULL);
  g_assert (test->cm->protocols[0] != NULL);
  g_assert (test->cm->protocols[1] != NULL);
  g_assert (test->cm->protocols[2] != NULL);
  g_assert (test->cm->protocols[3] == NULL);

  /* FIXME: it's not technically an API guarantee that protocols and params
   * come out in this order... */

  protocol = test->cm->protocols[0];

  g_assert_cmpstr (protocol->name, ==, "foo");

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "account");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));
  g_assert_cmpstr (g_value_get_string (&param->default_value), ==,
      "foo@default");

  param = &protocol->params[1];
  g_assert_cmpstr (param->name, ==, "password");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));

  param = &protocol->params[2];
  g_assert_cmpstr (param->name, ==, "encryption-key");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));

  param = &protocol->params[3];
  g_assert_cmpstr (param->name, ==, "port");
  g_assert_cmpstr (param->dbus_signature, ==, "q");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (G_VALUE_HOLDS_UINT (&param->default_value));
  g_assert_cmpuint (g_value_get_uint (&param->default_value), ==, 1234);

  param = &protocol->params[4];
  g_assert_cmpstr (param->name, ==, "register");
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert_cmpuint (param->flags, ==, 0);
  g_assert (G_VALUE_HOLDS_BOOLEAN (&param->default_value));

  param = &protocol->params[5];
  g_assert_cmpstr (param->name, ==, "server-list");
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "foo");
  g_assert_cmpstr (strv[1], ==, "bar");
  g_assert (strv[2] == NULL);

  param = &protocol->params[6];
  g_assert (param->name == NULL);

  protocol = test->cm->protocols[1];
  g_assert_cmpstr (protocol->name, ==, "bar");

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "account");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));
  g_assert_cmpstr (g_value_get_string (&param->default_value), ==,
      "bar@default");

  param = &protocol->params[1];
  g_assert_cmpstr (param->name, ==, "encryption-key");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));

  param = &protocol->params[2];
  g_assert_cmpstr (param->name, ==, "password");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));

  param = &protocol->params[3];
  g_assert_cmpstr (param->name, ==, "port");
  g_assert_cmpstr (param->dbus_signature, ==, "q");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (G_VALUE_HOLDS_UINT (&param->default_value));
  g_assert_cmpuint (g_value_get_uint (&param->default_value), ==, 4321);

  param = &protocol->params[4];
  g_assert_cmpstr (param->name, ==, "register");
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert_cmpuint (param->flags, ==, 0);
  g_assert (G_VALUE_HOLDS_BOOLEAN (&param->default_value));

  param = &protocol->params[5];
  g_assert_cmpstr (param->name, ==, "server-list");
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "bar");
  g_assert_cmpstr (strv[1], ==, "foo");
  g_assert (strv[2] == NULL);

  param = &protocol->params[6];
  g_assert (param->name == NULL);

  protocol = test->cm->protocols[2];
  g_assert_cmpstr (test->cm->protocols[2]->name, ==, "somewhat-pathological");

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "foo");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));
  g_assert_cmpstr (g_value_get_string (&param->default_value), ==,
      "hello world");

  param = &protocol->params[1];
  g_assert_cmpstr (param->name, ==, "semicolons");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));
  g_assert_cmpstr (g_value_get_string (&param->default_value), ==,
      "list;of;misc;");

  param = &protocol->params[2];
  g_assert_cmpstr (param->name, ==, "list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);

  param = &protocol->params[3];
  g_assert_cmpstr (param->name, ==, "unterminated-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);

  param = &protocol->params[4];
  g_assert_cmpstr (param->name, ==, "spaces-in-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, " of");
  g_assert_cmpstr (strv[2], ==, " misc ");
  g_assert (strv[3] == NULL);

  param = &protocol->params[5];
  g_assert_cmpstr (param->name, ==, "escaped-semicolon-in-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list;of");
  g_assert_cmpstr (strv[1], ==, "misc");
  g_assert (strv[2] == NULL);

  param = &protocol->params[6];
  g_assert_cmpstr (param->name, ==, "doubly-escaped-semicolon-in-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list\\");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);

  param = &protocol->params[7];
  g_assert_cmpstr (param->name, ==, "triply-escaped-semicolon-in-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "list\\;of");
  g_assert_cmpstr (strv[1], ==, "misc");
  g_assert (strv[2] == NULL);

  param = &protocol->params[8];
  g_assert_cmpstr (param->name, ==, "empty-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert (strv == NULL || strv[0] == NULL);

  param = &protocol->params[9];
  g_assert_cmpstr (param->name, ==, "escaped-semicolon");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert (G_VALUE_HOLDS_STRING (&param->default_value));
  g_assert_cmpstr (g_value_get_string (&param->default_value), ==,
      "foo\\;bar");

  param = &protocol->params[10];
  g_assert_cmpstr (param->name, ==, "object");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "o");
  g_assert (G_VALUE_HOLDS (&param->default_value, DBUS_TYPE_G_OBJECT_PATH));
  g_assert_cmpstr (g_value_get_boxed (&param->default_value), ==, "/misc");

  param = &protocol->params[11];
  g_assert_cmpstr (param->name, ==, "q");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "q");
  g_assert (G_VALUE_HOLDS_UINT (&param->default_value));
  g_assert_cmpint (g_value_get_uint (&param->default_value), ==, 42);

  param = &protocol->params[12];
  g_assert_cmpstr (param->name, ==, "u");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "u");
  g_assert (G_VALUE_HOLDS_UINT (&param->default_value));
  g_assert_cmpint (g_value_get_uint (&param->default_value), ==, 42);

  param = &protocol->params[13];
  g_assert_cmpstr (param->name, ==, "t");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "t");
  g_assert (G_VALUE_HOLDS_UINT64 (&param->default_value));
  g_assert_cmpuint ((guint) g_value_get_uint64 (&param->default_value), ==,
      42);

  param = &protocol->params[14];
  g_assert_cmpstr (param->name, ==, "n");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "n");
  g_assert (G_VALUE_HOLDS_INT (&param->default_value));
  g_assert_cmpint (g_value_get_int (&param->default_value), ==, -42);

  param = &protocol->params[15];
  g_assert_cmpstr (param->name, ==, "i");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "i");
  g_assert (G_VALUE_HOLDS_INT (&param->default_value));
  g_assert_cmpint (g_value_get_int (&param->default_value), ==, -42);

  param = &protocol->params[16];
  g_assert_cmpstr (param->name, ==, "x");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "x");
  g_assert (G_VALUE_HOLDS_INT64 (&param->default_value));
  g_assert_cmpint ((int) g_value_get_int64 (&param->default_value), ==, -42);

  param = &protocol->params[17];
  g_assert_cmpstr (param->name, ==, "d");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "d");
  g_assert (G_VALUE_HOLDS_DOUBLE (&param->default_value));

  param = &protocol->params[18];
  g_assert_cmpstr (param->name, ==, "empty-string-in-list");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "as");
  g_assert (G_VALUE_HOLDS (&param->default_value, G_TYPE_STRV));
  strv = g_value_get_boxed (&param->default_value);
  g_assert_cmpstr (strv[0], ==, "");
  g_assert (strv[1] == NULL);

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
  g_test_add ("/cm/file (complex)", Test, NULL, setup,
      test_complex_file_got_info, teardown);
  g_test_add ("/cm/dbus", Test, NULL, setup, test_dbus_got_info, teardown);

  return g_test_run ();
}
