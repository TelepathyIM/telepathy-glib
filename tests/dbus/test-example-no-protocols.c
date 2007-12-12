#include <glib-object.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

static void
prepare (void)
{
  GError *error = NULL;
  const gchar *abs_top_builddir = g_getenv ("abs_top_builddir");
  gchar *command[] = {
      g_strdup_printf ("%s/%s",
          abs_top_builddir,
          "examples/cm/no-protocols/telepathy-example-no-protocols"),
      NULL
  };

  g_assert (abs_top_builddir != NULL);

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
  g_message ("Emitted got-info (source=%d)", source);

  if (source > 0)
    g_main_loop_quit (mainloop);
}

gboolean
time_out (gpointer mainloop)
{
  g_error ("Timed out");
  g_assert_not_reached ();
  return FALSE;
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpConnectionManager *cm;

  prepare ();

  g_type_init ();

  tp_debug_set_flags ("all");

  mainloop = g_main_loop_new (NULL, FALSE);

  cm = tp_connection_manager_new (tp_dbus_daemon_new (tp_get_bus ()),
      "example_no_protocols", NULL, NULL);
  g_assert (cm != NULL);

  g_signal_connect (cm, "got-info",
      G_CALLBACK (connection_manager_got_info), mainloop);

  g_timeout_add (5000, time_out, mainloop);

  g_main_loop_run (mainloop);

  g_object_unref (cm);
  g_main_loop_unref (mainloop);

  return 0;
}
