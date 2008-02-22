#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/proxy-subclass.h>    /* for _invalidated etc. */
#include <telepathy-glib/util.h>

#include "tests/myassert.h"

typedef struct { GObject p; } StubObject;
typedef struct { GObjectClass p; } StubObjectClass;

static GType stub_object_get_type (void);

G_DEFINE_TYPE (StubObject, stub_object, G_TYPE_OBJECT)

static void
stub_object_class_init (StubObjectClass *klass)
{
}

static void
stub_object_init (StubObject *self)
{
}

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
static TpDBusDaemon *z;
TpIntSet *caught_signal;
TpIntSet *freed_user_data;
int fail = 0;

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
    TEST_Z = 25,
    N_DAEMONS
};

static void
destroy_user_data (gpointer user_data)
{
  guint which = GPOINTER_TO_UINT (user_data);
  g_message ("User data %c destroyed", 'A' + which);
  MYASSERT (!tp_intset_is_member (freed_user_data, which), "");
  tp_intset_add (freed_user_data, which);
}

static void
requested_name (TpDBusDaemon *proxy,
                guint result,
                const GError *error,
                gpointer user_data,
                GObject *weak_object)
{
  g_message ("RequestName raised %s",
      (error == NULL ? "no error" : error->message));
  /* we're on a private bus, so certainly nobody else should own this name */
  MYASSERT_NO_ERROR (error);
  MYASSERT (result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER, ": %u", result);
}

static void
prop_changed (TpProxy *proxy,
              const GPtrArray *properties,
              gpointer user_data,
              GObject *weak_object)
{
  g_critical ("prop_changed called - a signal connection which should have "
      "failed has succeeded. Args: proxy=%p user_data=%p", proxy, user_data);
  fail = 1;
}

static void
dummy_noc (TpDBusDaemon *proxy,
           const gchar *name,
           const gchar *old,
           const gchar *new,
           gpointer user_data,
           GObject *weak_object)
{
  g_critical ("dummy_noc called - a signal connection which should have "
      "failed has succeeded. Args: proxy=%p user_data=%p", proxy, user_data);
  fail = 1;
}

