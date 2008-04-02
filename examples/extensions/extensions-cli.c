#include "extensions.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/proxy-subclass.h>

static void _example_ext_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/signals-marshal.h"
#include "_gen/cli-connection-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

static gpointer
example_cli_once (gpointer data)
{
  _example_ext_register_dbus_glib_marshallers ();

  tp_connection_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_CONNECTION,
      example_cli_connection_add_signals);

  return NULL;
}

void
example_cli_init (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, example_cli_once, NULL);
}
