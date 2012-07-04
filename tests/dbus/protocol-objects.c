/* Regression test for Protocol objects in the echo-2 example CM.
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/protocol.h>
#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/echo-cm.h"

#include "examples/cm/echo-message-parts/connection-manager.h"
#include "examples/cm/echo-message-parts/chan.h"
#include "examples/cm/echo-message-parts/conn.h"

#include "tests/lib/util.h"

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  GError *error /* statically initialized to NULL */ ;

  ExampleEcho2ConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpProtocol *protocol;

  TpTestsEchoConnectionManager *old_service_cm;
  TpConnectionManager *old_cm;
  TpProtocol *old_protocol;

  TpConnectionManager *file_cm;
  TpProtocol *file_protocol;
} Test;

static void
setup (Test *test,
       gconstpointer data G_GNUC_UNUSED)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  test->service_cm = EXAMPLE_ECHO_2_CONNECTION_MANAGER (g_object_new (
        EXAMPLE_TYPE_ECHO_2_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_connection_manager_new (test->dbus, "example_echo_2",
      NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_tests_proxy_run_until_prepared (test->cm, NULL);

  test->old_service_cm = TP_TESTS_ECHO_CONNECTION_MANAGER (g_object_new (
        TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER,
        NULL));
  g_assert (test->old_service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->old_service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->old_cm = tp_connection_manager_new (test->dbus, "example_echo",
      NULL, &test->error);
  g_assert (test->old_cm != NULL);
  tp_tests_proxy_run_until_prepared (test->old_cm, NULL);

  test->file_cm = tp_connection_manager_new (test->dbus, "test_manager_file",
      NULL, &test->error);
  g_assert (test->file_cm != NULL);
  tp_tests_proxy_run_until_prepared (test->file_cm, NULL);

  test->old_protocol = NULL;
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  tp_clear_object (&test->protocol);
  tp_clear_object (&test->cm);
  tp_clear_object (&test->service_cm);
  tp_clear_object (&test->old_service_cm);
  tp_clear_object (&test->old_cm);
  tp_clear_object (&test->old_protocol);
  tp_clear_object (&test->file_cm);
  tp_clear_object (&test->file_protocol);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

const gchar * const expected_interfaces[] = {
    TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
    TP_IFACE_CONNECTION_INTERFACE_CONTACTS,
    NULL };

const gchar * const expected_protocol_interfaces[] = {
    TP_IFACE_PROTOCOL_INTERFACE_AVATARS,
    TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING,
    NULL };

const gchar * const expected_cm_interfaces[] = {
    "im.telepathy.Tests.Example",
    NULL };

const gchar * const expected_supported_avatar_mime_types[] = {
  "image/png",
  "image/jpeg",
  "image/gif",
  NULL };

const gchar * const expected_addressable_vcard_fields[] = {
  "x-jabber",
  "tel",
  NULL };

const gchar * const expected_addressable_uri_schemes[] = {
  "xmpp",
  "tel",
  NULL };

static void
test_protocol_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties = NULL;
  GPtrArray *arr;
  GValueArray *va;
  GHashTable *fixed;

  test->protocol = tp_protocol_new (test->dbus, "example_echo_2",
      "example", NULL, NULL);
  g_assert (test->protocol != NULL);

  tp_cli_dbus_properties_run_get_all (test->protocol, -1,
      TP_IFACE_PROTOCOL, &properties, &test->error, NULL);
  g_assert_no_error (test->error);

  tp_tests_assert_strv_equals (
      tp_asv_get_boxed (properties, "Interfaces", G_TYPE_STRV),
      expected_protocol_interfaces);

  g_assert_cmpstr (tp_asv_get_string (properties, "Icon"), ==, "im-icq");
  g_assert_cmpstr (tp_asv_get_string (properties, "EnglishName"), ==,
      "Echo II example");
  g_assert_cmpstr (tp_asv_get_string (properties, "VCardField"), ==,
      "x-telepathy-example");
  g_assert_cmpstr (tp_asv_get_string (properties, "VCardField"), ==,
      "x-telepathy-example");

  tp_tests_assert_strv_equals (tp_asv_get_boxed (properties,
        "ConnectionInterfaces", G_TYPE_STRV), expected_interfaces);

  arr = tp_asv_get_boxed (properties, "RequestableChannelClasses",
      TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, ==, 1);

  va = g_ptr_array_index (arr, 0);
  g_assert (G_VALUE_HOLDS (va->values + 0, TP_HASH_TYPE_CHANNEL_CLASS));
  g_assert (G_VALUE_HOLDS (va->values + 1, G_TYPE_STRV));

  fixed = g_value_get_boxed (va->values + 0);
  g_assert_cmpstr (tp_asv_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);

  arr = tp_asv_get_boxed (properties, "Parameters",
      TP_ARRAY_TYPE_PARAM_SPEC_LIST);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, >=, 1);
}

static void
test_protocol_avatar_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties = NULL;
  gboolean is_set;
  guint num;

  test->protocol = tp_protocol_new (test->dbus, "example_echo_2",
      "example", NULL, NULL);
  g_assert (test->protocol != NULL);

  tp_cli_dbus_properties_run_get_all (test->protocol, -1,
      TP_IFACE_PROTOCOL_INTERFACE_AVATARS, &properties, &test->error, NULL);
  g_assert_no_error (test->error);

  tp_tests_assert_strv_equals (
      tp_asv_get_boxed (properties, "SupportedAvatarMIMETypes", G_TYPE_STRV),
      expected_supported_avatar_mime_types);

  num = tp_asv_get_uint32 (properties, "MinimumAvatarHeight", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 32);

  num = tp_asv_get_uint32 (properties, "MinimumAvatarWidth", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 32);

  num = tp_asv_get_uint32 (properties, "RecommendedAvatarHeight", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 64);

  num = tp_asv_get_uint32 (properties, "RecommendedAvatarWidth", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 64);

  num = tp_asv_get_uint32 (properties, "MaximumAvatarHeight", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 96);

  num = tp_asv_get_uint32 (properties, "MaximumAvatarWidth", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 96);

  num = tp_asv_get_uint32 (properties, "MaximumAvatarBytes", &is_set);
  g_assert (is_set);
  g_assert_cmpuint (num, ==, 37748736);
}

static void
test_protocol_addressing_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties = NULL;

  test->protocol = tp_protocol_new (test->dbus, "example_echo_2",
      "example", NULL, NULL);
  g_assert (test->protocol != NULL);

  tp_cli_dbus_properties_run_get_all (test->protocol, -1,
      TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING, &properties, &test->error, NULL);
  g_assert_no_error (test->error);

  tp_tests_assert_strv_equals (
      tp_asv_get_boxed (properties, "AddressableVCardFields", G_TYPE_STRV),
      expected_addressable_vcard_fields);

  tp_tests_assert_strv_equals (
      tp_asv_get_boxed (properties, "AddressableURISchemes", G_TYPE_STRV),
      expected_addressable_uri_schemes);
}

