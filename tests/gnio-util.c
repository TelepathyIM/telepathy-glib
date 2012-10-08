/* tests of the GNIO utility functions */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gio/gio.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/gtypes.h>

#define IPV4_ADDR "127.0.1.1"
#define IPV6_ADDR "::1"
#define UNIX_ADDR "/tmp/socket/test/123456"
#define ABST_ADDR "\000" "123456" "NOT_COPIED"
#define ABST_ADDR_LEN 7
#define PORT 41414

static void
test_variant_to_sockaddr_ipv4 (void)
{
  GSocketAddress *sockaddr;
  GInetSocketAddress *inetaddr;
  GInetAddress *hostaddr;
  char *host;
  guint16 port;
  GError *error = NULL;

  sockaddr = tp_g_socket_address_from_g_variant (TP_SOCKET_ADDRESS_TYPE_IPV4,
      g_variant_new_parsed ("(%s, %u)", IPV4_ADDR, (guint32) PORT), &error);

  /* check the socket address */
  g_assert_no_error (error);
  g_assert (sockaddr != NULL);
  g_assert (G_IS_INET_SOCKET_ADDRESS (sockaddr));

  inetaddr = G_INET_SOCKET_ADDRESS (sockaddr);
  hostaddr = g_inet_socket_address_get_address (inetaddr);

  host = g_inet_address_to_string (hostaddr);
  port = g_inet_socket_address_get_port (inetaddr);

  g_assert_cmpstr (host, ==, IPV4_ADDR);
  g_assert_cmpuint (port, ==, PORT);

  g_free (host);
  g_object_unref (sockaddr);
}

static void
test_variant_to_sockaddr_ipv6 (void)
{
  GSocketAddress *sockaddr;
  GInetSocketAddress *inetaddr;
  GInetAddress *hostaddr;
  char *host;
  guint16 port;
  GError *error = NULL;

  sockaddr = tp_g_socket_address_from_g_variant (TP_SOCKET_ADDRESS_TYPE_IPV6,
      g_variant_new_parsed ("(%s, %u)", IPV6_ADDR, (guint32) PORT), &error);

  /* check the socket address */
  g_assert_no_error (error);
  g_assert (sockaddr != NULL);
  g_assert (G_IS_INET_SOCKET_ADDRESS (sockaddr));

  inetaddr = G_INET_SOCKET_ADDRESS (sockaddr);
  hostaddr = g_inet_socket_address_get_address (inetaddr);

  host = g_inet_address_to_string (hostaddr);
  port = g_inet_socket_address_get_port (inetaddr);

  g_assert_cmpstr (host, ==, IPV6_ADDR);
  g_assert_cmpuint (port, ==, PORT);

  g_free (host);
  g_object_unref (sockaddr);
}

static void
test_sockaddr_to_variant_ipv4 (void)
{
  GInetAddress *hostaddr = g_inet_address_new_from_string (IPV4_ADDR);
  GSocketAddress *sockaddr = g_inet_socket_address_new (hostaddr, PORT);
  GValue *variant, *value;
  GVariant *gvariant, *other;
  GValueArray *array;
  TpSocketAddressType type;
  GError *error = NULL;

  g_object_unref (hostaddr);

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type, &error);
  g_assert_no_error (error);
  gvariant = tp_address_g_variant_from_g_socket_address (sockaddr, &type,
      &error);
  g_object_unref (sockaddr);

  g_assert_no_error (error);
  g_assert_cmpuint (type, ==, TP_SOCKET_ADDRESS_TYPE_IPV4);
  g_assert (G_VALUE_HOLDS (variant, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV4));

  array = g_value_get_boxed (variant);
  value = g_value_array_get_nth (array, 0);

  g_assert (G_VALUE_HOLDS_STRING (value));
  g_assert_cmpstr (g_value_get_string (value), ==, IPV4_ADDR);

  value = g_value_array_get_nth (array, 1);

  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert_cmpuint (g_value_get_uint (value), ==, PORT);

  tp_g_value_slice_free (variant);

  g_assert (g_variant_is_floating (gvariant));
  other = g_variant_new_parsed ("(%s, %u)", IPV4_ADDR, (guint32) PORT);
  g_assert (g_variant_equal (gvariant, other));
  g_variant_unref (gvariant);
  g_variant_unref (other);
}

