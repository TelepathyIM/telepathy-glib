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

#include "tests/lib/util.h"

typedef struct {
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;

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

  test->account_manager = tp_account_manager_dup ();
  g_assert (test->account_manager != NULL);

  test->account = NULL;
}

static void
teardown (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_clear_object (&test->account);

  /* make sure any pending calls on the account have happened, so it can die */
  tp_tests_proxy_run_until_dbus_queue_processed (test->dbus);

  g_clear_object (&test->dbus);
  tp_clear_pointer (&test->mainloop, g_main_loop_unref);

  g_clear_error (&test->error);
  tp_clear_object (&test->result);
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

  test->account = tp_future_account_new (test->account_manager,
      "gabble", "jabber");

  g_object_get (test->account,
      "properties", &props,
      NULL);

  g_assert_cmpuint (g_hash_table_size (props), ==, 0);

  g_hash_table_unref (props);
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

  return g_test_run ();
}
