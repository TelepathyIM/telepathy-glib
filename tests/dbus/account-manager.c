/* A very basic feature test for TpAccountManager
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

#include "tests/lib/simple-account-manager.h"

typedef struct {
    GFunc action;
    gpointer user_data;
} ScriptAction;

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    DBusGConnection *bus;

    SimpleAccountManager *service /* initialized in prepare_service */;
    TpAccountManager *am;
    TpAccount *account;
    gboolean prepared /* The result of prepare_finish */;
    guint timeout_id;
    GQueue *script /* A list of GAsyncReadyCallback */;

    GError *error /* initialized where needed */;
} Test;

/**
  * Functions for manipulating scripts follow this comment.
  * In order to be generally useful, the script should probably be stored in its
  * own data structure, rather than passed around with the test, and the
  * user_data argument would need to be the test or something. As it is, these
  * library-like functions rely on being defined after the Test struct.
  */
static ScriptAction *
script_action_new (GFunc action,
    gpointer data)
{
  ScriptAction *script_action = g_new (ScriptAction, 1);
  script_action->action = action;
  script_action->user_data = data;
  return script_action;
}

static void
script_action_free (ScriptAction *action)
{
  g_free (action);
}

/**
  * If data is passed in, you are responsible for freeing it. This will not be
  * done for you.
  */
static void
script_append_action (Test *test,
    GFunc action,
    gpointer data)
{
  g_queue_push_tail (test->script, script_action_new (action, data));
}

static void
script_continue (gpointer script_data)
{
  Test *test = (Test *) script_data;
  ScriptAction *action;
  /* pop the next action */
  action = (ScriptAction *) g_queue_pop_head (test->script);
  action->action (script_data, action->user_data);
  script_action_free (action);
}

static gboolean
test_timed_out (gpointer data)
{
  Test *test = (Test *) data;
  g_assert_not_reached ();
  test->prepared = FALSE;
  /* Note that this is a completely bogus error, but it only gets returned if
   * you comment out the g_assert_not_reached() above. */
  test->error = g_error_new_literal (TP_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
                                     "timeout");
  g_print ("about to quit");
  g_main_loop_quit (test->mainloop);
  g_print ("just quit");
  return FALSE;
}

static void
quit_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;
  g_main_loop_quit (test->mainloop);
}

static void
script_start_with_deadline (Test *test,
    guint timeout)
{
  script_append_action (test, quit_action, NULL);
  test->timeout_id = g_timeout_add (timeout, test_timed_out, test);
  script_continue (test);
  g_main_loop_run (test->mainloop);
}

/**
  * Setup and teardown functions follow this comment.
  */

static void
setup (Test *test,
    gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);

  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->am = NULL;
  test->timeout_id = 0;
  test->script = g_queue_new ();
}

static void
setup_service (Test *test,
    gconstpointer data)
{

  setup (test, data);

  test->bus = tp_get_bus ();

  g_assert (tp_dbus_daemon_request_name (test->dbus,
          TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &test->error));

  test->service = g_object_new (SIMPLE_TYPE_ACCOUNT_MANAGER, NULL);
  dbus_g_connection_register_g_object (test->bus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      (GObject *) test->service);
}

static void
teardown (Test *test,
    gconstpointer data)
{
  if (test->am != NULL)
    {
      g_object_unref (test->am);
      test->am = NULL;
    }
  if (test->timeout_id != 0)
    {
      g_source_remove (test->timeout_id);
      test->timeout_id = 0;
    }
  g_queue_free (test->script);
  test->script = NULL;
  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
teardown_service (Test *test,
    gconstpointer data)
{
  script_start_with_deadline (test, 1000);
  g_assert (
      tp_dbus_daemon_release_name (test->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
                               &test->error));
  dbus_g_connection_unregister_g_object (test->bus, G_OBJECT (test->service));
  g_object_unref (test->service);
  test->service = NULL;
  teardown (test, data);
}

/**
  * Non-dbus tests follow this comment
  */

static void
test_new (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test->am = tp_account_manager_new (test->dbus);
}

static void
test_dup (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAccountManager *one, *two;
  TpDBusDaemon *dbus_one, *dbus_two;

  one = tp_account_manager_dup ();
  two = tp_account_manager_dup ();

  g_assert (one == two);

  dbus_one = tp_dbus_daemon_dup (NULL);
  dbus_two = tp_proxy_get_dbus_daemon (one);

  g_assert (dbus_one == dbus_two);

  g_object_unref (dbus_one);
  g_object_unref (two);
  g_object_unref (one);
}

/**
  * Actions for use with script_append_action() follow this comment. They are
  * used in tests which involve asyncronous actions.
  */

static void
noop_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  script_continue (script_data);
}

