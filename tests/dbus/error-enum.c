#include "config.h"

#include <telepathy-glib/errors.h>

#include "tests/lib/util.h"

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

  g_test_add_func ("/test-error-enum/TP_ERROR", test_tp_errors);

  return tp_tests_run_with_bus ();
}
