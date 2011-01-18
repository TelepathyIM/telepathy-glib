#include <glib.h>
#include <glib/gprintf.h>
#include <telepathy-logger/entity.h>
#include <telepathy-logger/entity-internal.h>

static void
test_entity_instantiation (void)
{
  TplEntity *entity;

  entity = g_object_new (TPL_TYPE_ENTITY,
      "identifier", "my-identifier",
      "type", TPL_ENTITY_CONTACT,
      "alias", "my-alias",
      "avatar-token", "my-token",
      NULL);

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "my-identifier");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_CONTACT);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, "my-alias");
  g_assert_cmpstr (tpl_entity_get_avatar_token (entity), ==, "my-token");

  g_object_unref (entity);
}

static void
test_entity_instantiation_from_room_id (void)
{
  TplEntity *entity;

  entity = _tpl_entity_new_from_room_id ("my-room-id");

  g_assert_cmpstr (tpl_entity_get_identifier (entity), ==, "my-room-id");
  g_assert (tpl_entity_get_entity_type (entity) == TPL_ENTITY_ROOM);
  g_assert_cmpstr (tpl_entity_get_alias (entity), ==, "my-room-id");
  g_assert (tpl_entity_get_avatar_token (entity) == NULL);

  g_object_unref (entity);
}

static void
test_entity_instantiation_from_tp_contact (void)
{
  /* TODO figure-out how to obtain a TpContact to test
   * _tpl_entity_new_from_tp_contact() */
  g_printf ("- TODO - ");
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
