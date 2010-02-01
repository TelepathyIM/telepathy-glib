#include <telepathy-logger/conf.h>

#define gconf_client_get_bool(obj,key,err) g_print ("%s", key)

int main (int argc, char **argv)
{
  TplChannelTest *chan;
  GError *error;

  g_type_init ();

  tp_connection_new (

  chan = tpl_channel_test_new (conn, path, props, acc, &err);

  /* TplConf is a singleton, be sure both point to the same memory */
  conf2 = tpl_conf_dup ();
  g_assert (conf == conf2);

  /* unref the second singleton pointer and check that the it is still
   * valid: checking correct object ref-counting after each _dup() call */
  g_object_unref (conf2);
  g_assert (TPL_IS_CONF (conf));

  g_object_unref (conf);

  return 0;
}