static void
finish_prepare_action (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  Test *test = (Test *) user_data;
  gboolean is_prepared_reply;
  TpAccountManager *am = TP_ACCOUNT_MANAGER (source_object);

  g_assert (test->am == am);
  test->prepared = tp_account_manager_prepare_finish (am, res, &test->error);
  is_prepared_reply = tp_account_manager_is_prepared (test->am,
      TP_ACCOUNT_MANAGER_FEATURE_CORE);
  g_assert_cmpint (is_prepared_reply, ==, test->prepared);
  script_continue (test);
}

static void
prepare_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  tp_account_manager_prepare_async (test->am, NULL, finish_prepare_action, test);
}

static void
manager_new_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  test->am = tp_account_manager_new (test->dbus);
  script_continue (test);
}

/* We really don't want to have MC being launched during this test */
static void
finish_assert_am_not_activatable_action (TpDBusDaemon *proxy,
    const gchar * const *names,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  guint i;

  g_assert (error == NULL);

  for (i=0; names[i] != NULL; i++)
    {
      g_assert_cmpstr (names[i], !=, TP_ACCOUNT_MANAGER_BUS_NAME);
      g_assert_cmpstr (names[i], !=, "org.freedesktop.Telepathy.MissionControl5");
    }

  script_continue (user_data);
}

static void
assert_am_not_activatable_action (gpointer script_data,
    gpointer user_data)
{
  Test *test = (Test *) script_data;

  tp_dbus_daemon_list_activatable_names (test->dbus, 500,
      finish_assert_am_not_activatable_action, test, NULL, NULL);
}

static void
assert_core_not_ready_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  g_assert (!tp_account_manager_is_prepared (test->am,
      TP_ACCOUNT_MANAGER_FEATURE_CORE));

  script_continue (script_data);
}

static void
assert_feature_not_ready_action (gpointer script_data,
    gpointer user_data)
{
  Test *test = (Test *) script_data;

  g_assert (!tp_account_manager_is_prepared (test->am,
      g_quark_from_string ((gchar *) user_data)));

  g_free (user_data);
  script_continue (script_data);
}

static void
prepare_feature_action (gpointer script_data,
    gpointer user_data)
{
  Test *test = (Test *) script_data;
  GQuark features[3];

  features[0] = TP_ACCOUNT_MANAGER_FEATURE_CORE;
  features[1] = g_quark_from_string ((gchar *) user_data);
  features[2] = 0;

  tp_account_manager_prepare_async (test->am, features, finish_prepare_action, test);

  g_free (user_data);
}

static void
assert_ok_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  g_assert_no_error (test->error);
  g_assert (test->prepared);

  script_continue (script_data);
}

static void
assert_failed_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  g_assert (test->error != NULL);
  g_error_free (test->error);
  test->error = NULL;

  script_continue (script_data);
}

/**
  * account related functions below this comment
  */

static void
ensure_action (gpointer script_data,
    gpointer user_data)
{
  char *path = (char *) user_data;
  Test *test = (Test *) script_data;
  g_assert (test != NULL);
  g_assert (test->am != NULL);
  g_assert (tp_account_manager_is_prepared (test->am, TP_ACCOUNT_MANAGER_FEATURE_CORE));
  test->account = tp_account_manager_ensure_account (test->am,
      path);

  script_continue (script_data);
}

