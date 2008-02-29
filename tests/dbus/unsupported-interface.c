#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

#include "tests/myassert.h"

static int fail = 0;
static gboolean had_unsupported = FALSE;
static gboolean had_supported = FALSE;
static GMainLoop *mainloop = NULL;

static void
myassert_failed (void)
{
  fail = 1;
}

static void
supported_cb (TpDBusDaemon *bus_daemon,
              const gchar **names,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  MYASSERT (user_data == NULL, "");
  MYASSERT (weak_object == NULL, "");
  MYASSERT (names != NULL, "");
  MYASSERT (bus_daemon != NULL, "");
  MYASSERT (error == NULL, "");
  MYASSERT (mainloop != NULL, "");

  MYASSERT (!had_supported, "");
  had_supported = TRUE;

  if (had_unsupported && had_supported)
    g_main_loop_quit (mainloop);
}

static void
unsupported_cb (TpProxy *proxy,
                const GPtrArray *out0,
                const GError *error,
                gpointer user_data,
                GObject *weak_object)
{
  MYASSERT (weak_object == NULL, "");
  MYASSERT (user_data == NULL, "");
  MYASSERT (proxy != NULL, "");
  MYASSERT (out0 == NULL, "");
  MYASSERT (error != NULL, "");
  MYASSERT (mainloop != NULL, "");

  MYASSERT (!had_unsupported, "");
  had_unsupported = TRUE;

  if (had_unsupported && had_supported)
    g_main_loop_quit (mainloop);
}

static void
do_nothing (void)
{
}

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *bus_daemon;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags ("all");

  bus_daemon = tp_dbus_daemon_new (tp_get_bus ());

  /* this interface is automatically supported... */
  MYASSERT (tp_cli_dbus_daemon_run_list_names (bus_daemon, -1, NULL,
        NULL, NULL), "");

  /* ... but this one is not */
  MYASSERT (!tp_cli_properties_interface_run_list_properties (bus_daemon, -1,
        NULL, &error, NULL), "");
  MYASSERT (error != NULL, "");
  g_error_free (error);
  error = NULL;

  /* the same, but with async API */

  mainloop = g_main_loop_new (NULL, FALSE);

  MYASSERT (tp_cli_dbus_daemon_call_list_names (bus_daemon, -1, supported_cb,
        NULL, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_properties_interface_call_list_properties (bus_daemon, -1,
        unsupported_cb, NULL, NULL, NULL) == NULL, "");

  /* the same, but with signals */
  MYASSERT (tp_cli_dbus_daemon_connect_to_name_acquired (bus_daemon,
        (tp_cli_dbus_daemon_signal_callback_name_acquired) do_nothing,
        NULL, NULL, NULL, NULL) != NULL, "");
  MYASSERT (tp_cli_properties_interface_connect_to_property_flags_changed
        (bus_daemon,
        (tp_cli_properties_interface_signal_callback_property_flags_changed)
            do_nothing,
        NULL, NULL, NULL, &error) == NULL, "");
  MYASSERT (error != NULL, "");
  g_error_free (error);
  error = NULL;

  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  return fail;
}
