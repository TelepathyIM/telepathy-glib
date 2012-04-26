/* A very basic feature test for TpFutureAccount
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/future-account.h>

#include "tests/lib/simple-account.h"
#include "tests/lib/simple-account-manager.h"
#include "tests/lib/util.h"

typedef struct {
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;

  TpTestsSimpleAccountManager *am;
  TpTestsSimpleAccount *account_service;

  TpAccountManager *account_manager;
  TpFutureAccount *account;

  GAsyncResult *result;
  GError *error /* initialized where needed */;
} Test;

static void
setup (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  /* create the account manager service */
  tp_dbus_daemon_request_name (test->dbus,
      TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);
  test->am = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_ACCOUNT_MANAGER, NULL);
  tp_dbus_daemon_register_object (test->dbus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      test->am);

  /* and now the account manager proxy */
  test->account_manager = tp_account_manager_dup ();
  g_assert (test->account_manager != NULL);

  /* finally create the account service */
  test->account_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
  tp_dbus_daemon_register_object (test->dbus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/lospolloshermanos",
      test->account_service);

  test->account = NULL;
}

static void
teardown (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_clear_object (&test->account);

  tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);
  tp_dbus_daemon_unregister_object (test->dbus, test->am);
  g_clear_object (&test->am);

  tp_dbus_daemon_unregister_object (test->dbus, test->account_service);
  g_clear_object (&test->account_service);

  g_clear_object (&test->dbus);
  tp_clear_pointer (&test->mainloop, g_main_loop_unref);

  g_clear_error (&test->error);
  g_clear_object (&test->result);
}

static void
test_new (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");
  g_assert (TP_IS_FUTURE_ACCOUNT (test->account));
}

static void
test_gobject_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccountManager *am;
  gchar *manager, *protocol, *display_name;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  tp_future_account_set_display_name (test->account, "Charles Dickens");

  g_object_get (test->account,
      "account-manager", &am,
      "connection-manager", &manager,
      "protocol", &protocol,
      "display-name", &display_name,
      NULL);

  g_assert (am == test->account_manager);
  g_assert_cmpstr (manager, ==, "gabble");
  g_assert_cmpstr (protocol, ==, "jabber");
  g_assert_cmpstr (display_name, ==, "Charles Dickens");

  g_object_unref (am);
  g_free (manager);
  g_free (protocol);
  g_free (display_name);
}

