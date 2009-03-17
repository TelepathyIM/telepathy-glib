#include <glib.h>
#include <telepathy-glib/connection.h>

int main (int argc, char **argv)
{
  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_AWAY, TP_CONNECTION_PRESENCE_TYPE_UNSET) == 1);

  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_BUSY, TP_CONNECTION_PRESENCE_TYPE_AVAILABLE) == -1);

  g_assert (tp_connection_presence_type_cmp_availability (
    TP_CONNECTION_PRESENCE_TYPE_UNKNOWN, 100) == 0);

  return 0;
}
