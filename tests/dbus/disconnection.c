#include "config.h"

#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/proxy-subclass.h>    /* for _invalidated etc. */
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-channel-dispatcher.h"
#include "tests/lib/stub-object.h"
#include "tests/lib/util.h"

/* just for convenience, since it's used a lot */
#define PTR(ui) GUINT_TO_POINTER(ui)

/* state tracking */
static GMainLoop *mainloop;
static TpIntset *caught_signal;
static TpIntset *freed_user_data;

enum {
    TEST_A,
    TEST_B,
    TEST_C,
    TEST_D,
    TEST_E,
    TEST_F,
    TEST_G,
    TEST_H,
    TEST_Z = 25,
    N_PROXIES
};

typedef struct {
    TpDBusDaemon *dbus_daemon;
    TpProxy *proxies[N_PROXIES];
    GObject *cd_service;
} Fixture;

static Fixture *f;

static void
h_stub_destroyed (gpointer data,
                  GObject *stub)
{
  TpProxySignalConnection **p = data;

  tp_proxy_signal_connection_disconnect (*p);
}

static void
destroy_user_data (gpointer user_data)
{
  guint which = GPOINTER_TO_UINT (user_data);
  g_message ("User data %c destroyed", 'A' + which);
  MYASSERT (!tp_intset_is_member (freed_user_data, which), "");
  tp_intset_add (freed_user_data, which);
}

static void
unwanted_signal_cb (TpProxy *proxy,
    const gchar *iface,
    GHashTable *changed,
    const gchar **invalidated,
    gpointer user_data,
    GObject *weak_object)
{
  g_error ("unwanted_signal_cb called - a signal connection which should have "
      "failed has succeeded. Args: proxy=%p user_data=%p", proxy, user_data);
}

static void
signal_cb (TpProxy *proxy,
    const gchar *iface,
    GHashTable *changed,
    const gchar **invalidated,
    gpointer user_data,
    GObject *weak_object)
{
  guint which = GPOINTER_TO_UINT (user_data);
  TpProxy *want_proxy = NULL;
  GObject *want_object = NULL;

  g_message ("Caught signal with proxy #%d '%c' according to "
      "user_data", which, 'a' + which);
  g_message ("Proxy is %p, weak object is %p", proxy,
      weak_object);
  tp_intset_add (caught_signal, which);

  want_proxy = f->proxies[which];

  switch (which)
    {
    case TEST_A:
      want_object = (GObject *) f->proxies[TEST_Z];
      break;
    case TEST_Z:
      want_object = (GObject *) f->proxies[TEST_A];
      break;
    default:
      g_error ("%c (%p) got the signal, which shouldn't have happened",
          'a' + which, proxy);
    }

  g_message ("Expecting proxy %p, weak object %p", want_proxy, want_object);

  MYASSERT (proxy == want_proxy, ": %p != %p", proxy, want_proxy);
  MYASSERT (weak_object == want_object, ": %p != %p", weak_object,
      want_object);

  if (tp_intset_is_member (caught_signal, TEST_A) &&
      tp_intset_is_member (caught_signal, TEST_Z))
    {
      /* we've had all the signals we're going to */
      g_main_loop_quit (mainloop);
    }
}

static void
set_freed (gpointer user_data)
{
  gboolean *boolptr = user_data;

  MYASSERT (*boolptr == FALSE, "");
  *boolptr = TRUE;
}

static void
setup (void)
{
  f->dbus_daemon = tp_tests_dbus_daemon_dup_or_die ();

  /* Any random object with an interface: what matters is that it can
   * accept a method call and emit a signal. We use the Properties
   * interface here. */
  f->cd_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCHER,
      NULL);
  tp_dbus_daemon_register_object (f->dbus_daemon, "/", f->cd_service);
}

static void
teardown (void)
{
  tp_tests_assert_last_unref (&f->cd_service);
  tp_tests_assert_last_unref (&f->dbus_daemon);
}

static TpProxy *
new_proxy (void)
{
  return tp_tests_object_new_static_class (TP_TYPE_PROXY,
      "dbus-daemon", f->dbus_daemon,
      "bus-name", tp_dbus_daemon_get_unique_name (f->dbus_daemon),
      "object-path", "/",
      NULL);
}

