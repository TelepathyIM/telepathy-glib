#include "extensions.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/proxy-subclass.h>

static void _example_ext_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/signals-marshal.h"
#include "_gen/cli-connection-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

/* I know, I know, init functions considered harmful. However, we need it. */
void
example_cli_init (void)
{
  _example_ext_register_dbus_glib_marshallers ();

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_CONNECTION,
      example_cli_connection_add_signals);
}
