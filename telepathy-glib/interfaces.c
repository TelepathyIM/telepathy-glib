#include <telepathy-glib/interfaces.h>

#include <dbus/dbus-shared.h>

/* This one isn't auto-generated */

/**
 * TP_IFACE_QUARK_DBUS_DAEMON:
 *
 * Expands to a call to a function that returns a quark whose string value is
 * %DBUS_INTERFACE_DBUS, the main interface exported by the D-Bus daemon
 */
GQuark
tp_iface_quark_dbus_daemon (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    {
      quark = g_quark_from_static_string (DBUS_INTERFACE_DBUS);
    }
  return quark;
}

/* auto-generated implementation stubs */
#include "_gen/interfaces-body.h"
