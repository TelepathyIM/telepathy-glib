#include "config.h"

#include "extensions.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>

static void _tpl_ext_register_dbus_glib_marshallers (void);

/* include auto-generated stubs for client-specific code */
#include "_gen/cli-misc-body.h"
#include "_gen/register-dbus-glib-marshallers-body.h"

static gpointer
tpl_cli_once (gpointer data)
{
  _tpl_ext_register_dbus_glib_marshallers ();

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (TP_TYPE_PROXY,
      tpl_cli_misc_add_signals);

  return NULL;
}

void
tpl_cli_init (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tpl_cli_once, NULL);
}
