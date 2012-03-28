#include "telepathy-logger/log-store-xml.c"

#include "lib/util.h"

#include "telepathy-logger/debug-internal.h"
#include "telepathy-logger/log-manager-internal.h"
#include "telepathy-logger/log-store-internal.h"

#include <telepathy-glib/debug-sender.h>
#include <glib.h>

/* it was defined in telepathy-logger/log-store-xml.c */
#undef DEBUG_FLAG
#define DEBUG_FLAG TPL_DEBUG_TESTSUITE


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

  tp_debug_divert_messages (g_getenv ("TPL_LOGFILE"));

#ifdef ENABLE_DEBUG
  _tpl_debug_set_flags_from_env ();
#endif /* ENABLE_DEBUG */
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

      if (system (command) == -1)
          g_warning ("Failed to cleanup tempory test log dir: %s",
                  fixture->tmp_basedir);

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

  g_assert_cmpstr (tpl_text_event_get_message (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_message (TPL_TEXT_EVENT (stored_event)));
  g_assert_cmpint (tpl_text_event_get_message_type (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_message_type (TPL_TEXT_EVENT (stored_event)));
  g_assert_cmpstr (tpl_text_event_get_message_token (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_message_token (TPL_TEXT_EVENT (stored_event)));
  g_assert_cmpint (tpl_event_get_timestamp (event), ==,
      tpl_event_get_timestamp (stored_event));
  g_assert_cmpint (tpl_text_event_get_edit_timestamp (TPL_TEXT_EVENT (event)),
      ==, tpl_text_event_get_edit_timestamp (TPL_TEXT_EVENT (stored_event)));
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

  /* 4. Incoming message from a room that hit some network lag. */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", room,
      "timestamp", timestamp - 1,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_TEXT, 2, NULL, NULL);

  /* Events appear in their dbus-order for the most part
   * (ignoring timestamps). */
  g_assert_cmpint (g_list_length (events), ==, 2);
  g_assert (TPL_IS_TEXT_EVENT (g_list_last (events)->data));

  assert_cmp_text_event (event, g_list_last (events)->data);

  g_object_unref (event);
  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* 5. Delayed delivery of incoming message from a room */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", contact,
      "receiver", room,
      "timestamp", timestamp - (60 * 60 * 24),
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);

  /* Ask for all of the events to this room... */
  events = _tpl_log_store_get_filtered_events (fixture->store, account, room,
      TPL_EVENT_MASK_ANY, 1000000, NULL, NULL);

  /* ... but there are only 3. */
  g_assert_cmpint (g_list_length (events), ==, 3);
  g_assert (TPL_IS_TEXT_EVENT (events->data));
  /* Also, because of the day discrepancy, this event will not appear in the
   * order it arrived (note that the order is actually undefined (the only
   * invariant is that we don't lose the message), so don't cry if you break
   * this assertion, as long as you don't break message edits). */
  assert_cmp_text_event (event, events->data);

  g_object_unref (event);
  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);
}

