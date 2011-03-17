#include <telepathy-logger/conf-internal.h>

int
main (int argc, char **argv)
{
  TplConf *conf, *conf2;

  g_type_init ();

  conf = _tpl_conf_dup ();

  /* TplConf is a singleton, be sure both point to the same memory */
  conf2 = _tpl_conf_dup ();
  g_assert (conf == conf2);

  /* unref the second singleton pointer and check that the it is still
   * valid: checking correct object ref-counting after each _dup () call */
  g_object_unref (conf2);
  g_assert (TPL_IS_CONF (conf));

  /* it points to the same mem area, it should be still valid */
  g_assert (TPL_IS_CONF (conf2));

  /* proper disposal for the singleton when no references are present */
  g_object_unref (conf);

  return 0;
}

