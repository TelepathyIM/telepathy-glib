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

#include "config.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
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
finish (gpointer r)
{
  Result *result = r;

  g_main_loop_quit (result->loop);
}

/*
 * Assert that RequestHandles + unref doesn't crash. (It doesn't
 * do anything any more, however.)
 */
static void
test_request_and_release (TpTestsSimpleConnection *service_conn,
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

  g_assert_no_error (result.error);
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

  /* this used to release the handles but let's just leave this for
   * now */

  tp_tests_proxy_run_until_dbus_queue_processed (client_conn);

  /* clean up */

  g_strfreev (result.ids);
  g_array_unref (result.handles);
  g_assert (result.error == NULL);
  g_main_loop_unref (result.loop);
}

int
main (int argc,
      char **argv)
{
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpConnection *client_conn;

  /* Setup */

  tp_tests_abort_after (10);
  g_type_init ();
  tp_debug_set_flags ("all");

  tp_tests_create_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION, "me@example.com",
      TRUE, &service_conn_as_base, &client_conn);
  service_conn = TP_TESTS_SIMPLE_CONNECTION (service_conn_as_base);

  /* Tests */

  test_request_and_release (service_conn, client_conn);

  /* Teardown */

  tp_tests_connection_assert_disconnect_succeeds (client_conn);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);

  return 0;
}
