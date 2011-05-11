/* FIXME: hugly kludge: we need to include all the declarations which are used
 * by the GInterface and thus not in the -internal.h */
#include "telepathy-logger/log-store-pidgin.c"


#include "lib/util.h"
#include "lib/simple-account.h"
#include "lib/simple-account-manager.h"

#include <telepathy-logger/log-store-pidgin-internal.h>
#include <telepathy-logger/text-event-internal.h>

#include <telepathy-glib/debug-sender.h>

/* it was defined in telepathy-logger/log-store-pidgin.c */
#undef DEBUG_FLAG
#define DEBUG_FLAG TPL_DEBUG_TESTSUITE
#include <telepathy-logger/debug-internal.h>

#include <glib.h>

#define ACCOUNT_PATH_JABBER TP_ACCOUNT_OBJECT_PATH_BASE "foo/jabber/baz"
#define ACCOUNT_PATH_IRC    TP_ACCOUNT_OBJECT_PATH_BASE "foo/irc/baz"
#define ACCOUNT_PATH_ICQ    TP_ACCOUNT_OBJECT_PATH_BASE "foo/icq/baz"

typedef struct
{
  gchar *basedir;

  GMainLoop *main_loop;

  TpDBusDaemon *dbus;
  TpAccount *account;
  TpTestsSimpleAccount *account_service;

  TplLogStorePidgin *store;
  TplEntity *room;
  TplEntity *irc_room;
  TplEntity *contact;
} PidginTestCaseFixture;

#ifdef ENABLE_DEBUG
static TpDebugSender *debug_sender = NULL;
static gboolean stamp_logs = FALSE;


static void
log_to_debug_sender (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *string)
{
  GTimeVal now;

  g_return_if_fail (TP_IS_DEBUG_SENDER (debug_sender));

  g_get_current_time (&now);

  tp_debug_sender_add_message (debug_sender, &now, log_domain, log_level,
      string);
}


static void
log_handler (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  if (stamp_logs)
    {
      GTimeVal now;
      gchar now_str[32];
      gchar *tmp;
      struct tm tm;

      g_get_current_time (&now);
      localtime_r (&(now.tv_sec), &tm);
      strftime (now_str, 32, "%Y-%m-%d %H:%M:%S", &tm);
      tmp = g_strdup_printf ("%s.%06ld: %s",
        now_str, now.tv_usec, message);

      g_log_default_handler (log_domain, log_level, tmp, NULL);

      g_free (tmp);
    }
  else
    {
      g_log_default_handler (log_domain, log_level, message, NULL);
    }

  log_to_debug_sender (log_domain, log_level, message);
}
#endif /* ENABLE_DEBUG */


static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  PidginTestCaseFixture *fixture = user_data;
  GError *error = NULL;

  tp_proxy_prepare_finish (source, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->main_loop);
}


static void
setup_service (PidginTestCaseFixture* fixture,
    gconstpointer user_data)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  const gchar *account_path;
  GValue *boxed_params;
  GHashTable *params = (GHashTable *) user_data;
  GError *error = NULL;

  g_assert (params != NULL);

  fixture->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (fixture->dbus != NULL);

  tp_dbus_daemon_request_name (fixture->dbus,
      TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &error);
  g_assert_no_error (error);

  /* Create service-side Account object with the passed parameters */
  fixture->account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      NULL);
  g_assert (fixture->account_service != NULL);

  /* account-path will be set-up as parameter as well, this is not an issue */
  account_path = g_value_get_string (
      (const GValue *) g_hash_table_lookup (params, "account-path"));
  g_assert (account_path != NULL);

  boxed_params = tp_g_value_slice_new_boxed (TP_HASH_TYPE_STRING_VARIANT_MAP,
      params);
  g_object_set_property (G_OBJECT (fixture->account_service),
      "parameters", boxed_params);

  tp_dbus_daemon_register_object (fixture->dbus, account_path,
      fixture->account_service);

  fixture->account = tp_account_new (fixture->dbus, account_path, NULL);
  g_assert (fixture->account != NULL);
  tp_proxy_prepare_async (fixture->account, account_features,
      account_prepare_cb, fixture);
  g_main_loop_run (fixture->main_loop);

  g_assert (tp_account_is_prepared (fixture->account,
        TP_ACCOUNT_FEATURE_CORE));

  tp_g_value_slice_free (boxed_params);
}

