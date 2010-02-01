#include <telepathy-logger/observer.h>

int
main (int argc, char **argv)
{
  TplObserver *obs, *obs2;

  g_type_init ();

  obs = tpl_observer_new ();

  /* TplObserver is a singleton, be sure both point to the same memory */
  obs2 = tpl_observer_new ();
  g_assert (obs == obs2);

  /* unref the second singleton pointer and check that the it is still
   * valid: checking correct object ref-counting after each _dup() call */
  g_object_unref (obs2);
  g_assert (TPL_IS_OBSERVER (obs));

  /* it points to the same mem area, it should be still valid */
  g_assert (TPL_IS_OBSERVER (obs2));

  /* proper disposal for the singleton when no references are present */
  g_object_unref (obs);
  g_assert (!TPL_IS_OBSERVER (obs));


  return 0;
}

