#include "config.h"

#include <dbus/dbus-shared.h>
#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

#include "telepathy-glib/reentrants.h"

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

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add_func ("/dbus/validation", test_validation);

  return tp_tests_run_with_bus ();
}