static void
setup (PidginTestCaseFixture* fixture,
    gconstpointer user_data)
{
  DEBUG ("setting up");

  fixture->main_loop = g_main_loop_new (NULL, FALSE);
  g_assert (fixture->main_loop != NULL);

  fixture->basedir = g_build_path (G_DIR_SEPARATOR_S,
      g_getenv ("TPL_TEST_LOG_DIR"), "purple", NULL);
  DEBUG ("basedir is %s", fixture->basedir);

  fixture->store = g_object_new (TPL_TYPE_LOG_STORE_PIDGIN,
      "name", "testcase",
      "testmode", TRUE,
      NULL);

  fixture->room = tpl_entity_new_from_room_id (
      "test@conference.collabora.co.uk");

  fixture->irc_room = tpl_entity_new_from_room_id ("#telepathy");

  fixture->contact = tpl_entity_new ("user2@collabora.co.uk",
      TPL_ENTITY_CONTACT, NULL, NULL);

  if (user_data != NULL)
    setup_service (fixture, user_data);

  DEBUG ("set up finished");
}

static void
teardown_service (PidginTestCaseFixture* fixture,
    gconstpointer user_data)
{
  GError *error = NULL;

  g_assert (user_data != NULL);

  if (fixture->account != NULL)
    {
      /* FIXME is it useful in this suite */
      tp_tests_proxy_run_until_dbus_queue_processed (fixture->account);

      g_object_unref (fixture->account);
      fixture->account = NULL;
    }

  tp_dbus_daemon_unregister_object (fixture->dbus, fixture->account_service);
  g_object_unref (fixture->account_service);
  fixture->account_service = NULL;

  tp_dbus_daemon_release_name (fixture->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &error);
  g_assert_no_error (error);

  g_object_unref (fixture->dbus);
  fixture->dbus = NULL;
}

static void
teardown (PidginTestCaseFixture* fixture,
    gconstpointer user_data)
{
  g_free (fixture->basedir);
  fixture->basedir = NULL;

  g_object_unref (fixture->store);
  fixture->store = NULL;

  g_object_unref (fixture->room);
  g_object_unref (fixture->irc_room);
  g_object_unref (fixture->contact);

  if (user_data != NULL)
    teardown_service (fixture, user_data);

  g_main_loop_unref (fixture->main_loop);
  fixture->main_loop = NULL;
}

static void
test_basedir (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TplLogStorePidgin *store;
  gchar *dir;

  g_assert_cmpstr (log_store_pidgin_get_basedir (fixture->store), ==,
      fixture->basedir);

  /* try to instantiate the default store, without passing basedir, it has to
   * match the real libpurple basedir */
  store = g_object_new (TPL_TYPE_LOG_STORE_PIDGIN,
      "name", "testcase",
      "readable", FALSE,
      "writable", FALSE,
      NULL);
  dir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".purple",
      "logs", NULL);
  g_assert_cmpstr (log_store_pidgin_get_basedir (store), ==, dir);

  g_object_unref (store);
  g_free (dir);
}

static void
test_get_dates_jabber (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *dates = NULL;
  GDate *date = NULL;

  /* Chatroom messages */
  dates = log_store_pidgin_get_dates (TPL_LOG_STORE (fixture->store),
      fixture->account, fixture->room, TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (dates), ==, 2);

  date = g_list_nth_data (dates, 0);
  g_assert_cmpint (0, ==,
      g_date_compare (date, g_date_new_dmy (12, G_DATE_APRIL, 2010)));

  g_date_free (date);

  date = g_list_nth_data (dates, 1);
  g_assert_cmpint (0, ==,
      g_date_compare (date, g_date_new_dmy (29, G_DATE_APRIL, 2010)));

  g_date_free (date);
  g_list_free (dates);

  /* 1-1 messages */
  dates = log_store_pidgin_get_dates (TPL_LOG_STORE (fixture->store),
      fixture->account, fixture->contact, TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (dates), ==, 1);

  date = g_list_nth_data (dates, 0);
  g_assert_cmpint (0, ==,
      g_date_compare (date, g_date_new_dmy (10, G_DATE_DECEMBER, 2010)));

  g_date_free (date);
  g_list_free (dates);
}

