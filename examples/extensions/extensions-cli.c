#include "extensions.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/proxy-subclass.h>

#include "_gen/signals-marshal.h"

/* FIXME: evil hack */
#define TP_IFACE_QUARK_CONNECTION_INTERFACE_HATS \
  EXAMPLE_IFACE_QUARK_CONNECTION_INTERFACE_HATS

/* include auto-generated stubs for client-specific code */
#include "_gen/cli-connection-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

void
example_cli_conn_add_signals (TpProxy *self,
                              guint quark,
                              DBusGProxy *proxy,
                              gpointer unused)
{
  example_cli_connection_add_signals (self, quark, proxy, unused);
}
