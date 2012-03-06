/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=18291
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/echo-cm.h"
#include "tests/lib/util.h"

typedef enum {
    ACTIVATE_CM = (1 << 0),
    USE_CWR = (1 << 1)
} TestFlags;

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    TpTestsEchoConnectionManager *service_cm;

    TpConnectionManager *cm;
    GError *error /* initialized where needed */;
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
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->service_cm = TP_TESTS_ECHO_CONNECTION_MANAGER (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER,
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
  g_assert_no_error (error);
  g_test_queue_unref (test->cm);

  /* Spin the mainloop until we get the got-info signal. This API is rubbish,
   * but it's better than it used to be... */
  g_test_bug ("18207");
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
  gchar **strv;
  GValue value = { 0 };
  gboolean ok;

  test->cm = tp_connection_manager_new (test->dbus, "spurious",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (error);
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

  strv = tp_connection_manager_dup_protocol_names (test->cm);

  if (tp_strdiff (strv[0], "normal"))
    {
      g_assert_cmpstr (strv[0], ==, "weird");
      g_assert_cmpstr (strv[1], ==, "normal");
    }
  else
    {
      g_assert_cmpstr (strv[0], ==, "normal");
      g_assert_cmpstr (strv[1], ==, "weird");
    }

  g_assert (strv[2] == NULL);
  g_strfreev (strv);

  g_assert (tp_connection_manager_has_protocol (test->cm, "normal"));
  g_assert (!tp_connection_manager_has_protocol (test->cm, "not-there"));

  protocol = tp_connection_manager_get_protocol (test->cm, "normal");

  g_assert_cmpstr (protocol->name, ==, "normal");
  g_assert (tp_connection_manager_protocol_can_register (protocol));

  g_assert (tp_connection_manager_protocol_has_param (protocol, "account"));
  g_assert (!tp_connection_manager_protocol_has_param (protocol, "not-there"));

  /* FIXME: it's not technically an API guarantee that params
   * come out in this order... */

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "account");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER);
  g_assert (param == tp_connection_manager_protocol_get_param (protocol,
        "account"));
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==,
      "account");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param),
      ==, "s");
  g_assert (tp_connection_manager_param_is_required (param));
  g_assert (tp_connection_manager_param_is_required_for_registration (param));
  g_assert (!tp_connection_manager_param_is_secret (param));
  g_assert (!tp_connection_manager_param_is_dbus_property (param));
  g_assert (!tp_connection_manager_param_is_dbus_property (param));
  ok = tp_connection_manager_param_get_default (param, &value);
  g_assert (!ok);
  g_assert (!G_IS_VALUE (&value));

  param = &protocol->params[1];
  g_assert_cmpstr (param->name, ==, "password");
  g_assert_cmpstr (param->dbus_signature, ==, "s");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER |
      TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (param == tp_connection_manager_protocol_get_param (protocol,
        "password"));

  param = &protocol->params[2];
  g_assert_cmpstr (param->name, ==, "register");
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (param == tp_connection_manager_protocol_get_param (protocol,
        "register"));
  ok = tp_connection_manager_param_get_default (param, &value);
  g_assert (ok);
  g_assert (G_IS_VALUE (&value));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_value_unset (&value);

  param = &protocol->params[3];
  g_assert (param->name == NULL);

  strv = tp_connection_manager_protocol_dup_param_names (protocol);
  g_assert_cmpstr (strv[0], ==, "account");
  g_assert_cmpstr (strv[1], ==, "password");
  g_assert_cmpstr (strv[2], ==, "register");
  g_assert (strv[3] == NULL);
  g_strfreev (strv);

  /* switch to the other protocol, whichever one that actually is */
  if (protocol == test->cm->protocols[0])
    {
      protocol = test->cm->protocols[1];
    }
  else
    {
      g_assert (protocol == test->cm->protocols[1]);
      protocol = test->cm->protocols[0];
    }

  g_assert_cmpstr (protocol->name, ==, "weird");
  g_assert (protocol == tp_connection_manager_get_protocol (test->cm,
        "weird"));
  g_assert (!tp_connection_manager_protocol_can_register (protocol));

  param = &protocol->params[0];
  g_assert_cmpstr (param->name, ==, "com.example.Bork.Bork.Bork");
  g_assert_cmpuint (param->flags, ==,
      TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY |
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "u");

  param = &protocol->params[1];
  g_assert (param->name == NULL);

  g_assert (test->cm->protocols[2] == NULL);
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
  GPtrArray *arr;

  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (error);
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

  /* FIXME: it's not technically an API guarantee that params
   * come out in this order... */

  protocol = tp_connection_manager_get_protocol (test->cm, "foo");

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

  protocol = tp_connection_manager_get_protocol (test->cm, "bar");
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

  protocol = tp_connection_manager_get_protocol (test->cm,
      "somewhat-pathological");
  g_assert_cmpstr (protocol->name, ==, "somewhat-pathological");

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

  param = &protocol->params[19];
  g_assert_cmpstr (param->name, ==, "true");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert (G_VALUE_HOLDS_BOOLEAN (&param->default_value));
  g_assert_cmpint (g_value_get_boolean (&param->default_value), ==, TRUE);

  param = &protocol->params[20];
  g_assert_cmpstr (param->name, ==, "false");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "b");
  g_assert (G_VALUE_HOLDS_BOOLEAN (&param->default_value));
  g_assert_cmpint (g_value_get_boolean (&param->default_value), ==, FALSE);

  param = &protocol->params[21];
  g_assert_cmpstr (param->name, ==, "y");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "y");
  g_assert (G_VALUE_HOLDS_UCHAR (&param->default_value));
  g_assert_cmpint (g_value_get_uchar (&param->default_value), ==, 42);

  param = &protocol->params[22];
  g_assert_cmpstr (param->name, ==, "ao");
  g_assert_cmpuint (param->flags, ==, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (param->dbus_signature, ==, "ao");
  g_assert (G_VALUE_HOLDS (&param->default_value,
        TP_ARRAY_TYPE_OBJECT_PATH_LIST));
  arr = g_value_get_boxed (&param->default_value);
  g_assert_cmpuint (arr->len, ==, 2);
  g_assert_cmpstr ((gchar *) g_ptr_array_index (arr, 0), ==, "/misc");
  g_assert_cmpstr ((gchar *) g_ptr_array_index (arr, 1), ==, "/other");

  param = &protocol->params[23];
  g_assert (param->name == NULL);
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
  g_assert_no_error (error);
  g_test_queue_unref (test->cm);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_live), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);
}

