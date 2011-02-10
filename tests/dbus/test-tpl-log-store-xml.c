#include "telepathy-logger/log-store-xml.c"

#include "telepathy-logger/log-manager-internal.h"
#include "telepathy-logger/log-store-internal.h"
#include "lib/util.h"

#include <glib.h>

typedef struct
{
  gchar *tmp_basedir;
  TplLogStoreXml *store;
  TpDBusDaemon *bus;
} XmlTestCaseFixture;


static void
copy_dir (const gchar *from_dir, const gchar *to_dir)
{
  gchar *command;

  // If destination directory exist erase it
  command = g_strdup_printf ("rm -rf %s", to_dir);
  g_assert (system (command) == 0);
  g_free (command);

  command = g_strdup_printf ("cp -r %s %s", from_dir, to_dir);
  g_assert (system (command) == 0);
  g_free (command);
}


static void
setup (XmlTestCaseFixture* fixture,
    gconstpointer user_data)
{
  fixture->store = g_object_new (TPL_TYPE_LOG_STORE_XML,
      "name", "testcase",
      "testmode", TRUE,
      NULL);

  if (fixture->tmp_basedir != NULL)
    log_store_xml_set_basedir (fixture->store, fixture->tmp_basedir);

  fixture->bus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (fixture->bus != NULL);
}


static void
setup_for_writing (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  gchar *readonly_dir;
  gchar *writable_dir;

  readonly_dir = g_build_path (G_DIR_SEPARATOR_S,
      g_getenv ("TPL_TEST_LOG_DIR"), "TpLogger", "logs", NULL);

  writable_dir = g_build_path (G_DIR_SEPARATOR_S,
      g_get_tmp_dir (), "logger-test-logs", NULL);

  copy_dir (readonly_dir, writable_dir);
  fixture->tmp_basedir = writable_dir;
  g_free (readonly_dir);

  setup (fixture, user_data);
}


static void
teardown (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  if (fixture->tmp_basedir != NULL)
    {
      gchar *command = g_strdup_printf ("rm -rf %s", fixture->tmp_basedir);
      system (command);
      g_free (fixture->tmp_basedir);
    }

  if (fixture->store == NULL)
    g_object_unref (fixture->store);
}


static void
test_clear (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *hits;
  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      "1263405203");

  g_assert (hits != NULL);
  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  _tpl_log_store_clear (TPL_LOG_STORE (fixture->store));

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      "1263405203");

  g_assert_cmpint (g_list_length (hits), ==, 0);
}


static void
test_clear_account (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *hits;
  TpAccount *account;
  GError *error = NULL;
  const gchar *kept = "1263405203";
  const gchar *cleared = "f95e605a3ae97c463b626a3538567bc90fc58730";

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      cleared);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/test2_40collabora_2eco_2euk0",
      &error);

  g_assert_no_error (error);
  g_assert (account != NULL);

  _tpl_log_store_clear_account (TPL_LOG_STORE (fixture->store), account);
  g_object_unref (account);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      cleared);

  g_assert_cmpint (g_list_length (hits), ==, 0);
}


static void
test_clear_entity (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  gboolean is_room = GPOINTER_TO_INT (user_data);
  GList *hits;
  TpAccount *account;
  TplEntity *entity;
  GError *error = NULL;
  const gchar *always_kept, *kept, *cleared;

  always_kept = "1263405203";

  if (is_room)
    {
      kept = "f95e605a3ae97c463b626a3538567bc90fc58730";
      cleared = "8957fb4064049e7a1f9d8f84234d3bf09fb6778c";
    }
  else
    {
      kept = "8957fb4064049e7a1f9d8f84234d3bf09fb6778c";
      cleared = "f95e605a3ae97c463b626a3538567bc90fc58730";
    }

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      always_kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      cleared);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/test2_40collabora_2eco_2euk0",
      &error);

  g_assert_no_error (error);
  g_assert (account != NULL);

  if (is_room)
    entity = g_object_new (TPL_TYPE_ENTITY,
        "type", TPL_ENTITY_ROOM,
        "identifier", "meego@conference.collabora.co.uk",
        NULL);
  else
    entity = g_object_new (TPL_TYPE_ENTITY,
        "type", TPL_ENTITY_CONTACT,
        "identifier", "derek.foreman@collabora.co.uk",
        NULL);

  _tpl_log_store_clear_entity (TPL_LOG_STORE (fixture->store), account, entity);
  g_object_unref (account);
  g_object_unref (entity);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      always_kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      kept);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (TPL_LOG_STORE (fixture->store),
      cleared);

  g_assert_cmpint (g_list_length (hits), ==, 0);
}


gint main (gint argc, gchar **argv)
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/log-store-xml/clear",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_clear, teardown);

  g_test_add ("/log-store-xml/clear-account",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_clear_account, teardown);

  g_test_add ("/log-store-xml/clear-entity",
      XmlTestCaseFixture, GINT_TO_POINTER (FALSE),
      setup_for_writing, test_clear_entity, teardown);

  g_test_add ("/log-store-xml/clear-entity-room",
      XmlTestCaseFixture, GINT_TO_POINTER (TRUE),
      setup_for_writing, test_clear_entity, teardown);

  return g_test_run ();
}
