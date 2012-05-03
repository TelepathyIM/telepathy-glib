#include "config.h"

#include <glib-object.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/errors.h>

#include "tests/lib/util.h"

static void
prepare (void)
{
  GError *error = NULL;
  const gchar *abs_top_builddir = g_getenv ("abs_top_builddir");
  const gchar *libexec = g_getenv ("libexec");
  gchar *command[] = { NULL, NULL };

  g_assert (abs_top_builddir != NULL || libexec != NULL);

  if (abs_top_builddir != NULL)
    {
      command[0] = g_strdup_printf ("%s/%s",
            abs_top_builddir,
            "examples/cm/no-protocols/telepathy-example-no-protocols");
    }
  else
    {
      command[0] = g_strdup_printf ("%s/%s",
          libexec,
          "telepathy-example-no-protocols");
    }

  if (!g_spawn_async (NULL, command, NULL, 0, NULL, NULL, NULL, &error))
    {
      g_error ("g_spawn_async: %s", error->message);
    }

  g_free (command[0]);
}

static void
connection_manager_got_info (TpConnectionManager *cm,
                             guint source,
                             GMainLoop *mainloop)
{
  GHashTable *empty = g_hash_table_new (NULL, NULL);
  gchar *bus_name = NULL;
  gchar *object_path = NULL;
  GError *error = NULL;

  g_message ("Emitted got-info (source=%d)", source);

  if (source < TP_CM_INFO_SOURCE_LIVE)
    return;

  tp_cli_connection_manager_run_request_connection (cm, -1,
      "jabber", empty, &bus_name, &object_path, &error, NULL);

  g_assert (error != NULL);
  g_assert (error->domain == TP_ERROR);
  g_assert (error->code == TP_ERROR_NOT_IMPLEMENTED);

  g_error_free (error);

  g_main_loop_quit (mainloop);

  g_hash_table_unref (empty);
}

static void
wait_for_name_owner_cb (TpDBusDaemon *dbus_daemon,
    const gchar *name,
    const gchar *new_owner,
    gpointer main_loop)
{
  if (new_owner[0] != '\0')
    g_main_loop_quit (main_loop);
}

static void
early_cm_exited (TpConnectionManager *cm,
    gboolean *saw_exited)
{
  *saw_exited = TRUE;
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpConnectionManager *early_cm, *late_cm;
  TpDBusDaemon *dbus_daemon;
  gulong handler;
  GError *error = NULL;
  gboolean saw_exited;

  tp_tests_abort_after (5);
  g_type_init ();

  tp_debug_set_flags ("all");

  mainloop = g_main_loop_new (NULL, FALSE);

  dbus_daemon = tp_tests_dbus_daemon_dup_or_die ();

  /* First try making a TpConnectionManager before the CM is available. This
   * will fail. */
  early_cm = tp_connection_manager_new (dbus_daemon, "example_no_protocols",
      NULL, NULL);
  g_assert (early_cm != NULL);

  /* Failure to introspect is signalled as 'exited' */
  handler = g_signal_connect (early_cm, "exited",
      G_CALLBACK (early_cm_exited), &saw_exited);

  tp_tests_proxy_run_until_prepared_or_failed (early_cm, NULL, &error);
  g_assert (error != NULL);
  g_assert (tp_proxy_get_invalidated (early_cm) == NULL);
  g_assert_cmpuint (error->domain, ==, DBUS_GERROR);
  g_assert_cmpint (error->code, ==, DBUS_GERROR_SERVICE_UNKNOWN);
  g_clear_error (&error);

  if (!saw_exited)
    {
      g_debug ("waiting for 'exited'...");

      while (!saw_exited)
        g_main_context_iteration (NULL, TRUE);
    }

  g_signal_handler_disconnect (early_cm, handler);

  /* Now start the connection manager and wait for it to start */
  prepare ();
  tp_dbus_daemon_watch_name_owner (dbus_daemon,
      TP_CM_BUS_NAME_BASE "example_no_protocols", wait_for_name_owner_cb,
      g_main_loop_ref (mainloop), (GDestroyNotify) g_main_loop_unref);
  g_main_loop_run (mainloop);

  /* This TpConnectionManager works fine. */
  late_cm = tp_connection_manager_new (dbus_daemon, "example_no_protocols",
      NULL, NULL);
  g_assert (late_cm != NULL);

  handler = g_signal_connect (late_cm, "got-info",
      G_CALLBACK (connection_manager_got_info), mainloop);
  g_main_loop_run (mainloop);
  g_signal_handler_disconnect (late_cm, handler);

  /* Now both objects can become ready */
  tp_tests_proxy_run_until_prepared (early_cm, NULL);
  tp_tests_proxy_run_until_prepared (late_cm, NULL);

  g_object_unref (late_cm);
  g_object_unref (early_cm);
  g_object_unref (dbus_daemon);
  g_main_loop_unref (mainloop);

  return 0;
}
