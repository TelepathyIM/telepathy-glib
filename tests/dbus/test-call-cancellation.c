#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/proxy-subclass.h>    /* for _invalidated etc. */
#include <telepathy-glib/util.h>

#include "tests/myassert.h"
#include "tests/stub-object.h"

/* just for convenience, since it's used a lot */
#define PTR(ui) GUINT_TO_POINTER(ui)

/* state tracking */
static GMainLoop *mainloop;
static TpDBusDaemon *a;
static TpDBusDaemon *b;
static TpDBusDaemon *c;
static TpDBusDaemon *d;
static TpDBusDaemon *e;
static TpDBusDaemon *f;
static TpDBusDaemon *g;
static TpDBusDaemon *h;
static TpDBusDaemon *i;
static TpDBusDaemon *j;
static TpDBusDaemon *k;
static TpDBusDaemon *z;
static TpIntSet *method_ok;
static TpIntSet *method_error;
static TpIntSet *freed_user_data;
int fail = 0;
static gpointer copy_of_d;
static gpointer copy_of_g;
static gpointer copy_of_h;
static gpointer copy_of_i;

static void
myassert_failed (void)
{
  fail = 1;
}

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
    N_DAEMONS
};

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
listed_names (TpDBusDaemon *proxy,
              const gchar **names,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  guint which = GPOINTER_TO_UINT (user_data);
  TpDBusDaemon *want_proxy = NULL;
  GObject *want_object = NULL;

  if (error == NULL)
    {
      g_message ("ListNames() succeeded (first name: %s), according to "
          "user_data this was on proxy #%d '%c'", *names, which, 'a' + which);
      tp_intset_add (method_ok, which);

      switch (which)
        {
        case TEST_A:
          want_proxy = a;
          want_object = (GObject *) z;
          break;
        case TEST_C:
          want_proxy = c;
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
          want_proxy = z;
          want_object = (GObject *) a;
          break;
        default:
          MYASSERT (FALSE, ": %c (%p) method call succeeded, which shouldn't "
              "happen", 'a' + which, proxy);
          return;
        }
    }
  else
    {
      g_message ("ListNames() failed (%s), according to "
          "user_data this was on proxy #%d '%c'", error->message,
          which, 'a' + which);
      tp_intset_add (method_error, which);

      switch (which)
        {
        case TEST_C:
          want_proxy = c;
          want_object = NULL;
          break;
        case TEST_F:
          want_proxy = f;
          want_object = NULL;
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
    g_main_loop_quit (mainloop);
}

static void
noc (TpDBusDaemon *proxy,
     const gchar *name,
     const gchar *old,
     const gchar *new,
     gpointer user_data,
     GObject *weak_object)
{
  /* do nothing */
}

int
main (int argc,
      char **argv)
{
  GObject *b_stub, *i_stub, *j_stub, *k_stub;
  GError err = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Because I said so" };
  TpProxyPendingCall *pc;
  gpointer tmp_obj;

  g_type_init ();
  tp_debug_set_flags ("all");

  freed_user_data = tp_intset_sized_new (N_DAEMONS);
  method_ok = tp_intset_sized_new (N_DAEMONS);
  method_error = tp_intset_sized_new (N_DAEMONS);

  mainloop = g_main_loop_new (NULL, FALSE);

  /* We use TpDBusDaemon because it's a convenient concrete subclass of
   * TpProxy. */
  g_message ("Creating proxies");
  a = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("a=%p", a);
  b = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("b=%p", b);
  c = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("c=%p", c);
  d = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("d=%p", d);
  e = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("e=%p", e);
  f = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("f=%p", f);
  g = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("g=%p", g);
  h = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("h=%p", h);
  i = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("i=%p", i);
  j = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("j=%p", j);
  k = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("k=%p", k);
  z = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("z=%p", z);

  /* a survives */
  g_message ("Starting call on a");
  tp_cli_dbus_daemon_call_list_names (a, -1, listed_names, PTR (TEST_A),
      destroy_user_data, (GObject *) z);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_A), "");

  /* b gets its pending call cancelled because the weak object is
   * destroyed */
  b_stub = g_object_new (stub_object_get_type (), NULL);
  g_message ("Starting call on b");
  tp_cli_dbus_daemon_call_list_names (b, -1, listed_names, PTR (TEST_B),
      destroy_user_data, b_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_B), "");
  g_object_unref (b_stub);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_B), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_B), "");

  /* c is explicitly invalidated for an application-specific reason,
   * but its call still proceeds */
  g_message ("Starting call on c");
  tp_cli_dbus_daemon_call_list_names (c, -1, listed_names, PTR (TEST_C),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  g_message ("Forcibly invalidating c");
  tp_proxy_invalidate ((TpProxy *) c, &err);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_C), "");

  /* d gets unreferenced, but survives long enough for the call to complete
   * successfully later, because the pending call holds a reference */
  g_message ("Starting call on d");
  tp_cli_dbus_daemon_call_list_names (d, -1, listed_names, PTR (TEST_D),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  g_message ("Unreferencing d");
  copy_of_d = d;
  g_object_add_weak_pointer (copy_of_d, &copy_of_d);
  g_object_unref (d);
  d = NULL;
  MYASSERT (copy_of_d != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_D), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_D), "");

  /* e gets its method call cancelled explicitly */
  g_message ("Starting call on e");
  pc = tp_cli_dbus_daemon_call_list_names (e, -1, listed_names, PTR (TEST_E),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_E), "");
  g_message ("Cancelling call on e");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_E), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_E), "");

  /* f's method call fails with an error, because it's implicitly
   * invalidated by its DBusGProxy being destroyed.
   *
   * Note that this test case exploits implementation details of dbus-glib.
   * If it stops working after a dbus-glib upgrade, that's probably why. */
  g_message ("Starting call on f");
  tp_cli_dbus_daemon_call_list_names (f, -1, listed_names, PTR (TEST_F),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");
  g_message ("Forcibly disposing f's DBusGProxy to simulate name owner loss");
  tmp_obj = tp_proxy_borrow_interface_by_id ((TpProxy *) f,
      TP_IFACE_QUARK_DBUS_DAEMON, NULL);
  MYASSERT (tmp_obj != NULL, "");
  g_object_run_dispose (tmp_obj);
  /* the callback will be queued (to avoid reentrancy), so we don't get it
   * until the main loop runs */
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_F), "");

  /* g gets unreferenced, but survives long enough for the call to complete
   * successfully later, because the pending call holds a reference;
   * however, unlike case D, here the pending call weakly references the
   * proxy. This is never necessary, but is an interesting corner case that
   * should be tested. */
  g_message ("Starting call on g");
  tp_cli_dbus_daemon_call_list_names (g, -1, listed_names, PTR (TEST_G),
      destroy_user_data, (GObject *) g);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  g_message ("Unreferencing g");
  copy_of_g = g;
  g_object_add_weak_pointer (copy_of_g, &copy_of_g);
  g_object_unref (g);
  g = NULL;
  MYASSERT (copy_of_g != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_G), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_G), "");

  /* h gets unreferenced, *and* the call is cancelled (regression test for
   * fd.o #14576) */
  g_message ("Starting call on h");
  pc = tp_cli_dbus_daemon_call_list_names (h, -1, listed_names, PTR (TEST_H),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_H), "");
  g_message ("Unreferencing h");
  copy_of_h = h;
  g_object_add_weak_pointer (copy_of_h, &copy_of_h);
  g_object_unref (h);
  h = NULL;
  MYASSERT (copy_of_h != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_H), "");
  g_message ("Cancelling call on h");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_H), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_H), "");
  /* Now that it's been cancelled, h will have gone away */
  MYASSERT (copy_of_h == NULL, "");

  /* i gets its pending call cancelled because i_stub is
   * destroyed, *and* the pending call holds the last reference to it,
   * *and* there is a signal connection
   * (used to reproduce fd.o #14750 - see case h in test-disconnection.c
   * for the minimal regression test) */
  i_stub = g_object_new (stub_object_get_type (), NULL);
  tp_cli_dbus_daemon_connect_to_name_owner_changed (i, noc, PTR (TEST_I),
      NULL, i_stub, NULL);
  g_message ("Starting call on i");
  tp_cli_dbus_daemon_call_list_names (i, -1, listed_names, PTR (TEST_I),
      destroy_user_data, i_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_I), "");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (i, noc, PTR (TEST_I),
      NULL, i_stub, NULL);
  g_message ("Unreferencing i");
  copy_of_i = i;
  g_object_add_weak_pointer (copy_of_i, &copy_of_i);
  g_object_unref (i);
  i = NULL;
  MYASSERT (copy_of_i != NULL, "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_I), "");
  g_object_unref (i_stub);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_I), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_I), "");
  /* Now that it's been cancelled, i will have gone away */
  MYASSERT (copy_of_i == NULL, "");

  /* j gets its pending call cancelled explicitly, and j_stub is
   * destroyed in response (related to fd.o #14750) */
  j_stub = g_object_new (stub_object_get_type (), NULL);
  g_object_weak_ref (j_stub, j_stub_destroyed, PTR (TEST_J));
  g_message ("Starting call on j");
  pc = tp_cli_dbus_daemon_call_list_names (j, -1, listed_names, j_stub,
      g_object_unref, j_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_J), "");
  g_message ("Cancelling call on j");
  tp_proxy_pending_call_cancel (pc);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_J), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_J), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_J), "");

  /* k gets its pending call cancelled explicitly because its weak object
   * is destroyed, meaning there are simultaneously two reasons for it
   * to become cancelled (equivalent to fd.o#14750, but for pending calls
   * rather than signal connections) */
  k_stub = g_object_new (stub_object_get_type (), NULL);
  g_message ("Starting call on k");
  g_object_weak_ref (k_stub, k_stub_destroyed, &pc);
  pc = tp_cli_dbus_daemon_call_list_names (k, -1, listed_names, PTR (TEST_K),
      destroy_user_data, k_stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_K), "");
  g_object_unref (k_stub);
  MYASSERT (!tp_intset_is_member (method_ok, TEST_K), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_K), "");

  /* z survives too; we assume that method calls succeed in order,
   * so when z has had its reply, we can stop the main loop */
  g_message ("Starting call on z");
  tp_cli_dbus_daemon_call_list_names (z, -1, listed_names, PTR (TEST_Z),
      destroy_user_data, (GObject *) a);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_Z), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_Z), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_Z), "");

  g_message ("Running main loop");
  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);

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
  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
  MYASSERT (d == NULL, "");
  g_object_unref (e);
  g_object_unref (f);
  MYASSERT (g == NULL, "");
  MYASSERT (h == NULL, "");
  MYASSERT (i == NULL, "");
  g_object_unref (j);
  g_object_unref (z);

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

  tp_intset_destroy (freed_user_data);
  tp_intset_destroy (method_ok);
  tp_intset_destroy (method_error);

  return fail;
}
