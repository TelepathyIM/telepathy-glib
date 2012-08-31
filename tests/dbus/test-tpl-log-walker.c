#include "config.h"

#include <string.h>

#include "lib/simple-account.h"
#include "lib/util.h"

#include "telepathy-logger/call-event.h"
#include "telepathy-logger/debug-internal.h"
#include "telepathy-logger/log-manager.h"
#include "telepathy-logger/text-event.h"

#include <telepathy-glib/telepathy-glib.h>
#include <glib.h>

#define DEBUG_FLAG TPL_DEBUG_TESTSUITE


typedef struct
{
  GList *events;
  GMainLoop *main_loop;
  TplLogManager *manager;
  TpAccount *account;
  TpDBusDaemon *bus;
  TpSimpleClientFactory *factory;
  TpTestsSimpleAccount *account_service;
} WalkerTestCaseFixture;


static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WalkerTestCaseFixture *fixture = user_data;
  GError *error = NULL;

  tp_proxy_prepare_finish (source, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->main_loop);
}


static void
setup (WalkerTestCaseFixture* fixture,
    gconstpointer user_data)
{
  GArray *features;
  GError *error = NULL;
  GHashTable *params = (GHashTable *) user_data;
  GValue *boxed_params;
  const gchar *account_path;

  fixture->main_loop = g_main_loop_new (NULL, FALSE);
  g_assert (fixture->main_loop != NULL);

  fixture->manager = tpl_log_manager_dup_singleton ();

  fixture->bus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (fixture->bus != NULL);

  tp_dbus_daemon_request_name (fixture->bus,
      TP_ACCOUNT_MANAGER_BUS_NAME,
      FALSE,
      &error);
  g_assert_no_error (error);

  /* Create service-side Account object with the passed parameters */
  fixture->account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      NULL);
  g_assert (fixture->account_service != NULL);

  /* account-path will be set-up as parameter as well, this is not an issue */
  account_path = tp_asv_get_string (params, "account-path");
  g_assert (account_path != NULL);

  boxed_params = tp_g_value_slice_new_boxed (TP_HASH_TYPE_STRING_VARIANT_MAP,
      params);
  g_object_set_property (G_OBJECT (fixture->account_service),
      "parameters",
      boxed_params);
  tp_g_value_slice_free (boxed_params);

  tp_dbus_daemon_register_object (fixture->bus,
      account_path,
      fixture->account_service);

  fixture->factory = tp_simple_client_factory_new (fixture->bus);
  g_assert (fixture->factory != NULL);

  fixture->account = tp_simple_client_factory_ensure_account (fixture->factory,
      tp_asv_get_string (params, "account-path"),
      params,
      &error);
  g_assert_no_error (error);
  g_assert (fixture->account != NULL);

  features = tp_simple_client_factory_dup_account_features (fixture->factory,
      fixture->account);

  tp_proxy_prepare_async (fixture->account,
      (GQuark *) features->data,
      account_prepare_cb,
      fixture);
  g_free (features->data);
  g_array_free (features, FALSE);

  g_main_loop_run (fixture->main_loop);

  tp_debug_divert_messages (g_getenv ("TPL_LOGFILE"));

#ifdef ENABLE_DEBUG
  _tpl_debug_set_flags_from_env ();
#endif /* ENABLE_DEBUG */
}


static void
teardown (WalkerTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GError *error = NULL;

  tp_dbus_daemon_release_name (fixture->bus,
      TP_ACCOUNT_MANAGER_BUS_NAME,
      &error);
  g_assert_no_error (error);

  g_clear_object (&fixture->account);
  g_clear_object (&fixture->factory);

  tp_dbus_daemon_unregister_object (fixture->bus, fixture->account_service);
  g_clear_object (&fixture->account_service);

  g_clear_object (&fixture->bus);
  g_clear_object (&fixture->manager);
  g_main_loop_unref (fixture->main_loop);
}


static gboolean
filter_events (TplEvent *event, gpointer user_data)
{
  const gchar *message;

  message = tpl_text_event_get_message (TPL_TEXT_EVENT (event));
  return strstr (message, "'") == NULL;
}


static void
rewind_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WalkerTestCaseFixture *fixture = user_data;
  GError *error = NULL;

  tpl_log_walker_rewind_finish (TPL_LOG_WALKER (source),
      result,
      &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->main_loop);
}


static void
rewind (WalkerTestCaseFixture *fixture,
    TplLogWalker *walker,
    guint num_events)
{
  tpl_log_walker_rewind_async (walker, num_events, rewind_cb, fixture);
  g_main_loop_run (fixture->main_loop);
}