static void
test_protocols_property (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties = NULL;
  GHashTable *protocols;
  GHashTable *pp;
  GPtrArray *arr;
  GValueArray *va;
  GHashTable *fixed;

  tp_cli_dbus_properties_run_get_all (test->cm, -1,
      TP_IFACE_CONNECTION_MANAGER, &properties, &test->error, NULL);
  g_assert_no_error (test->error);

  g_assert (tp_asv_lookup (properties, "Interfaces") != NULL);
  test_assert_empty_strv (tp_asv_get_boxed (properties, "Interfaces",
        G_TYPE_STRV));

  protocols = tp_asv_get_boxed (properties, "Protocols",
      TP_HASH_TYPE_PROTOCOL_PROPERTIES_MAP);
  g_assert (protocols != NULL);
  g_assert_cmpuint (g_hash_table_size (protocols), ==, 1);

  pp = g_hash_table_lookup (protocols, "example");
  g_assert (pp != NULL);

  tp_tests_assert_strv_equals (
      tp_asv_get_boxed (pp, TP_PROP_PROTOCOL_INTERFACES, G_TYPE_STRV),
      expected_protocol_interfaces);

  g_assert_cmpstr (tp_asv_get_string (pp, TP_PROP_PROTOCOL_ICON), ==,
      "im-icq");
  g_assert_cmpstr (tp_asv_get_string (pp, TP_PROP_PROTOCOL_ENGLISH_NAME), ==,
      "Echo II example");
  g_assert_cmpstr (tp_asv_get_string (pp, TP_PROP_PROTOCOL_VCARD_FIELD), ==,
      "x-telepathy-example");

  tp_tests_assert_strv_equals (tp_asv_get_boxed (pp,
        TP_PROP_PROTOCOL_CONNECTION_INTERFACES, G_TYPE_STRV),
      expected_interfaces);

  arr = tp_asv_get_boxed (pp, TP_PROP_PROTOCOL_REQUESTABLE_CHANNEL_CLASSES,
      TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, ==, 1);

  va = g_ptr_array_index (arr, 0);
  g_assert (G_VALUE_HOLDS (va->values + 0, TP_HASH_TYPE_CHANNEL_CLASS));
  g_assert (G_VALUE_HOLDS (va->values + 1, G_TYPE_STRV));

  fixed = g_value_get_boxed (va->values + 0);
  g_assert_cmpstr (tp_asv_get_string (fixed, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);

  arr = tp_asv_get_boxed (pp, TP_PROP_PROTOCOL_PARAMETERS,
      TP_ARRAY_TYPE_PARAM_SPEC_LIST);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, >=, 1);
}

