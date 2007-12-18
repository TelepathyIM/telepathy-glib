#include "extensions.h"

#include <telepathy-glib/proxy-subclass.h>

/* include auto-generated stubs */
#include "_gen/gtypes-body.h"
#include "_gen/interfaces-body.h"

/* FIXME: evil hack */
#define TP_IFACE_QUARK_CONNECTION_INTERFACE_HATS \
  EXAMPLE_IFACE_QUARK_CONNECTION_INTERFACE_HATS

#include "_gen/cli-body.h"

void
example_cli_add_signals (TpProxy *self,
                         guint quark,
                         DBusGProxy *proxy,
                         gpointer unused)
{
  example_cli_example_add_signals (self, quark, proxy, unused);
}

#include "_gen/signals-marshal.h"
#include "_gen/register-dbus-glib-marshallers-body.h"
