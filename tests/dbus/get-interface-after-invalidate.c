#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

static void
test_get_interface_after_invalidate (void)
{
  TpDBusDaemon *bus_daemon;
  DBusGProxy *props;
  GError invalidation_reason = { TP_ERRORS, TP_ERROR_NOT_YOURS, "bees!" };
  GError *error = NULL;

  bus_daemon = tp_dbus_daemon_dup (NULL);
  g_assert (bus_daemon != NULL);
  tp_proxy_invalidate ((TpProxy *) bus_daemon, &invalidation_reason);

  props = tp_proxy_borrow_interface_by_id ((TpProxy *) bus_daemon,
      TP_IFACE_QUARK_DBUS_DAEMON, &error);

  /* Borrowing the interface should fail because the proxy is invalidated. */
  g_assert (props == NULL);
  g_assert (error != NULL);
  g_assert_cmpuint (error->domain, ==, invalidation_reason.domain);
  g_assert_cmpint (error->code, ==, invalidation_reason.code);
  g_assert_cmpstr (error->message, ==, invalidation_reason.message);

  g_error_free (error);
  g_object_unref (bus_daemon);
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/test-get-interface-after-invalidate",
      test_get_interface_after_invalidate);

  return g_test_run ();
}
