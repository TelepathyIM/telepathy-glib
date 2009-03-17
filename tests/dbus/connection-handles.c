/* Feature test for handle reference tracking.
 *
 * Code missing coverage in connection-handles.c:
 * - having two connections, one of them becoming invalid
 * - unreffing handles on a dead connection
 * - failing to request handles
 * - inconsistent CMs
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *loop;
    GError *error /* initialized to 0 */;
    GArray *handles;
    gchar **ids;
} Result;

static void
requested (TpConnection *connection,
           TpHandleType handle_type,
           guint n_handles,
           const TpHandle *handles,
           const gchar * const *ids,
           const GError *error,
           gpointer user_data,
           GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->ids == NULL);
  g_assert (result->handles == NULL);
  g_assert (result->error == NULL);

  if (error == NULL)
    {
      DEBUG ("got %u handles", n_handles);
      result->ids = g_strdupv ((GStrv) ids);
      result->handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle),
          n_handles);
      g_array_append_vals (result->handles, handles, n_handles);
    }
  else
    {
      DEBUG ("got an error");
      result->error = g_error_copy (error);
    }
}

static void
held (TpConnection *connection,
      TpHandleType handle_type,
      guint n_handles,
      const TpHandle *handles,
      const GError *error,
      gpointer user_data,
      GObject *weak_object)
{
  Result *result = user_data;

  g_assert (result->ids == NULL);
  g_assert (result->handles == NULL);
  g_assert (result->error == NULL);

  if (error == NULL)
    {
      DEBUG ("got %u handles", n_handles);
      result->handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle),
          n_handles);
      g_array_append_vals (result->handles, handles, n_handles);
    }
  else
    {
      DEBUG ("got an error");
      result->error = g_error_copy (error);
    }
}

static void
finish (gpointer r)
{
  Result *result = r;

  g_main_loop_quit (result->loop);
}

/*
 * Assert that RequestHandles + unref releases the handles.
 */
static void
test_request_and_release (SimpleConnection *service_conn,
                          TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  const gchar * const ids[] = { "alice", "bob", "chris", NULL };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  guint i;

  g_message (G_STRFUNC);

  /* request three handles */

  tp_connection_request_handles (client_conn, -1,
      TP_HANDLE_TYPE_CONTACT, ids, requested, &result, finish, NULL);

  g_main_loop_run (result.loop);

  test_assert_no_error (result.error);
  MYASSERT (result.ids != NULL, "");
  MYASSERT (result.handles != NULL, "");

  for (i = 0; i < 3; i++)
    {
      MYASSERT (result.ids[i] != NULL, " [%u]", i);
      MYASSERT (!tp_strdiff (result.ids[i], ids[i]), " [%u] %s != %s",
          i, result.ids[i], ids[i]);
    }

  MYASSERT (result.ids[3] == NULL, "");

  MYASSERT (result.handles->len == 3, ": %u != 3", result.handles->len);

  /* check that the service and the client agree */

  MYASSERT (tp_handles_are_valid (service_repo, result.handles, FALSE, NULL),
      "");

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = g_array_index (result.handles, TpHandle, i);

      MYASSERT (!tp_strdiff (tp_handle_inspect (service_repo, handle), ids[i]),
          "%s != %s", tp_handle_inspect (service_repo, handle), ids[i]);
    }

  /* release the handles */

  tp_connection_unref_handles (client_conn, TP_HANDLE_TYPE_CONTACT,
      result.handles->len, (const TpHandle *) result.handles->data);
  test_connection_run_until_dbus_queue_processed (client_conn);

  /* check that the handles have been released */

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = g_array_index (result.handles, TpHandle, i);

      MYASSERT (!tp_handle_is_valid (service_repo, handle, NULL), "");
    }

  /* clean up */

  g_strfreev (result.ids);
  g_array_free (result.handles, TRUE);
  g_assert (result.error == NULL);
  g_main_loop_unref (result.loop);
}

/*
 * Assert that RequestHandles + HoldHandles + unref does not release the
 * handles, but a second unref does.
 */
