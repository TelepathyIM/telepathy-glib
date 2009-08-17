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

static void
test_handles (void)
{
  TpDBusDaemon *bus_daemon = tp_dbus_daemon_dup (NULL);
  TpHandleRepoIface *tp_repo = NULL;
  GError *error = NULL;
  TpHandle handle = 0;
  const gchar *jid = "handle.test@foobar";
  const gchar *return_jid;

  g_assert (bus_daemon != NULL);

  tp_repo = g_object_new (TP_TYPE_DYNAMIC_HANDLE_REPO,
      "handle-type", TP_HANDLE_TYPE_CONTACT,
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

  /* Hold the handle on behalf of a bus name that will not go away (my own)
   * [ref 2] */
  g_assert (tp_handle_client_hold (tp_repo,
        tp_dbus_daemon_get_unique_name (bus_daemon), handle, NULL) == TRUE);

  /* Now unref it [ref 1] */
  tp_handle_unref (tp_repo, handle);

  /* Validate it, should be all healthy because client holds it still */
  g_assert (tp_handle_is_valid (tp_repo, handle, NULL) == TRUE);

  /* Ref it again [ref 3] */
  tp_handle_ref (tp_repo, handle);

  /* Client releases it [ref 2] */
  g_assert (tp_handle_client_release (tp_repo,
        tp_dbus_daemon_get_unique_name (bus_daemon), handle, NULL) == TRUE);

  /* Hold the handle on behalf of a bus name that does not, in fact, exist;
   * this will be detected asynchronously [ref 4] */
  g_assert (tp_handle_client_hold (tp_repo, ":666.666", handle, NULL) == TRUE);

  /* Now unref it [ref 3] */
  tp_handle_unref (tp_repo, handle);

  /* ref 4 ensures still valid */
  g_assert (tp_handle_is_valid (tp_repo, handle, NULL) == TRUE);

  /* wait for D-Bus to catch up */
  test_proxy_run_until_dbus_queue_processed (bus_daemon);

  /* No more refs */
  g_assert (tp_handle_is_valid (tp_repo, handle, NULL) == FALSE);

  g_object_unref (tp_repo);
  g_object_unref (bus_daemon);
}

int main (int argc, char **argv)
{
  g_type_init ();

  test_handles ();

  return 0;
}
