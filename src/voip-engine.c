#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tp-chan.h>


int
main (int argc, char **argv)
{
  DBusGConnection *connection;
  GError *error;
  DBusGProxy *channel;
  DBusGProxy *session;
  DBusGProxy *stream;
  DBusGProxyCall *call;
    
  g_type_init ();

  loop = g_main_loop_new (NULL, FALSE);

  error = NULL;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION,
                               &error);
  if (connection == NULL)
    lose_gerror ("Failed to open connection to bus", error);
}
  