static void
test_request_hold_release (SimpleConnection *service_conn,
                           TpConnection *client_conn)
{
  Result result = { g_main_loop_new (NULL, FALSE), NULL, NULL, NULL };
  const gchar * const ids[] = { "alice", "bob", "chris", NULL };
  TpHandleRepoIface *service_repo = tp_base_connection_get_handles (
      (TpBaseConnection *) service_conn, TP_HANDLE_TYPE_CONTACT);
  guint i;
  GArray *saved_handles;

  g_message (G_STRFUNC);

  /* request three handles */

  tp_connection_request_handles (client_conn, -1,
      TP_HANDLE_TYPE_CONTACT, ids, requested, &result, finish, NULL);

  g_main_loop_run (result.loop);

  test_assert_no_error (result.error);
  MYASSERT (result.ids != NULL, "");
  MYASSERT (result.handles != NULL, "");

  for (i = 0; i < 3; i++)
    {
      MYASSERT (result.ids[i] != NULL, " [%u]", i);
      MYASSERT (!tp_strdiff (result.ids[i], ids[i]), " [%u] %s != %s",
          i, result.ids[i], ids[i]);
    }

  MYASSERT (result.ids[3] == NULL, "");

  MYASSERT (result.handles->len == 3, ": %u != 3", result.handles->len);

  /* check that the service and the client agree */

  MYASSERT (tp_handles_are_valid (service_repo, result.handles, FALSE, NULL),
      "");

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = g_array_index (result.handles, TpHandle, i);

      MYASSERT (!tp_strdiff (tp_handle_inspect (service_repo, handle), ids[i]),
          "%s != %s", tp_handle_inspect (service_repo, handle), ids[i]);
    }

  /* hold the handles */

  g_strfreev (result.ids);
  result.ids = NULL;
  saved_handles = result.handles;
  result.handles = NULL;
  g_assert (result.error == NULL);

  tp_connection_hold_handles (client_conn, -1,
      TP_HANDLE_TYPE_CONTACT, saved_handles->len,
      (const TpHandle *) saved_handles->data,
      held, &result, finish, NULL);

  g_main_loop_run (result.loop);

  test_assert_no_error (result.error);
  MYASSERT (result.ids == NULL, "");
  MYASSERT (result.handles != NULL, "");

  for (i = 0; i < 3; i++)
    {
      TpHandle want = g_array_index (saved_handles, TpHandle, i);
      TpHandle got = g_array_index (result.handles, TpHandle, i);

      MYASSERT (want == got, "%u != %u", want, got);
    }

  g_array_free (saved_handles, TRUE);

  /* unref the handles */

  tp_connection_unref_handles (client_conn, TP_HANDLE_TYPE_CONTACT,
      result.handles->len, (const TpHandle *) result.handles->data);
  test_connection_run_until_dbus_queue_processed (client_conn);

  /* check that the handles have not been released */

  MYASSERT (tp_handles_are_valid (service_repo, result.handles, FALSE, NULL),
      "");

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = g_array_index (result.handles, TpHandle, i);

      MYASSERT (!tp_strdiff (tp_handle_inspect (service_repo, handle), ids[i]),
          "%s != %s", tp_handle_inspect (service_repo, handle), ids[i]);
    }

  /* release the handles by unreffing them again */

  tp_connection_unref_handles (client_conn, TP_HANDLE_TYPE_CONTACT,
      result.handles->len, (const TpHandle *) result.handles->data);
  test_connection_run_until_dbus_queue_processed (client_conn);

  /* check that the handles have been released */

  for (i = 0; i < 3; i++)
    {
      TpHandle handle = g_array_index (result.handles, TpHandle, i);

      MYASSERT (!tp_handle_is_valid (service_repo, handle, NULL), "");
    }

  /* clean up */

  g_main_loop_unref (result.loop);
  g_strfreev (result.ids);
  g_array_free (result.handles, TRUE);
  g_assert (result.error == NULL);
}

int
main (int argc,
      char **argv)
{
  TpDBusDaemon *dbus;
  SimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error = NULL;
  TpConnection *client_conn;

  /* Setup */

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_dbus_daemon_new (tp_get_bus ());

  service_conn = SIMPLE_CONNECTION (g_object_new (
        SIMPLE_TYPE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);

  client_conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (client_conn != NULL, "");
  test_assert_no_error (error);
  MYASSERT (tp_connection_run_until_ready (client_conn, TRUE, &error, NULL),
      "");
  test_assert_no_error (error);

  /* Tests */

  test_request_and_release (service_conn, client_conn);
  test_request_hold_release (service_conn, client_conn);

  /* Teardown */

  MYASSERT (tp_cli_connection_run_disconnect (client_conn, -1, &error, NULL),
      "");
  test_assert_no_error (error);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);

  return 0;
}
