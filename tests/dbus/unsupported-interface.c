#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *bus_daemon;
  GError *error = NULL;

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");

  bus_daemon = tp_tests_dbus_daemon_dup_or_die ();

  /* this interface is automatically supported... */
  MYASSERT (tp_cli_dbus_daemon_run_list_names (bus_daemon, -1, NULL,
        NULL, NULL), "");

  /* ... but this one is not */
  MYASSERT (!tp_cli_dbus_properties_run_get_all (bus_daemon, -1, NULL,
          NULL, &error, NULL), "");
  MYASSERT (error != NULL, "");
  g_error_free (error);
  error = NULL;

  /* Proxies are assumed to have
   * org.freedesktop.DBus.{Peer,Ping,Properties,…} so async calls will
   * these will not fail to be dispatched even if they are going to
   * fail. We can't test using, say, Account methods/signals because
   * they have TP_IS_ACCOUNT typechecks which won't pass here.
   * Therefore, we don't test the async API or signals here. */

  g_object_unref (bus_daemon);

  return 0;
}
