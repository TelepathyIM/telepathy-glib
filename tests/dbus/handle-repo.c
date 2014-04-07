#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/errors.h>

#include "tests/lib/util.h"

typedef struct {
    GTestDBus *test_dbus;
} Fixture;

static void
test_handles (Fixture *f,
    gconstpointer data)
{
  GDBusConnection *bus_connection = tp_tests_dbus_dup_or_die ();
  TpHandleRepoIface *tp_repo = NULL;
  GError *error = NULL;
  TpHandle handle = 0;
  const gchar *jid = "handle.test@foobar";
  const gchar *return_jid;

  tp_repo = tp_tests_object_new_static_class (TP_TYPE_DYNAMIC_HANDLE_REPO,
      "entity-type", TP_ENTITY_TYPE_CONTACT,
      NULL);
  g_assert (tp_repo != NULL);

  /* Handle zero is never valid */
  g_assert (tp_handle_is_valid (tp_repo, 0, &error) == FALSE);
  /* this should probably be InvalidHandle, but it was InvalidArgument in
   * older telepathy-glib */
  g_assert (error->code == TP_ERROR_INVALID_ARGUMENT ||
      error->code == TP_ERROR_INVALID_HANDLE);

  g_error_free (error);
  error = NULL;

  /* Properly return error when handle isn't in the repo */
  g_assert (tp_handle_is_valid (tp_repo, 65536, &error) == FALSE);
  /* this should really be InvalidHandle, but it was InvalidArgument in
   * older telepathy-glib */
  g_assert (error->code == TP_ERROR_INVALID_ARGUMENT ||
      error->code == TP_ERROR_INVALID_HANDLE);

  g_error_free (error);
  error = NULL;

  /* Properly return when error out argument isn't provided */
  g_assert (tp_handle_is_valid (tp_repo, 65536, NULL) == FALSE);

  /* It's not there to start with */
  handle = tp_handle_lookup (tp_repo, jid, NULL, NULL);
  g_assert (handle == 0);
  /* ... but when we call tp_handle_ensure we get a new ref to it [ref 1] */
  handle = tp_handle_ensure (tp_repo, jid, NULL, NULL);
  g_assert (handle != 0);

  /* Try to inspect it */
  return_jid = tp_handle_inspect (tp_repo, handle);
  g_assert (!strcmp (return_jid, jid));

  /* Validate it, should be all healthy because client holds it still */
  g_assert (tp_handle_is_valid (tp_repo, handle, NULL) == TRUE);

  /* wait for D-Bus to catch up (just to detect any crashes) but don't assert
   * that the handle doesn't remain valid - unref is a no-op since
   * 0.13.8 */
  tp_tests_proxy_run_until_dbus_queue_processed (bus_connection);

  g_object_unref (tp_repo);
  g_object_unref (bus_connection);
}

static void
setup (Fixture *f,
    gconstpointer data)
{
  tp_tests_abort_after (10);

  g_test_dbus_unset ();
  f->test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (f->test_dbus);
}

static void
teardown (Fixture *f,
    gconstpointer data)
{
  g_test_dbus_down (f->test_dbus);
  tp_tests_assert_last_unref (&f->test_dbus);
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/handle-repo", Fixture, NULL, setup, test_handles, teardown);

  return g_test_run ();
}
