#include "config.h"

#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/proxy-subclass.h>    /* for _invalidated etc. */
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-channel-dispatcher.h"
#include "tests/lib/stub-object.h"
#include "tests/lib/util.h"

/* just for convenience, since it's used a lot */
#define PTR(ui) GUINT_TO_POINTER(ui)

/* state tracking (FIXME: move this into the Fixture) */
static TpIntset *method_ok;
static TpIntset *method_error;
static TpIntset *freed_user_data;
static gpointer copy_of_d;
static gpointer copy_of_g;
static gpointer copy_of_h;
static gpointer copy_of_i;

enum {
    TEST_A,
    TEST_B,
    TEST_C,
    TEST_D,
    TEST_E,
    TEST_F,
    TEST_G,
    TEST_H,
    TEST_I,
    TEST_J,
    TEST_K,
    TEST_Z = 25,
    N_PROXIES
};

typedef struct {
    GTestDBus *test_dbus;

    TpClientFactory *factory;
    TpProxy *proxies[N_PROXIES];
    GObject *cd_service;

    GDBusConnection *private_gdbus;
    TpClientFactory *private_factory;

    gboolean had_last_reply;
} Fixture;

/* FIXME: it would be better not to need this */
static Fixture *global_fixture;

static void
destroy_user_data (gpointer user_data)
{
  guint which = GPOINTER_TO_UINT (user_data);
  g_message ("User data %c destroyed", 'A' + which);
  tp_intset_add (freed_user_data, which);
}

static void
j_stub_destroyed (gpointer data,
                  GObject *stub)
{
  destroy_user_data (data);
}

static void
k_stub_destroyed (gpointer data,
                  GObject *stub)
{
  TpProxyPendingCall **p = data;

  tp_proxy_pending_call_cancel (*p);
}

static void
method_cb (TpProxy *proxy,
    GHashTable *props,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Fixture *f = global_fixture;
  guint which = GPOINTER_TO_UINT (user_data);
  TpProxy *want_proxy = NULL;
  GObject *want_object = NULL;

  if (error == NULL)
    {
      g_message ("GetAll() succeeded, according to "
          "user_data this was on proxy #%d '%c'", which, 'a' + which);
      tp_intset_add (method_ok, which);

      want_proxy = f->proxies[which];

      switch (which)
        {
        case TEST_A:
          want_object = (GObject *) f->proxies[TEST_Z];
          break;
        case TEST_C:
          want_object = NULL;
          break;
        case TEST_D:
          want_proxy = copy_of_d;
          want_object = NULL;
          break;
        case TEST_G:
          want_proxy = copy_of_g;
          want_object = (GObject *) copy_of_g;
          break;
        case TEST_Z:
          want_object = (GObject *) f->proxies[TEST_A];
          break;
        default:
          MYASSERT (FALSE, ": %c (%p) method call succeeded, which shouldn't "
              "happen", 'a' + which, proxy);
          return;
        }
    }
  else
    {
      g_message ("GetAll() failed, according to "
          "user_data this was on proxy #%d '%c'",
          which, 'a' + which);
      tp_intset_add (method_error, which);

      want_proxy = f->proxies[which];
      want_object = NULL;

      switch (which)
        {
        case TEST_C:
          break;
        case TEST_F:
          break;
        default:
          MYASSERT (FALSE, ": %c (%p) method call failed, which shouldn't "
              "happen", 'a' + which, proxy);
        }
    }

  MYASSERT (proxy == want_proxy, ": Proxy is %p, expected %p", proxy,
      want_proxy);
  MYASSERT (weak_object == want_object, ": Weak object is %p, expected %p",
      weak_object, want_object);

  if (which == TEST_Z)
    f->had_last_reply = TRUE;
}

static void
signal_cb (TpProxy *proxy,
     const gchar *iface,
     GHashTable *changed,
     const gchar **invalidated,
     gpointer user_data,
     GObject *weak_object)
{
  /* do nothing */
}

static void
setup (Fixture *f,
    gconstpointer data)
{
  TpDBusDaemon *dbus_daemon;
  GError *error = NULL;

  global_fixture = f;

  tp_tests_abort_after (10);
  tp_debug_set_flags ("all");

  freed_user_data = tp_intset_sized_new (N_PROXIES);
  method_ok = tp_intset_sized_new (N_PROXIES);
  method_error = tp_intset_sized_new (N_PROXIES);

  g_test_dbus_unset ();
  f->test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (f->test_dbus);

  f->factory = tp_client_factory_dup (&error);
  g_assert_no_error (error);
  dbus_daemon = tp_client_factory_get_dbus_daemon (f->factory);

  /* Any random object with an interface: what matters is that it can
   * accept a method call and emit a signal. We use the Properties
   * interface here. */
  f->cd_service = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_CHANNEL_DISPATCHER,
      NULL);
  tp_dbus_daemon_register_object (dbus_daemon, "/", f->cd_service);

  f->private_gdbus = tp_tests_get_private_bus ();
  g_assert (f->private_gdbus != NULL);
  dbus_daemon = tp_dbus_daemon_new (f->private_gdbus);
  g_assert (dbus_daemon != NULL);
  f->private_factory = tp_client_factory_new (dbus_daemon);
  g_object_unref (dbus_daemon);
}

