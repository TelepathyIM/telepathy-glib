#ifndef __TP_STREAM_ENGINE_UTIL_H__
#define __TP_STREAM_ENGINE_UTIL_H__

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

gboolean g_object_has_property (GObject *object, const gchar *property);
void media_server_disable (DBusGProxy **media_server_proxy);
void media_server_enable (DBusGProxy **media_server_proxy);

G_END_DECLS

#endif /* __TP_STREAM_ENGINE_UTIL_H__ */
