/* A very basic feature test for TpAccount
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/account.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

#include "tests/lib/util.h"
#include "tests/lib/simple-account.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

static void
test_parse_failure (gconstpointer test_data)
{
  GError *error = NULL;

  g_assert (!tp_account_parse_object_path (test_data, NULL, NULL, NULL,
      &error));
  g_assert (error != NULL);
  g_error_free (error);
}

typedef struct {
    const gchar *path;
    const gchar *cm;
    const gchar *protocol;
    const gchar *account_id;
} TestParseData;

static TestParseData *
test_parse_data_new (const gchar *path,
    const gchar *cm,
    const gchar *protocol,
    const gchar *account_id)
{
  TestParseData *t = g_slice_new (TestParseData);

  t->path = path;
  t->cm = cm;
  t->protocol = protocol;
  t->account_id = account_id;

  return t;
}

static void
test_parse_success (gconstpointer test_data)
{
  TestParseData *t = (TestParseData *) test_data;
  gchar *cm, *protocol, *account_id;
  GError *error = NULL;

  g_assert (tp_account_parse_object_path (t->path, &cm, &protocol, &account_id,
      &error));
  g_assert_no_error (error);
  g_assert_cmpstr (cm, ==, t->cm);
  g_assert_cmpstr (protocol, ==, t->protocol);
  g_assert_cmpstr (account_id, ==, t->account_id);

  g_free (cm);
  g_free (protocol);
  g_free (account_id);

  g_slice_free (TestParseData, t);
}

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpAccount *account;
    GError *error /* initialized where needed */;

    SimpleAccount *account_service /* initialized in prepare_service */;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = test_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->account = NULL;
}

static void
setup_service (Test *test,
    gconstpointer data)
{
  setup (test, data);

  tp_dbus_daemon_request_name (test->dbus,
      TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error);
  g_assert_no_error (test->error);

  test->account_service = g_object_new (SIMPLE_TYPE_ACCOUNT, NULL);

  dbus_g_connection_register_g_object (
      tp_proxy_get_dbus_connection (test->dbus), ACCOUNT_PATH,
      G_OBJECT (test->account_service));
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->account != NULL)
    {
      g_object_unref (test->account);
      test->account = NULL;
    }

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
teardown_service (Test *test,
    gconstpointer data)
{
  tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &test->error);
  g_assert_no_error (test->error);

  dbus_g_connection_unregister_g_object (
      tp_proxy_get_dbus_connection (test->dbus),
      G_OBJECT (test->account_service));

  g_object_unref (test->account_service);
  test->account_service = NULL;

  teardown (test, data);
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  test->account = tp_account_new (test->dbus,
      "/secretly/not/an/object", NULL);
  g_assert (test->account == NULL);

  test->account = tp_account_new (test->dbus,
      "not even syntactically valid", NULL);
  g_assert (test->account == NULL);

  test->account = tp_account_new (test->dbus,
      "/org/freedesktop/Telepathy/Account/what/ev/er", NULL);
  g_assert (test->account != NULL);
}

static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;
  GError *error = NULL;

  tp_account_prepare_finish (TP_ACCOUNT (source), result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (test->mainloop);
}

static void
test_prepare_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };

  test->account = tp_account_new (test->dbus, ACCOUNT_PATH, NULL);
  g_assert (test->account != NULL);

  tp_account_prepare_async (test->account, account_features,
      account_prepare_cb, test);
  g_main_loop_run (test->mainloop);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_data_func ("/account/parse/spaces",
      "this is not an object path", test_parse_failure);
  g_test_add_data_func ("/account/parse/no-prefix",
      "/this/is/not/an/account/path", test_parse_failure);
  g_test_add_data_func ("/account/parse/too-few-components",
      "/org/freedesktop/Telepathy/Account/wrong", test_parse_failure);
  g_test_add_data_func ("/account/parse/too-many-components",
      "/org/freedesktop/Telepathy/Account/a/b/c/d", test_parse_failure);
  g_test_add_data_func ("/account/parse/illegal-components",
      "/org/freedesktop/Telepathy/Account/1/2/3", test_parse_failure);

  g_test_add_data_func ("/account/parse/legal",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/badgers",
          "gabble", "jabber", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/hyphenated-protocol",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_xmpp/badgers",
          "salut", "local-xmpp", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/wrongly-escaped-protocol",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_2dxmpp/badgers",
          "salut", "local-xmpp", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/wrongly-escaped-corner-case",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "salut/local_2d/badgers",
          "salut", "local-", "badgers"),
      test_parse_success);
  g_test_add_data_func ("/account/parse/underscored-account",
      test_parse_data_new (
          TP_ACCOUNT_OBJECT_PATH_BASE "haze/msn/_thisseemsunlikely",
          "haze", "msn", "_thisseemsunlikely"),
      test_parse_success);

  g_test_add ("/account/new", Test, NULL, setup, test_new, teardown);

  g_test_add ("/account/prepare/success", Test, NULL, setup_service,
              test_prepare_success, teardown_service);

  return g_test_run ();
}