static void
noc (TpDBusDaemon *proxy,
     const gchar *name,
     const gchar *old,
     const gchar *new,
     gpointer user_data,
     GObject *weak_object)
{
  guint which = GPOINTER_TO_UINT (user_data);
  TpDBusDaemon *want_proxy = NULL;
  GObject *want_object = NULL;

  g_message ("Caught signal (%s: %s -> %s) with proxy #%d '%c' according to "
      "user_data", name, old, new, which, 'a' + which);
  g_message ("Proxy is %p, weak object is %p", proxy,
      weak_object);
  tp_intset_add (caught_signal, which);

  switch (which)
    {
    case TEST_A:
      want_proxy = a;
      want_object = (GObject *) z;
      break;
    case TEST_Z:
      want_proxy = z;
      want_object = (GObject *) a;
      break;
    default:
      g_critical ("%c (%p) got the signal, which shouldn't have happened",
          'a' + which, proxy);
      fail = 1;
      return;
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

int
main (int argc,
      char **argv)
{
  GObject *stub;
  GError *error_out = NULL;
  GError err = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Because I said so" };
  TpProxySignalConnection *sc;
  gpointer tmp_obj;
  gboolean freed = FALSE;

  g_type_init ();
  tp_debug_set_flags ("all");

  freed_user_data = tp_intset_sized_new (N_DAEMONS);
  caught_signal = tp_intset_sized_new (N_DAEMONS);

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
  z = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("z=%p", z);

  /* a survives */
  g_message ("Connecting signal to a");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (a, noc, PTR (TEST_A),
      destroy_user_data, (GObject *) z, &error_out);
  MYASSERT_NO_ERROR (error_out);

  /* assert that connecting to a signal on an interface we don't have fails */
  freed = FALSE;
  tp_cli_properties_interface_connect_to_properties_changed (a, prop_changed,
      &freed, set_freed, NULL, &error_out);
  MYASSERT (freed, "");
  MYASSERT (error_out != NULL, "");
  MYASSERT (error_out->code == TP_DBUS_ERROR_NO_INTERFACE, "");
  g_error_free (error_out);
  error_out = NULL;

  /* b gets its signal connection cancelled because stub is
   * destroyed */
  stub = g_object_new (stub_object_get_type (), NULL);
  g_message ("Connecting signal to b");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (b, noc, PTR (TEST_B),
      destroy_user_data, stub, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_B), "");
  g_object_unref (stub);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");

  /* c gets its signal connection cancelled because it's explicitly
   * invalidated */
  g_message ("Connecting signal to c");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (c, noc, PTR (TEST_C),
      destroy_user_data, NULL, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  g_message ("Forcibly invalidating c");
  tp_proxy_invalidate ((TpProxy *) c, &err);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  /* assert that connecting to a signal on an invalid proxy fails */
  freed = FALSE;
  tp_cli_dbus_daemon_connect_to_name_owner_changed (c, dummy_noc, &freed,
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
  tp_cli_dbus_daemon_connect_to_name_owner_changed (d, noc, PTR (TEST_D),
      destroy_user_data, NULL, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_D), "");
  g_message ("Destroying d");
  tmp_obj = d;
  g_object_add_weak_pointer (tmp_obj, &tmp_obj);
  g_object_unref (d);
  MYASSERT (tmp_obj == NULL, "");
  d = NULL;
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_D), "");

  /* e gets its signal connection cancelled explicitly */
  g_message ("Connecting signal to e");
  sc = tp_cli_dbus_daemon_connect_to_name_owner_changed (e, noc, PTR (TEST_E),
      destroy_user_data, NULL, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_E), "");
  g_message ("Disconnecting signal from e");
  tp_proxy_signal_connection_disconnect (sc);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");

  /* f gets its signal connection cancelled because it's implicitly
   * invalidated by its DBusGProxy being destroyed.
   *
   * Note that this test case exploits implementation details of dbus-glib.
   * If it stops working after a dbus-glib upgrade, that's probably why. */
  g_message ("Connecting signal to f");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (f, noc, PTR (TEST_F),
      destroy_user_data, NULL, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_F), "");
  g_message ("Forcibly disposing f's DBusGProxy to simulate name owner loss");
  tmp_obj = tp_proxy_borrow_interface_by_id ((TpProxy *) f,
      TP_IFACE_QUARK_DBUS_DAEMON, NULL);
  MYASSERT (tmp_obj != NULL, "");
  g_object_run_dispose (tmp_obj);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  /* assert that connecting to a signal on an invalid proxy fails */
  freed = FALSE;
  tp_cli_dbus_daemon_connect_to_name_owner_changed (f, dummy_noc, &freed,
      set_freed, NULL, &error_out);
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
  tp_cli_dbus_daemon_connect_to_name_owner_changed (g, noc, PTR (TEST_G),
      destroy_user_data, (GObject *) g, &error_out);
  MYASSERT_NO_ERROR (error_out);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_G), "");
  g_message ("Destroying g");
  tmp_obj = g;
  g_object_add_weak_pointer (tmp_obj, &tmp_obj);
  g_object_unref (g);
  MYASSERT (tmp_obj == NULL, "");
  g = NULL;
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_G), "");

  /* z survives; we assume that the signals are delivered in either
   * forward or reverse order, so if both a and z have had their signal, we
   * can stop the main loop */
  g_message ("Connecting signal to z");
  tp_cli_dbus_daemon_connect_to_name_owner_changed (z, noc, PTR (TEST_Z),
      destroy_user_data, (GObject *) a, &error_out);
  MYASSERT_NO_ERROR (error_out);

  /* make sure a NameOwnerChanged signal occurs */
  g_message ("Requesting name");
  tp_cli_dbus_daemon_call_request_name (a, -1, "com.example.NameTest",
      0, requested_name, NULL, NULL, NULL);

  g_message ("Running main loop");
  g_main_loop_run (mainloop);
  g_main_loop_unref (mainloop);

  /* both A and Z are still listening for signals, so their user data is
   * still held */
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_Z), "");

  g_message ("Dereferencing remaining proxies");
  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);
  MYASSERT (d == NULL, "");
  g_object_unref (e);
  g_object_unref (f);
  MYASSERT (g == NULL, "");
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
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_Z), "");

  tp_intset_destroy (freed_user_data);
  tp_intset_destroy (caught_signal);

  return fail;
}
