#ifndef __EXAMPLE_EXTENSIONS_H__
#define __EXAMPLE_EXTENSIONS_H__

#include <glib-object.h>
#include <telepathy-glib/proxy.h>

#include "_gen/enums.h"
#include "_gen/cli.h"

G_BEGIN_DECLS

#include "_gen/gtypes.h"
#include "_gen/interfaces.h"

void example_cli_add_signals (TpProxy *self, guint quark, DBusGProxy *proxy,
    gpointer unused);

void _example_ext_register_dbus_glib_marshallers (void);

G_END_DECLS

#endif