int
main (int argc,
      char **argv)
{
  Fixture fixture = { NULL };
  GObject *stub;
  GError *error_out = NULL;
  GError err = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT, "Because I said so" };
  TpProxySignalConnection *sc;
  gpointer tmp_obj;
  gboolean freed = FALSE;
  GHashTable *empty_asv;
  int i;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  freed_user_data = tp_intset_sized_new (N_PROXIES);
  caught_signal = tp_intset_sized_new (N_PROXIES);

  /* it's on the stack, but it's valid until we leave main(), which will
   * do for now... one day this test should use GTest, but this might
   * not be that day */
  f = &fixture;

  setup ();

  mainloop = g_main_loop_new (NULL, FALSE);

  g_message ("Creating proxies");

  for (i = TEST_A; i <= TEST_H; i++)
    {
      f->proxies[i] = new_proxy ();
      g_message ("%c=%p", 'a' + i, f->proxies[i]);
    }

  f->proxies[TEST_Z] = new_proxy ();
  g_message ("z=%p", f->proxies[TEST_Z]);

  /* a survives */
  g_message ("Connecting signal to a");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_A],
      signal_cb, PTR (TEST_A),
      destroy_user_data, (GObject *) f->proxies[TEST_Z], &error_out);
  g_assert_no_error (error_out);

  /* b gets its signal connection cancelled because stub is
   * destroyed */
  stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  g_message ("Connecting signal to b");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_B],
      signal_cb, PTR (TEST_B),
      destroy_user_data, stub, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_B), "");
  tp_tests_assert_last_unref (&stub);

  /* c gets its signal connection cancelled because it's explicitly
   * invalidated */
  g_message ("Connecting signal to c");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_C],
      signal_cb, PTR (TEST_C),
      destroy_user_data, NULL, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  g_message ("Forcibly invalidating c");
  tp_proxy_invalidate ((TpProxy *) f->proxies[TEST_C], &err);
  /* assert that connecting to a signal on an invalid proxy fails */
  freed = FALSE;
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_C],
      unwanted_signal_cb, &freed,
      set_freed, NULL, &error_out);
  MYASSERT (freed, "");
  MYASSERT (error_out != NULL, "");
  g_message ("%d: %d: %s", error_out->domain, error_out->code,
      error_out->message);
  MYASSERT (error_out->code == err.code, "%d != %d", error_out->code,
      err.code);
  g_error_free (error_out);
  error_out = NULL;

  /* d gets its signal connection cancelled because it's
   * implicitly invalidated by being destroyed */
  g_message ("Connecting signal to d");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_D],
      signal_cb, PTR (TEST_D),
      destroy_user_data, NULL, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  g_message ("Destroying d");
  tp_tests_assert_last_unref (&f->proxies[TEST_D]);

  /* e gets its signal connection cancelled explicitly */
  g_message ("Connecting signal to e");
  sc = tp_cli_dbus_properties_connect_to_properties_changed (
      f->proxies[TEST_E], signal_cb, PTR (TEST_E),
      destroy_user_data, NULL, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_E), "");
  g_message ("Disconnecting signal from e");
  tp_proxy_signal_connection_disconnect (sc);

  /* f gets its signal connection cancelled because it's implicitly
   * invalidated by its DBusGProxy being destroyed.
   *
   * Note that this test case exploits implementation details of dbus-glib.
   * If it stops working after a dbus-glib upgrade, that's probably why. */
  g_message ("Connecting signal to f");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_F],
      signal_cb, PTR (TEST_F),
      destroy_user_data, NULL, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");
  g_message ("Forcibly disposing f's DBusGProxy to simulate name owner loss");
  tmp_obj = tp_proxy_get_interface_by_id (f->proxies[TEST_F],
      TP_IFACE_QUARK_DBUS_PROPERTIES, NULL);
  MYASSERT (tmp_obj != NULL, "");
  g_object_run_dispose (tmp_obj);
  /* assert that connecting to a signal on an invalid proxy fails */
  freed = FALSE;
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_F],
      unwanted_signal_cb, &freed, set_freed, NULL, &error_out);
  MYASSERT (freed, "");
  MYASSERT (error_out != NULL, "");
  MYASSERT (error_out->code == DBUS_GERROR_NAME_HAS_NO_OWNER, "");
  g_error_free (error_out);
  error_out = NULL;

  /* g gets its signal connection cancelled because it's
   * implicitly invalidated by being destroyed; unlike d, the signal
   * connection weakly references the proxy. This is never necessary, but is
   * an interesting corner case that should be tested. */
  g_message ("Connecting signal to g");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_G],
      signal_cb, PTR (TEST_G),
      destroy_user_data, (GObject *) f->proxies[TEST_G], &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  g_message ("Destroying g");
  tp_tests_assert_last_unref (&f->proxies[TEST_G]);

  /* h gets its signal connection cancelled because its weak object is
   * destroyed, meaning there are simultaneously two reasons for it to become
   * cancelled (fd.o#14750) */
  stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  g_object_weak_ref (stub, h_stub_destroyed, &sc);
  g_message ("Connecting signal to h");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_H],
      signal_cb, PTR (TEST_H),
      destroy_user_data, stub, &error_out);
  g_assert_no_error (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_H), "");
  tp_tests_assert_last_unref (&stub);

  /* z survives; we assume that the signals are delivered in either
   * forward or reverse order, so if both a and z have had their signal, we
   * can stop the main loop */
  g_message ("Connecting signal to z");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_Z],
      signal_cb, PTR (TEST_Z),
      destroy_user_data, (GObject *) f->proxies[TEST_A], &error_out);
  g_assert_no_error (error_out);

  g_message ("Emitting signal");
  empty_asv = tp_asv_new (NULL, NULL);
  tp_svc_dbus_properties_emit_properties_changed (f->cd_service,
      TP_IFACE_CHANNEL_DISPATCHER, empty_asv, NULL);
  g_hash_table_unref (empty_asv);

  g_message ("Running main loop");
  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);

  /* Now that the main loop has run, cancelled signal connections have been
   * freed */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_G), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_H), "");

  /* both A and Z are still listening for signals, so their user data is
   * still held */
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_Z), "");

  g_message ("Dereferencing remaining proxies");
  tp_tests_assert_last_unref (&f->proxies[TEST_A]);
  tp_tests_assert_last_unref (&f->proxies[TEST_B]);
  tp_tests_assert_last_unref (&f->proxies[TEST_C]);
  g_assert (f->proxies[TEST_D] == NULL);
  tp_tests_assert_last_unref (&f->proxies[TEST_E]);
  tp_tests_assert_last_unref (&f->proxies[TEST_F]);
  g_assert (f->proxies[TEST_G] == NULL);
  tp_tests_assert_last_unref (&f->proxies[TEST_H]);
  tp_tests_assert_last_unref (&f->proxies[TEST_Z]);

  /* we should already have checked each of these at least once, but just to
   * make sure we have a systematic test that all user data is freed... */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_G), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_H), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_Z), "");

  tp_intset_destroy (freed_user_data);
  tp_intset_destroy (caught_signal);

  teardown ();

  return 0;
}
