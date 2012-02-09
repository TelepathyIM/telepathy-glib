#include "config.h"

#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/proxy.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/util.h>

#include "_gen/svc.h"
#include "tests/lib/util.h"

#define WITH_PROPERTIES_IFACE "com.example.WithProperties"

typedef struct _TestProperties {
    GObject parent;
} TestProperties;
typedef struct _TestPropertiesClass {
    GObjectClass parent;
    TpDBusPropertiesMixinClass props;
} TestPropertiesClass;

GType test_properties_get_type (void);

#define TEST_TYPE_PROPERTIES \
  (test_properties_get_type ())
#define TEST_PROPERTIES(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_PROPERTIES, \
                               TestProperties))
#define TEST_PROPERTIES_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_PROPERTIES, \
                            TestPropertiesClass))
#define TEST_IS_PROPERTIES(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_PROPERTIES))
#define TEST_IS_PROPERTIES_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_PROPERTIES))
#define TEST_PROPERTIES_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_PROPERTIES, \
                              TestPropertiesClass))

G_DEFINE_TYPE_WITH_CODE (TestProperties,
    test_properties,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TEST_TYPE_SVC_WITH_PROPERTIES, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init));

static void
test_properties_init (TestProperties *self)
{
}

static void
prop_getter (GObject *object,
             GQuark interface,
             GQuark name,
             GValue *value,
             gpointer user_data)
{
  if (tp_strdiff (user_data, "read"))
    g_assert_cmpstr (user_data, ==, "full-access");

  g_value_set_uint (value, 42);
}

static gboolean
prop_setter (GObject *object,
             GQuark interface,
             GQuark name,
             const GValue *value,
             gpointer user_data,
             GError **error)
{
  g_assert (G_VALUE_HOLDS_UINT (value));

  if (tp_strdiff (user_data, "FULL ACCESS"))
    g_assert_cmpstr (user_data, ==, "BLACK HOLE");

  g_assert_cmpuint (g_value_get_uint (value), ==, 57);
  return TRUE;
}

static void
test_properties_class_init (TestPropertiesClass *cls)
{
  static TpDBusPropertiesMixinPropImpl with_properties_props[] = {
        { "ReadOnly", "read", "READ" },
        { "ReadWrite", "full-access", "FULL ACCESS" },
        { "WriteOnly", "black-hole", "BLACK HOLE" },
        { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl interfaces[] = {
      { WITH_PROPERTIES_IFACE, prop_getter, prop_setter,
        with_properties_props },
      { NULL }
  };

  cls->props.interfaces = interfaces;

  tp_dbus_properties_mixin_class_init (G_OBJECT_CLASS (cls),
      G_STRUCT_OFFSET (TestPropertiesClass, props));
}

static void
test_get (TpProxy *proxy)
{
  GValue *value;

  g_assert (tp_cli_dbus_properties_run_get (proxy, -1,
        WITH_PROPERTIES_IFACE, "ReadOnly", &value, NULL, NULL));
  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert_cmpuint (g_value_get_uint (value), ==, 42);

  g_boxed_free (G_TYPE_VALUE, value);
}

static void
test_set (TpProxy *proxy)
{
  GValue value = { 0, };

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, 57);

  g_assert (tp_cli_dbus_properties_run_set (proxy, -1,
        WITH_PROPERTIES_IFACE, "ReadWrite", &value, NULL, NULL));
  g_assert (tp_cli_dbus_properties_run_set (proxy, -1,
        WITH_PROPERTIES_IFACE, "WriteOnly", &value, NULL, NULL));

  g_value_unset (&value);
}

static void
test_get_all (TpProxy *proxy)
{
  GHashTable *hash;
  GValue *value;

  g_assert (tp_cli_dbus_properties_run_get_all (proxy, -1,
        WITH_PROPERTIES_IFACE, &hash, NULL, NULL));
  g_assert (hash != NULL);
  tp_asv_dump (hash);
  g_assert_cmpuint (g_hash_table_size (hash), ==, 2);

  value = g_hash_table_lookup (hash, "WriteOnly");
  g_assert (value == NULL);

  value = g_hash_table_lookup (hash, "ReadOnly");
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert_cmpuint (g_value_get_uint (value), ==, 42);

  value = g_hash_table_lookup (hash, "ReadWrite");
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert_cmpuint (g_value_get_uint (value), ==, 42);

  g_hash_table_unref (hash);
}

static void
properties_changed_cb (
    TpProxy *proxy,
    const gchar *interface_name,
    GHashTable *changed_properties,
    const gchar **invalidated_properties,
    gpointer user_data,
    GObject *weak_object)
{
  GMainLoop *loop = user_data;
  guint value;
  gboolean valid;

  g_assert_cmpuint (g_hash_table_size (changed_properties), ==, 1);
  value = tp_asv_get_uint32 (changed_properties, "ReadOnly", &valid);
  g_assert (valid);
  g_assert_cmpuint (value, ==, 42);

  g_assert_cmpuint (g_strv_length ((gchar **) invalidated_properties), ==, 1);
  g_assert_cmpstr (invalidated_properties[0], ==, "ReadWrite");

  g_main_loop_quit (loop);
}

typedef struct {
    TestProperties *obj;
    TpProxy *proxy;
} Context;

static void
test_emit_changed (Context *ctx)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  TpProxySignalConnection *signal_conn;
  const gchar *properties[] = { "ReadOnly", "ReadWrite", NULL };
  GError *error = NULL;

  signal_conn = tp_cli_dbus_properties_connect_to_properties_changed (
      ctx->proxy, properties_changed_cb, loop, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (signal_conn != NULL);

  tp_dbus_properties_mixin_emit_properties_changed (G_OBJECT (ctx->obj),
      WITH_PROPERTIES_IFACE, properties);
  g_main_loop_run (loop);

  tp_dbus_properties_mixin_emit_properties_changed_varargs (G_OBJECT (ctx->obj),
      WITH_PROPERTIES_IFACE, "ReadOnly", "ReadWrite", NULL);
  g_main_loop_run (loop);

  tp_proxy_signal_connection_disconnect (signal_conn);
}

int
main (int argc, char **argv)
{
  Context ctx;
  TpDBusDaemon *dbus_daemon;

  tp_tests_init (&argc, &argv);

  dbus_daemon = tp_tests_dbus_daemon_dup_or_die ();
  ctx.obj = tp_tests_object_new_static_class (TEST_TYPE_PROPERTIES, NULL);
  tp_dbus_daemon_register_object (dbus_daemon, "/", ctx.obj);

  /* Open a D-Bus connection to myself */
  ctx.proxy = TP_PROXY (tp_tests_object_new_static_class (TP_TYPE_PROXY,
      "dbus-daemon", dbus_daemon,
      "bus-name", tp_dbus_daemon_get_unique_name (dbus_daemon),
      "object-path", "/",
      NULL));

  g_assert (tp_proxy_has_interface (ctx.proxy, "org.freedesktop.DBus.Properties"));

  g_test_add_data_func ("/properties/get", ctx.proxy, (GTestDataFunc) test_get);
  g_test_add_data_func ("/properties/set", ctx.proxy, (GTestDataFunc) test_set);
  g_test_add_data_func ("/properties/get-all", ctx.proxy, (GTestDataFunc) test_get_all);

  g_test_add_data_func ("/properties/changed", &ctx, (GTestDataFunc) test_emit_changed);

  g_test_run ();

  g_object_unref (ctx.obj);
  g_object_unref (ctx.proxy);
  g_object_unref (dbus_daemon);

  return 0;
}