static void
test_add_superseding_event (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TpAccount *account;
  TplEntity *me, *contact;
  TplEvent *event;
  TplTextEvent *new_event;
  TplTextEvent *new_new_event;
  TplTextEvent *late_event;
  TplTextEvent *early_event;
  GError *error = NULL;
  GList *events;
  GList *superseded;
  gint64 timestamp = time (NULL);

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "idle/irc/me",
      &error);
  g_assert_no_error (error);
  g_assert (account != NULL);

  me = tpl_entity_new ("me", TPL_ENTITY_SELF, "my-alias", "my-avatar");
  contact = tpl_entity_new ("contact", TPL_ENTITY_CONTACT, "contact-alias",
      "contact-token");

  /* 1. Outgoing message to a contact. */
  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "message-token", "OMGCOMPLETELYRANDOMSTRING1",
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "my message 1",
      NULL);

  /* add and re-retrieve the event */
  _tpl_log_store_add_event (fixture->store, event, &error);
  g_assert_no_error (error);
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);

  g_assert_cmpint (g_list_length (events), ==, 1);
  g_assert (TPL_IS_TEXT_EVENT (events->data));

  assert_cmp_text_event (event, events->data);

  g_object_unref (events->data);
  g_list_free (events);

  /* 2. Edit message 1. */
  new_event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp,
      /* TplTextEvent */
      "edit-timestamp", timestamp + 1,
      "message-token", "OMGCOMPLETELYRANDOMSTRING2",
      "supersedes-token", "OMGCOMPLETELYRANDOMSTRING1",
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "My message 1 [FIXED]",
      NULL);

  /* add and re-retrieve the event */
  _tpl_log_store_add_event (fixture->store, TPL_EVENT (new_event), &error);
  g_assert_no_error (error);
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);
  assert_cmp_text_event (TPL_EVENT (new_event), events->data);

  /* Check that the two events are linked */
  superseded = tpl_text_event_get_supersedes (events->data);
  g_assert (superseded != NULL);
  assert_cmp_text_event (event, superseded->data);
  g_assert (tpl_text_event_get_supersedes (superseded->data) == NULL);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* 3. Edit it again.
   * Note that the (broken) edit-timestamp should not make any
   * difference to the message processing, but it should be preserved.*/
  new_new_event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp,
      /* TplTextEvent */
      "edit-timestamp", timestamp + (60 * 60 * 24),
      "message-token", "OMGCOMPLETELYRANDOMSTRING3",
      "supersedes-token", "OMGCOMPLETELYRANDOMSTRING1",
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "My Message 1 [FIXED] [FIXED]",
      NULL);

  /* add and re-retrieve the event */
  _tpl_log_store_add_event (fixture->store, TPL_EVENT (new_new_event), &error);
  g_assert_no_error (error);
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);
  assert_cmp_text_event (TPL_EVENT (new_new_event), events->data);

  /* Check that the three events are linked */
  superseded = tpl_text_event_get_supersedes (events->data);
  g_assert (superseded != NULL);
  assert_cmp_text_event (TPL_EVENT (new_event), superseded->data);
  g_assert (superseded->next != NULL);
  assert_cmp_text_event (event, superseded->next->data);
  g_assert (tpl_text_event_get_supersedes (superseded->next->data) == NULL);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* Also note that the superseding events *replace* the old ones. */
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1000000, NULL, NULL);
  g_assert_cmpint (g_list_length (events), == , 1);
  assert_cmp_text_event (TPL_EVENT (new_new_event), events->data);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* 4. Edit comes in with the wrong timestamp.
   * Note that the (also broken) edit-timestamp should not make any
   * difference to the message processing, but it should be preserved.*/
  late_event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp + (60 * 60 * 24),
      /* TplTextEvent */
      "edit-timestamp", timestamp - (60 * 60 * 24),
      "message-token", "OMGCOMPLETELYRANDOMSTRING4",
      "supersedes-token", "OMGCOMPLETELYRANDOMSTRING1",
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "My Message 1 [FIXED_LATE]",
      NULL);

  /* add and re-retrieve the event */
  _tpl_log_store_add_event (fixture->store, TPL_EVENT (late_event), &error);
  g_assert_no_error (error);
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1, NULL, NULL);
  assert_cmp_text_event (TPL_EVENT (late_event), events->data);

  /* Check that the events are not linked (and a dummy was inserted instead)
   * because the timestamp was wrong. */
  superseded = tpl_text_event_get_supersedes (events->data);
  g_assert (superseded != NULL);
  g_assert_cmpstr (tpl_text_event_get_message (superseded->data), ==, "");

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* And if we ask for all of the events, there will be 2 there. */
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1000000, NULL, NULL);
  g_assert_cmpint (g_list_length (events), == , 2);
  assert_cmp_text_event (TPL_EVENT (new_new_event), events->data);
  assert_cmp_text_event (TPL_EVENT (late_event), g_list_last (events)->data);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* 5. If we have an event that is broken in the other direction then it will
   * also come out as a separate event (since each day is parsed on its own).
   * Even though we don't currently omit edit-timestamp, we might as well
   * see what happens if we forget it. */
  early_event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      "sender", me,
      "receiver", contact,
      "timestamp", timestamp - (60 * 60 * 24),
      /* TplTextEvent */
      "message-token", "OMGCOMPLETELYRANDOMSTRING5",
      "supersedes-token", "OMGCOMPLETELYRANDOMSTRING1",
      "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      "message", "My Message 1 [FIXED_EARLY]",
      NULL);

  /* And if we ask for all of the events, there will be 3 there. */
  _tpl_log_store_add_event (fixture->store, TPL_EVENT (early_event), &error);
  g_assert_no_error (error);
  events = _tpl_log_store_get_filtered_events (fixture->store, account, contact,
      TPL_EVENT_MASK_TEXT, 1000000, NULL, NULL);
  g_assert_cmpint (g_list_length (events), ==, 3);
  assert_cmp_text_event (TPL_EVENT (early_event), events->data);
  assert_cmp_text_event (TPL_EVENT (new_new_event), events->next->data);
  assert_cmp_text_event (TPL_EVENT (late_event), g_list_last (events)->data);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  g_object_unref (event);
  g_object_unref (new_event);
  g_object_unref (new_new_event);
  g_object_unref (late_event);
  g_object_unref (early_event);
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
      "end-reason", TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
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
      "end-reason", TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
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
      "end-reason", TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED,
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
      "end-reason", TP_CALL_STATE_CHANGE_REASON_NO_ANSWER,
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