static void
get_events_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  WalkerTestCaseFixture *fixture = user_data;
  GError *error = NULL;

  tpl_log_walker_get_events_finish (TPL_LOG_WALKER (source),
      result,
      &fixture->events,
      &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->main_loop);
}


static void
get_events (WalkerTestCaseFixture *fixture,
    TplLogWalker *walker,
    guint num_events)
{
  tpl_log_walker_get_events_async (walker, num_events, get_events_cb, fixture);
  g_main_loop_run (fixture->main_loop);
}


static void
test_get_events_call (WalkerTestCaseFixture *fixture,
    TplLogWalker *walker,
    guint num_events,
    gint64 timestamp,
    GTimeSpan duration)
{
  GList *events;

  get_events (fixture, walker, num_events);

  events = fixture->events;
  g_assert (events != NULL);
  g_assert_cmpuint (g_list_length (events), ==, num_events);
  g_assert_cmpint (tpl_event_get_timestamp (TPL_EVENT (events->data)),
      ==,
      timestamp);
  g_assert_cmpint (tpl_call_event_get_duration (TPL_CALL_EVENT (events->data)),
      ==,
      duration);
  g_list_free_full (events, g_object_unref);
}


static void
test_get_events_text (WalkerTestCaseFixture *fixture,
    TplLogWalker *walker,
    guint num_events,
    gint64 timestamp,
    const gchar *message)
{
  GList *events;

  get_events (fixture, walker, num_events);

  events = fixture->events;
  g_assert (events != NULL);
  g_assert_cmpuint (g_list_length (events), ==, num_events);
  g_assert_cmpint (tpl_event_get_timestamp (TPL_EVENT (events->data)),
      ==,
      timestamp);
  g_assert_cmpstr (tpl_text_event_get_message (TPL_TEXT_EVENT (events->data)),
      ==,
      message);
  g_list_free_full (events, g_object_unref);
}


static void
test_get_events (WalkerTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *events;
  TplEntity *user5;
  TplLogWalker *walker;

  user5 = tpl_entity_new ("user5@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User5", "");

  /* Both text and call events without a filter */
  walker = tpl_log_manager_walk_filtered_events (fixture->manager,
      fixture->account,
      user5,
      TPL_EVENT_MASK_ANY,
      NULL,
      NULL);

  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 2, 1263427264, "L''");
  test_get_events_text (fixture, walker, 5, 1263427262, "J");
  test_get_events_text (fixture, walker, 1, 1263427261, "I'''");
  test_get_events_text (fixture, walker, 5, 1263427205, "12");
  test_get_events_text (fixture, walker, 2, 1263427202, "11'");
  test_get_events_call (fixture, walker, 4, 1263404881, 1);
  test_get_events_text (fixture, walker, 4, 1263254401, "5''");
  test_get_events_text (fixture, walker, 2, 1263254401, "5");
  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 3, 1263168066, "H'");
  test_get_events_text (fixture, walker, 3, 1263168065, "G''");
  test_get_events_text (fixture, walker, 6, 1263168063, "E");
  test_get_events_text (fixture, walker, 1, 1263168062, "D''");
  test_get_events_text (fixture, walker, 2, 1263168062, "D");
  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 4, 1263168005, "4");
  test_get_events_text (fixture, walker, 2, 1263168003, "2");
  test_get_events_text (fixture, walker, 4, 1263081661, "A");

  tpl_log_walker_get_events_async (walker, 2, get_events_cb, fixture);
  g_main_loop_run (fixture->main_loop);

  events = fixture->events;
  g_assert (events == NULL);

  g_object_unref (walker);

  /* Only text events with a filter */
  walker = tpl_log_manager_walk_filtered_events (fixture->manager,
      fixture->account,
      user5,
      TPL_EVENT_MASK_TEXT,
      filter_events,
      NULL);

  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 2, 1263427263, "K");
  test_get_events_text (fixture, walker, 5, 1263427202, "11");
  test_get_events_text (fixture, walker, 1, 1263427201, "10");
  test_get_events_text (fixture, walker, 5, 1263254401, "5");
  test_get_events_text (fixture, walker, 2, 1263168065, "G");
  test_get_events_text (fixture, walker, 4, 1263168061, "C");
  test_get_events_text (fixture, walker, 2, 1263168004, "3");
  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 3, 1263168001, "0");
  test_get_events_text (fixture, walker, 2, 1263081661, "A");

  tpl_log_walker_get_events_async (walker, 2, get_events_cb, fixture);
  g_main_loop_run (fixture->main_loop);

  events = fixture->events;
  g_assert (events == NULL);

  g_object_unref (walker);
  g_object_unref (user5);
}


