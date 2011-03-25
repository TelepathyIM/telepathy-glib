#include "telepathy-logger/log-store-xml.c"

#include "telepathy-logger/log-manager-internal.h"
#include "telepathy-logger/log-store-internal.h"
#include "lib/util.h"

#include <glib.h>

typedef struct
{
  gchar *tmp_basedir;
  TplLogStore *store;
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

  // In distcheck mode the files and directory are read-only, fix that
  command = g_strdup_printf ("chmod -R +w %s", to_dir);
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
    log_store_xml_set_basedir (TPL_LOG_STORE_XML (fixture->store),
        fixture->tmp_basedir);

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
      int res;

      res = system (command);
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
  hits = _tpl_log_store_search_new (fixture->store,
      "user@collabora.co.uk",
      TPL_EVENT_MASK_TEXT);

  g_assert (hits != NULL);
  g_assert_cmpint (g_list_length (hits), ==, 4);

  tpl_log_manager_search_free (hits);

  _tpl_log_store_clear (fixture->store);

  hits = _tpl_log_store_search_new (fixture->store,
      "user@collabora.co.uk",
      TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 0);
}


static void
test_clear_account (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *hits;
  TpAccount *account;
  GError *error = NULL;
  const gchar *kept = "user2@collabora.co.uk";
  const gchar *cleared = "test2@collabora.co.uk";

  hits = _tpl_log_store_search_new (fixture->store,
      kept, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 4);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store,
      cleared, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/test2_40collabora_2eco_2euk0",
      &error);

  g_assert_no_error (error);
  g_assert (account != NULL);

  _tpl_log_store_clear_account (fixture->store, account);
  g_object_unref (account);

  hits = _tpl_log_store_search_new (fixture->store, kept, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 4);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store, cleared,
      TPL_EVENT_MASK_TEXT);

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

  always_kept = "user2@collabora.co.uk";

  if (is_room)
    {
      kept = "Hey, Just generating logs";
      cleared = "meego@conference.collabora.co.uk/test2@collabora.co.uk";
    }
  else
    {
      kept = "meego@conference.collabora.co.uk/test2@collabora.co.uk";
      cleared = "Hey, Just generating logs";
    }

  hits = _tpl_log_store_search_new (fixture->store, always_kept,
      TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 4);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store, kept, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store, cleared,
      TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/test2_40collabora_2eco_2euk0",
      &error);

  g_assert_no_error (error);
  g_assert (account != NULL);

  if (is_room)
    entity = tpl_entity_new_from_room_id ("meego@conference.collabora.co.uk");
  else
    entity = tpl_entity_new ("derek.foreman@collabora.co.uk",
        TPL_ENTITY_CONTACT, NULL, NULL);

  _tpl_log_store_clear_entity (fixture->store, account, entity);
  g_object_unref (account);
  g_object_unref (entity);

  hits = _tpl_log_store_search_new (fixture->store,
      always_kept, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 4);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store, kept, TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 1);

  tpl_log_manager_search_free (hits);

  hits = _tpl_log_store_search_new (fixture->store, cleared,
      TPL_EVENT_MASK_TEXT);

  g_assert_cmpint (g_list_length (hits), ==, 0);
}


static void
assert_cmp_text_event (TplEvent *event,
    TplEvent *stored_event)
{
  TplEntity *sender, *stored_sender;
  TplEntity *receiver, *stored_receiver;

  g_assert (TPL_IS_TEXT_EVENT (event));
  g_assert (TPL_IS_TEXT_EVENT (stored_event));
  g_assert_cmpstr (tpl_event_get_account_path (event), ==,
      tpl_event_get_account_path (stored_event));

  sender = tpl_event_get_sender (event);
  stored_sender = tpl_event_get_sender (stored_event);

  g_assert (_tpl_entity_compare (sender, stored_sender) == 0);
  g_assert_cmpstr (tpl_entity_get_alias (sender), ==,
      tpl_entity_get_alias (stored_sender));
  g_assert_cmpstr (tpl_entity_get_avatar_token (sender), ==,
      tpl_entity_get_avatar_token (stored_sender));

  receiver = tpl_event_get_receiver (event);
  stored_receiver = tpl_event_get_receiver (stored_event);

  g_assert (_tpl_entity_compare (receiver, stored_receiver) == 0);
  /* No support for receiver alias/token */

  g_assert_cmpint (tpl_event_get_timestamp (event), ==,
      tpl_event_get_timestamp (stored_event));
  g_assert_cmpint (tpl_text_event_get_message_type (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_message_type (TPL_TEXT_EVENT (stored_event)));
  g_assert_cmpstr (tpl_text_event_get_message (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_message (TPL_TEXT_EVENT (stored_event)));
}


static void
test_add_text_event (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TpAccount *account;
  TplEntity *me, *contact, *room;
  TplEvent *event;
  GError *error = NULL;
  GList *events;
  gint64 timestamp = time (NULL);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "idle/irc/me",
      &error);
  g_assert_no_error (error);
  g_assert (account != NULL);

  me = tpl_entity_new ("me", TPL_ENTITY_SELF, "my-alias", "my-avatar");
  contact = tpl_entity_new ("contact", TPL_ENTITY_CONTACT, "contact-alias",
      "contact-token");
  room = tpl_entity_new_from_room_id ("room");


  /* 1. Outgoing message to a contact */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_TEXT_EVENT (events->data));

  assert_cmp_text_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 2. Incoming message from contact (a /me action) */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", me,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_TEXT_EVENT (events->data));

  assert_cmp_text_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 3. Outgoing message to a room */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", room,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_TEXT_EVENT (events->data));

  assert_cmp_text_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 4. Incoming message from a room */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", room,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_TEXT_EVENT (events->data));

  assert_cmp_text_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);
}