static void
test_exists (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TpAccount *account1, *account2;
  TplEntity *user2, *user3;
  GError *error = NULL;

  account1 = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/test2_40collabora_2eco_2euk0",
      &error);
  g_assert_no_error (error);
  g_assert (account1 != NULL);

  account2 = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/user_40collabora_2eco_2euk",
      &error);
  g_assert_no_error (error);
  g_assert (account1 != NULL);

  user2 = tpl_entity_new ("user2@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User2", "");

  user3 = tpl_entity_new ("user3@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User3", "");

  g_assert (_tpl_log_store_exists (fixture->store, account1, NULL, TPL_EVENT_MASK_ANY));
  g_assert (_tpl_log_store_exists (fixture->store, account1, NULL, TPL_EVENT_MASK_TEXT));
  g_assert (!_tpl_log_store_exists (fixture->store, account1, NULL, TPL_EVENT_MASK_CALL));

  g_assert (_tpl_log_store_exists (fixture->store, account2, NULL, TPL_EVENT_MASK_ANY));
  g_assert (_tpl_log_store_exists (fixture->store, account2, NULL, TPL_EVENT_MASK_TEXT));
  g_assert (_tpl_log_store_exists (fixture->store, account2, NULL, TPL_EVENT_MASK_CALL));

  g_assert (!_tpl_log_store_exists (fixture->store, account1, user2, TPL_EVENT_MASK_ANY));
  g_assert (!_tpl_log_store_exists (fixture->store, account1, user2, TPL_EVENT_MASK_TEXT));
  g_assert (!_tpl_log_store_exists (fixture->store, account1, user2, TPL_EVENT_MASK_CALL));

  g_assert (_tpl_log_store_exists (fixture->store, account2, user2, TPL_EVENT_MASK_ANY));
  g_assert (_tpl_log_store_exists (fixture->store, account2, user2, TPL_EVENT_MASK_TEXT));
  g_assert (!_tpl_log_store_exists (fixture->store, account2, user2, TPL_EVENT_MASK_CALL));

  g_assert (_tpl_log_store_exists (fixture->store, account2, user3, TPL_EVENT_MASK_ANY));

  g_assert (!_tpl_log_store_exists (fixture->store, account2, user3, TPL_EVENT_MASK_TEXT));
  g_assert (_tpl_log_store_exists (fixture->store, account2, user3, TPL_EVENT_MASK_CALL));

  g_object_unref (account1);
  g_object_unref (account2);
  g_object_unref (user2);
  g_object_unref (user3);
}


