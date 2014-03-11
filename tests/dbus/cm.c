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
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "examples/cm/echo-message-parts/connection-manager.h"
#include "tests/lib/util.h"

typedef enum {
    ACTIVATE_CM = (1 << 0),
    DROP_NAME_ON_GET = (1 << 3),
    DROP_NAME_ON_GET_TWICE = (1 << 4)
} TestFlags;

typedef struct {
    ExampleEcho2ConnectionManager parent;
    guint drop_name_on_get;
} MyConnectionManager;
typedef ExampleEcho2ConnectionManagerClass MyConnectionManagerClass;

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    MyConnectionManager *service_cm;

    TpConnectionManager *cm;
    TpConnectionManager *echo;
    TpConnectionManager *spurious;
    GError *error /* initialized where needed */;
} Test;

static void my_properties_iface_init (gpointer iface);
static GType my_connection_manager_get_type (void);

G_DEFINE_TYPE_WITH_CODE (MyConnectionManager,
    my_connection_manager,
    EXAMPLE_TYPE_ECHO_2_CONNECTION_MANAGER,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      my_properties_iface_init))

static void
my_connection_manager_class_init (MyConnectionManagerClass *cls)
{
}

static void
my_connection_manager_init (MyConnectionManager *self)
{
}

static void
my_get (TpSvcDBusProperties *iface G_GNUC_UNUSED,
    const gchar *i G_GNUC_UNUSED,
    const gchar *p G_GNUC_UNUSED,
    GDBusMethodInvocation *context)
{
  /* The telepathy-glib client side should never call this:
   * GetAll() is better. */
  g_assert_not_reached ();
}

static void
my_get_all (TpSvcDBusProperties *iface,
    const gchar *i,
    GDBusMethodInvocation *context)
{
  MyConnectionManager *cm = (MyConnectionManager *) iface;
  GHashTable *ht;

  /* If necessary, emulate the CM exiting and coming back. */
  if (cm->drop_name_on_get)
    {
      TpDBusDaemon *dbus = tp_base_connection_manager_get_dbus_daemon (
          TP_BASE_CONNECTION_MANAGER (cm));
      GString *string = g_string_new (TP_CM_BUS_NAME_BASE);
      GError *error = NULL;

      g_string_append (string, "example_echo_2");

      cm->drop_name_on_get--;

      tp_dbus_daemon_release_name (dbus, string->str, &error);
      g_assert_no_error (error);
      tp_dbus_daemon_request_name (dbus, string->str, FALSE, &error);
      g_assert_no_error (error);
    }

  ht = tp_dbus_properties_mixin_dup_all ((GObject *) cm, i);
  tp_svc_dbus_properties_return_from_get_all (context, ht);
  g_hash_table_unref (ht);
}

static void
my_properties_iface_init (gpointer iface)
{
  TpSvcDBusPropertiesClass *cls = iface;

#define IMPLEMENT(x) \
    tp_svc_dbus_properties_implement_##x (cls, my_##x)
  IMPLEMENT (get);
  IMPLEMENT (get_all);
#undef IMPLEMENT
}

static void
setup (Test *test,
       gconstpointer data)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->service_cm = tp_tests_object_new_static_class (
      my_connection_manager_get_type (),
      NULL);
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
  g_clear_object (&test->service_cm);
  g_clear_object (&test->dbus);
  g_clear_object (&test->cm);
  g_clear_object (&test->echo);
  g_clear_object (&test->spurious);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