static void
ready_or_not (TpConnectionManager *self,
              const GError *error,
              gpointer user_data,
              GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;

  if (error != NULL)
    test->error = g_error_copy (error);

  g_main_loop_quit (test->mainloop);
}

static void
test_nothing_ready (Test *test,
                    gconstpointer data)
{
  gchar *name;
  guint info_source;
  TestFlags flags = GPOINTER_TO_INT (data);

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "nonexistent_cm",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);
  g_test_queue_unref (test->cm);

  g_test_bug ("18291");

  if (flags & USE_CWR)
    {
      tp_connection_manager_call_when_ready (test->cm, ready_or_not,
          test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert (test->error != NULL);
      g_clear_error (&test->error);
    }
  else
    {
      tp_tests_proxy_run_until_prepared_or_failed (test->cm, NULL,
          &test->error);

      g_assert_error (test->error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN);
    }

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "nonexistent_cm");
  g_assert_cmpuint (tp_connection_manager_is_ready (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, FALSE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_NONE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "connection-manager", &name,
      NULL);
  g_assert_cmpstr (name, ==, "nonexistent_cm");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_NONE);
  g_free (name);
}

static void
test_file_ready (Test *test,
                 gconstpointer data)
{
  gchar *name;
  guint info_source;
  TestFlags flags = GPOINTER_TO_INT (data);

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "spurious",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);
  g_test_queue_unref (test->cm);

  g_test_bug ("18291");

  if (flags & USE_CWR)
    {
      tp_connection_manager_call_when_ready (test->cm, ready_or_not,
          test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert_no_error (test->error);
    }
  else
    {
      tp_tests_proxy_run_until_prepared (test->cm, NULL);
    }

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "spurious");
  g_assert_cmpuint (tp_connection_manager_is_ready (test->cm), ==, TRUE);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "connection-manager", &name,
      NULL);
  g_assert_cmpstr (name, ==, "spurious");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_free (name);

}

static void
test_complex_file_ready (Test *test,
                         gconstpointer data)
{
  gchar *name;
  guint info_source;
  TestFlags flags = GPOINTER_TO_INT (data);

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);
  g_test_queue_unref (test->cm);

  g_test_bug ("18291");

  if (flags & USE_CWR)
    {
      tp_connection_manager_call_when_ready (test->cm, ready_or_not,
          test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert_no_error (test->error);
    }
  else
    {
      tp_tests_proxy_run_until_prepared (test->cm, NULL);
    }

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "test_manager_file");
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_ready (test->cm), ==, TRUE);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "connection-manager", &name,
      NULL);
  g_assert_cmpstr (name, ==, "test_manager_file");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_free (name);
}

static gboolean
idle_activate (gpointer cm)
{
  tp_connection_manager_activate (cm);
  return FALSE;
}

