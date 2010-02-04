#include "tpl-channel-test.h"

#include <telepathy-logger/conf.h>

#define gconf_client_get_bool(obj,key,err) g_print ("%s", key)

#define CONNECTION_PATH "org.freedesktop.Telepathy.Connection.gabble.jabber.cosimo_2ealfarano_40collabora_2eco_2euk_2fKazoo"
#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/FOO/BAR/BAZ"
#define CHANNEL_PATH "/BAZ"

int main (int argc, char **argv)
{
/*
  TpDBusDaemon *tp_bus_daemon;
  TpAccount *acc;
  TpConnection *conn;
  TplChannelTest *chan;
  GError *error = NULL;

  g_type_init ();

  g_debug ("FOO");
  tp_bus_daemon = tp_dbus_daemon_dup (&error);
  if (tp_bus_daemon == NULL)
    {
      g_critical ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      return 1;
    }

  g_debug ("FOO");
  acc = tp_account_new (tp_bus_daemon, ACCOUNT_PATH, &error);
  if (acc == NULL)
    {
      g_critical ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      return 1;
    }

  g_debug ("FOO");
  conn = tp_connection_new (tp_bus_daemon, CONNECTION_PATH, NULL, &error);
  if (conn == NULL)
    {
      g_critical ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      return 1;
    }

  g_debug ("FOO");
  chan = tpl_channel_test_new (conn, CHANNEL_PATH, NULL, acc, &error);
  if (chan == NULL)
    {
      g_critical ("%s", error->message);
      g_clear_error (&error);
      g_error_free (error);
      return 1;
    }
*/

  g_debug ("FOO");
  return 0;
}