static void
drop_private_connection (Fixture *f)
{
  g_dbus_connection_flush_sync (f->private_gdbus, NULL, NULL);
  g_dbus_connection_close_sync (f->private_gdbus, NULL, NULL);
  g_clear_object (&f->private_gdbus);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  tp_tests_assert_last_unref (&f->cd_service);
  tp_tests_assert_last_unref (&f->factory);

  tp_tests_assert_last_unref (&f->private_factory);

  g_test_dbus_down (f->test_dbus);
  tp_tests_assert_last_unref (&f->test_dbus);

  tp_intset_destroy (freed_user_data);
  tp_intset_destroy (method_ok);
  tp_intset_destroy (method_error);

  global_fixture = NULL;
}

static TpProxy *
new_proxy (Fixture *f,
    int which)
{
  TpClientFactory *local_factory;
  TpDBusDaemon *local_dbus_daemon;

  if (which == TEST_F)
    local_factory = f->private_factory;
  else
    local_factory = f->factory;

  local_dbus_daemon = tp_client_factory_get_dbus_daemon (local_factory);

  return tp_tests_object_new_static_class (TP_TYPE_PROXY,
      "dbus-daemon", local_dbus_daemon,
      "bus-name", tp_dbus_daemon_get_unique_name (
          tp_client_factory_get_dbus_daemon (f->factory)),
      "object-path", "/",
      "factory", local_factory,
      NULL);
}