static void
test_dbus_ready (Test *test,
                 gconstpointer data)
{
  gchar *name;
  guint info_source;
  const TestFlags flags = GPOINTER_TO_INT (data);

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus,
      TP_BASE_CONNECTION_MANAGER_GET_CLASS (test->service_cm)->cm_dbus_name,
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);
  g_test_queue_unref (test->cm);

  if (flags & ACTIVATE_CM)
    {
      g_test_bug ("23524");

      /* The bug being tested here was caused by ListProtocols being called
       * twice on the same CM; this can be triggered by _activate()ing at
       * exactly the wrong moment. But the wrong moment involves racing an
       * idle. This triggered the assertion about 1/3 of the time on my laptop.
       * --wjt
       */
      g_idle_add (idle_activate, test->cm);
    }
  else
    {
      g_test_bug ("18291");
    }

  if (flags & USE_CWR)
    {
      tp_connection_manager_call_when_ready (test->cm, ready_or_not,
          test, NULL, NULL);
      g_main_loop_run (test->mainloop);
      g_assert_no_error (test->error);
    }
  else
    {
      tp_tests_proxy_run_until_prepared (test->cm, NULL);
    }

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "example_echo");
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_ready (test->cm), ==, TRUE);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, TRUE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_LIVE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "connection-manager", &name,
      NULL);
  g_assert_cmpstr (name, ==, "example_echo");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_LIVE);
  g_free (name);
}

static void
on_listed_connection_managers (TpConnectionManager * const * cms,
                               gsize n_cms,
                               const GError *error,
                               gpointer user_data,
                               GObject *weak_object G_GNUC_UNUSED)
{
  Test *test = user_data;
  TpConnectionManager *spurious;
  TpConnectionManager *echo;

  g_assert_cmpuint ((guint) n_cms, ==, 2);
  g_assert (cms[2] == NULL);

  if (tp_connection_manager_is_running (cms[0]))
    {
      echo = cms[0];
      spurious = cms[1];
    }
  else
    {
      spurious = cms[0];
      echo = cms[1];
    }

  g_assert (tp_connection_manager_is_running (echo));
  g_assert (!tp_connection_manager_is_running (spurious));

  g_assert (tp_proxy_is_prepared (echo, TP_CONNECTION_MANAGER_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (spurious,
        TP_CONNECTION_MANAGER_FEATURE_CORE));

  g_assert (tp_proxy_get_invalidated (echo) == NULL);
  g_assert (tp_proxy_get_invalidated (spurious) == NULL);

  g_assert (tp_connection_manager_is_ready (echo));
  g_assert (tp_connection_manager_is_ready (spurious));

  g_assert_cmpuint (tp_connection_manager_get_info_source (echo),
      ==, TP_CM_INFO_SOURCE_LIVE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (spurious),
      ==, TP_CM_INFO_SOURCE_FILE);

  g_assert (tp_connection_manager_has_protocol (echo, "example"));
  g_assert (tp_connection_manager_has_protocol (spurious, "normal"));

  g_main_loop_quit (test->mainloop);
}

static void
test_list (Test *test,
           gconstpointer data G_GNUC_UNUSED)
{
  tp_list_connection_managers (test->dbus, on_listed_connection_managers,
      test, NULL, NULL);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_func ("/cm/valid-name", test_valid_name);

  g_test_add ("/cm/nothing (old)", Test, NULL, setup, test_nothing_got_info,
      teardown);
  g_test_add ("/cm/file (old)", Test, NULL, setup, test_file_got_info,
      teardown);
  g_test_add ("/cm/file (old, complex)", Test, NULL, setup,
      test_complex_file_got_info, teardown);
  g_test_add ("/cm/dbus (old)", Test, NULL, setup, test_dbus_got_info,
      teardown);

  g_test_add ("/cm/nothing", Test, GINT_TO_POINTER (0),
      setup, test_nothing_ready, teardown);
  g_test_add ("/cm/nothing/cwr", Test, GINT_TO_POINTER (USE_CWR),
      setup, test_nothing_ready, teardown);
  g_test_add ("/cm/file", Test, GINT_TO_POINTER (0),
      setup, test_file_ready, teardown);
  g_test_add ("/cm/file/cwr", Test, GINT_TO_POINTER (USE_CWR),
      setup, test_file_ready, teardown);
  g_test_add ("/cm/file/complex", Test, GINT_TO_POINTER (0), setup,
      test_complex_file_ready, teardown);
  g_test_add ("/cm/file/complex/cwr", Test, GINT_TO_POINTER (USE_CWR), setup,
      test_complex_file_ready, teardown);
  g_test_add ("/cm/dbus", Test, GINT_TO_POINTER (0), setup,
      test_dbus_ready, teardown);
  g_test_add ("/cm/dbus/cwr", Test, GINT_TO_POINTER (USE_CWR), setup,
      test_dbus_ready, teardown);

  g_test_add ("/cm/list", Test, NULL, setup, test_list, teardown);

  return g_test_run ();
}
