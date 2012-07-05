/* Feature test for Conn.I.Balance
 *
 * Copyright © 2007-2011 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/simple-conn.h"
#include "tests/lib/util.h"

#define BALANCE 1234
#define BALANCE_SCALE 2
#define BALANCE_CURRENCY "BDD" /* badger dollars */
#define MANAGE_CREDIT_URI "http://chat.badger.net/topup"

/* -- BalancedConnection -- */
typedef TpTestsSimpleConnection BalancedConnection;
typedef TpTestsSimpleConnectionClass BalancedConnectionClass;

#define TYPE_BALANCED_CONNECTION (balanced_connection_get_type ())
static GType balanced_connection_get_type (void);

G_DEFINE_TYPE_WITH_CODE (BalancedConnection,
    balanced_connection,
    TP_TESTS_TYPE_SIMPLE_CONNECTION,

    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_BALANCE, NULL))

enum
{
  PROP_0,
  PROP_ACCOUNT_BALANCE,
  PROP_MANAGE_CREDIT_URI
};

static void
balanced_connection_get_property (GObject *self G_GNUC_UNUSED,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_ACCOUNT_BALANCE:
        /* known balance */
        g_value_take_boxed (value, tp_value_array_build (3,
              G_TYPE_INT, BALANCE,
              G_TYPE_UINT, BALANCE_SCALE,
              G_TYPE_STRING, BALANCE_CURRENCY,
              G_TYPE_INVALID));
        break;

      case PROP_MANAGE_CREDIT_URI:
        g_value_set_static_string (value, MANAGE_CREDIT_URI);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}

static void
balanced_connection_init (BalancedConnection *self G_GNUC_UNUSED)
{
}

static GPtrArray *
get_interfaces (TpBaseConnection *base)
{
  GPtrArray *interfaces;

  interfaces = TP_BASE_CONNECTION_CLASS (
      balanced_connection_parent_class)->get_interfaces_always_present (base);

  g_ptr_array_add (interfaces, TP_IFACE_CONNECTION_INTERFACE_BALANCE);

  return interfaces;
}

static void
balanced_connection_class_init (BalancedConnectionClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;
  TpBaseConnectionClass *base_class = TP_BASE_CONNECTION_CLASS (cls);

  static TpDBusPropertiesMixinPropImpl balance_props[] = {
        { "AccountBalance", "account-balance", NULL },
        { "ManageCreditURI", "manage-credit-uri", NULL },
        { NULL }
  };

  object_class->get_property = balanced_connection_get_property;

  base_class->get_interfaces_always_present = get_interfaces;

  g_object_class_install_property (object_class, PROP_ACCOUNT_BALANCE,
      g_param_spec_boxed ("account-balance", "", "",
        TP_STRUCT_TYPE_CURRENCY_AMOUNT,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MANAGE_CREDIT_URI,
      g_param_spec_string ("manage-credit-uri", "", "",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_BALANCE,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      balance_props);
}

/* -- UnbalancedConnection -- */
typedef TpTestsSimpleConnection UnbalancedConnection;
typedef TpTestsSimpleConnectionClass UnbalancedConnectionClass;

#define TYPE_UNBALANCED_CONNECTION (unbalanced_connection_get_type ())
static GType unbalanced_connection_get_type (void);

G_DEFINE_TYPE (UnbalancedConnection,
    unbalanced_connection,
    TYPE_BALANCED_CONNECTION)

static void
unbalanced_connection_get_property (GObject *self G_GNUC_UNUSED,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_ACCOUNT_BALANCE:
        /* unknown balance */
        g_value_take_boxed (value, tp_value_array_build (3,
              G_TYPE_INT, 0,
              G_TYPE_UINT, G_MAXUINT32,
              G_TYPE_STRING, "",
              G_TYPE_INVALID));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}

static void
unbalanced_connection_init (UnbalancedConnection *self G_GNUC_UNUSED)
{
}

static void
unbalanced_connection_class_init (UnbalancedConnectionClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;

  object_class->get_property = unbalanced_connection_get_property;

  g_object_class_override_property (object_class, PROP_ACCOUNT_BALANCE,
      "account-balance");
}

/* -- Tests -- */
typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;
    DBusConnection *client_libdbus;
    DBusGConnection *client_dbusglib;
    TpDBusDaemon *client_bus;
    TpTestsSimpleConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    gboolean cwr_ready;
    GError *cwr_error /* initialized in setup */;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  GType conn_type = GPOINTER_TO_SIZE (data);

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->error = NULL;

  test->client_libdbus = dbus_bus_get_private (DBUS_BUS_STARTER, NULL);
  g_assert (test->client_libdbus != NULL);
  dbus_connection_setup_with_g_main (test->client_libdbus, NULL);
  dbus_connection_set_exit_on_disconnect (test->client_libdbus, FALSE);
  test->client_dbusglib = dbus_connection_get_g_connection (
      test->client_libdbus);
  dbus_g_connection_ref (test->client_dbusglib);
  test->client_bus = tp_dbus_daemon_new (test->client_dbusglib);
  g_assert (test->client_bus != NULL);

  test->service_conn = tp_tests_object_new_static_class (
        conn_type,
        "account", "me@example.com",
        "protocol", "simple-protocol",
        NULL);
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "simple",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;

  test->conn = tp_connection_new (test->client_bus, test->conn_name,
      test->conn_path, &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));
  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_BALANCE));

  tp_tests_proxy_run_until_prepared (test->conn, features);
}

