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
      { "com.example.WithProperties", prop_getter, prop_setter,
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
        "com.example.WithProperties", "ReadOnly", &value, NULL, NULL));
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
        "com.example.WithProperties", "ReadWrite", &value, NULL, NULL));
  g_assert (tp_cli_dbus_properties_run_set (proxy, -1,
        "com.example.WithProperties", "WriteOnly", &value, NULL, NULL));

  g_value_unset (&value);
}

static void
test_get_all (TpProxy *proxy)
{
  GHashTable *hash;
  GValue *value;

  g_assert (tp_cli_dbus_properties_run_get_all (proxy, -1,
        "com.example.WithProperties", &hash, NULL, NULL));
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

  g_hash_table_destroy (hash);
}

int
main (int argc, char **argv)
{
  TestProperties *obj;
  TpDBusDaemon *dbus_daemon;
  TpProxy *proxy;

  tp_tests_init (&argc, &argv);

  dbus_daemon = tp_tests_dbus_daemon_dup_or_die ();
  obj = tp_tests_object_new_static_class (TEST_TYPE_PROPERTIES, NULL);
  tp_dbus_daemon_register_object (dbus_daemon, "/", obj);

  /* Open a D-Bus connection to myself */
  proxy = TP_PROXY (tp_tests_object_new_static_class (TP_TYPE_PROXY,
      "dbus-daemon", dbus_daemon,
      "bus-name", tp_dbus_daemon_get_unique_name (dbus_daemon),
      "object-path", "/",
      NULL));

  g_assert (tp_proxy_has_interface (proxy, "org.freedesktop.DBus.Properties"));

  g_test_add_data_func ("/properties/get", proxy, (GTestDataFunc) test_get);
  g_test_add_data_func ("/properties/set", proxy, (GTestDataFunc) test_set);
  g_test_add_data_func ("/properties/get-all", proxy, (GTestDataFunc) test_get_all);

  g_test_run ();

  g_object_unref (obj);
  g_object_unref (proxy);
  g_object_unref (dbus_daemon);

  return 0;
}
