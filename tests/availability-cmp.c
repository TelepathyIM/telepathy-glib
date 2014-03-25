#include "config.h"

#include <glib.h>
#include <telepathy-glib/connection.h>

typedef struct {
    int dummy;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer data)
{
}

static void
test (Fixture *f,
    gconstpointer data)
{
  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_AWAY, TP_CONNECTION_PRESENCE_TYPE_UNSET) == 1);

  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_BUSY, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE) == -1);

  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_UNKNOWN, 100) == 0);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/availability-cmp", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