static void
test_protocols_property_old (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *properties = NULL;
  GHashTable *protocols;
  GHashTable *pp;
  GPtrArray *arr;

  tp_cli_dbus_properties_run_get_all (test->old_cm, -1,
      TP_IFACE_CONNECTION_MANAGER, &properties, &test->error, NULL);
  g_assert_no_error (test->error);

  g_assert (tp_asv_lookup (properties, "Interfaces") != NULL);
  tp_tests_assert_strv_equals (tp_asv_get_boxed (properties,
          "Interfaces", G_TYPE_STRV), expected_cm_interfaces);

  protocols = tp_asv_get_boxed (properties, "Protocols",
      TP_HASH_TYPE_PROTOCOL_PROPERTIES_MAP);
  g_assert (protocols != NULL);
  g_assert_cmpuint (g_hash_table_size (protocols), ==, 1);

  pp = g_hash_table_lookup (protocols, "example");
  g_assert (pp != NULL);

  g_assert (tp_asv_lookup (pp, TP_PROP_PROTOCOL_INTERFACES) == NULL);
  g_assert (tp_asv_lookup (pp, TP_PROP_PROTOCOL_ICON) == NULL);
  g_assert (tp_asv_lookup (pp, TP_PROP_PROTOCOL_ENGLISH_NAME) == NULL);
  g_assert (tp_asv_lookup (pp, TP_PROP_PROTOCOL_VCARD_FIELD) == NULL);
  g_assert (tp_asv_lookup (pp,
        TP_PROP_PROTOCOL_CONNECTION_INTERFACES) == NULL);
  g_assert (tp_asv_lookup (pp, TP_PROP_PROTOCOL_REQUESTABLE_CHANNEL_CLASSES)
      == NULL);

  arr = tp_asv_get_boxed (pp, TP_PROP_PROTOCOL_PARAMETERS,
      TP_ARRAY_TYPE_PARAM_SPEC_LIST);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, >=, 1);

}

