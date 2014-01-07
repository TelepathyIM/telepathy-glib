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
  TpContact *contact;
  GMainLoop *loop;
} Result;

static void
ensure_contact_cb (GObject *source,
    GAsyncResult *op_result,
    gpointer user_data)
{
  Result *result = user_data;
  GError *error = NULL;

  result->contact = tp_client_factory_ensure_contact_by_id_finish (
      TP_CLIENT_FACTORY (source), op_result, &error);

  g_assert_no_error (error);
  g_assert (TP_IS_CONTACT (result->contact));

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
  Result result;
  TplEntity *entity;
  TpContact *alice, *bob;
  TpClientFactory *factory;

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

  factory = tp_proxy_get_factory (client_connection);
  tp_client_factory_add_contact_features_varargs (factory,
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN,
      0);

  result.loop = g_main_loop_new (NULL, FALSE);

  tp_client_factory_ensure_contact_by_id_async (factory,
      client_connection, "alice", ensure_contact_cb, &result);
  g_main_loop_run (result.loop);
  alice = result.contact;

  tp_client_factory_ensure_contact_by_id_async (factory,
      client_connection, "bob", ensure_contact_cb, &result);
  g_main_loop_run (result.loop);
  bob = result.contact;

  entity = tpl_entity_new_from_tp_contact (alice, TPL_ENTITY_SELF);

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "alice");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_SELF);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, alias[0]);
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, avatar_tokens[0]);
  g_object_unref (entity);

  entity = tpl_entity_new_from_tp_contact (bob, TPL_ENTITY_CONTACT);

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "bob");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_CONTACT);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, alias[1]);
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "");
  g_object_unref (entity);

  g_object_unref (alice);
  g_object_unref (bob);
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

  g_test_add_func ("/entity/instantiation",
      test_entity_instantiation);

  g_test_add_func ("/entity/instantiation-from-room-id",
      test_entity_instantiation_from_room_id);

  g_test_add_func ("/entity/instantiation-from-tp-contact",
      test_entity_instantiation_from_tp_contact);

  return g_test_run ();
}
