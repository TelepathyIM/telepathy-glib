/* tests of the GNIO utility functions */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gio/gio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/util.h>

#define IPV4_ADDR "127.0.1.1"
#define IPV6_ADDR "::1"
#define UNIX_ADDR "/tmp/socket/test/123456"
#define ABST_ADDR "\000123456"
#define PORT 41414

static void
test_variant_to_sockaddr_ipv4 (void)
{
  GValueArray *array = g_value_array_new (2);
  GValue value = { 0, };
  GSocketAddress *sockaddr;
  GInetSocketAddress *inetaddr;
  GInetAddress *hostaddr;
  char *host;
  guint16 port;

  /* set up an address variant */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, IPV4_ADDR);
  g_value_array_append (array, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, PORT);
  g_value_array_append (array, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_VALUE_ARRAY);
  g_value_take_boxed (&value, array);

  /* convert to a GSocketAddress */
  sockaddr = tp_g_socket_address_from_variant (TP_SOCKET_ADDRESS_TYPE_IPV4,
                                               &value);
  g_value_unset (&value);

  /* check the socket address */
  g_assert (sockaddr != NULL);
  g_assert (G_IS_INET_SOCKET_ADDRESS (sockaddr));

  inetaddr = G_INET_SOCKET_ADDRESS (sockaddr);
  hostaddr = g_inet_socket_address_get_address (inetaddr);

  host = g_inet_address_to_string (hostaddr);
  port = g_inet_socket_address_get_port (inetaddr);

  g_assert (strcmp (host, IPV4_ADDR) == 0);
  g_assert (port == PORT);

  g_free (host);
  g_object_unref (sockaddr);
}

static void
test_variant_to_sockaddr_ipv6 (void)
{
  GValueArray *array = g_value_array_new (2);
  GValue value = { 0, };
  GSocketAddress *sockaddr;
  GInetSocketAddress *inetaddr;
  GInetAddress *hostaddr;
  char *host;
  guint16 port;

  /* set up an address variant */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, IPV6_ADDR);
  g_value_array_append (array, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, PORT);
  g_value_array_append (array, &value);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_VALUE_ARRAY);
  g_value_take_boxed (&value, array);

  /* convert to a GSocketAddress */
  sockaddr = tp_g_socket_address_from_variant (TP_SOCKET_ADDRESS_TYPE_IPV6,
                                               &value);
  g_value_unset (&value);

  /* check the socket address */
  g_assert (sockaddr != NULL);
  g_assert (G_IS_INET_SOCKET_ADDRESS (sockaddr));

  inetaddr = G_INET_SOCKET_ADDRESS (sockaddr);
  hostaddr = g_inet_socket_address_get_address (inetaddr);

  host = g_inet_address_to_string (hostaddr);
  port = g_inet_socket_address_get_port (inetaddr);

  g_assert (strcmp (host, IPV6_ADDR) == 0);
  g_assert (port == PORT);

  g_free (host);
  g_object_unref (sockaddr);
}

static void
test_sockaddr_to_variant_ipv4 (void)
{
  GInetAddress *hostaddr = g_inet_address_new_from_string (IPV4_ADDR);
  GSocketAddress *sockaddr = g_inet_socket_address_new (hostaddr, PORT);
  GValue *variant, *value;
  GValueArray *array;
  TpSocketAddressType type;

  g_object_unref (hostaddr);

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type);
  g_object_unref (sockaddr);

  g_assert (type == TP_SOCKET_ADDRESS_TYPE_IPV4);
  g_assert (G_VALUE_HOLDS (variant, G_TYPE_VALUE_ARRAY));

  array = g_value_get_boxed (variant);
  value = g_value_array_get_nth (array, 0);

  g_assert (G_VALUE_HOLDS_STRING (value));
  g_assert (strcmp (g_value_get_string (value), IPV4_ADDR) == 0);

  value = g_value_array_get_nth (array, 1);

  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert (g_value_get_uint (value) == PORT);

  tp_g_value_slice_free (variant);
}