assert_param_flags (const TpConnectionManagerParam *param,
    TpConnMgrParamFlags flags)
{
  GValue value = {0, };

  g_assert (tp_connection_manager_param_is_required (param) ==
      ((flags & TP_CONN_MGR_PARAM_FLAG_REQUIRED) != 0));
  g_assert (tp_connection_manager_param_is_required_for_registration (param) ==
      ((flags & TP_CONN_MGR_PARAM_FLAG_REGISTER) != 0));
  g_assert (tp_connection_manager_param_get_default (param, &value) ==
      ((flags & TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT) != 0));
  g_assert (tp_connection_manager_param_is_secret (param) ==
      ((flags & TP_CONN_MGR_PARAM_FLAG_SECRET) != 0));
  g_assert (tp_connection_manager_param_is_dbus_property (param) ==
      ((flags & TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY) != 0));

  if (G_IS_VALUE (&value))
    g_value_unset (&value);
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
  g_assert_cmpuint (info_source, ==,
      tp_connection_manager_get_info_source (test->cm));

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

  /* Spin the mainloop until we get the got-info signal. This API is rubbish,
   * but it's better than it used to be... */
  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_none), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "not_actually_there");
  g_assert (!tp_connection_manager_is_running (test->cm));
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_NONE);
  g_assert (tp_connection_manager_dup_protocols (test->cm) == NULL);
}

static void
on_got_info_expect_file (TpConnectionManager *self,
                         guint info_source,
                         gpointer p)
{
  Test *test = p;

  g_assert (self == test->cm);
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_assert_cmpuint (info_source, ==,
      tp_connection_manager_get_info_source (test->cm));

  g_main_loop_quit (test->mainloop);
}

static void
test_file_got_info (Test *test,
                    gconstpointer data)
{
  GError *error = NULL;
  gulong id;
  const TpConnectionManagerParam *param;
  TpProtocol *protocol;
  gchar **strv;
  GValue value = { 0 };
  gboolean ok;
  GVariant *variant;

  test->cm = tp_connection_manager_new (test->dbus, "spurious",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (error);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_file), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==, "spurious");
  g_assert (!tp_connection_manager_is_running (test->cm));
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

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

  g_assert_cmpstr (tp_protocol_get_name (protocol), ==, "normal");
  g_assert (tp_protocol_can_register (protocol));

  g_assert (tp_protocol_has_param (protocol, "account"));
  g_assert (!tp_protocol_has_param (protocol, "not-there"));

  /* FIXME: it's not technically an API guarantee that params
   * come out in this order... */

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 0);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "account");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER);
  g_assert (param == tp_protocol_get_param (protocol, "account"));
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
  g_assert (tp_connection_manager_param_dup_default_variant (param) == NULL);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 1);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "password");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER |
      TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (param == tp_protocol_get_param (protocol, "password"));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 2);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "register");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "b");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert (param == tp_protocol_get_param (protocol, "register"));
  ok = tp_connection_manager_param_get_default (param, &value);
  g_assert (ok);
  g_assert (G_IS_VALUE (&value));
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_value_unset (&value);
  variant = tp_connection_manager_param_dup_default_variant (param);
  g_assert_cmpstr (g_variant_get_type_string (variant), ==, "b");
  g_assert_cmpint (g_variant_get_boolean (variant), ==, TRUE);

  g_assert_cmpuint (tp_protocol_get_params (protocol)->len, ==, 3);

  strv = tp_protocol_dup_param_names (protocol);
  g_assert_cmpstr (strv[0], ==, "account");
  g_assert_cmpstr (strv[1], ==, "password");
  g_assert_cmpstr (strv[2], ==, "register");
  g_assert (strv[3] == NULL);
  g_strfreev (strv);

  protocol = tp_connection_manager_get_protocol (test->cm, "weird");

  g_assert_cmpstr (tp_protocol_get_name (protocol), ==, "weird");
  g_assert (protocol == tp_connection_manager_get_protocol (test->cm,
        "weird"));
  g_assert (!tp_protocol_can_register (protocol));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 0);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "com.example.Bork.Bork.Bork");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY |
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "u");

  g_assert_cmpuint (tp_protocol_get_params (protocol)->len, ==, 1);
}

