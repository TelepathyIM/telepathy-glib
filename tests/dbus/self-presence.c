/* Feature test for setting your own presence.
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <telepathy-glib/reentrants.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
}

static void
test_presence (TpTestsContactsConnection *service_conn,
               TpConnection *client_conn)
{
  GError *error = NULL;
  GValue *value = NULL;
  GHashTable *statuses;
  GValueArray *spec;

  MYASSERT (tp_cli_dbus_properties_run_get (client_conn, -1,
        TP_IFACE_CONNECTION_INTERFACE_PRESENCE1, "Statuses",
        &value, &error, NULL), "");
  g_assert_no_error (error);

  MYASSERT (G_VALUE_TYPE (value) == TP_HASH_TYPE_STATUS_SPEC_MAP,
      ": %s != %s", G_VALUE_TYPE_NAME (value),
      g_type_name (TP_HASH_TYPE_STATUS_SPEC_MAP));

  statuses = g_value_get_boxed (value);

  spec = g_hash_table_lookup (statuses, "available");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_AVAILABLE);
  MYASSERT (g_value_get_boolean (spec->values + 1), ""); /* can set on self */
  MYASSERT (g_value_get_boolean (spec->values + 2), ""); /* can have message */

  spec = g_hash_table_lookup (statuses, "busy");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_BUSY);
  MYASSERT (g_value_get_boolean (spec->values + 1), ""); /* can set on self */
  MYASSERT (g_value_get_boolean (spec->values + 2), ""); /* can have message */

  spec = g_hash_table_lookup (statuses, "away");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_AWAY);
  MYASSERT (g_value_get_boolean (spec->values + 1), ""); /* can set on self */
  MYASSERT (g_value_get_boolean (spec->values + 2), ""); /* can have message */

  spec = g_hash_table_lookup (statuses, "offline");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_OFFLINE);
  MYASSERT (!g_value_get_boolean (spec->values + 1), ""); /* can set on self */

  spec = g_hash_table_lookup (statuses, "unknown");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_UNKNOWN);
  MYASSERT (!g_value_get_boolean (spec->values + 1), ""); /* can set on self */

  spec = g_hash_table_lookup (statuses, "error");
  MYASSERT (spec != NULL, "");
  g_assert_cmpuint (g_value_get_uint (spec->values + 0), ==,
      TP_CONNECTION_PRESENCE_TYPE_ERROR);
  MYASSERT (!g_value_get_boolean (spec->values + 1), ""); /* can set on self */

  g_value_unset (value);
  g_free (value);

  MYASSERT (!tp_cli_connection_interface_presence1_run_set_presence (
        client_conn, -1, "offline", "", &error, NULL), "");
  g_assert_cmpstr (g_quark_to_string (error->domain), ==,
      g_quark_to_string (TP_ERROR));
  g_error_free (error);
  error = NULL;

  MYASSERT (tp_cli_connection_interface_presence1_run_set_presence (
        client_conn, -1, "available", "Here I am", &error, NULL), "");
  g_assert_no_error (error);

  value = NULL;

  MYASSERT (tp_cli_dbus_properties_run_get (client_conn, -1,
        TP_IFACE_CONNECTION_INTERFACE_PRESENCE1,
        "MaximumStatusMessageLength",
        &value, &error, NULL), "");
  g_assert_no_error (error);

  MYASSERT (G_VALUE_TYPE (value) == G_TYPE_UINT,
      ": %s != %s", G_VALUE_TYPE_NAME (value),
      g_type_name (G_TYPE_UINT));
  g_assert_cmpuint (g_value_get_uint (value), ==,
      512);

  g_value_unset (value);
  g_free (value);
}

static void
test (Fixture *f,
    gconstpointer data)
{
  GDBusConnection *dbus;
  TpTestsContactsConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error = NULL;
  TpConnection *client_conn;
  guint status;
  gchar **interfaces;
  GValue *value = NULL;
  GQuark connected_feature[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  GTestDBus *test_dbus;

  /* Setup */

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  g_test_dbus_unset ();
  test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (test_dbus);

  dbus = tp_tests_dbus_dup_or_die ();

  service_conn = TP_TESTS_CONTACTS_CONNECTION (
      tp_tests_object_new_static_class (
        TP_TESTS_TYPE_CONTACTS_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  g_assert_no_error (error);

  client_conn = tp_tests_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  g_assert_no_error (error);

  /* Assert that GetInterfaces succeeds before we're CONNECTED */
  MYASSERT (tp_cli_dbus_properties_run_get (client_conn, -1,
          TP_IFACE_CONNECTION, "Interfaces", &value, &error, NULL), "");
  g_assert_no_error (error);
  interfaces = g_value_get_boxed (value);
  MYASSERT (tp_strv_contains ((const gchar * const *) interfaces,
      TP_IFACE_CONNECTION_INTERFACE_ALIASING1), "");
  MYASSERT (tp_strv_contains ((const gchar * const *) interfaces,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS1), "");
  MYASSERT (tp_strv_contains ((const gchar * const *) interfaces,
      TP_IFACE_CONNECTION_INTERFACE_PRESENCE1), "");
  MYASSERT (tp_strv_contains ((const gchar * const *) interfaces,
      TP_IFACE_CONNECTION_INTERFACE_PRESENCE1), "");
  g_value_unset (value);
  tp_clear_pointer (&value, g_free);

  MYASSERT (tp_cli_dbus_properties_run_get (client_conn, -1,
          TP_IFACE_CONNECTION, "Status", &value, &error, NULL), "");
  g_assert_no_error (error);
  status = g_value_get_uint (value);
  g_assert_cmpuint (status, ==, (guint) TP_CONNECTION_STATUS_DISCONNECTED);
  g_value_unset (value);
  g_free (value);

  tp_cli_connection_call_connect (client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (client_conn, connected_feature);

  /* Tests */

  test_presence (service_conn, client_conn);

  /* Teardown */

  tp_tests_connection_assert_disconnect_succeeds (client_conn);
  g_object_unref (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);

  g_test_dbus_down (test_dbus);
  tp_tests_assert_last_unref (&test_dbus);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/self-presence", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
