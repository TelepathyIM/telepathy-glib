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
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "telepathy-glib/reentrants.h"

#include "examples/cm/echo-message-parts/connection-manager.h"
#include "examples/cm/echo-message-parts/chan.h"
#include "examples/cm/echo-message-parts/conn.h"

#include "tests/lib/util.h"

typedef struct
{
  GMainLoop *mainloop;
  GDBusConnection *dbus;
  TpClientFactory *factory;
  GError *error /* statically initialized to NULL */ ;

  ExampleEcho2ConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpProtocol *protocol;

  TpConnectionManager *file_cm;
  TpProtocol *file_protocol;
} Test;

static void
setup (Test *test,
       gconstpointer data G_GNUC_UNUSED)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->factory = tp_automatic_client_factory_new (test->dbus);

  test->service_cm = EXAMPLE_ECHO_2_CONNECTION_MANAGER (g_object_new (
        EXAMPLE_TYPE_ECHO_2_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_client_factory_ensure_connection_manager (test->factory,
      "example_echo_2", NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_tests_proxy_run_until_prepared (test->cm, NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->file_cm = tp_client_factory_ensure_connection_manager (test->factory,
      "test_manager_file", NULL, &test->error);
  g_assert (test->file_cm != NULL);
  tp_tests_proxy_run_until_prepared (test->file_cm, NULL);
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  tp_clear_object (&test->protocol);
  tp_clear_object (&test->cm);
  tp_clear_object (&test->service_cm);
  tp_clear_object (&test->file_cm);
  tp_clear_object (&test->file_protocol);

  tp_clear_object (&test->factory);
  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

const gchar * const expected_interfaces[] = {
    NULL };

const gchar * const expected_protocol_interfaces[] = {
    TP_IFACE_PROTOCOL_INTERFACE_AVATARS1,
    TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING1,
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

  test->protocol = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_2", "example", NULL, NULL);
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

  test->protocol = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_2", "example", NULL, NULL);
  g_assert (test->protocol != NULL);

  tp_cli_dbus_properties_run_get_all (test->protocol, -1,
      TP_IFACE_PROTOCOL_INTERFACE_AVATARS1, &properties, &test->error, NULL);
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

  test->protocol = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_2", "example", NULL, NULL);
  g_assert (test->protocol != NULL);

  tp_cli_dbus_properties_run_get_all (test->protocol, -1,
      TP_IFACE_PROTOCOL_INTERFACE_ADDRESSING1, &properties, &test->error, NULL);
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
check_tp_protocol (TpProtocol *protocol)
{
  TpAvatarRequirements *req;
  GList *l;
  TpConnectionManagerParam *param;
  GList *params;

  g_assert_cmpstr (tp_protocol_get_name (protocol), ==, "example");

  g_assert_cmpstr (tp_protocol_get_cm_name (protocol),
      ==, "example_echo_2");

  g_assert (tp_proxy_has_interface_by_id (protocol,
      TP_IFACE_QUARK_PROTOCOL));
  g_assert (tp_proxy_has_interface_by_id (protocol,
      TP_IFACE_QUARK_PROTOCOL_INTERFACE_AVATARS1));

  g_assert (tp_proxy_is_prepared (protocol,
        TP_PROTOCOL_FEATURE_PARAMETERS));

  g_assert (tp_protocol_has_param (protocol, "account"));
  g_assert (!tp_protocol_has_param (protocol, "no-way"));

  g_assert (tp_proxy_is_prepared (protocol, TP_PROTOCOL_FEATURE_CORE));

  g_assert_cmpstr (tp_protocol_get_icon_name (protocol), ==,
      "im-icq");
  g_assert_cmpstr (tp_protocol_get_english_name (protocol), ==,
      "Echo II example");
  g_assert_cmpstr (tp_protocol_get_vcard_field (protocol), ==,
      "x-telepathy-example");
  g_assert (TP_IS_CAPABILITIES (tp_protocol_get_capabilities (
          protocol)));

  req = tp_protocol_get_avatar_requirements (protocol);
  check_avatar_requirements (req);

  g_object_get (protocol, "avatar-requirements", &req, NULL);
  check_avatar_requirements (req);

  l = tp_protocol_dup_params (protocol);
  g_assert_cmpuint (g_list_length (l), ==, 1);
  param = l->data;
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "account");
  g_list_free_full (l, (GDestroyNotify) tp_connection_manager_param_free);

  param = tp_protocol_dup_param (protocol, "account");
  /* it's a copy */
  g_assert (param != tp_protocol_get_param (protocol, "account"));
  g_assert_cmpstr (tp_connection_manager_param_get_name (param), ==, "account");
  tp_connection_manager_param_free (param);

  params = tp_protocol_dup_params (protocol);
  g_assert_cmpuint (g_list_length (params), ==, 1);
  g_assert_cmpstr (tp_connection_manager_param_get_name (params->data), ==,
      "account");
  g_list_free_full (params, (GDestroyNotify) tp_connection_manager_param_free);
}

static void
test_protocol_object (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GVariant *props;
  TpProtocol *protocol;

  g_assert_cmpstr (tp_connection_manager_get_name (test->cm), ==,
      "example_echo_2");
  tp_tests_proxy_run_until_prepared (test->cm, NULL);
  test->protocol = g_object_ref (
      tp_connection_manager_get_protocol (test->cm, "example"));

  check_tp_protocol (test->protocol);

  /* Create a new TpProtocol for the same protocol but by passing it all its
   * immutable properities */
  g_object_get (test->protocol,
      "protocol-properties", &props,
      NULL);

  protocol = tp_client_factory_ensure_protocol (test->factory, "example_echo_2",
      "example", props, &test->error);
  g_assert_no_error (test->error);
  g_assert (TP_IS_PROTOCOL (protocol));

  check_tp_protocol (protocol);

  g_object_unref (protocol);

  g_variant_unref (props);
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
      tp_connection_manager_get_protocol (test->file_cm, "foo"));

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

static void
test_normalize (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GAsyncResult *result = NULL;
  gchar *s;

  tp_tests_proxy_run_until_prepared (test->cm, NULL);
  test->protocol = g_object_ref (
      tp_connection_manager_get_protocol (test->cm, "example"));

  tp_protocol_normalize_contact_async (test->protocol,
      "MiXeDcAsE", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_contact_finish (test->protocol, result,
      &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpstr (s, ==, "mixedcase");
  g_clear_object (&result);
  g_free (s);

  tp_protocol_normalize_contact_async (test->protocol,
      "", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_contact_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_HANDLE);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_normalize_contact_uri_async (test->protocol,
      "xmpp:MiXeDcAsE", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_contact_uri_finish (test->protocol, result,
      &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpstr (s, ==, "xmpp:mixedcase");
  g_clear_object (&result);
  g_free (s);

  tp_protocol_normalize_contact_uri_async (test->protocol,
      "xmpp:", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_contact_uri_finish (test->protocol, result,
      &test->error);
  g_assert_cmpstr (s, ==, NULL);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_normalize_contact_uri_async (test->protocol,
      "http://example.com", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_contact_uri_finish (test->protocol, result,
      &test->error);
  g_assert_cmpstr (s, ==, NULL);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_normalize_vcard_address_async (test->protocol,
      "x-jabber", "MiXeDcAsE", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_vcard_address_finish (test->protocol, result,
      &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpstr (s, ==, "mixedcase");
  g_clear_object (&result);
  g_free (s);

  tp_protocol_normalize_vcard_address_async (test->protocol,
      "x-jabber", "", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_vcard_address_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_normalize_vcard_address_async (test->protocol,
      "x-skype", "", NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_normalize_vcard_address_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);
}

static void
test_id (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GAsyncResult *result = NULL;
  gchar *s;

  tp_tests_proxy_run_until_prepared (test->cm, NULL);
  test->protocol = g_object_ref (
      tp_connection_manager_get_protocol (test->cm, "example"));

  tp_protocol_identify_account_async (test->protocol,
      g_variant_new_parsed ("{ 'account': <'Hello'> }"),
      NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_identify_account_finish (test->protocol, result,
      &test->error);
  g_assert_no_error (test->error);
  g_assert_cmpstr (s, ==, "hello");
  g_clear_object (&result);
  g_free (s);

  tp_protocol_identify_account_async (test->protocol,
      g_variant_new_parsed ("{ 'account': <'Hello'>, 'unknown-param': <42> }"),
      NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_identify_account_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_identify_account_async (test->protocol,
      g_variant_new_parsed ("@a{sv} {}"),
      NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_identify_account_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);

  tp_protocol_identify_account_async (test->protocol,
      g_variant_new_parsed ("@a{sv} { 'account': <''> }"),
      NULL, tp_tests_result_ready_cb, &result);
  tp_tests_run_until_result (&result);
  s = tp_protocol_identify_account_finish (test->protocol, result,
      &test->error);
  g_assert_error (test->error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (s, ==, NULL);
  g_clear_object (&result);
  g_clear_error (&test->error);
}

static void
test_factory (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpProtocol *p1, *p2, *p3;
  GArray *arr;

  p1 = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_3", "example", NULL, NULL);
  g_assert (TP_IS_PROTOCOL (p1));

  p2 = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_3", "example", NULL, NULL);
  g_assert (p1 == p2);

  g_object_unref (p1);
  g_object_unref (p2);

  p3 = tp_client_factory_ensure_protocol (test->factory,
      "example_echo_3", "example", NULL, NULL);
  g_assert (TP_IS_PROTOCOL (p3));
  /* the object has been removed from the cache */
  g_assert (p3 != p1);

  arr = tp_client_factory_dup_protocol_features (test->factory,
      p3);
  g_assert (arr != NULL);
  g_assert_cmpuint (arr->len, ==, 1);
  g_assert_cmpuint (g_array_index (arr, guint, 0), ==,
      TP_PROTOCOL_FEATURE_CORE);
  g_array_unref (arr);

  g_object_unref (p3);
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
  g_test_add ("/protocol-objects/object", Test, NULL, setup,
      test_protocol_object, teardown);
  g_test_add ("/protocol-objects/object-from-file", Test, NULL, setup,
      test_protocol_object_from_file, teardown);
  g_test_add ("/protocol-objects/normalize", Test, NULL, setup,
      test_normalize, teardown);
  g_test_add ("/protocol-objects/id", Test, NULL, setup,
      test_id, teardown);
  g_test_add ("/protocol-objects/factory", Test, NULL, setup,
      test_factory, teardown);

  return tp_tests_run_with_bus ();
}
