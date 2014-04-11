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

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/svc-connection.h>
#include <telepathy-glib/svc-interface.h>
#include <telepathy-glib/value-array.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

#define BALANCE 1234
#define BALANCE_SCALE 2
#define BALANCE_CURRENCY "BDD" /* badger dollars */
#define MANAGE_CREDIT_URI "http://chat.badger.net/topup"

/* -- BalancedConnection -- */
typedef TpTestsContactsConnection BalancedConnection;
typedef TpTestsContactsConnectionClass BalancedConnectionClass;

#define TYPE_BALANCED_CONNECTION (balanced_connection_get_type ())
static GType balanced_connection_get_type (void);

G_DEFINE_TYPE_WITH_CODE (BalancedConnection,
    balanced_connection,
    TP_TESTS_TYPE_CONTACTS_CONNECTION,

    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_BALANCE1, NULL))

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
balanced_connection_init (BalancedConnection *self)
{
}

static void
balanced_connection_constructed (GObject *object)
{
  GDBusObjectSkeleton *skel = G_DBUS_OBJECT_SKELETON (object);
  GDBusInterfaceSkeleton *iface;
  void (*parent_impl) (GObject *) =
    G_OBJECT_CLASS (balanced_connection_parent_class)->constructed;

  if (parent_impl != NULL)
    parent_impl (object);

  iface = tp_svc_interface_skeleton_new (skel,
      TP_TYPE_SVC_CONNECTION_INTERFACE_BALANCE1);
  g_dbus_object_skeleton_add_interface (skel, iface);
  g_object_unref (iface);
}

static void
balanced_connection_class_init (BalancedConnectionClass *cls)
{
  GObjectClass *object_class = (GObjectClass *) cls;

  static TpDBusPropertiesMixinPropImpl balance_props[] = {
        { "AccountBalance", "account-balance", NULL },
        { "ManageCreditURI", "manage-credit-uri", NULL },
        { NULL }
  };

  object_class->constructed = balanced_connection_constructed;
  object_class->get_property = balanced_connection_get_property;

  g_object_class_install_property (object_class, PROP_ACCOUNT_BALANCE,
      g_param_spec_boxed ("account-balance", "", "",
        TP_STRUCT_TYPE_CURRENCY_AMOUNT,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MANAGE_CREDIT_URI,
      g_param_spec_string ("manage-credit-uri", "", "",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_BALANCE1,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      balance_props);
}

/* -- UnbalancedConnection -- */
typedef TpTestsContactsConnection UnbalancedConnection;
typedef TpTestsContactsConnectionClass UnbalancedConnectionClass;

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
    GDBusConnection *dbus;
    GDBusConnection *client_gdbus;
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

  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_dup_or_die ();

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->error = NULL;

  test->client_gdbus = tp_tests_get_private_bus ();
  g_assert (test->client_gdbus != NULL);

  test->service_conn = tp_tests_object_new_static_class (
        conn_type,
        "account", "me@example.com",
        "protocol", "simple_protocol",
        NULL);
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "simple",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);

  test->cwr_ready = FALSE;
  test->cwr_error = NULL;

  test->conn = tp_tests_connection_new (test->client_gdbus, test->conn_name,
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
  conn = tp_tests_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);

  tp_tests_connection_assert_disconnect_succeeds (conn);
  tp_tests_proxy_run_until_prepared_or_failed (conn, NULL, &error);
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;

  g_dbus_connection_close_sync (test->client_gdbus, NULL, NULL);
  g_object_unref (test->client_gdbus);
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

  tp_svc_connection_interface_balance1_emit_balance_changed (
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
  tp_tests_abort_after (5);
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/conn/balance", Test,
      GSIZE_TO_POINTER (TYPE_BALANCED_CONNECTION),
      setup, test_balance, teardown);
  g_test_add ("/conn/balance-unknown", Test,
      GSIZE_TO_POINTER (TYPE_UNBALANCED_CONNECTION),
      setup, test_balance_unknown, teardown);
  g_test_add ("/conn/balance-unimplemented", Test,
      GSIZE_TO_POINTER (TP_TESTS_TYPE_CONTACTS_CONNECTION),
      setup, test_balance_unknown, teardown);

  return tp_tests_run_with_bus ();
}