static void
test_get_dates_irc (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *dates = NULL;
  GDate *date = NULL;

  dates = log_store_pidgin_get_dates (TPL_LOG_STORE (fixture->store),
      fixture->account,
      fixture->irc_room,
      TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (dates), ==, 1);

  date = g_list_nth_data (dates, 0);
  g_assert_cmpint (0, ==,
      g_date_compare (date, g_date_new_dmy (30, G_DATE_NOVEMBER, 2010)));

  g_list_foreach (dates, (GFunc) g_date_free, NULL);
  g_list_free (dates);
}

static void
test_get_time (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GDate *date;

  date = log_store_pidgin_get_time ("2010-04-29.140346+0100BST.html");

  g_assert_cmpint (g_date_get_day (date), ==, 29);
  g_assert_cmpint (g_date_get_month (date), ==, G_DATE_APRIL);
  g_assert_cmpint (g_date_get_year (date), ==, 2010);

  g_date_free (date);
}

static void
test_get_name (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  const gchar *name;

  name = log_store_pidgin_get_name (TPL_LOG_STORE (fixture->store));

  g_assert_cmpstr (name, ==, "testcase");
}

static void
test_get_events_for_date_jabber (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *l;
  TplTextEvent *msg = NULL;
  GDate *date = g_date_new_dmy (12, G_DATE_APRIL, 2010);

  /* chatroom messages */
  l = log_store_pidgin_get_events_for_date (TPL_LOG_STORE (fixture->store),
      fixture->account,
      fixture->room,
      TPL_EVENT_MASK_ANY,
      date);

  g_assert_cmpint (g_list_length (l), ==, 6);

  msg = g_list_nth_data (l, 0);
  g_assert (_tpl_event_target_is_room (TPL_EVENT (msg)) == TRUE);
  g_assert_cmpstr (tpl_text_event_get_message (msg), ==, "1");

  g_list_foreach (l, (GFunc) g_object_unref, NULL);
  g_list_free (l);

  /* 1-1 messages */
  g_date_set_dmy (date, 10, G_DATE_DECEMBER, 2010);
  l = log_store_pidgin_get_events_for_date (TPL_LOG_STORE (fixture->store),
      fixture->account,
      fixture->contact,
      TPL_EVENT_MASK_ANY,
      date);

  g_assert_cmpint (g_list_length (l), ==, 2);

  msg = g_list_nth_data (l, 0);
  g_assert (_tpl_event_target_is_room (TPL_EVENT (msg)) == FALSE);
  g_assert_cmpstr (tpl_text_event_get_message (msg), ==, "hi");

  g_list_foreach (l, (GFunc) g_object_unref, NULL);
  g_list_free (l);

  g_date_free (date);
}

static int
cmp_entities (gconstpointer a,
    gconstpointer b)
{
  return -1 * g_strcmp0 (
      tpl_entity_get_identifier (TPL_ENTITY (a)),
      tpl_entity_get_identifier (TPL_ENTITY (b)));
}

static void
test_get_entities_jabber (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *l = NULL;
  TplEntity *entity;

  l = log_store_pidgin_get_entities (TPL_LOG_STORE (fixture->store),
      fixture->account);

  g_assert_cmpint (g_list_length (l), ==, 2);

  /* sort the entities, since their ordering depends on the file order */
  l = g_list_sort (l, cmp_entities);

  entity = g_list_nth_data (l, 0);
  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==,
      "user2@collabora.co.uk");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_CONTACT);

  entity = g_list_nth_data (l, 1);
  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==,
      "test@conference.collabora.co.uk");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_ROOM);

  g_list_foreach (l, (GFunc) g_object_unref, NULL);
  g_list_free (l);
}

static void
test_search_new (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *l = NULL;

  /* empty search */
  l = log_store_pidgin_search_new (TPL_LOG_STORE (fixture->store),
      "I do not exist in this log store data base!",
      TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (l), ==, 0);

  tpl_log_manager_search_free (l);

  /* non empty search matching 1-1 */
  l = log_store_pidgin_search_new (TPL_LOG_STORE (fixture->store),
      "hey you",
      TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (l), ==, 1);

  tpl_log_manager_search_free (l);

  /* non empty search, checking chatrooms are also searched */
  l = log_store_pidgin_search_new (TPL_LOG_STORE (fixture->store),
      "disco remote servers",
      TPL_EVENT_MASK_ANY);

  g_assert_cmpint (g_list_length (l), ==, 1);

  tpl_log_manager_search_free (l);
}

