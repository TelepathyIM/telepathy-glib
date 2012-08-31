#include "config.h"

#include "lib/simple-account.h"
#include "lib/util.h"

#include "telepathy-logger/debug-internal.h"
#include "telepathy-logger/log-iter-internal.h"
#include "telepathy-logger/log-iter-pidgin-internal.h"
#include "telepathy-logger/log-store-pidgin-internal.h"
#include "telepathy-logger/text-event.h"

#include <telepathy-glib/telepathy-glib.h>
#include <glib.h>

#define DEBUG_FLAG TPL_DEBUG_TESTSUITE


typedef struct
{
  GMainLoop *main_loop;
  TplLogStore *store;
  TpAccount *account;
  TpDBusDaemon *bus;
  TpSimpleClientFactory *factory;
  TpTestsSimpleAccount *account_service;
} PidginTestCaseFixture;


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
setup (PidginTestCaseFixture* fixture,
    gconstpointer user_data)
{
  GArray *features;
  GError *error = NULL;
  GHashTable *params = (GHashTable *) user_data;
  GValue *boxed_params;
  const gchar *account_path;

  fixture->main_loop = g_main_loop_new (NULL, FALSE);
  g_assert (fixture->main_loop != NULL);

  fixture->store = g_object_new (TPL_TYPE_LOG_STORE_PIDGIN,
      "testmode", TRUE,
      NULL);

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
teardown (PidginTestCaseFixture *fixture,
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
  g_clear_object (&fixture->store);
  g_main_loop_unref (fixture->main_loop);
}


static void
test_get_events (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TplEntity *room;
  TplLogIter *iter;
  GList *events;
  GError *error = NULL;
  const gchar *message;
  gint64 timestamp;

  room = tpl_entity_new_from_room_id ("#telepathy");

  iter = tpl_log_iter_pidgin_new (fixture->store, fixture->account, room,
      TPL_EVENT_MASK_ANY);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291133254);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "tbh it&apos;s not necessarily too niche to have in telepathy-spec");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 3);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291133097);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "I think that&apos;s better than modifying the client libraries");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291133035);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "oh right I thought by &quot;alongside&quot; you meant in o.fd.T.AM");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 7, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 7);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291132904);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "you&apos;re just moving the incompatibility into the client libraries");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 1, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 1);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291132892);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "if the libraries hide those accounts by default, that&apos;s no more "
      "compatible than changing the D-Bus API");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291132838);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "alternative possibly less-beating-worthy proposals include just "
      "adding the flag to the account and then modifying tp-{glib,qt4,...} "
      "to hide &apos;em by default");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 10, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131885);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "wjt: hrm, can you disco remote servers for their jud and does gabble "
      "do that if needed or does it rely on the given server being the jud?");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 4, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 4);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131667);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "one of whose possible values is the dreaded NetworkError");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131614);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "nod");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 3);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131587);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "ejabberd isn&apos;t even telling me why it&apos;s disconnecting some "
      "test accounts");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131566);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "Heh");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 7, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 7);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131502);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "if the server provides &lt;text/&gt;, use that; otherwise, use a "
      "locally-supplied debug string");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 1, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 1);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131493);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "MattJ: what language is the &lt;text&gt; in btw?");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131480);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "hey");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 10, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131383);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "Good :)");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131350);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "that&apos;s mostly fixed though");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 3);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131335);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "\\o\\ /o/");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131288);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "Good that a proper register interface is getting higher on the todo "
      "list");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 7, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 7);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130982);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "no biscuit.");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 1, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 1);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130967);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "no gitorious merge request.");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130885);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "pessi: Hi, I fixed some bugs in ring: "
      "http://git.collabora.co.uk/?p=user/jonny/telepathy-ring.git;a="
      "shortlog;h=refs/heads/trivia");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 10, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130110);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "i guess the collabora xmpp server does privacy list-based "
      "invisibility, so it&apos;s only doing what i asked");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 4, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 4);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130015);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "MattJ: so about that xep-0186 support? ;-)");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291129872);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "Oh, i noticed that our iq request queue somethings fill up and then "
      "doesn&apos;t seem to get unstuck");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 3);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291129805);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "huh");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291128926);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "kkszysiu, heya; i seem to remember you were hacking on a "
      "im-via-web-using-telepathy stuff? how&apos;s that going? i&apos;d be "
      "interested in doing something along the same lines");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 7, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 7);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291126346);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "invisible&apos;s a good idea. we do implement xmpp ping");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 1, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 1);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291126340);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "oh yeah, dwd implemented google:queue in M-Link");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 2, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 2);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291126290);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "not sure if we implement this one");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 8, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 8);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291123078);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "those who like contact lists: "
      "https://bugs.freedesktop.org/show_bug.cgi?id=31997");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events == NULL);

  g_object_unref (iter);
  g_object_unref (room);
}