static void
test_complex_file_got_info (Test *test,
                            gconstpointer data)
{
  GError *error = NULL;
  gulong id;
  const TpConnectionManagerParam *param;
  TpProtocol *protocol;
  gchar **strv;
  GPtrArray *arr;
  GList *protocols;
  GValue value = {0, };

  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (error);

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_file), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "test_manager_file");
  g_assert (!tp_connection_manager_is_running (test->cm));
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

  protocols = tp_connection_manager_dup_protocols (test->cm);
  g_assert_cmpuint (g_list_length (protocols), ==, 3);
  g_list_free_full (protocols, g_object_unref);

  /* FIXME: it's not technically an API guarantee that params
   * come out in this order... */

  protocol = tp_connection_manager_get_protocol (test->cm, "foo");

  g_assert_cmpstr (tp_protocol_get_name (protocol), ==, "foo");

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 0);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "account");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "foo@default");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 1);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "password");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 2);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "encryption-key");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 3);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "port");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "q");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpuint (g_value_get_uint (&value), ==, 1234);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 4);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "register");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "b");
  assert_param_flags (param, 0);
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 5);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "server-list");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "foo");
  g_assert_cmpstr (strv[1], ==, "bar");
  g_assert (strv[2] == NULL);
  g_value_unset (&value);

  g_assert_cmpuint (tp_protocol_get_params (protocol)->len, ==, 6);

  protocol = tp_connection_manager_get_protocol (test->cm, "bar");
  g_assert_cmpstr (tp_protocol_get_name (protocol), ==, "bar");

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 0);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "account");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "bar@default");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 1);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "encryption-key");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 2);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "password");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 3);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "port");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "q");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpuint (g_value_get_uint (&value), ==, 4321);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 4);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "register");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "b");
  assert_param_flags (param, 0);
  g_assert (!tp_connection_manager_param_get_default (param, &value));

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 5);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "server-list");
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "bar");
  g_assert_cmpstr (strv[1], ==, "foo");
  g_assert (strv[2] == NULL);
  g_value_unset (&value);

  g_assert_cmpuint (tp_protocol_get_params (protocol)->len, ==, 6);

  protocol = tp_connection_manager_get_protocol (test->cm,
      "somewhat_pathological");
  g_assert_cmpstr (tp_protocol_get_name (protocol), ==,
      "somewhat_pathological");

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 0);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "foo");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "hello world");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 1);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "semicolons");
  assert_param_flags (param,
      TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT | TP_CONN_MGR_PARAM_FLAG_SECRET);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "list;of;misc;");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 2);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 3);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "unterminated-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 4);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "spaces-in-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list");
  g_assert_cmpstr (strv[1], ==, " of");
  g_assert_cmpstr (strv[2], ==, " misc ");
  g_assert (strv[3] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 5);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "escaped-semicolon-in-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list;of");
  g_assert_cmpstr (strv[1], ==, "misc");
  g_assert (strv[2] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 6);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "doubly-escaped-semicolon-in-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list\\");
  g_assert_cmpstr (strv[1], ==, "of");
  g_assert_cmpstr (strv[2], ==, "misc");
  g_assert (strv[3] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 7);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "triply-escaped-semicolon-in-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "list\\;of");
  g_assert_cmpstr (strv[1], ==, "misc");
  g_assert (strv[2] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 8);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "empty-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert (strv == NULL || strv[0] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 9);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "escaped-semicolon");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "s");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "foo\\;bar");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 10);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "object");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "o");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, DBUS_TYPE_G_OBJECT_PATH));
  g_assert_cmpstr (g_value_get_boxed (&value), ==, "/misc");
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 11);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "q");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "q");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpint (g_value_get_uint (&value), ==, 42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 12);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "u");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "u");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpint (g_value_get_uint (&value), ==, 42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 13);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "t");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "t");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UINT64 (&value));
  g_assert_cmpuint ((guint) g_value_get_uint64 (&value), ==, 42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 14);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "n");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "n");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_INT (&value));
  g_assert_cmpint (g_value_get_int (&value), ==, -42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 15);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "i");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "i");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_INT (&value));
  g_assert_cmpint (g_value_get_int (&value), ==, -42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 16);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "x");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "x");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_INT64 (&value));
  g_assert_cmpint ((int) g_value_get_int64 (&value), ==, -42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 17);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "d");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "d");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_DOUBLE (&value));
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 18);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "empty-string-in-list");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "as");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value, G_TYPE_STRV));
  strv = g_value_get_boxed (&value);
  g_assert_cmpstr (strv[0], ==, "");
  g_assert (strv[1] == NULL);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 19);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "true");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "b");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_cmpint (g_value_get_boolean (&value), ==, TRUE);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 20);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "false");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "b");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_cmpint (g_value_get_boolean (&value), ==, FALSE);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 21);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "y");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "y");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS_UCHAR (&value));
  g_assert_cmpint (g_value_get_uchar (&value), ==, 42);
  g_value_unset (&value);

  param = g_ptr_array_index (tp_protocol_get_params (protocol), 22);
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "ao");
  assert_param_flags (param, TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT);
  g_assert_cmpstr (tp_connection_manager_param_get_dbus_signature (param), ==, "ao");
  tp_connection_manager_param_get_default (param, &value);
  g_assert (G_VALUE_HOLDS (&value,
        TP_ARRAY_TYPE_OBJECT_PATH_LIST));
  arr = g_value_get_boxed (&value);
  g_assert_cmpuint (arr->len, ==, 2);
  g_assert_cmpstr ((gchar *) g_ptr_array_index (arr, 0), ==, "/misc");
  g_assert_cmpstr ((gchar *) g_ptr_array_index (arr, 1), ==, "/other");
  g_value_unset (&value);

  g_assert_cmpuint (tp_protocol_get_params (protocol)->len, ==, 23);
}