static void
test (Fixture *f,
    gconstpointer data)
{
  GObject *b_stub, *i_stub, *j_stub, *k_stub;
  GError err = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT, "Because I said so" };
  TpProxyPendingCall *pc;
  guint i;

  g_message ("Creating proxies");

  for (i = TEST_A; i <= TEST_K; i++)
    {
      f->proxies[i] = new_proxy (f, i);
      g_message ("%c=%p", 'a' + i, f->proxies[i]);
    }

  f->proxies[TEST_Z] = new_proxy (f, TEST_Z);
  g_message ("z=%p", f->proxies[TEST_Z]);

  /* a survives */
  g_message ("Starting call on a");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_A], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_A),
      destroy_user_data, (GObject *) f->proxies[TEST_Z]);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_A), "");

  /* b gets its pending call cancelled because the weak object is
   * destroyed */
  b_stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  g_message ("Starting call on b");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_B], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_B),
      destroy_user_data, b_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_B), "");
  tp_tests_assert_last_unref (&b_stub);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_B), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_B), "");

  /* c is explicitly invalidated for an application-specific reason,
   * but its call still proceeds */
  g_message ("Starting call on c");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_C], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_C),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  g_message ("Forcibly invalidating c");
  tp_proxy_invalidate (f->proxies[TEST_C], &err);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_C), "");

  /* d gets unreferenced, but survives long enough for the call to complete
   * successfully later, because the pending call holds a reference */
  g_message ("Starting call on d");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_D], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_D),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  g_message ("Unreferencing d");
  copy_of_d = f->proxies[TEST_D];
  g_object_add_weak_pointer (copy_of_d, &copy_of_d);
  g_clear_object (&f->proxies[TEST_D]);
  MYASSERT (copy_of_d != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_D), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_D), "");

  /* e gets its method call cancelled explicitly */
  g_message ("Starting call on e");
  pc = tp_cli_dbus_properties_call_get_all (f->proxies[TEST_E], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_E),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_E), "");
  g_message ("Cancelling call on e");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_E), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_E), "");

  /* f's method call fails with an error, because it's implicitly
   * invalidated by its own connection disconnecting. */
  g_message ("Starting call on f");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_F], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_F),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");

  /* g gets unreferenced, but survives long enough for the call to complete
   * successfully later, because the pending call holds a reference;
   * however, unlike case D, here the pending call weakly references the
   * proxy. This is never necessary, but is an interesting corner case that
   * should be tested. */
  g_message ("Starting call on g");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_G], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_G),
      destroy_user_data, (GObject *) f->proxies[TEST_G]);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  g_message ("Unreferencing g");
  copy_of_g = f->proxies[TEST_G];
  g_object_add_weak_pointer (copy_of_g, &copy_of_g);
  g_clear_object (&f->proxies[TEST_G]);
  MYASSERT (copy_of_g != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_G), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_G), "");

  /* h gets unreferenced, *and* the call is cancelled (regression test for
   * fd.o #14576) */
  g_message ("Starting call on h");
  pc = tp_cli_dbus_properties_call_get_all (f->proxies[TEST_H], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_H),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_H), "");
  g_message ("Unreferencing h");
  copy_of_h = f->proxies[TEST_H];
  g_object_add_weak_pointer (copy_of_h, &copy_of_h);
  g_clear_object (&f->proxies[TEST_H]);
  MYASSERT (copy_of_h != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_H), "");
  g_message ("Cancelling call on h");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_H), "");

  /* i gets its pending call cancelled because i_stub is
   * destroyed, *and* the pending call holds the last reference to it,
   * *and* there is a signal connection
   * (used to reproduce fd.o #14750 - see case h in test-disconnection.c
   * for the minimal regression test) */
  i_stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_I],
      signal_cb, PTR (TEST_I), NULL, i_stub, NULL);
  g_message ("Starting call on i");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_I], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_I),
      destroy_user_data, i_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_I), "");
  tp_cli_dbus_properties_connect_to_properties_changed (f->proxies[TEST_I],
      signal_cb, PTR (TEST_I), NULL, i_stub, NULL);
  g_message ("Unreferencing i");
  copy_of_i = f->proxies[TEST_I];
  g_object_add_weak_pointer (copy_of_i, &copy_of_i);
  g_clear_object (&f->proxies[TEST_I]);
  MYASSERT (copy_of_i != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_I), "");
  tp_tests_assert_last_unref (&i_stub);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_I), "");

  /* j gets its pending call cancelled explicitly, and j_stub is
   * destroyed in response (related to fd.o #14750) */
  j_stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  g_object_weak_ref (j_stub, j_stub_destroyed, PTR (TEST_J));
  g_message ("Starting call on j");
  pc = tp_cli_dbus_properties_call_get_all (f->proxies[TEST_J], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, j_stub,
      g_object_unref, j_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_J), "");
  g_message ("Cancelling call on j");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_J), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_J), "");

  /* k gets its pending call cancelled explicitly because its weak object
   * is destroyed, meaning there are simultaneously two reasons for it
   * to become cancelled (equivalent to fd.o#14750, but for pending calls
   * rather than signal connections) */
  k_stub = tp_tests_object_new_static_class (tp_tests_stub_object_get_type (),
      NULL);
  g_message ("Starting call on k");
  g_object_weak_ref (k_stub, k_stub_destroyed, &pc);
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_K], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_K),
      destroy_user_data, k_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_K), "");
  tp_tests_assert_last_unref (&k_stub);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_K), "");

  /* z survives too; we assume that method calls succeed in order,
   * so when z has had its reply, we can stop the main loop */
  g_message ("Starting call on z");
  tp_cli_dbus_properties_call_get_all (f->proxies[TEST_Z], -1,
      TP_IFACE_CHANNEL_DISPATCHER, method_cb, PTR (TEST_Z),
      destroy_user_data, (GObject *) f->proxies[TEST_A]);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_Z), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_Z), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_Z), "");

  g_message ("Dropping private D-Bus connection");
  drop_private_connection (f);
  /* the callback will be queued (to avoid reentrancy), so we don't get it
   * until the main loop runs */
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_F), "");

  g_message ("Running main loop");

  /* There's no guarantee that proxy F will detect that its socket closed
   * in any particular order relative to the signals, so wait for both. */
  while (!f->had_last_reply ||
      tp_proxy_get_invalidated (f->proxies[TEST_F]) == NULL)
    g_main_context_iteration (NULL, TRUE);

  /* now that the calls have been delivered, d will finally have gone away */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_D), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_D), "");
  MYASSERT (copy_of_d == NULL, "");

  /* ... and g too */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_G), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_G), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_G), "");
  MYASSERT (copy_of_g == NULL, "");

  /* also, F will have been invalidated */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_F), "");
  MYASSERT (tp_intset_is_member (method_error, TEST_F), "");

  /* Now that its call has been cancelled, h will have gone away. Likewise
   * for i */
  MYASSERT (copy_of_h == NULL, "");
  MYASSERT (copy_of_i == NULL, "");

  /* User data for all the cancelled calls has also gone away */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_H), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_I), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_J), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_K), "");

  /* the calls have been delivered to A, C and Z by now */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_A), "");

  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_C), "");

  MYASSERT (tp_intset_is_member (freed_user_data, TEST_Z), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_Z), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_Z), "");

  g_message ("Dereferencing remaining proxies");
  tp_tests_assert_last_unref (&f->proxies[TEST_A]);
  tp_tests_assert_last_unref (&f->proxies[TEST_B]);
  tp_tests_assert_last_unref (&f->proxies[TEST_C]);
  g_assert (f->proxies[TEST_D] == NULL);
  tp_tests_assert_last_unref (&f->proxies[TEST_E]);
  tp_tests_assert_last_unref (&f->proxies[TEST_F]);
  g_assert (f->proxies[TEST_G] == NULL);
  g_assert (f->proxies[TEST_H] == NULL);
  g_assert (f->proxies[TEST_I] == NULL);
  tp_tests_assert_last_unref (&f->proxies[TEST_J]);
  tp_tests_assert_last_unref (&f->proxies[TEST_K]);
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
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_I), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_J), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_K), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_Z), "");
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/call-cancellation", Fixture, NULL, setup, test, teardown);

  return g_test_run ();
}