static void
test_sockaddr_to_variant_ipv6 (void)
{
  GInetAddress *hostaddr = g_inet_address_new_from_string (IPV6_ADDR);
  GSocketAddress *sockaddr = g_inet_socket_address_new (hostaddr, PORT);
  GValue *variant, *value;
  GValueArray *array;
  TpSocketAddressType type;

  g_object_unref (hostaddr);

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type);
  g_object_unref (sockaddr);

  g_assert (type == TP_SOCKET_ADDRESS_TYPE_IPV6);
  g_assert (G_VALUE_HOLDS (variant, G_TYPE_VALUE_ARRAY));

  array = g_value_get_boxed (variant);
  value = g_value_array_get_nth (array, 0);

  g_assert (G_VALUE_HOLDS_STRING (value));
  g_assert (strcmp (g_value_get_string (value), IPV6_ADDR) == 0);

  value = g_value_array_get_nth (array, 1);

  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert (g_value_get_uint (value) == PORT);

  tp_g_value_slice_free (variant);
}

#ifdef HAVE_GIO_UNIX
static void
test_variant_to_sockaddr_unix (void)
{
  GArray *array;
  GValue value = { 0, };
  GSocketAddress *sockaddr;
  GUnixSocketAddress *unixaddr;
  guint pathlen = strlen (UNIX_ADDR);

  array = g_array_sized_new (TRUE, FALSE, sizeof (char), pathlen);
  g_array_append_vals (array, UNIX_ADDR, pathlen);

  g_value_init (&value, DBUS_TYPE_G_UCHAR_ARRAY);
  g_value_take_boxed (&value, array);

  sockaddr = tp_g_socket_address_from_variant (TP_SOCKET_ADDRESS_TYPE_UNIX,
      &value);
  g_value_unset (&value);

  g_assert (G_IS_UNIX_SOCKET_ADDRESS (sockaddr));

  unixaddr = G_UNIX_SOCKET_ADDRESS (sockaddr);

  g_assert (g_unix_socket_address_get_is_abstract (unixaddr) == FALSE);
  g_assert (g_unix_socket_address_get_path_len (unixaddr) == pathlen);
  g_assert (strcmp (g_unix_socket_address_get_path (unixaddr), UNIX_ADDR) == 0);

  g_object_unref (sockaddr);
}

static void
test_sockaddr_to_variant_unix (void)
{
  GSocketAddress *sockaddr = g_unix_socket_address_new (UNIX_ADDR);
  GValue *variant;
  GArray *array;
  TpSocketAddressType type;

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type);
  g_object_unref (sockaddr);

  g_assert (type == TP_SOCKET_ADDRESS_TYPE_UNIX);
  g_assert (G_VALUE_HOLDS (variant, DBUS_TYPE_G_UCHAR_ARRAY));

  array = g_value_get_boxed (variant);

  g_assert (array->len == strlen (UNIX_ADDR));
  g_assert (strcmp (array->data, UNIX_ADDR) == 0);

  tp_g_value_slice_free (variant);
}

#endif /* HAVE_GIO_UNIX */

int
main (int argc, char **argv)
{
  DBusGConnection *connection;
  GError *error = NULL;

  g_type_init ();

  /* we seem to need to make a connection in order to initialise the special
   * dbus-glib types, I'm sure there must be a better way to do this */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  test_variant_to_sockaddr_ipv4 ();
  test_variant_to_sockaddr_ipv6 ();
  test_sockaddr_to_variant_ipv4 ();
  test_sockaddr_to_variant_ipv6 ();
#ifdef HAVE_GIO_UNIX
  test_variant_to_sockaddr_unix ();
  test_sockaddr_to_variant_unix ();
#endif /* HAVE_GIO_UNIX */

  return 0;
}
