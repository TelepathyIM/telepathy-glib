#include <telepathy-glib/interfaces.h>

#include <dbus/dbus-shared.h>

/* This one isn't auto-generated */
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