static void
check_avatar_requirements (TpAvatarRequirements *req)
{
  g_assert (req != NULL);

  g_assert (req->supported_mime_types != NULL);
  g_assert_cmpuint (g_strv_length (req->supported_mime_types), ==, 3);
  g_assert (tp_strv_contains ((const gchar * const *) req->supported_mime_types,
        "image/png"));
  g_assert (tp_strv_contains ((const gchar * const *) req->supported_mime_types,
        "image/jpeg"));
  g_assert (tp_strv_contains ((const gchar * const *) req->supported_mime_types,
        "image/gif"));

  g_assert_cmpuint (req->minimum_width, ==, 32);
  g_assert_cmpuint (req->minimum_height, ==, 32);
  g_assert_cmpuint (req->recommended_width, ==, 64);
  g_assert_cmpuint (req->recommended_height, ==, 64);
  g_assert_cmpuint (req->maximum_width, ==, 96);
  g_assert_cmpuint (req->maximum_height, ==, 96);
  g_assert_cmpuint (req->maximum_bytes, ==, 37748736);
}

static void
test_protocol_object (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAvatarRequirements *req;
  GList *l;
  TpConnectionManagerParam *param;

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "example_echo_2");
  tp_tests_proxy_run_until_prepared (test->cm, NULL);
  test->protocol = g_object_ref (
      tp_connection_manager_get_protocol_object (test->cm, "example"));

  g_assert_cmpstr (tp_protocol_get_name (test->protocol), ==, "example");

  g_assert_cmpstr (tp_protocol_get_cm_name (test->protocol),
      ==, "example_echo_2");

  g_assert (tp_proxy_has_interface_by_id (test->protocol,
      TP_IFACE_QUARK_PROTOCOL));
  g_assert (tp_proxy_has_interface_by_id (test->protocol,
      TP_IFACE_QUARK_PROTOCOL_INTERFACE_AVATARS));

  g_assert (tp_proxy_is_prepared (test->protocol,
        TP_PROTOCOL_FEATURE_PARAMETERS));

  g_assert (tp_protocol_has_param (test->protocol, "account"));
  g_assert (!tp_protocol_has_param (test->protocol, "no-way"));

  g_assert (tp_proxy_is_prepared (test->protocol, TP_PROTOCOL_FEATURE_CORE));

  g_assert_cmpstr (tp_protocol_get_icon_name (test->protocol), ==,
      "im-icq");
  g_assert_cmpstr (tp_protocol_get_english_name (test->protocol), ==,
      "Echo II example");
  g_assert_cmpstr (tp_protocol_get_vcard_field (test->protocol), ==,
      "x-telepathy-example");
  g_assert (TP_IS_CAPABILITIES (tp_protocol_get_capabilities (
          test->protocol)));

  req = tp_protocol_get_avatar_requirements (test->protocol);
  check_avatar_requirements (req);

  g_object_get (test->protocol, "avatar-requirements", &req, NULL);
  check_avatar_requirements (req);

  l = tp_protocol_dup_params (test->protocol);
  g_assert_cmpuint (g_list_length (l), ==, 1);
  param = l->data;
  g_assert_cmpstr (param->name, ==, "account");
  g_list_free_full (l, (GDestroyNotify) tp_connection_manager_param_free);

  g_assert_cmpstr (tp_protocol_get_param (test->protocol, "account")->name, ==,
      "account");

  param = tp_protocol_dup_param (test->protocol, "account");
  /* it's a copy */
  g_assert (param != tp_protocol_get_param (test->protocol, "account"));
  g_assert_cmpstr (param->name, ==, "account");
  tp_connection_manager_param_free (param);

  g_assert_cmpstr (tp_protocol_borrow_params (test->protocol)[0].name, ==,
      "account");
  g_assert_cmpstr (tp_protocol_borrow_params (test->protocol)[1].name, ==,
      NULL);
}

