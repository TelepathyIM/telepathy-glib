#include <telepathy-glib/errors.h>

static void
test_tp_errors (void)
{
#include "tests/dbus/_gen/errors-check.h"
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/test-error-enum/TP_ERRORS", test_tp_errors);

  return g_test_run ();
}