static void
on_got_info_expect_live (TpConnectionManager *self,
                         guint info_source,
                         gpointer p)
{
  Test *test = p;

  g_assert (self == test->cm);
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_LIVE);
  g_assert_cmpuint (info_source, ==,
      tp_connection_manager_get_info_source (test->cm));

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

  g_test_bug ("18207");
  id = g_signal_connect (test->cm, "got-info",
      G_CALLBACK (on_got_info_expect_live), test);
  g_main_loop_run (test->mainloop);
  g_signal_handler_disconnect (test->cm, id);
}

static void
test_nothing_ready (Test *test,
                    gconstpointer data)
{
  gchar *name;
  guint info_source;

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "nonexistent_cm",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);

  g_test_bug ("18291");

  tp_tests_proxy_run_until_prepared_or_failed (test->cm, NULL,
      &test->error);
  g_assert_error (test->error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "nonexistent_cm");
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, FALSE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_NONE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "cm-name", &name,
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
  GList *l;

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "spurious",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);

  g_test_bug ("18291");

  tp_tests_proxy_run_until_prepared (test->cm, NULL);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "spurious");
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "cm-name", &name,
      NULL);
  g_assert_cmpstr (name, ==, "spurious");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_FILE);
  g_free (name);

  l = tp_connection_manager_dup_protocols (test->cm);
  g_assert_cmpuint (g_list_length (l), ==, 2);
  g_assert (TP_IS_PROTOCOL (l->data));
  g_assert (TP_IS_PROTOCOL (l->next->data));
  g_list_free_full (l, g_object_unref);
}