static void
test_sockaddr_to_variant_ipv6 (void)
{
  GInetAddress *hostaddr = g_inet_address_new_from_string (IPV6_ADDR);
  GSocketAddress *sockaddr = g_inet_socket_address_new (hostaddr, PORT);
  GValue *variant, *value;
  GValueArray *array;
  GVariant *gvariant, *other;
  TpSocketAddressType type;
  GError *error = NULL;

  g_object_unref (hostaddr);

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type, &error);
  g_assert_no_error (error);
  gvariant = tp_address_g_variant_from_g_socket_address (sockaddr, &type,
      &error);
  g_object_unref (sockaddr);

  g_assert_no_error (error);
  g_assert_cmpuint (type, ==, TP_SOCKET_ADDRESS_TYPE_IPV6);
  g_assert (G_VALUE_HOLDS (variant, TP_STRUCT_TYPE_SOCKET_ADDRESS_IPV6));

  array = g_value_get_boxed (variant);
  value = g_value_array_get_nth (array, 0);

  g_assert (G_VALUE_HOLDS_STRING (value));
  g_assert_cmpstr (g_value_get_string (value), ==, IPV6_ADDR);

  value = g_value_array_get_nth (array, 1);

  g_assert (G_VALUE_HOLDS_UINT (value));
  g_assert_cmpuint (g_value_get_uint (value), ==, PORT);

  tp_g_value_slice_free (variant);

  g_assert (g_variant_is_floating (gvariant));
  other = g_variant_new_parsed ("(%s, %u)", IPV6_ADDR, (guint32) PORT);
  g_assert (g_variant_equal (gvariant, other));
  g_variant_unref (gvariant);
  g_variant_unref (other);
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
  GError *error = NULL;

  array = g_array_sized_new (TRUE, FALSE, sizeof (char), pathlen);
  g_array_append_vals (array, UNIX_ADDR, pathlen);

  g_value_init (&value, DBUS_TYPE_G_UCHAR_ARRAY);
  g_value_take_boxed (&value, array);

  sockaddr = tp_g_socket_address_from_variant (TP_SOCKET_ADDRESS_TYPE_UNIX,
      &value, &error);
  g_value_unset (&value);

  g_assert_no_error (error);
  g_assert (G_IS_UNIX_SOCKET_ADDRESS (sockaddr));

  unixaddr = G_UNIX_SOCKET_ADDRESS (sockaddr);

  g_assert (g_unix_socket_address_get_address_type (unixaddr) !=
            G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  g_assert_cmpuint (g_unix_socket_address_get_path_len (unixaddr), ==, pathlen);
  g_assert_cmpstr (g_unix_socket_address_get_path (unixaddr), ==, UNIX_ADDR);

  g_object_unref (sockaddr);
}

static void
test_variant_to_sockaddr_abstract_unix (void)
{
  GArray *array;
  GValue value = { 0, };
  GSocketAddress *sockaddr;
  GUnixSocketAddress *unixaddr;
  GError *error = NULL;

  array = g_array_sized_new (TRUE, FALSE, sizeof (char), ABST_ADDR_LEN);
  g_array_append_vals (array, ABST_ADDR, ABST_ADDR_LEN);

  g_value_init (&value, DBUS_TYPE_G_UCHAR_ARRAY);
  g_value_take_boxed (&value, array);

  sockaddr = tp_g_socket_address_from_variant (
      TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX,
      &value, &error);
  g_value_unset (&value);

  g_assert_no_error (error);
  g_assert (G_IS_UNIX_SOCKET_ADDRESS (sockaddr));

  unixaddr = G_UNIX_SOCKET_ADDRESS (sockaddr);

  g_assert (g_unix_socket_address_get_address_type (unixaddr) ==
            G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  g_assert_cmpuint (g_unix_socket_address_get_path_len (unixaddr), ==,
      ABST_ADDR_LEN);
  g_assert (memcmp (g_unix_socket_address_get_path (unixaddr), ABST_ADDR,
        ABST_ADDR_LEN) == 0);

  g_object_unref (sockaddr);
}

static void
test_sockaddr_to_variant_unix (void)
{
  GSocketAddress *sockaddr = g_unix_socket_address_new (UNIX_ADDR);
  GValue *variant;
  GArray *array;
  TpSocketAddressType type;
  GError *error = NULL;

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type, &error);
  g_object_unref (sockaddr);

  g_assert_no_error (error);
  g_assert_cmpuint (type, ==, TP_SOCKET_ADDRESS_TYPE_UNIX);
  g_assert (G_VALUE_HOLDS (variant, DBUS_TYPE_G_UCHAR_ARRAY));

  array = g_value_get_boxed (variant);

  g_assert_cmpuint (array->len, ==, strlen (UNIX_ADDR));
  g_assert_cmpstr (array->data, ==, UNIX_ADDR);

  tp_g_value_slice_free (variant);
}

static void
test_sockaddr_to_variant_abstract_unix (void)
{
  GSocketAddress *sockaddr = g_unix_socket_address_new_with_type (
      ABST_ADDR, ABST_ADDR_LEN, G_UNIX_SOCKET_ADDRESS_ABSTRACT);

  GValue *variant;
  GArray *array;
  TpSocketAddressType type;
  GError *error = NULL;

  variant = tp_address_variant_from_g_socket_address (sockaddr, &type, &error);
  g_object_unref (sockaddr);

  g_assert_no_error (error);
  g_assert_cmpuint (type, ==, TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX);
  g_assert (G_VALUE_HOLDS (variant, DBUS_TYPE_G_UCHAR_ARRAY));

  array = g_value_get_boxed (variant);

  g_assert_cmpuint (array->len, ==, ABST_ADDR_LEN);
  g_assert (memcmp (array->data, ABST_ADDR, ABST_ADDR_LEN) == 0);

  tp_g_value_slice_free (variant);
}
#endif /* HAVE_GIO_UNIX */

int
main (int argc, char **argv)
{
  g_type_init ();
  dbus_g_type_specialized_init ();

  test_variant_to_sockaddr_ipv4 ();
  test_variant_to_sockaddr_ipv6 ();
  test_sockaddr_to_variant_ipv4 ();
  test_sockaddr_to_variant_ipv6 ();
#ifdef HAVE_GIO_UNIX
  test_variant_to_sockaddr_unix ();
  test_variant_to_sockaddr_abstract_unix ();
  test_sockaddr_to_variant_unix ();
  test_sockaddr_to_variant_abstract_unix ();
#endif /* HAVE_GIO_UNIX */

  return 0;
}
