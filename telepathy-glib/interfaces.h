#ifndef __TP_INTERFACES_H__
#define __TP_INTERFACES_H__

#include <dbus/dbus-shared.h>
#include <glib/gquark.h>

G_BEGIN_DECLS

#include <telepathy-glib/_gen/telepathy-interfaces.h>

/* This one isn't auto-generated */
#define TP_IFACE_QUARK_DBUS_DAEMON (tp_iface_quark_dbus_daemon ())
GQuark tp_iface_quark_dbus_daemon (void);

G_END_DECLS

#endif