static void
test_get_events_for_empty_file (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *l = NULL;
  TplEntity *entity;
  GDate *date;

  entity = tpl_entity_new ("87654321", TPL_ENTITY_CONTACT, NULL, NULL);

  /* Check with empty file */
  date = g_date_new_dmy (7, 2, 2010);

  l = log_store_pidgin_get_events_for_date (TPL_LOG_STORE (fixture->store),
      fixture->account, entity, TPL_EVENT_MASK_ANY, date);

  g_assert_cmpint (g_list_length (l), ==, 0);
  g_date_free (date);

  /* Check with file that contains null bytes */
  date = g_date_new_dmy (6, 2, 2010);

  l = log_store_pidgin_get_events_for_date (TPL_LOG_STORE (fixture->store),
      fixture->account, entity, TPL_EVENT_MASK_ANY, date);

  g_assert_cmpint (g_list_length (l), ==, 0);
  g_date_free (date);

  g_object_unref (entity);
}

static void
setup_debug (void)
{
  tp_debug_divert_messages (g_getenv ("TPL_LOGFILE"));

#ifdef ENABLE_DEBUG
  _tpl_debug_set_flags_from_env ();

  stamp_logs = (g_getenv ("TPL_TIMING") != NULL);
  debug_sender = tp_debug_sender_dup ();

  g_log_set_default_handler (log_handler, NULL);
#endif /* ENABLE_DEBUG */
}


int
main (int argc, char **argv)
{
  GHashTable *params = NULL;
  GList *l = NULL;
  int retval;

  g_type_init ();

  setup_debug ();

  /* no account tests */
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/log-store-pidgin/get-name",
      PidginTestCaseFixture, NULL,
      setup, test_get_name, teardown);

  g_test_add ("/log-store-pidgin/get-time",
      PidginTestCaseFixture, NULL,
      setup, test_get_time, teardown);

  /* this searches all over the account in the log stores */
  g_test_add ("/log-store-pidgin/search-new",
      PidginTestCaseFixture, NULL,
      setup, test_search_new, teardown);

  /* jabber account tests */
  params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_assert (params != NULL);

  l = g_list_prepend (l, params);

  g_hash_table_insert (params, "account",
      tp_g_value_slice_new_static_string ("user@collabora.co.uk"));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (ACCOUNT_PATH_JABBER));

  g_test_add ("/log-store-pidgin/basedir",
      PidginTestCaseFixture, params,
      setup, test_basedir, teardown);

  g_test_add ("/log-store-pidgin/get-dates-jabber",
      PidginTestCaseFixture, params,
      setup, test_get_dates_jabber, teardown);

  g_test_add ("/log-store-pidgin/get-events-for-date-jabber",
      PidginTestCaseFixture, params,
      setup, test_get_events_for_date_jabber, teardown);

  g_test_add ("/log-store-pidgin/get-entities-jabber",
      PidginTestCaseFixture, params,
      setup, test_get_entities_jabber, teardown);

  /* IRC account tests */
  params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_assert (params != NULL);

  l = g_list_prepend (l, params);

  g_hash_table_insert (params, "account",
      tp_g_value_slice_new_static_string ("user"));
  g_hash_table_insert (params, "server",
      tp_g_value_slice_new_static_string ("irc.freenode.net"));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (ACCOUNT_PATH_IRC));

  g_test_add ("/log-store-pidgin/get-dates-irc",
      PidginTestCaseFixture, params,
      setup, test_get_dates_irc, teardown);

  /* Empty file */
  params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_assert (params != NULL);

  l = g_list_prepend (l, params);

  g_hash_table_insert (params, "account",
      tp_g_value_slice_new_static_string ("12345678"));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (ACCOUNT_PATH_ICQ));

  g_test_add ("/log-store-pidgin/get-event-for-empty-file",
      PidginTestCaseFixture, params,
      setup, test_get_events_for_empty_file, teardown);

  retval = g_test_run ();

  g_list_foreach (l, (GFunc) g_hash_table_unref, NULL);

  return retval;
}