static void
test_rewind (PidginTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TplEntity *room;
  TplLogIter *iter;
  GList *events;
  GError *error = NULL;
  const gchar *message;
  gint64 timestamp;

  room = tpl_entity_new_from_room_id ("#telepathy");

  iter = tpl_log_iter_pidgin_new (fixture->store, fixture->account, room,
      TPL_EVENT_MASK_ANY);

  tpl_log_iter_rewind (iter, 8, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 0, &error);
  g_assert_no_error (error);
  g_assert (events == NULL);

  tpl_log_iter_rewind (iter, 8, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291133254);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "tbh it&apos;s not necessarily too niche to have in telepathy-spec");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 8, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 5, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 5);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291133254);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "tbh it&apos;s not necessarily too niche to have in telepathy-spec");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 20, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 20);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291132137);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "wjt: we should probably cope with both cases.. i wonder if jud server "
      "correctly indicate in a disco response that they&apos;re the jud "
      "server");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 7, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 17, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 17);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131655);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "the primary thing to present is a D-Bus error code which UIs are "
      "expected to localize");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 7, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 13, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 13);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131595);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "There are vague errors like &quot;bad-request&quot; or "
      "&quot;not-authorized&quot; where Prosody usually gives more specific "
      "information about why the error occured");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 17, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 33, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 33);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131445);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "dear ejabberd, why are you not showing your xep 55 in your disco "
      "response");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 5, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 10, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131401);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "the UI doesn&apos;t show it though");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 25, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 10, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131537);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "well, s/you/this channel/");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 25, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 25);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291131335);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "\\o\\ /o/");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 3, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 15, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 15);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130885);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "pessi: Hi, I fixed some bugs in ring: "
      "http://git.collabora.co.uk/?p=user/jonny/telepathy-ring.git;a="
      "shortlog;h=refs/heads/trivia");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 1, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 10, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 10);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291130210);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "wjt, how can you test if you are actually invisible? The account "
      "presence is always sync with your real status?");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 7, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 20, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 20);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291129805);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "huh");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 23, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 20, &error);
  events = events;
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 20);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291129872);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "Oh, i noticed that our iq request queue somethings fill up and then "
      "doesn&apos;t seem to get unstuck");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 3, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 20, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 20);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291126206);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "invisible is a good one");
  g_list_free_full (events, g_object_unref);

  tpl_log_iter_rewind (iter, 3, &error);
  g_assert_no_error (error);

  events = tpl_log_iter_get_events (iter, 9, &error);
  g_assert_no_error (error);
  g_assert (events != NULL);
  g_assert_cmpint (g_list_length (events), ==, 9);
  timestamp = tpl_event_get_timestamp (TPL_EVENT (events->data));
  g_assert_cmpint (timestamp, ==, 1291123078);
  message = tpl_text_event_get_message (TPL_TEXT_EVENT (events->data));
  g_assert_cmpstr (message,
      ==,
      "those who like contact lists: "
      "https://bugs.freedesktop.org/show_bug.cgi?id=31997");
  g_list_free_full (events, g_object_unref);

  events = tpl_log_iter_get_events (iter, 3, &error);
  g_assert_no_error (error);
  g_assert (events == NULL);

  g_object_unref (iter);
  g_object_unref (room);
}


gint
main (gint argc, gchar **argv)
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
      tp_g_value_slice_new_static_string ("user"));
  g_hash_table_insert (params, "server",
      tp_g_value_slice_new_static_string ("irc.freenode.net"));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (
          TP_ACCOUNT_OBJECT_PATH_BASE "foo/irc/baz"));

  g_test_add ("/log-iter-xml/get-events",
      PidginTestCaseFixture, params,
      setup, test_get_events, teardown);

  g_test_add ("/log-iter-xml/rewind",
      PidginTestCaseFixture, params,
      setup, test_rewind, teardown);

  retval = g_test_run ();

  g_hash_table_unref (params);

  return retval;
}
