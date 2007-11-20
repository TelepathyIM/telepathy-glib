#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>

static GPtrArray *events;
static TpDBusDaemon *bus;
static GMainLoop *mainloop;
static gchar *two = "2", *five = "5";

static void
noc (TpDBusDaemon *bus,
     const gchar *name,
     const gchar *old_owner,
     const gchar *new_owner,
     gpointer user_data)
{
  const gchar *tag = user_data;

  g_ptr_array_add (events, g_strdup_printf ("[%s] %s %d %d",
        tag, name, old_owner[0], new_owner[0]));

  if (!tp_strdiff (name, "net.example"))
    {
      if (new_owner[0] == '\0')
        {
          g_main_loop_quit (mainloop);
        }
      else
        {
          guint ret;
          GError *error = NULL;

          g_assert (tp_dbus_daemon_cancel_name_owner_watch (bus,
                "org.example", noc, five));
          g_assert (tp_cli_dbus_daemon_block_on_release_name (bus, -1,
                "org.example", &ret, &error));
          g_assert (ret == 1 && error == NULL);
          g_assert (tp_cli_dbus_daemon_block_on_release_name (bus, -1,
                "net.example", &ret, &error));
          g_assert (ret == 1 && error == NULL);
        }
    }
}

static gboolean
idle1 (gpointer unused)
{
  guint ret;
  GError *error = NULL;

  g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1, "com.example",
        0, &ret, &error));
  g_assert (ret == 1 && error == NULL);
  g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1, "org.example",
        0, &ret, &error));
  g_assert (ret == 1 && error == NULL);
  g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1, "net.example",
        0, &ret, &error));
  g_assert (ret == 1 && error == NULL);

  return FALSE;
}

int
main (int argc,
      char **argv)
{
  guint i;
  TpDBusDaemon *bus2;

  mainloop = g_main_loop_new (NULL, FALSE);

  events = g_ptr_array_new ();

  g_type_init ();

  bus = tp_dbus_daemon_new (tp_get_bus ());
  bus2 = tp_dbus_daemon_new (tp_get_bus ());

  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "1", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, two, NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "3", NULL);
  tp_dbus_daemon_cancel_name_owner_watch (bus, "com.example", noc, two);
  tp_dbus_daemon_watch_name_owner (bus, "net.example", noc, "4", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "org.example", noc, five, NULL);

  g_idle_add (idle1, NULL);

  g_main_loop_run (mainloop);

  for (i = 0; i < events->len; i++)
    {
      g_message ("at %u: %s", i, (gchar *) g_ptr_array_index (events, i));
    }

  g_assert (events->len == 5);

  /* 58 == ':' - i.e. the beginning of a unique name */
  g_assert (!tp_strdiff (g_ptr_array_index (events, 0),
        "[1] com.example 0 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 1),
        "[3] com.example 0 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 2),
        "[5] org.example 0 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 3),
        "[4] net.example 0 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 4),
        "[4] net.example 58 0"));

  return 0;
}