static void
test_rewind (WalkerTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TplEntity *user5;
  TplLogWalker *walker;

  user5 = tpl_entity_new ("user5@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User5", "");

  /* Both text and call events without a filter */
  walker = tpl_log_manager_walk_filtered_events (fixture->manager,
      fixture->account,
      user5,
      TPL_EVENT_MASK_ANY,
      NULL,
      NULL);

  rewind (fixture, walker, 8);
  get_events (fixture, walker, 0);
  rewind (fixture, walker, 8);
  get_events (fixture, walker, 2);
  rewind (fixture, walker, 8);
  test_get_events_text (fixture, walker, 8, 1263427261, "I'''");
  rewind (fixture, walker, 3);
  test_get_events_text (fixture, walker, 5, 1263427261, "I'");
  rewind (fixture, walker, 1);
  test_get_events_text (fixture, walker, 7, 1263427202, "11");
  rewind (fixture, walker, 2);
  test_get_events_call (fixture, walker, 5, 1263404881, 1);
  rewind (fixture, walker, 2);
  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 1, 1263404950, "9");
  rewind (fixture, walker, 0);
  test_get_events_text (fixture, walker, 5, 1263254401, "5''");
  rewind (fixture, walker, 1);
  test_get_events_text (fixture, walker, 8, 1263168065, "G'''");
  rewind (fixture, walker, 7);
  test_get_events_text (fixture, walker, 7, 1263168065, "G'''");
  test_get_events_text (fixture, walker, 7, 1263168063, "E");
  rewind (fixture, walker, 2);
  test_get_events_text (fixture, walker, 6, 1263168061, "C");
  rewind (fixture, walker, 10);
  rewind (fixture, walker, 0);
  rewind (fixture, walker, 5);
  test_get_events_text (fixture, walker, 16, 1263168005, "4''");
  rewind (fixture, walker, 3);
  test_get_events_text (fixture, walker, 6, 1263168004, "3");
  rewind (fixture, walker, 1);
  test_get_events_text (fixture, walker, 6, 1263081661, "A");

  tpl_log_walker_get_events_async (walker, 2, get_events_cb, fixture);
  g_main_loop_run (fixture->main_loop);
  g_assert (fixture->events == NULL);

  g_object_unref (walker);

  /* Only text events with a filter */
  walker = tpl_log_manager_walk_filtered_events (fixture->manager,
      fixture->account,
      user5,
      TPL_EVENT_MASK_TEXT,
      filter_events,
      NULL);

  rewind (fixture, walker, 8);
  get_events (fixture, walker, 0);
  rewind (fixture, walker, 8);
  get_events (fixture, walker, 2);
  rewind (fixture, walker, 8);
  test_get_events_text (fixture, walker, 8, 1263427201, "10");
  rewind (fixture, walker, 3);
  test_get_events_text (fixture, walker, 5, 1263254406, "8");
  rewind (fixture, walker, 1);
  test_get_events_text (fixture, walker, 7, 1263168064, "F");
  rewind (fixture, walker, 2);
  test_get_events_text (fixture, walker, 5, 1263168061, "C");
  rewind (fixture, walker, 2);
  get_events (fixture, walker, 0);
  test_get_events_text (fixture, walker, 1, 1263168062, "D");
  rewind (fixture, walker, 0);
  test_get_events_text (fixture, walker, 5, 1263168002, "1");
  rewind (fixture, walker, 1);
  test_get_events_text (fixture, walker, 4, 1263081661, "A");

  tpl_log_walker_get_events_async (walker, 2, get_events_cb, fixture);
  g_main_loop_run (fixture->main_loop);
  g_assert (fixture->events == NULL);

  g_object_unref (walker);
  g_object_unref (user5);
}


gint main (gint argc, gchar **argv)
{
  GHashTable *params;
  gint retval;

  g_type_init ();

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_assert (params != NULL);

  g_hash_table_insert (params, "account",
      tp_g_value_slice_new_static_string ("user@collabora.co.uk"));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (
          TP_ACCOUNT_OBJECT_PATH_BASE
          "gabble/jabber/user_40collabora_2eco_2euk"));

  g_test_add ("/log-walker/get-events",
      WalkerTestCaseFixture, params,
      setup, test_get_events, teardown);

  g_test_add ("/log-walker/rewind",
      WalkerTestCaseFixture, params,
      setup, test_rewind, teardown);

  retval = g_test_run ();

  g_hash_table_unref (params);

  return retval;
}