static void
assert_account_ok_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;
  g_assert (test->account != NULL);

  script_continue (script_data);
}

static void
finish_account_prepare_action (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  Test *test = (Test *) user_data;
  TpAccount *account = TP_ACCOUNT (source_object);

  g_assert (test->account == account);
  test->prepared = tp_account_prepare_finish (account, res, &test->error);
  g_assert (test->prepared == tp_account_is_prepared (account, TP_ACCOUNT_FEATURE_CORE));

  script_continue (test);
}

static void
account_prepare_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  tp_account_prepare_async (test->account, NULL, finish_account_prepare_action, test);
}

static void
register_service_action (gpointer script_data,
    gpointer user_data G_GNUC_UNUSED)
{
  Test *test = (Test *) script_data;

  dbus_g_connection_register_g_object (test->bus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      (GObject *) test->service);

  script_continue (test);
}

/**
  * Asyncronous tests below this comment. Tests append action functions and
  * arguments to a script. Once the test function has returned, the teardown
  * function is responsible for running the script, and quitting the mainloop
  * afterwards.
  * Action functions are each responsible for ensuring that the next action is
  * called.
  */

static void
test_prepare (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  script_append_action (test, assert_am_not_activatable_action, NULL);
  script_append_action (test, manager_new_action, NULL);
  script_append_action (test, assert_core_not_ready_action, NULL);
  script_append_action (test, prepare_action, NULL);
  script_append_action (test, noop_action, NULL);
}

/**
 * Tests the usual case where prepare succeeds.
 */
static void
test_prepare_success (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);
  script_append_action (test, assert_ok_action, NULL);
}

/**
 * Tests the case where the well-known name is not provided.
 * This should be run with setup rather than setup_service to make this the case.
 * TODO: use g_assert_error (err, dom, c) to fix the domain and code.
 */
static void
test_prepare_no_name (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);

  script_append_action (test, assert_failed_action, NULL);
  /* Since we are using teardown rather than teardown_service, we need to
   * run the script ourselves */
  script_start_with_deadline (test, 1000);
}

/**
 * Tests the case where the object has been destroyed.
 * TODO: use g_assert_error (err, dom, c) to fix the domain and code.
 */
static void
test_prepare_destroyed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  dbus_g_connection_unregister_g_object (test->bus, G_OBJECT (test->service));
  test_prepare (test, data);
  script_append_action (test, assert_failed_action, NULL);
  script_append_action (test, register_service_action, NULL);
}

/**
 * Calling prepare with unknown features should succeed, but is_prepared()
 * on an unknown feature should return FALSE.
 */
static void
test_prepare_unknown_features (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test_prepare_success (test, data);
  script_append_action (test, prepare_feature_action, g_strdup ("fake-feature"));
  script_append_action (test, assert_ok_action, NULL);
  script_append_action (test, assert_feature_not_ready_action, g_strdup ("fake-feature"));
}

static void
test_ensure (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  test_prepare (test, data);
  script_append_action (test, assert_ok_action, NULL);

  script_append_action (test, ensure_action,
                        TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/account");
  script_append_action (test, assert_account_ok_action, NULL);
  script_append_action (test, account_prepare_action, NULL);
  script_append_action (test, assert_failed_action, NULL);
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/am/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/am/dup", Test, NULL, setup, test_dup, teardown);

  g_test_add ("/am/prepare/success", Test, NULL, setup_service,
              test_prepare_success, teardown_service);
  g_test_add ("/am/prepare/destroyed", Test, NULL, setup_service,
              test_prepare_destroyed, teardown_service);
  /* WARNING: This test is run using setup/teardown rather than setup_service*/
  g_test_add ("/am/prepare/name-not-provided", Test, NULL, setup,
              test_prepare_no_name, teardown);
  g_test_add ("/am/prepare/unknown_features", Test, NULL, setup_service,
              test_prepare_unknown_features, teardown_service);

  g_test_add ("/am/ensure", Test, NULL, setup_service,
              test_ensure, teardown_service);
  return g_test_run ();
}