static void
test_get_events_for_date (XmlTestCaseFixture *fixture,
    gconstpointer user_data)
{
  TpAccount *account;
  TplEntity *user2, *user3, *user4, *user5;
  GList *events;
  GDate *date;
  GError *error = NULL;
  gint idx;

  account = tp_account_new (fixture->bus,
      TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/user_40collabora_2eco_2euk",
      &error);
  g_assert_no_error (error);
  g_assert (account != NULL);

  date = g_date_new_dmy (13, 1, 2010);

  user2 = tpl_entity_new ("user2@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User2", "");

  user3 = tpl_entity_new ("user3@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User3", "");

  user4 = tpl_entity_new ("user4@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User4", "");

  user5 = tpl_entity_new ("user5@collabora.co.uk", TPL_ENTITY_CONTACT,
      "User5", "");

  /* Check that text event and call event are merged properly, call events
   * should come after any older or same timestamp event. */
  events = _tpl_log_store_get_events_for_date (fixture->store, account, user4,
      TPL_EVENT_MASK_ANY, date);

  g_assert_cmpint (g_list_length (events), ==, 6);
  idx = -1;

  g_assert (TPL_IS_TEXT_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpstr (
      tpl_text_event_get_message (TPL_TEXT_EVENT (g_list_nth_data (events, idx))),
      ==, "7");

  g_assert (TPL_IS_TEXT_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpstr (
      tpl_text_event_get_message (TPL_TEXT_EVENT (g_list_nth_data (events, idx))),
      ==, "8");

  g_assert (TPL_IS_CALL_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpint (
      tpl_call_event_get_duration (TPL_CALL_EVENT (g_list_nth_data (events, idx))),
      ==, 1);

  g_assert (TPL_IS_CALL_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpint (
      tpl_call_event_get_duration (TPL_CALL_EVENT (g_list_nth_data (events, idx))),
      ==, 2);

  g_assert (TPL_IS_CALL_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpint (
      tpl_call_event_get_duration (TPL_CALL_EVENT (g_list_nth_data (events, idx))),
      ==, 3);

  g_assert (TPL_IS_TEXT_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpstr (
      tpl_text_event_get_message (TPL_TEXT_EVENT (g_list_nth_data (events, idx))),
      ==, "9");

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* Check that a call older then any text event is sorted first */
  events = _tpl_log_store_get_events_for_date (fixture->store, account, user5,
      TPL_EVENT_MASK_ANY, date);

  g_assert_cmpint (g_list_length (events), ==, 2);
  idx = -1;

  g_assert (TPL_IS_CALL_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpint (
      tpl_call_event_get_duration (TPL_CALL_EVENT (g_list_nth_data (events, idx))),
      ==, 1);

  g_assert (TPL_IS_TEXT_EVENT (g_list_nth_data (events, ++idx)));
  g_assert_cmpstr (
      tpl_text_event_get_message (TPL_TEXT_EVENT (g_list_nth_data (events, idx))),
      ==, "9");

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* Check that call mask work */
  events = _tpl_log_store_get_events_for_date (fixture->store, account, user4,
      TPL_EVENT_MASK_CALL, date);

  g_assert_cmpint (g_list_length (events), ==, 3);
  g_assert (TPL_IS_CALL_EVENT (g_list_nth_data (events, 0)));
  g_assert_cmpint (
      tpl_call_event_get_duration (TPL_CALL_EVENT (g_list_nth_data (events, 0))),
      ==, 1);

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* Check that text mask work */
  events = _tpl_log_store_get_events_for_date (fixture->store, account, user4,
      TPL_EVENT_MASK_TEXT, date);

  g_assert_cmpint (g_list_length (events), ==, 3);

  g_assert (TPL_IS_TEXT_EVENT (g_list_nth_data (events, 0)));
  g_assert_cmpstr (
      tpl_text_event_get_message (TPL_TEXT_EVENT (g_list_nth_data (events, 0))),
      ==, "7");

  g_list_foreach (events, (GFunc) g_object_unref, NULL);
  g_list_free (events);

  /* Check that getting empty list is working */
  events = _tpl_log_store_get_events_for_date (fixture->store, account, user2,
      TPL_EVENT_MASK_CALL, date);
  g_assert_cmpint (g_list_length (events), ==, 0);

  events = _tpl_log_store_get_events_for_date (fixture->store, account, user3,
      TPL_EVENT_MASK_TEXT, date);
  g_assert_cmpint (g_list_length (events), ==, 0);

  g_object_unref (account);
  g_object_unref (user2);
  g_object_unref (user3);
  g_object_unref (user4);
  g_object_unref (user5);
  g_date_free (date);
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

  g_test_add ("/log-store-xml/add-superseding-event",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_add_superseding_event, teardown);

  g_test_add ("/log-store-xml/add-call-event",
      XmlTestCaseFixture, NULL,
      setup_for_writing, test_add_call_event, teardown);

  g_test_add ("/log-store-xml/exists",
      XmlTestCaseFixture, NULL,
      setup, test_exists, teardown);

  g_test_add ("/log-store-xml/get-events-for-date",
      XmlTestCaseFixture, NULL,
      setup, test_get_events_for_date, teardown);

  return g_test_run ();
}