static void
assert_cmp_call_event (TplEvent *event,
    TplEvent *stored_event)
{
  TplEntity *sender, *stored_sender;
  TplEntity *receiver, *stored_receiver;
  TplEntity *actor, *stored_actor;

  g_assert (TPL_IS_CALL_EVENT (event));
  g_assert (TPL_IS_CALL_EVENT (stored_event));
  g_assert_cmpstr (tpl_event_get_account_path (event), ==,
      tpl_event_get_account_path (stored_event));

  sender = tpl_event_get_sender (event);
  stored_sender = tpl_event_get_sender (stored_event);

  g_assert (_tpl_entity_compare (sender, stored_sender) == 0);
  g_assert_cmpstr (tpl_entity_get_alias (sender), ==,
      tpl_entity_get_alias (stored_sender));
  g_assert_cmpstr (tpl_entity_get_avatar_token (sender), ==,
      tpl_entity_get_avatar_token (stored_sender));

  receiver = tpl_event_get_receiver (event);
  stored_receiver = tpl_event_get_receiver (stored_event);

  g_assert (_tpl_entity_compare (receiver, stored_receiver) == 0);
  /* No support for receiver alias/token */

  g_assert_cmpint (tpl_event_get_timestamp (event), ==,
      tpl_event_get_timestamp (stored_event));

  g_assert_cmpint (tpl_call_event_get_duration (TPL_CALL_EVENT (event)),
      ==, tpl_call_event_get_duration (TPL_CALL_EVENT (stored_event)));

  actor = tpl_call_event_get_end_actor (TPL_CALL_EVENT (event));
  stored_actor = tpl_call_event_get_end_actor (TPL_CALL_EVENT (stored_event));

  g_print ("%s == %s\n", tpl_entity_get_identifier (actor), tpl_entity_get_identifier (stored_actor));
  g_print ("%i == %i\n", tpl_entity_get_entity_type (actor), tpl_entity_get_entity_type (stored_actor));
  g_assert (_tpl_entity_compare (actor, stored_actor) == 0);
  g_assert_cmpstr (tpl_entity_get_alias (actor), ==,
      tpl_entity_get_alias (stored_actor));
  g_assert_cmpstr (tpl_entity_get_avatar_token (actor), ==,
      tpl_entity_get_avatar_token (stored_actor));
  g_assert_cmpstr (
      tpl_call_event_get_detailed_end_reason (TPL_CALL_EVENT (event)),
      ==,
      tpl_call_event_get_detailed_end_reason (TPL_CALL_EVENT (stored_event)));
}


static void
test_add_call_event (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TpAccount *account;
  TplEntity *me, *contact, *room;
  TplEvent *event;
  GError *error = NULL;
  GList *events;
  gint64 timestamp = time (NULL);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/me",
      &error);
  g_assert_no_error (error);
  g_assert (account != NULL);

  me = tpl_entity_new ("me", TPL_ENTITY_SELF, "my-alias", "my-avatar");
  contact = tpl_entity_new ("contact", TPL_ENTITY_CONTACT, "contact-alias",
      "contact-token");
  room = tpl_entity_new_from_room_id ("room");

  /* 1. Outgoing call to a contact */
  event = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp,
      /* TplCallEvent */
      "duration", (gint64) 1234,
      "end-actor", me,
      "end-reason", TPL_CALL_END_REASON_USER_REQUESTED,
      "detailed-end-reason", TP_ERROR_STR_CANCELLED,
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_CALL, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_CALL_EVENT (events->data));

  assert_cmp_call_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 2. Incoming call from contact */
  event = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", me,
      "timestamp", timestamp,
      /* TplCallEvent */
      "duration", (gint64) 2345,
      "end-actor", contact,
      "end-reason", TPL_CALL_END_REASON_USER_REQUESTED,
      "detailed-end-reason", TP_ERROR_STR_TERMINATED,
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_CALL, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_CALL_EVENT (events->data));

  assert_cmp_call_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 3. Outgoing call to a room */
  event = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", room,
      "timestamp", timestamp,
      /* TplCallEvent */
      "duration", (gint64) 3456,
      "end-actor", room,
      "end-reason", TPL_CALL_END_REASON_USER_REQUESTED,
      "detailed-end-reason", TP_ERROR_STR_CHANNEL_KICKED,
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_CALL, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_CALL_EVENT (events->data));

  assert_cmp_call_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);

  /* 4. Incoming missed call from a room */
  event = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", room,
      "timestamp", timestamp,
      /* TplCallEvent */
      "duration", (gint64) -1,
      "end-actor", room,
      "end-reason", TPL_CALL_END_REASON_NO_ANSWER,
      "detailed-end-reason", "",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_CALL, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_CALL_EVENT (events->data));

  assert_cmp_call_event (event, events->data);

  g_object_unref (event);
  g_object_unref (events->data);
  g_list_free (events);
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

  g_test_add ("/log-store-xml/add-text-event",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_add_text_event, teardown);

  g_test_add ("/log-store-xml/add-call-event",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_add_call_event, teardown);

  return g_test_run ();
}
