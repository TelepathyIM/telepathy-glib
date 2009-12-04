#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/channel-dispatch-operation.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>

#include <tpl_observer.h>

#include <tpl-log-store-empathy.h>

/* 
 * Initialization of TPL (TelePathy Logger), it futurely set all the
 * inernal structs. tpl_headless_logger_deinit will free/unref them
 */
void tpl_headless_logger_init(void)
{
	TplObserver *observer;
	DBusGConnection *bus;
	TpDBusDaemon *tp_bus;
	GError *error = NULL;

	bus = tp_get_bus();
	tp_bus = tp_dbus_daemon_new(bus);
	
	if ( tp_dbus_daemon_request_name (tp_bus, TPL_OBSERVER_WELL_KNOWN_BUS_NAME,
			TRUE, &error) ) {
		g_print("Well Known name requested successfully!\n");
	} else {
		g_print("Well Known name request error: %s\n", error->message);
		g_clear_error(&error);
		g_error_free(error);
	}


	observer = tpl_observer_new ();
	dbus_g_connection_register_g_object (bus,
			TPL_OBSERVER_OBJECT_PATH,
			G_OBJECT(observer));
}
