#include "config.h"

#include <dbus/dbus-shared.h>
#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

#include "tests/lib/util.h"

static void
test_validation (void)
{
  g_assert (tp_dbus_check_valid_object_path ("/", NULL));
  g_assert (tp_dbus_check_valid_object_path ("/a", NULL));
  g_assert (tp_dbus_check_valid_object_path ("/foo", NULL));
  g_assert (!tp_dbus_check_valid_object_path ("//", NULL));
  g_assert (!tp_dbus_check_valid_object_path ("/a//b", NULL));
  g_assert (tp_dbus_check_valid_object_path ("/a/b", NULL));
  g_assert (!tp_dbus_check_valid_object_path ("/a/b/", NULL));
  g_assert (!tp_dbus_check_valid_object_path ("a/b", NULL));
  g_assert (!tp_dbus_check_valid_object_path ("/*a", NULL));

#define TEST_LONG_BIT "excessively.long.name.longer.than._255.characters"
#define TEST_LONG (TEST_LONG_BIT TEST_LONG_BIT TEST_LONG_BIT TEST_LONG_BIT \
    TEST_LONG_BIT TEST_LONG_BIT TEST_LONG_BIT TEST_LONG_BIT)

  g_assert (!tp_dbus_check_valid_member_name ("", NULL));
  g_assert (!tp_dbus_check_valid_member_name ("123abc", NULL));
  g_assert (!tp_dbus_check_valid_member_name ("a.b", NULL));
  g_assert (!tp_dbus_check_valid_member_name ("a*b", NULL));
  g_assert (tp_dbus_check_valid_member_name ("example", NULL));
  g_assert (tp_dbus_check_valid_member_name ("_1", NULL));

  g_assert (!tp_dbus_check_valid_interface_name ("", NULL));
  g_assert (!tp_dbus_check_valid_interface_name (TEST_LONG, NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("hasnodot", NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("123abc.example", NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("com.1", NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("com.e*ample", NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("com..example", NULL));
  g_assert (!tp_dbus_check_valid_interface_name (".com.example", NULL));
  g_assert (!tp_dbus_check_valid_interface_name ("com.example.", NULL));
  g_assert (tp_dbus_check_valid_interface_name ("com.example", NULL));
  g_assert (tp_dbus_check_valid_interface_name ("com._1", NULL));

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

  g_assert (tp_dbus_check_valid_bus_name ("com._1",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name (TEST_LONG,
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("hasnodot",
        TP_DBUS_NAME_TYPE_ANY, NULL));
  g_assert (!tp_dbus_check_valid_bus_name ("123abc.example",
        TP_DBUS_NAME_TYPE_ANY, NULL));
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
}

static void
test_properties (void)
{
  TpDBusDaemon *bus = tp_dbus_daemon_dup (NULL);
  gchar *bus_name;
  gchar *object_path;
  DBusGConnection *dbus_conn;

  g_object_get (bus,
      "dbus-connection", &dbus_conn,
      "bus-name", &bus_name,
      "object-path", &object_path,
      NULL);

  if (object_path[0] != '/')
    g_error ("supposed object-path \"%s\" doesn't start with a /",
        object_path);

  g_assert_cmpstr (bus_name, ==, "org.freedesktop.DBus");
  g_assert (dbus_conn != NULL);
  g_assert (dbus_conn == tp_get_bus ());

  g_free (bus_name);
  g_free (object_path);
  dbus_g_connection_unref (dbus_conn);
  g_object_unref (bus);
}

static GPtrArray *events;
static GMainLoop *mainloop;
static gchar *two = "2", *five = "5";
static gboolean had_owners = FALSE;

static void
noc (TpDBusDaemon *obj,
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

              g_assert (tp_cli_dbus_daemon_run_request_name (obj, -1,
                    "com.example", 0, &ret, &error, NULL));
              g_assert (ret == 1 && error == NULL);
              g_assert (tp_cli_dbus_daemon_run_request_name (obj, -1,
                    "org.example", 0, &ret, &error, NULL));
              g_assert (ret == 1 && error == NULL);
              g_assert (tp_cli_dbus_daemon_run_request_name (obj, -1,
                    "net.example", 0, &ret, &error, NULL));
              g_assert (ret == 1 && error == NULL);
            }
        }
      else
        {
          guint ret;
          GError *error = NULL;

          g_assert (tp_dbus_daemon_cancel_name_owner_watch (obj,
                "org.example", noc, five));
          g_assert (tp_cli_dbus_daemon_run_release_name (obj, -1,
                "org.example", &ret, &error, NULL));
          g_assert (ret == 1 && error == NULL);
          g_assert (tp_cli_dbus_daemon_run_release_name (obj, -1,
                "net.example", &ret, &error, NULL));
          g_assert (ret == 1 && error == NULL);
        }
    }
}

static void
test_watch_name_owner (void)
{
  TpDBusDaemon *bus = tp_dbus_daemon_dup (NULL);
  guint i;

  events = g_ptr_array_new ();

  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "1", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, two, NULL);
  tp_dbus_daemon_watch_name_owner (bus, "com.example", noc, "3", NULL);
  tp_dbus_daemon_cancel_name_owner_watch (bus, "com.example", noc, two);
  tp_dbus_daemon_watch_name_owner (bus, "net.example", noc, "4", NULL);
  tp_dbus_daemon_watch_name_owner (bus, "org.example", noc, five, NULL);

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  g_assert_cmpuint (events->len, ==, 9);

  /* 58 == ':' - i.e. the beginning of a unique name */
  g_assert_cmpstr (g_ptr_array_index (events, 0), ==, "[1] com.example 0");
  g_assert_cmpstr (g_ptr_array_index (events, 1), ==, "[3] com.example 0");
  g_assert_cmpstr (g_ptr_array_index (events, 2), ==, "[4] net.example 0");
  g_assert_cmpstr (g_ptr_array_index (events, 3), ==, "[5] org.example 0");

  g_assert_cmpstr (g_ptr_array_index (events, 4), ==, "[1] com.example 58");
  g_assert_cmpstr (g_ptr_array_index (events, 5), ==, "[3] com.example 58");
  g_assert_cmpstr (g_ptr_array_index (events, 6), ==, "[5] org.example 58");
  g_assert_cmpstr (g_ptr_array_index (events, 7), ==, "[4] net.example 58");
  g_assert_cmpstr (g_ptr_array_index (events, 8), ==, "[4] net.example 0");

  /* keep valgrind happy, at least in the successful case */
  for (i = 0; i < events->len; i++)
    {
      g_free (events->pdata[i]);
    }

  g_ptr_array_unref (events);
  g_main_loop_unref (mainloop);
  mainloop = NULL;
}

/* Here's a regression test for a bug where, if a name owner watch callback
 * removes itself, subsequent callbacks for the same change would not fire.
 * This was because the implementation was an array of callbacks, with an index
 * i, calling each in turn. So imagine we're dispatching three callbacks,
 * starting with 'foo':
 *
 *   | foo | bar | baz |
 *     i=0
 *
 * If 'foo' cancels its own watch, it would be removed from the array. Then the
 * iteration would continue by incrementing i:
 *
 *   | bar | baz |
 *           i=1
 *
 * and 'bar' has not been called! Gulp. This test checks this case by setting
 * up ten numbered callbacks, each one of which removes itself and itself ±1,
 * so that each of (0, 1), (2, 3), (4, 5), (6, 7), (8, 9) should fire exactly
 * once. It should work regardless of the internal order of callbacks.
 */
#define N_CALLBACK_PAIRS 5
gboolean callbacks_fired[N_CALLBACK_PAIRS] =
    { FALSE, FALSE, FALSE, FALSE, FALSE };
/* Overwritten with '.' when "freed" */
gchar user_data_flags[N_CALLBACK_PAIRS * 2 + 1] = "0123456789";

static void
free_fake_user_data (gpointer user_data)
{
  guint i = GPOINTER_TO_UINT (user_data);

  if (user_data_flags[i] == '.')
    g_error ("Double 'free' of user-data %u. Still to free: %s", i,
        user_data_flags);

  g_assert_cmpuint ((guint) user_data_flags[i], ==, i + '0');
  user_data_flags[i] = '.';
}

static void
bbf3_performed_cb (
    TpDBusDaemon *bus_daemon,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data)
{
  guint i = GPOINTER_TO_UINT (user_data);
  guint even = i - (i % 2);
  guint odd = even + 1;
  guint j;

  g_message ("%u fired; cancelling %u and %u", i, even, odd);
  tp_dbus_daemon_cancel_name_owner_watch (bus_daemon, name, bbf3_performed_cb,
      GUINT_TO_POINTER (even));
  tp_dbus_daemon_cancel_name_owner_watch (bus_daemon, name, bbf3_performed_cb,
      GUINT_TO_POINTER (odd));

  g_assert (!callbacks_fired[even / 2]);
  callbacks_fired[even / 2] = TRUE;

  for (j = 0; j < N_CALLBACK_PAIRS; j++)
    if (!callbacks_fired[j])
      {
        g_message ("still waiting for %u or %u, at least", j * 2, j * 2 + 1);
        return;
      }

  g_main_loop_quit (mainloop);
}

static void
cancel_watch_during_dispatch (void)
{
  TpDBusDaemon *bus = tp_dbus_daemon_dup (NULL);
  guint i;

  tp_dbus_daemon_request_name (bus, "ca.bbf3", FALSE, NULL);

  for (i = 0; i < N_CALLBACK_PAIRS * 2; i++)
    {
      tp_dbus_daemon_watch_name_owner (bus, "ca.bbf3", bbf3_performed_cb,
          GUINT_TO_POINTER (i), free_fake_user_data);
      g_assert_cmpuint ((guint) user_data_flags[i], ==, i + '0');
    }

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);
  g_object_unref (bus);

  /* everything should have been "freed" */
  g_assert_cmpstr (user_data_flags, ==, "..........");
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add_func ("/dbus/validation", test_validation);
  g_test_add_func ("/dbus-daemon/properties", test_properties);
  g_test_add_func ("/dbus-daemon/watch-name-owner", test_watch_name_owner);
  g_test_add_func ("/dbus-daemon/cancel-watch-during-dispatch",
      cancel_watch_during_dispatch);

  return g_test_run ();
}