static void
test_parameters (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GVariant *v_str, *v_int;
  GHashTable *params;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  v_str = g_variant_new_string ("banana");
  tp_future_account_set_parameter (test->account, "cheese", v_str);

  v_int = g_variant_new_uint32 (42);
  tp_future_account_set_parameter (test->account, "life", v_int);

  tp_future_account_set_parameter_string (test->account,
      "great", "expectations");

  g_object_get (test->account,
      "parameters", &params,
      NULL);

  g_assert_cmpuint (g_hash_table_size (params), ==, 3);

  g_assert_cmpstr (tp_asv_get_string (params, "cheese"), ==, "banana");
  g_assert_cmpuint (tp_asv_get_uint32 (params, "life", NULL), ==, 42);
  g_assert_cmpstr (tp_asv_get_string (params, "great"), ==, "expectations");

  g_variant_unref (v_str);
  g_variant_unref (v_int);

  g_hash_table_unref (params);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *props;
  gchar *icon_name, *nickname;
  TpConnectionPresenceType presence_type;
  gchar *presence_status, *presence_message;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  g_object_get (test->account,
      "properties", &props,
      NULL);

  g_assert_cmpuint (g_hash_table_size (props), ==, 0);

  g_hash_table_unref (props);

  /* now set an icon and try again */
  tp_future_account_set_icon_name (test->account, "user32.dll");

  g_object_get (test->account,
      "properties", &props,
      "icon-name", &icon_name,
      NULL);

  g_assert_cmpuint (g_hash_table_size (props), ==, 1);
  g_assert_cmpstr (tp_asv_get_string (props, "Icon"), ==, "user32.dll");
  g_assert_cmpstr (icon_name, ==, "user32.dll");

  g_hash_table_unref (props);
  g_free (icon_name);

  /* now set the nickname and try again */
  tp_future_account_set_nickname (test->account, "Walter Jr.");

  g_object_get (test->account,
      "properties", &props,
      "nickname", &nickname,
      NULL);

  g_assert_cmpuint (g_hash_table_size (props), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (props, "Icon"), ==, "user32.dll");
  g_assert_cmpstr (tp_asv_get_string (props, "Nickname"), ==, "Walter Jr.");
  g_assert_cmpstr (nickname, ==, "Walter Jr.");

  g_hash_table_unref (props);
  g_free (nickname);

  /* next is requested presence */
  tp_future_account_set_requested_presence (test->account,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, "available",
      "come at me, bro!");

  g_object_get (test->account,
      "requested-presence-type", &presence_type,
      "requested-status", &presence_status,
      "requested-status-message", &presence_message,
      NULL);

  g_assert_cmpuint (presence_type, ==, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (presence_status, ==, "available");
  g_assert_cmpstr (presence_message, ==, "come at me, bro!");

  g_free (presence_status);
  g_free (presence_message);
}

static void
test_create_succeed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccount *account;
  GValueArray *array;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  tp_future_account_set_display_name (test->account, "Walter White");
  tp_future_account_set_icon_name (test->account, "gasmask");
  tp_future_account_set_nickname (test->account, "Heisenberg");
  tp_future_account_set_requested_presence (test->account,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, "available",
      "Better call Saul!");

  tp_future_account_set_parameter_string (test->account,
      "account", "walter@white.us");
  tp_future_account_set_parameter_string (test->account,
      "password", "holly");

  tp_future_account_create_account_async (test->account,
      tp_tests_result_ready_cb, &test->result);
  tp_tests_run_until_result (&test->result);

  account = tp_future_account_create_account_finish (test->account,
      test->result, &test->error);
  g_assert_no_error (test->error);
  g_assert (account != NULL);

  g_assert_cmpstr (test->am->create_cm, ==, "gabble");
  g_assert_cmpstr (test->am->create_protocol, ==, "jabber");
  g_assert_cmpstr (test->am->create_display_name, ==, "Walter White");
  g_assert_cmpuint (g_hash_table_size (test->am->create_parameters), ==, 2);
  g_assert_cmpstr (tp_asv_get_string (test->am->create_parameters, "account"),
      ==, "walter@white.us");
  g_assert_cmpstr (tp_asv_get_string (test->am->create_parameters, "password"),
      ==, "holly");
  g_assert_cmpuint (g_hash_table_size (test->am->create_properties), ==, 3);
  g_assert_cmpstr (tp_asv_get_string (test->am->create_properties, "Icon"),
      ==, "gasmask");
  g_assert_cmpstr (tp_asv_get_string (test->am->create_properties, "Nickname"),
      ==, "Heisenberg");

  array = tp_asv_get_boxed (test->am->create_properties, "RequestedPresence",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE);
  g_assert_cmpuint (g_value_get_uint (array->values), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  g_assert_cmpstr (g_value_get_string (array->values + 1), ==,
      "available");
  g_assert_cmpstr (g_value_get_string (array->values + 2), ==,
      "Better call Saul!");

  g_object_unref (account);
}

static void
test_create_fail (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccount *account;

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  tp_future_account_set_display_name (test->account, "Walter White");

  /* this will make CreateChannel fail */
  tp_future_account_set_parameter_string (test->account,
      "fail", "yes");

  tp_future_account_create_account_async (test->account,
      tp_tests_result_ready_cb, &test->result);
  tp_tests_run_until_result (&test->result);

  account = tp_future_account_create_account_finish (test->account,
      test->result, &test->error);
  g_assert (test->error != NULL);
  g_assert (account == NULL);

  g_clear_error (&test->error);
}

int
main (int argc,
    char **argv)
{
  g_type_init ();
  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/future-account/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/future-account/gobject-properties", Test, NULL, setup,
      test_gobject_properties, teardown);
  g_test_add ("/future-account/parameters", Test, NULL, setup,
      test_parameters, teardown);
  g_test_add ("/future-account/properties", Test, NULL, setup,
      test_properties, teardown);
  g_test_add ("/future-account/create-succeed", Test, NULL, setup,
      test_create_succeed, teardown);
  g_test_add ("/future-account/create-fail", Test, NULL, setup,
      test_create_fail, teardown);

  return g_test_run ();
}