static void
test_protocol_object_old (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpAvatarRequirements *req;

  g_assert_cmpstr (tp_connection_manager_get_name (test->old_cm), ==,
      "example_echo");
  tp_tests_proxy_run_until_prepared (test->old_cm, NULL);
  test->old_protocol = g_object_ref (
      tp_connection_manager_get_protocol_object (test->old_cm, "example"));

  g_assert_cmpstr (tp_protocol_get_name (test->old_protocol), ==, "example");

  g_assert (tp_proxy_is_prepared (test->old_protocol,
        TP_PROTOCOL_FEATURE_PARAMETERS));

  g_assert (tp_protocol_has_param (test->old_protocol, "account"));
  g_assert (!tp_protocol_has_param (test->old_protocol, "no-way"));

  g_assert (!tp_proxy_is_prepared (test->old_protocol,
        TP_PROTOCOL_FEATURE_CORE));

  g_assert_cmpstr (tp_protocol_get_icon_name (test->old_protocol), ==,
      "im-example");
  g_assert_cmpstr (tp_protocol_get_english_name (test->old_protocol), ==,
      "Example");
  g_assert_cmpstr (tp_protocol_get_vcard_field (test->old_protocol), ==, NULL);
  g_assert (tp_protocol_get_capabilities (test->old_protocol) == NULL);

  req = tp_protocol_get_avatar_requirements (test->old_protocol);
  g_assert (req == NULL);
}

static void
test_protocol_object_from_file (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GQuark features[] = { TP_PROTOCOL_FEATURE_CORE, 0 };
  TpCapabilities *caps;
  TpAvatarRequirements *req;

  g_assert_cmpstr (tp_connection_manager_get_name (test->file_cm), ==,
      "test_manager_file");
  tp_tests_proxy_run_until_prepared (test->file_cm, NULL);
  test->file_protocol = g_object_ref (
      tp_connection_manager_get_protocol_object (test->file_cm, "foo"));

  g_assert_cmpstr (tp_protocol_get_name (test->file_protocol), ==, "foo");

  g_assert (tp_proxy_is_prepared (test->file_protocol,
        TP_PROTOCOL_FEATURE_PARAMETERS));

  g_assert (tp_protocol_has_param (test->file_protocol, "account"));
  g_assert (!tp_protocol_has_param (test->file_protocol, "no-way"));

  tp_tests_proxy_run_until_prepared (test->file_protocol, features);
  g_assert (tp_proxy_is_prepared (test->file_protocol,
        TP_PROTOCOL_FEATURE_CORE));

  g_assert_cmpstr (tp_protocol_get_icon_name (test->file_protocol), ==,
      "im-icq");
  g_assert_cmpstr (tp_protocol_get_english_name (test->file_protocol), ==,
      "Regression tests");
  g_assert_cmpstr (tp_protocol_get_vcard_field (test->file_protocol), ==,
      "x-telepathy-tests");

  g_assert (tp_protocol_get_capabilities (test->file_protocol) != NULL);
  caps = tp_protocol_get_capabilities (test->file_protocol);
  g_assert (!tp_capabilities_is_specific_to_contact (caps));
  g_assert (tp_capabilities_supports_text_chats (caps));
  g_assert (!tp_capabilities_supports_text_chatrooms (caps));

  req = tp_protocol_get_avatar_requirements (test->file_protocol);
  check_avatar_requirements (req);

  g_object_get (test->file_protocol, "avatar-requirements", &req, NULL);
  check_avatar_requirements (req);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/protocol-objects/protocol-properties", Test, NULL, setup,
      test_protocol_properties, teardown);
  g_test_add ("/protocol-objects/protocol-avatar-properties", Test, NULL,
      setup, test_protocol_avatar_properties, teardown);
  g_test_add ("/protocol-objects/protocol-addressing-properties", Test, NULL,
      setup, test_protocol_addressing_properties, teardown);
  g_test_add ("/protocol-objects/protocols-property", Test, NULL, setup,
      test_protocols_property, teardown);
  g_test_add ("/protocol-objects/protocols-property-old", Test, NULL, setup,
      test_protocols_property_old, teardown);
  g_test_add ("/protocol-objects/object", Test, NULL, setup,
      test_protocol_object, teardown);
  g_test_add ("/protocol-objects/object-old", Test, NULL, setup,
      test_protocol_object_old, teardown);
  g_test_add ("/protocol-objects/object-from-file", Test, NULL, setup,
      test_protocol_object_from_file, teardown);

  return g_test_run ();
}
