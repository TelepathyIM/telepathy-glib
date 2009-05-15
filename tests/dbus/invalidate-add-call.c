#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#include "_gen/svc.h"
#include "tests/lib/myassert.h"

int
main (int argc, char **argv)
{
  DBusGConnection *bus;
  TpProxy *proxy;
  GValue *value;
  GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "gabba gabba hey" };
  GError *error = NULL;

  tp_debug_set_flags ("all");
  g_type_init ();
  bus = tp_get_bus ();

  /* Open a D-Bus connection to myself */
  proxy = TP_PROXY (g_object_new (TP_TYPE_PROXY,
      "dbus-connection", bus,
      "bus-name",
          dbus_bus_get_unique_name (dbus_g_connection_get_connection (bus)),
      "object-path", "/",
      NULL));

  MYASSERT (tp_proxy_has_interface (proxy, "org.freedesktop.DBus.Properties"),
      "");

  /* Invalidate it */
  tp_proxy_invalidate (proxy, &e);

  MYASSERT (!tp_proxy_has_interface (proxy, "org.freedesktop.DBus.Properties"),
      "");

  /* Now forcibly re-add the Properties interface... */
  tp_proxy_add_interface_by_id (proxy, TP_IFACE_QUARK_DBUS_PROPERTIES);

  MYASSERT (tp_proxy_has_interface (proxy, "org.freedesktop.DBus.Properties"),
      "");

  /* ...and try to call a method on it, which should fail immediately with the
   * given invalidation reason.
   */
  MYASSERT (!tp_cli_dbus_properties_run_get (proxy, -1,
        "com.example.WithProperties", "ReadOnly", &value, &error, NULL), "");
  MYASSERT_SAME_ERROR (error, &e);

  g_error_free (error);

  g_object_unref (proxy);

  return 0;
}
