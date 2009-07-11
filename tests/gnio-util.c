/* tests of the GNIO utility functions */

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/util.h>

#define IPV4_ADDR "127.0.1.1"
#define IPV6_ADDR "::1"
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

int
main (int argc, char **argv)
{
  g_type_init ();

  test_variant_to_sockaddr_ipv4 ();
  test_variant_to_sockaddr_ipv6 ();
  test_sockaddr_to_variant_ipv4 ();
  test_sockaddr_to_variant_ipv6 ();

  return 0;
}
