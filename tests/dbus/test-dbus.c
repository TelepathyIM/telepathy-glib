#include <dbus/dbus-shared.h>
#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

static GPtrArray *events;
static TpDBusDaemon *bus;
static GMainLoop *mainloop;
static gchar *two = "2", *five = "5";
static gboolean had_owners = FALSE;

static void
noc (TpDBusDaemon *bus,
     const gchar *name,
     const gchar *new_owner,
     gpointer user_data)
{
  const gchar *tag = user_data;

  g_message ("[%s] %s -> <%s>", tag, name, new_owner);

  g_ptr_array_add (events, g_strdup_printf ("[%s] %s %d",
        tag, name, new_owner[0]));

  if (new_owner[0] != '\0')
    had_owners = TRUE;

  if (!tp_strdiff (name, "net.example"))
    {
      if (new_owner[0] == '\0')
        {
          if (had_owners)
            {
              g_main_loop_quit (mainloop);
            }
          else
            {
              guint ret;
              GError *error = NULL;

              g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1,
                    "com.example", 0, &ret, &error));
              g_assert (ret == 1 && error == NULL);
              g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1,
                    "org.example", 0, &ret, &error));
              g_assert (ret == 1 && error == NULL);
              g_assert (tp_cli_dbus_daemon_block_on_request_name (bus, -1,
                    "net.example", 0, &ret, &error));
              g_assert (ret == 1 && error == NULL);
            }
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

int
main (int argc,
      char **argv)
{
  guint i;

  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);

  events = g_ptr_array_new ();

  g_type_init ();

    {
      GError *error = NULL;

      if (!tp_dbus_check_valid_bus_name (":1.1", TP_DBUS_NAME_TYPE_ANY,
            &error))
        {
          g_message ("%s", error->message);
        }
    }

  g_assert (tp_dbus_check_valid_bus_name (":1.1", TP_DBUS_NAME_TYPE_ANY,
        NULL));
  g_assert (tp_dbus_check_valid_bus_name ("com.example", TP_DBUS_NAME_TYPE_ANY,
        NULL));
  g_assert (tp_dbus_check_valid_bus_name (DBUS_SERVICE_DBUS,
        TP_DBUS_NAME_TYPE_ANY, NULL));

  g_assert (tp_dbus_check_valid_bus_name (":1.1",
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, NULL));
  g_assert (tp_dbus_check_valid_bus_name ("com.example",
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (DBUS_SERVICE_DBUS,
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, NULL));

  g_assert (!tp_dbus_check_valid_bus_name (":1.1",
        TP_DBUS_NAME_TYPE_BUS_DAEMON, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("com.example",
        TP_DBUS_NAME_TYPE_BUS_DAEMON, NULL));
  g_assert (tp_dbus_check_valid_bus_name (DBUS_SERVICE_DBUS,
        TP_DBUS_NAME_TYPE_BUS_DAEMON, NULL));

  g_assert (!tp_dbus_check_valid_bus_name (":1.1",
        TP_DBUS_NAME_TYPE_WELL_KNOWN, NULL));
  g_assert (tp_dbus_check_valid_bus_name ("com.example",
        TP_DBUS_NAME_TYPE_WELL_KNOWN, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (DBUS_SERVICE_DBUS,
        TP_DBUS_NAME_TYPE_WELL_KNOWN, NULL));

  g_assert (tp_dbus_check_valid_bus_name (":1.1",
        TP_DBUS_NAME_TYPE_UNIQUE, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("com.example",
        TP_DBUS_NAME_TYPE_UNIQUE, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (DBUS_SERVICE_DBUS,
        TP_DBUS_NAME_TYPE_UNIQUE, NULL));

  g_assert (!tp_dbus_check_valid_bus_name ("com.1",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("com.e*ample",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("com..example",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (".com.example",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("com.example.",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (":1.1.",
        TP_DBUS_NAME_TYPE_ANY, NULL));

  bus = tp_dbus_daemon_new (tp_get_bus ());

  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "1", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, two, NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "3", NULL);
  tp_dbus_daemon_cancel_name_owner_watch (bus, "com.example", noc, two);
  tp_dbus_daemon_watch_name_owner (bus, "net.example", noc, "4", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "org.example", noc, five, NULL);

  g_main_loop_run (mainloop);

  g_assert (events->len == 9);

  /* 58 == ':' - i.e. the beginning of a unique name */
  g_assert (!tp_strdiff (g_ptr_array_index (events, 0),
        "[1] com.example 0"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 1),
        "[3] com.example 0"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 2),
        "[4] net.example 0"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 3),
        "[5] org.example 0"));

  g_assert (!tp_strdiff (g_ptr_array_index (events, 4),
        "[1] com.example 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 5),
        "[3] com.example 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 6),
        "[5] org.example 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 7),
        "[4] net.example 58"));
  g_assert (!tp_strdiff (g_ptr_array_index (events, 8),
        "[4] net.example 0"));

  /* keep valgrind happy, at least in the successful case */
  for (i = 0; i < events->len; i++)
    {
      g_free (events->pdata[i]);
    }

  g_ptr_array_free (events, TRUE);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  return 0;
}
