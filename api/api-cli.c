#include "api/api.h"

#include <telepathy-glib/proxy-subclass.h>

static void _se_api_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/signals-marshal.h"
#include "_gen/cli-misc-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

static gpointer
stream_engine_cli_once (gpointer data)
{
  _se_api_register_dbus_glib_marshallers ();

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_PROXY,
      stream_engine_cli_misc_add_signals);

  return NULL;
}

void
stream_engine_cli_init (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, stream_engine_cli_once, NULL);
}
