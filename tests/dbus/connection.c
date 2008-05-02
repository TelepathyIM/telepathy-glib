/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=15300
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"

static int fail = 0;
static GMainLoop *mainloop;

static void
myassert_failed (void)
{
  fail = 1;
}

static GError invalidated_for_test = { 0, TP_ERROR_PERMISSION_DENIED,
      "No connection for you!" };

static void
test_run_until_invalid (TpDBusDaemon *dbus,
                        SimpleConnection *service_conn,
                        const gchar *name,
                        const gchar *conn_path)
{
  TpConnection *conn;
  GError *error = NULL;

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);
  tp_proxy_invalidate ((TpProxy *) conn, &invalidated_for_test);

  MYASSERT (!tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  MYASSERT (error != NULL, "");
  MYASSERT_SAME_ERROR (&invalidated_for_test, error);
  g_error_free (error);

  g_object_unref (conn);
}

static void
test_run_until_ready (TpDBusDaemon *dbus,
                      SimpleConnection *service_conn,
                      const gchar *name,
                      const gchar *conn_path)
{
  TpConnection *conn;
  GError *error = NULL;

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);

  g_object_unref (conn);
}

typedef struct {
    gboolean ready;
    GError *error;
    GMainLoop *mainloop;
} WhenReadyContext;

static void
conn_ready (TpConnection *connection,
            const GError *error,
            gpointer user_data)
{
  WhenReadyContext *ctx = user_data;

  ctx->ready = TRUE;

  if (error == NULL)
    {
      g_message ("connection %p ready", connection);
    }
  else
    {
      g_message ("connection %p invalidated: %s #%u \"%s\"", connection,
          g_quark_to_string (error->domain), error->code, error->message);

      ctx->error = g_error_copy (error);
    }

  if (ctx->mainloop != NULL)
    g_main_loop_quit (ctx->mainloop);
}

static void
test_call_when_ready (TpDBusDaemon *dbus,
                      SimpleConnection *service_conn,
                      const gchar *name,
                      const gchar *conn_path)
{
  TpConnection *conn;
  GError *error = NULL;
  WhenReadyContext ctx = { FALSE, NULL, mainloop };

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);

  tp_connection_call_when_ready (conn, conn_ready, &ctx);
  g_message ("Entering main loop");
  g_main_loop_run (mainloop);
  g_message ("Leaving main loop");
  MYASSERT (ctx.ready == TRUE, "");
  MYASSERT_NO_ERROR (ctx.error);

  /* Connection already ready, so we are called back synchronously */

  ctx.ready = FALSE;
  tp_connection_call_when_ready (conn, conn_ready, &ctx);
  MYASSERT (ctx.ready == TRUE, "");
  MYASSERT_NO_ERROR (ctx.error);

  g_object_unref (conn);
}

static void
test_call_when_invalid (TpDBusDaemon *dbus,
                        SimpleConnection *service_conn,
                        const gchar *name,
                        const gchar *conn_path)
{
  TpConnection *conn;
  GError *error = NULL;
  WhenReadyContext ctx = { FALSE, NULL, mainloop };

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);

  /* Connection becomes invalid, so we are called back synchronously */

  ctx.ready = FALSE;
  tp_connection_call_when_ready (conn, conn_ready, &ctx);
  tp_proxy_invalidate ((TpProxy *) conn, &invalidated_for_test);
  MYASSERT (ctx.ready == TRUE, "");
  MYASSERT_SAME_ERROR (&invalidated_for_test, ctx.error);
  g_error_free (ctx.error);
  ctx.error = NULL;

  /* Connection already invalid, so we are called back synchronously */

  ctx.ready = FALSE;
  tp_connection_call_when_ready (conn, conn_ready, &ctx);
  MYASSERT (ctx.ready == TRUE, "");
  MYASSERT_SAME_ERROR (&invalidated_for_test, ctx.error);
  g_error_free (ctx.error);
  ctx.error = NULL;

  g_object_unref (conn);
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
  TpConnection *conn;

  invalidated_for_test.domain = TP_ERRORS;

  g_type_init ();
  tp_debug_set_flags ("all");
  mainloop = g_main_loop_new (NULL, FALSE);
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
  MYASSERT_NO_ERROR (error);

  test_run_until_invalid (dbus, service_conn, name, conn_path);
  test_run_until_ready (dbus, service_conn, name, conn_path);
  test_call_when_ready (dbus, service_conn, name, conn_path);
  test_call_when_invalid (dbus, service_conn, name, conn_path);

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  MYASSERT_NO_ERROR (error);
  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  MYASSERT_NO_ERROR (error);
  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  MYASSERT_NO_ERROR (error);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);

  g_object_unref (dbus);
  g_main_loop_unref (mainloop);

  return fail;
}