static void
teardown (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpConnection *conn;
  GError *error = NULL;

  g_clear_error (&test->error);
  tp_clear_pointer (&test->mainloop, g_main_loop_unref);
  tp_clear_object (&test->conn);

  /* disconnect the connection so we don't leak it */
  conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);

  tp_tests_connection_assert_disconnect_succeeds (conn);

  g_assert (!tp_connection_run_until_ready (conn, FALSE, &error, NULL));
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_object_unref (test->client_bus);
  test->client_bus = NULL;

  dbus_g_connection_unref (test->client_dbusglib);
  dbus_connection_close (test->client_libdbus);
  dbus_connection_unref (test->client_libdbus);
}

static void
balance_changed_cb (TpConnection *conn,
    gint balance,
    guint scale,
    const gchar *currency,
    Test *test)
{
  g_assert_cmpint (balance, ==, BALANCE * 2);
  g_assert_cmpuint (scale, ==, BALANCE_SCALE);
  g_assert_cmpstr (currency, ==, BALANCE_CURRENCY);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_balance (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_BALANCE, 0 };
  gint balance;
  guint scale;
  const gchar *currency, *uri;
  gchar *currency_alloc, *uri_alloc;
  GValueArray *v;

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_BALANCE));

  tp_tests_proxy_run_until_prepared (test->conn, features);

  g_assert (tp_connection_get_balance (test->conn,
        &balance, &scale, &currency));

  g_assert_cmpint (balance, ==, BALANCE);
  g_assert_cmpuint (scale, ==, BALANCE_SCALE);
  g_assert_cmpstr (currency, ==, BALANCE_CURRENCY);

  uri = tp_connection_get_balance_uri (test->conn);

  g_assert_cmpstr (uri, ==, MANAGE_CREDIT_URI);

  g_object_get (test->conn,
      "balance", &balance,
      "balance-scale", &scale,
      "balance-currency", &currency_alloc,
      "balance-uri", &uri_alloc,
      NULL);

  g_assert_cmpint (balance, ==, BALANCE);
  g_assert_cmpuint (scale, ==, BALANCE_SCALE);
  g_assert_cmpstr (currency_alloc, ==, BALANCE_CURRENCY);
  g_assert_cmpstr (uri_alloc, ==, MANAGE_CREDIT_URI);

  v = tp_value_array_build (3,
              G_TYPE_INT, BALANCE * 2,
              G_TYPE_UINT, BALANCE_SCALE,
              G_TYPE_STRING, BALANCE_CURRENCY,
              G_TYPE_INVALID);

  tp_svc_connection_interface_balance_emit_balance_changed (
      test->service_conn_as_base, v);

  g_signal_connect (test->conn, "balance-changed",
      G_CALLBACK (balance_changed_cb), test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_free (currency_alloc);
  g_free (uri_alloc);
}

static void
test_balance_unknown (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GQuark features[] = { TP_CONNECTION_FEATURE_BALANCE, 0 };
  gint balance;
  guint scale;
  const gchar *currency;

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_BALANCE));

  tp_tests_proxy_run_until_prepared (test->conn, features);

  g_assert (!tp_connection_get_balance (test->conn,
        &balance, &scale, &currency));
}

int
main (int argc,
      char **argv)
{
  g_type_init ();

  tp_tests_abort_after (5);
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/conn/balance", Test,
      GSIZE_TO_POINTER (TYPE_BALANCED_CONNECTION),
      setup, test_balance, teardown);
  g_test_add ("/conn/balance-unknown", Test,
      GSIZE_TO_POINTER (TYPE_UNBALANCED_CONNECTION),
      setup, test_balance_unknown, teardown);
  g_test_add ("/conn/balance-unimplemented", Test,
      GSIZE_TO_POINTER (TP_TESTS_TYPE_SIMPLE_CONNECTION),
      setup, test_balance_unknown, teardown);

  return g_test_run ();
}
