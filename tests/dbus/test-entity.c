#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <telepathy-logger/entity.h>
#include <telepathy-logger/entity-internal.h>

#include "lib/util.h"
#include "lib/contacts-conn.h"

static void
test_entity_instantiation (void)
{
  TplEntity *entity;

  entity = tpl_entity_new ("my-identifier", TPL_ENTITY_CONTACT,
      "my-alias", "my-token");

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "my-identifier");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_CONTACT);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, "my-alias");
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "my-token");

  g_object_unref (entity);

  /* Check that identifier is copied in absence of ID */
  entity = tpl_entity_new ("my-identifier", TPL_ENTITY_CONTACT,
      NULL, NULL);

  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, "my-identifier");
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "");

  g_object_unref (entity);
}

static void
test_entity_instantiation_from_room_id (void)
{
  TplEntity *entity;

  entity = tpl_entity_new_from_room_id ("my-room-id");

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "my-room-id");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_ROOM);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, "my-room-id");
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "");

  g_object_unref (entity);
}


typedef struct {
  TpContact *contacts[2];
  GMainLoop *loop;
} Result;


static void
get_contacts_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Result *result = user_data;

  g_assert_no_error (error);
  g_assert (n_contacts == 2);
  g_assert (n_failed == 0);

  result->contacts[0] = g_object_ref (contacts[0]);
  result->contacts[1] = g_object_ref (contacts[1]);

  g_main_loop_quit (result->loop);
}


static void
test_entity_instantiation_from_tp_contact (void)
{
  TpBaseConnection *base_connection;
  TpConnection *client_connection;
  TpTestsContactsConnection *connection;
  TpHandleRepoIface *repo;
  TpHandle handles[2];
  const char *alias[] = {"Alice in Wonderland", "Bob the builder"};
  const char *avatar_tokens[] = {"alice-token", NULL};
  TpContactFeature features[] =
      { TP_CONTACT_FEATURE_ALIAS, TP_CONTACT_FEATURE_AVATAR_TOKEN };
  Result result;
  TplEntity *entity;

  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &base_connection, &client_connection);

  connection = TP_TESTS_CONTACTS_CONNECTION (base_connection);

  repo = tp_base_connection_get_handles (base_connection,
      TP_HANDLE_TYPE_CONTACT);

  handles[0] = tp_handle_ensure (repo, "alice", NULL, NULL);
  g_assert (handles[0] != 0);

  handles[1] = tp_handle_ensure (repo, "bob", NULL, NULL);
  g_assert (handles[1] != 0);

  tp_tests_contacts_connection_change_aliases (connection, 2, handles,
      alias);
  tp_tests_contacts_connection_change_avatar_tokens (connection, 2, handles,
      avatar_tokens);

  result.contacts[0] = result.contacts[1] = 0;
  result.loop = g_main_loop_new (NULL, FALSE);

  tp_connection_get_contacts_by_handle (client_connection,
      2, handles,
      2, features,
      get_contacts_cb, &result,
      NULL, NULL);
  g_main_loop_run (result.loop);

  entity = tpl_entity_new_from_tp_contact (result.contacts[0],
      TPL_ENTITY_SELF);

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "alice");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_SELF);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, alias[0]);
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, avatar_tokens[0]);
  g_object_unref (entity);

  entity = tpl_entity_new_from_tp_contact (result.contacts[1],
      TPL_ENTITY_CONTACT);

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "bob");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_CONTACT);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, alias[1]);
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "");
  g_object_unref (entity);

  g_object_unref (result.contacts[0]);
  g_object_unref (result.contacts[1]);
  g_main_loop_unref (result.loop);

  tp_base_connection_change_status (base_connection,
      TP_CONNECTION_STATUS_DISCONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_base_connection_finish_shutdown (base_connection);

  g_object_unref (base_connection);
  g_object_unref (client_connection);
}

int main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_type_init ();

  g_test_add_func ("/entity/instantiation",
      test_entity_instantiation);

  g_test_add_func ("/entity/instantiation-from-room-id",
      test_entity_instantiation_from_room_id);

  g_test_add_func ("/entity/instantiation-from-tp-contact",
      test_entity_instantiation_from_tp_contact);

  return g_test_run ();
}