static void
test_complex_file_ready (Test *test,
                         gconstpointer data)
{
  gchar *name;
  guint info_source;

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);

  g_test_bug ("18291");

  tp_tests_proxy_run_until_prepared (test->cm, NULL);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "test_manager_file");
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, FALSE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_FILE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "cm-name", &name,
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
  GList *l;

  test->error = NULL;
  test->cm = tp_connection_manager_new (test->dbus,
      TP_BASE_CONNECTION_MANAGER_GET_CLASS (test->service_cm)->cm_dbus_name,
      NULL, &test->error);
  g_assert (TP_IS_CONNECTION_MANAGER (test->cm));
  g_assert_no_error (test->error);

  if (flags & DROP_NAME_ON_GET_TWICE)
    {
      test->service_cm->drop_name_on_get = 2;
    }
  else if (flags & DROP_NAME_ON_GET)
    {
      test->service_cm->drop_name_on_get = 1;
    }

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

  tp_tests_proxy_run_until_prepared_or_failed (test->cm, NULL,
      &test->error);

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "example_echo_2");

  if (flags & DROP_NAME_ON_GET_TWICE)
    {
      /* If it dies during introspection *twice*, we assume it has crashed
       * or something. */
      g_assert_error (test->error, TP_DBUS_ERRORS,
          TP_DBUS_ERROR_NAME_OWNER_LOST);
      g_clear_error (&test->error);

      g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
            TP_CONNECTION_MANAGER_FEATURE_CORE), ==, FALSE);
      g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
          TP_CM_INFO_SOURCE_NONE);
      return;
    }

  g_assert_no_error (test->error);
  g_assert_cmpuint (tp_proxy_is_prepared (test->cm,
        TP_CONNECTION_MANAGER_FEATURE_CORE), ==, TRUE);
  g_assert (tp_proxy_get_invalidated (test->cm) == NULL);
  g_assert_cmpuint (tp_connection_manager_is_running (test->cm), ==, TRUE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->cm), ==,
      TP_CM_INFO_SOURCE_LIVE);

  g_object_get (test->cm,
      "info-source", &info_source,
      "cm-name", &name,
      NULL);
  g_assert_cmpstr (name, ==, "example_echo_2");
  g_assert_cmpuint (info_source, ==, TP_CM_INFO_SOURCE_LIVE);
  g_free (name);

  l = tp_connection_manager_dup_protocols (test->cm);
  g_assert_cmpuint (g_list_length (l), ==, 1);
  g_assert_cmpstr (tp_protocol_get_name (l->data), ==, "example");
  g_list_free_full (l, g_object_unref);
}

static void
test_list (Test *test,
           gconstpointer data)
{
  GAsyncResult *res = NULL;
  GList *cms;

  tp_list_connection_managers_async (test->dbus, tp_tests_result_ready_cb,
      &res);
  tp_tests_run_until_result (&res);

  cms = tp_list_connection_managers_finish (res, &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpuint (g_list_length (cms), ==, 2);

  /* transfer ownership */
  if (tp_connection_manager_is_running (cms->data))
    {
      test->echo = cms->data;
      test->spurious = cms->next->data;
    }
  else
    {
      test->spurious = cms->data;
      test->echo = cms->next->data;
    }

  g_object_unref (res);
  g_list_free (cms);

  g_assert (tp_connection_manager_is_running (test->echo));
  g_assert (!tp_connection_manager_is_running (test->spurious));

  g_assert (tp_proxy_is_prepared (test->echo,
        TP_CONNECTION_MANAGER_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->spurious,
        TP_CONNECTION_MANAGER_FEATURE_CORE));

  g_assert (tp_proxy_get_invalidated (test->echo) == NULL);
  g_assert (tp_proxy_get_invalidated (test->spurious) == NULL);

  g_assert_cmpuint (tp_connection_manager_get_info_source (test->echo),
      ==, TP_CM_INFO_SOURCE_LIVE);
  g_assert_cmpuint (tp_connection_manager_get_info_source (test->spurious),
      ==, TP_CM_INFO_SOURCE_FILE);

  g_assert (tp_connection_manager_has_protocol (test->echo, "example"));
  g_assert (tp_connection_manager_has_protocol (test->spurious, "normal"));
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
  g_test_add ("/cm/file", Test, GINT_TO_POINTER (0),
      setup, test_file_ready, teardown);
  g_test_add ("/cm/file/complex", Test, GINT_TO_POINTER (0), setup,
      test_complex_file_ready, teardown);
  g_test_add ("/cm/dbus", Test, GINT_TO_POINTER (0), setup,
      test_dbus_ready, teardown);
  g_test_add ("/cm/dbus/activate", Test, GINT_TO_POINTER (ACTIVATE_CM),
      setup, test_dbus_ready, teardown);

  g_test_add ("/cm/dbus/dies", Test,
      GINT_TO_POINTER (DROP_NAME_ON_GET),
      setup, test_dbus_ready, teardown);

  g_test_add ("/cm/dbus/dies-twice", Test,
      GINT_TO_POINTER (DROP_NAME_ON_GET_TWICE),
      setup, test_dbus_ready, teardown);

  g_test_add ("/cm/list", Test, GINT_TO_POINTER (0),
      setup, test_list, teardown);

  return tp_tests_run_with_bus ();
}
