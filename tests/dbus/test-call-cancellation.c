#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/util.h>

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
static TpDBusDaemon *z;
TpIntSet *method_ok;
TpIntSet *method_error;
TpIntSet *freed_user_data;
int fail = 0;
gpointer copy_of_d;

#define MYASSERT(x, m, ...) \
  do { \
      if (!(x)) \
        { \
          g_critical ("Assertion failed: %s" m, #x, ##__VA_ARGS__); \
          fail = 1; \
        } \
  } while(0)

enum {
    TEST_A,
    TEST_B,
    TEST_C,
    TEST_D,
    TEST_E,
    TEST_F,
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
listed_names (TpProxy *proxy,
              const gchar **names,
              const GError *error,
              gpointer user_data,
              GObject *weak_object)
{
  guint which = GPOINTER_TO_UINT (user_data);
  TpProxy *want_proxy = NULL;
  GObject *want_object = NULL;

  if (error == NULL)
    {
      g_message ("ListNames() succeeded (first name: %s), according to "
          "user_data this was on proxy #%d '%c'", *names, which, 'a' + which);
      tp_intset_add (method_ok, which);

      switch (which)
        {
        case TEST_A:
          want_proxy = (TpProxy *) a;
          want_object = (GObject *) z;
          break;
        case TEST_D:
          want_proxy = (TpProxy *) copy_of_d;
          want_object = NULL;
          break;
        case TEST_Z:
          want_proxy = (TpProxy *) z;
          want_object = (GObject *) a;
          break;
        default:
          MYASSERT (FALSE, "%c (%p) method call succeeded, which shouldn't "
              "happen", 'a' + which, proxy);
          fail = 1;
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
          want_proxy = (TpProxy *) c;
          want_object = NULL;
          break;
        case TEST_F:
          want_proxy = (TpProxy *) f;
          want_object = NULL;
          break;
        default:
          MYASSERT (FALSE, "%c (%p) method call failed, which shouldn't "
              "happen", 'a' + which, proxy);
          fail = 1;
        }
    }

  MYASSERT (proxy == want_proxy, ": Proxy is %p, expected %p", proxy,
      want_proxy);
  MYASSERT (weak_object == want_object, ": Weak object is %p, expected %p",
      weak_object, want_object);

  if (which == TEST_Z)
    g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  GObject *stub;
  GError err = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Because I said so" };
  const TpProxyPendingCall *pc;
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
  z = tp_dbus_daemon_new (tp_get_bus ());
  g_message ("z=%p", z);

  /* a survives */
  g_message ("Starting call on a");
  tp_cli_dbus_daemon_call_list_names (a, -1, listed_names, PTR (TEST_A),
      destroy_user_data, (GObject *) z);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_A), "");

  /* b gets its pending call cancelled because stub is
   * destroyed */
  stub = g_object_new (stub_object_get_type (), NULL);
  g_message ("Starting call on b");
  tp_cli_dbus_daemon_call_list_names (b, -1, listed_names, PTR (TEST_B),
      destroy_user_data, stub);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_B), "");
  g_object_unref (stub);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_B), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_B), "");

  /* c's call fails with error "Because I said so" because it's explicitly
   * invalidated */
  g_message ("Starting call on c");
  tp_cli_dbus_daemon_call_list_names (c, -1, listed_names, PTR (TEST_C),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_C), "");
  g_message ("Forcibly invalidating c");
  tp_proxy_invalidated ((TpProxy *) c, &err);
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_C), "");
  MYASSERT (tp_intset_is_member (method_error, TEST_C), "");

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

  /* e gets its signal connection cancelled explicitly */
  g_message ("Starting call on e");
  pc = tp_cli_dbus_daemon_call_list_names (e, -1, listed_names, PTR (TEST_E),
      destroy_user_data, NULL);
  MYASSERT (!tp_intset_is_member (freed_user_data, TEST_E), "");
  g_message ("Disconnecting signal from e");
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
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (!tp_intset_is_member (method_ok, TEST_F), "");
  MYASSERT (tp_intset_is_member (method_error, TEST_F), "");

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

  /* the calls have been delivered to both A and Z by now */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (tp_intset_is_member (method_ok, TEST_A), "");
  MYASSERT (!tp_intset_is_member (method_error, TEST_A), "");

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
  g_object_unref (z);

  /* we should already have checked each of these at least once, but just to
   * make sure we have a systematic test that all user data is freed... */
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_A), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_B), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_C), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_D), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_E), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_F), "");
  MYASSERT (tp_intset_is_member (freed_user_data, TEST_Z), "");

  tp_intset_destroy (freed_user_data);
  tp_intset_destroy (method_ok);
  tp_intset_destroy (method_error);

  return fail;
}
