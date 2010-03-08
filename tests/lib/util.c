/* Simple utility code used by the regression tests.
 *
 * Copyright © 2008-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "tests/lib/util.h"

static void
conn_ready_cb (TpConnection *conn G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data)
{
  GMainLoop *loop = user_data;

  test_assert_no_error (error);
  g_main_loop_quit (loop);
}

void
test_connection_run_until_ready (TpConnection *conn)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  if (tp_connection_is_ready (conn))
    return;

  tp_connection_call_when_ready (conn, conn_ready_cb, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}

static void
cm_ready_cb (TpConnectionManager *cm G_GNUC_UNUSED,
             const GError *error,
             gpointer user_data,
             GObject *weak_object G_GNUC_UNUSED)
{
  GMainLoop *loop = user_data;

  test_assert_no_error (error);
  g_main_loop_quit (loop);
}

void
test_connection_manager_run_until_ready (TpConnectionManager *cm)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  if (tp_connection_manager_is_ready (cm))
    return;

  tp_connection_manager_call_when_ready (cm, cm_ready_cb, loop, NULL,
      NULL);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}

void
test_proxy_run_until_dbus_queue_processed (gpointer proxy)
{
  tp_cli_dbus_introspectable_run_introspect (proxy, -1, NULL, NULL, NULL);
}

void
test_connection_run_until_dbus_queue_processed (TpConnection *connection)
{
  tp_cli_connection_run_get_protocol (connection, -1, NULL, NULL, NULL);
}

typedef struct {
    GMainLoop *loop;
    TpHandle handle;
} HandleRequestResult;

static void
handles_requested_cb (TpConnection *connection G_GNUC_UNUSED,
    TpHandleType handle_type G_GNUC_UNUSED,
    guint n_handles,
    const TpHandle *handles,
    const gchar * const *ids G_GNUC_UNUSED,
    const GError *error,
    gpointer user_data,
    GObject *weak_object G_GNUC_UNUSED)
{
  HandleRequestResult *result = user_data;

  test_assert_no_error (error);
  g_assert_cmpuint (n_handles, ==, 1);
  result->handle = handles[0];
}

static void
handle_request_result_finish (gpointer r)
{
  HandleRequestResult *result = r;

  g_main_loop_quit (result->loop);
}

TpHandle
test_connection_run_request_contact_handle (TpConnection *connection,
    const gchar *id)
{
  HandleRequestResult result = { g_main_loop_new (NULL, FALSE), 0 };
  const gchar * const ids[] = { id, NULL };

  tp_connection_request_handles (connection, -1, TP_HANDLE_TYPE_CONTACT, ids,
      handles_requested_cb, &result, handle_request_result_finish, NULL);
  g_main_loop_run (result.loop);
  g_main_loop_unref (result.loop);
  return result.handle;
}

void
_test_assert_no_error (const GError *error,
                       const char *file,
                       int line)
{
  if (error != NULL)
    {
      g_error ("%s:%d:%s: code %u: %s",
          file, line, g_quark_to_string (error->domain),
          error->code, error->message);
    }
}
