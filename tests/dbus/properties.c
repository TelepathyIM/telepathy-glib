#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/proxy.h>
#include <telepathy-glib/svc-generic.h>

#include "_gen/svc.h"
#include "tests/myassert.h"

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
  MYASSERT (G_VALUE_HOLDS_UINT (value), "");
  MYASSERT (g_value_get_uint (value) == 57, "%u", g_value_get_uint (value));
  return TRUE;
}

static void
test_properties_class_init (TestPropertiesClass *cls)
{
  static TpDBusPropertiesMixinPropImpl with_properties_props[] = {
        { "ReadOnly", "read" },
        { "ReadWrite", "full-access" },
        { "WriteOnly", "black-hole" },
        { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl interfaces[] = {
      { 0, 0, prop_getter, prop_setter, with_properties_props },
      { 0 }
  };

  interfaces[0].dbus_interface =
      g_quark_from_static_string ("com.example.WithProperties");
  interfaces[0].svc_interface = TEST_TYPE_SVC_WITH_PROPERTIES;

  cls->props.interfaces = interfaces;

  tp_dbus_properties_mixin_class_init (G_OBJECT_CLASS (cls),
      G_STRUCT_OFFSET (TestPropertiesClass, props));
}

static int fail = 0;

static void
myassert_failed (void)
{
  fail = 1;
}

static void
print_asv_item (gpointer key,
                gpointer value,
                gpointer unused)
{
  gchar *value_str = g_strdup_value_contents (value);

  g_print ("  \"%s\" -> %s\n", (gchar *) key, value_str);
  g_free (value_str);
}

static void
print_asv (GHashTable *hash)
{
  g_print ("{\n");
  g_hash_table_foreach (hash, print_asv_item, NULL);
  g_print ("}\n");
}

int
main (int argc, char **argv)
{
  TestProperties *obj;
  DBusGConnection *bus;
  TpProxy *proxy;
  GValue *value;
  GHashTable *hash;

  tp_debug_set_flags ("all");
  g_type_init ();
  bus = tp_get_bus ();

  obj = g_object_new (TEST_TYPE_PROPERTIES, NULL);
  dbus_g_connection_register_g_object (bus, "/", (GObject *) obj);

  /* Open a D-Bus connection to myself */
  proxy = TP_PROXY (g_object_new (TP_TYPE_PROXY,
      "dbus-connection", bus,
      "bus-name",
          dbus_bus_get_unique_name (dbus_g_connection_get_connection (bus)),
      "object-path", "/",
      NULL));

  MYASSERT (tp_proxy_has_interface (proxy, "org.freedesktop.DBus.Properties"),
      "");

  MYASSERT (tp_cli_dbus_properties_run_get (proxy, -1,
        "com.example.WithProperties", "ReadOnly", &value, NULL, NULL), "");
  MYASSERT (G_VALUE_HOLDS_UINT (value), "");
  MYASSERT (g_value_get_uint (value) == 42, "%u", g_value_get_uint (value));

  g_value_set_uint (value, 57);

  MYASSERT (tp_cli_dbus_properties_run_set (proxy, -1,
        "com.example.WithProperties", "ReadWrite", value, NULL, NULL), "");

  MYASSERT (tp_cli_dbus_properties_run_set (proxy, -1,
        "com.example.WithProperties", "WriteOnly", value, NULL, NULL), "");

  g_boxed_free (G_TYPE_VALUE, value);

  MYASSERT (tp_cli_dbus_properties_run_get_all (proxy, -1,
        "com.example.WithProperties", &hash, NULL, NULL), "");
  MYASSERT (hash != NULL, "");
  print_asv (hash);
  MYASSERT (g_hash_table_size (hash) == 2, "%u", g_hash_table_size (hash));
  value = g_hash_table_lookup (hash, "WriteOnly");
  MYASSERT (value == NULL, "");
  value = g_hash_table_lookup (hash, "ReadOnly");
  MYASSERT (value != NULL, "");
  MYASSERT (G_VALUE_HOLDS_UINT (value), "");
  MYASSERT (g_value_get_uint (value) == 42, "%u", g_value_get_uint (value));
  value = g_hash_table_lookup (hash, "ReadWrite");
  MYASSERT (value != NULL, "");
  MYASSERT (G_VALUE_HOLDS_UINT (value), "");
  MYASSERT (g_value_get_uint (value) == 42, "%u", g_value_get_uint (value));

  g_hash_table_destroy (hash);

  g_object_unref (obj);
  g_object_unref (proxy);

  return fail;
}
